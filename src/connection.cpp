#include "connection.h"
#include <thread>

inline Napi::Value ConvertField(Napi::Env env, const pqxx::field& field, int type) {
    if (field.is_null()) return env.Null();
    
    switch (type) {
        case 23: case 20: case 21: // int4, int8, int2
            return Napi::Number::New(env, field.as<long long>());
        case 16: // bool
            return Napi::Boolean::New(env, field.as<bool>());
        case 17: // bytea
            return Napi::Buffer<uint8_t>::Copy(env, (uint8_t*)field.c_str(), field.size());
        case 700: case 701: // float4, float8
            return Napi::Number::New(env, field.as<double>());
        default: // text, varchar, etc
            return Napi::String::New(env, field.c_str(), field.size());
    }
}

struct QueryWorker : Napi::AsyncWorker {
    std::shared_ptr<ConnectionPool> pool;
    std::string sql;
    std::vector<std::string> params;
    pqxx::result result;
    Napi::Promise::Deferred deferred;
    
    QueryWorker(Napi::Env env, std::shared_ptr<ConnectionPool> p, std::string s, std::vector<std::string> prm, Napi::Promise::Deferred d)
        : AsyncWorker(env), pool(p), sql(std::move(s)), params(std::move(prm)), deferred(d) {}
    
    void Execute() override {
        auto conn = pool->acquire();
        
        // Use nontransaction for SELECT queries (faster)
        bool isSelect = sql.size() >= 6 && 
                       (sql[0] == 'S' || sql[0] == 's') &&
                       (sql[1] == 'E' || sql[1] == 'e') &&
                       (sql[2] == 'L' || sql[2] == 'l');
        
        if (isSelect) {
            pqxx::nontransaction txn(*conn);
            result = params.empty() ? txn.exec(sql) : txn.exec_params(sql, pqxx::prepare::make_dynamic_params(params));
        } else {
            pqxx::work txn(*conn);
            result = params.empty() ? txn.exec(sql) : txn.exec_params(sql, pqxx::prepare::make_dynamic_params(params));
            txn.commit();
        }
        
        pool->release(conn);
    }
    
    void OnOK() override {
        auto env = Env();
        size_t rowCount = result.size();
        auto rows = Napi::Array::New(env, rowCount);
        
        if (rowCount == 0) {
            deferred.Resolve(rows);
            return;
        }
        
        size_t colCount = result.columns();
        
        for (size_t i = 0; i < rowCount; ++i) {
            auto row = Napi::Object::New(env);
            const auto& dbRow = result[i];
            
            for (size_t j = 0; j < colCount; ++j) {
                row.Set(result.column_name(j), ConvertField(env, dbRow[j], result.column_type(j)));
            }
            rows[i] = row;
        }
        deferred.Resolve(rows);
    }
    
    void OnError(const Napi::Error& e) override {
        deferred.Reject(e.Value());
    }
};

struct PipelineWorker : Napi::AsyncWorker {
    std::shared_ptr<ConnectionPool> pool;
    std::vector<std::string> queries;
    std::vector<size_t> affected;
    Napi::Promise::Deferred deferred;
    
    PipelineWorker(Napi::Env env, std::shared_ptr<ConnectionPool> p, std::vector<std::string> q, Napi::Promise::Deferred d)
        : AsyncWorker(env), pool(p), queries(std::move(q)), deferred(d) {}
    
    void Execute() override {
        auto conn = pool->acquire();
        pqxx::work txn(*conn);
        pqxx::pipeline pipe(txn);
        std::vector<pqxx::pipeline::query_id> ids;
        ids.reserve(queries.size());
        
        for (const auto& sql : queries) {
            ids.push_back(pipe.insert(sql));
        }
        pipe.complete();
        
        affected.reserve(ids.size());
        for (auto id : ids) {
            affected.push_back(pipe.retrieve(id).affected_rows());
        }
        txn.commit();
        pool->release(conn);
    }
    
    void OnOK() override {
        auto env = Env();
        auto results = Napi::Array::New(env, affected.size());
        for (size_t i = 0; i < affected.size(); ++i) {
            results[i] = Napi::Number::New(env, affected[i]);
        }
        deferred.Resolve(results);
    }
    
    void OnError(const Napi::Error& e) override {
        deferred.Reject(e.Value());
    }
};

Napi::Object Connection::Init(Napi::Env env, Napi::Object exports) {
    auto func = DefineClass(env, "Connection", {
        InstanceMethod("query", &Connection::Query),
        InstanceMethod("prepare", &Connection::Prepare),
        InstanceMethod("execute", &Connection::Execute),
        InstanceMethod("pipeline", &Connection::Pipeline),
        InstanceMethod("listen", &Connection::Listen),
        InstanceMethod("unlisten", &Connection::Unlisten),
        InstanceMethod("close", &Connection::Close)
    });
    auto* constructor = new Napi::FunctionReference();
    *constructor = Napi::Persistent(func);
    env.SetInstanceData(constructor);
    exports.Set("Connection", func);
    return exports;
}

Connection::Connection(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Connection>(info) {
    std::string connStr = info[0].As<Napi::String>().Utf8Value();
    size_t poolSize = info.Length() > 1 ? info[1].As<Napi::Number>().Uint32Value() : 10;
    pool_ = std::make_shared<ConnectionPool>(connStr, poolSize);
}

Connection::~Connection() {
    for (auto& [_, listener] : listeners_) listener->stop();
    pool_->close();
}

Napi::Value Connection::Query(const Napi::CallbackInfo& info) {
    auto env = info.Env();
    auto deferred = Napi::Promise::Deferred::New(env);
    
    std::vector<std::string> params;
    if (info.Length() > 1 && info[1].IsArray()) {
        auto arr = info[1].As<Napi::Array>();
        uint32_t len = arr.Length();
        params.reserve(len);
        for (uint32_t i = 0; i < len; ++i) {
            auto val = arr.Get(i);
            if (val.IsString()) {
                params.emplace_back(val.As<Napi::String>().Utf8Value());
            } else if (val.IsNumber()) {
                params.emplace_back(std::to_string(val.As<Napi::Number>().Int64Value()));
            } else if (val.IsBoolean()) {
                params.emplace_back(val.As<Napi::Boolean>().Value() ? "true" : "false");
            }
        }
    }
    
    auto* worker = new QueryWorker(env, pool_, info[0].As<Napi::String>().Utf8Value(), std::move(params), deferred);
    worker->Queue();
    return deferred.Promise();
}

Napi::Value Connection::Prepare(const Napi::CallbackInfo& info) {
    std::string name = info[0].As<Napi::String>().Utf8Value();
    std::string sql = info[1].As<Napi::String>().Utf8Value();
    prepared_[std::move(name)] = std::move(sql);
    return info.Env().Undefined();
}

Napi::Value Connection::Execute(const Napi::CallbackInfo& info) {
    auto env = info.Env();
    std::string name = info[0].As<Napi::String>().Utf8Value();
    auto it = prepared_.find(name);
    if (it == prepared_.end()) {
        Napi::Error::New(env, "Prepared statement not found").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    
    auto deferred = Napi::Promise::Deferred::New(env);
    std::vector<std::string> params;
    if (info.Length() > 1 && info[1].IsArray()) {
        auto arr = info[1].As<Napi::Array>();
        uint32_t len = arr.Length();
        params.reserve(len);
        for (uint32_t i = 0; i < len; ++i) {
            auto val = arr.Get(i);
            if (val.IsString()) {
                params.emplace_back(val.As<Napi::String>().Utf8Value());
            } else if (val.IsNumber()) {
                params.emplace_back(std::to_string(val.As<Napi::Number>().Int64Value()));
            }
        }
    }
    
    auto* worker = new QueryWorker(env, pool_, it->second, std::move(params), deferred);
    worker->Queue();
    return deferred.Promise();
}

Napi::Value Connection::Pipeline(const Napi::CallbackInfo& info) {
    auto env = info.Env();
    auto deferred = Napi::Promise::Deferred::New(env);
    auto queries = info[0].As<Napi::Array>();
    
    std::vector<std::string> queryList;
    uint32_t len = queries.Length();
    queryList.reserve(len);
    for (uint32_t i = 0; i < len; ++i) {
        queryList.emplace_back(queries.Get(i).As<Napi::String>().Utf8Value());
    }
    
    auto* worker = new PipelineWorker(env, pool_, std::move(queryList), deferred);
    worker->Queue();
    return deferred.Promise();
}

Napi::Value Connection::Listen(const Napi::CallbackInfo& info) {
    auto env = info.Env();
    std::string channel = info[0].As<Napi::String>().Utf8Value();
    auto callback = info[1].As<Napi::Function>();
    
    auto conn = pool_->acquire();
    std::string connStr = conn->connection_string();
    pool_->release(conn);
    
    auto listener = std::make_unique<Listener>(connStr, channel, callback);
    listener->start();
    listeners_[channel] = std::move(listener);
    
    return env.Undefined();
}

Napi::Value Connection::Unlisten(const Napi::CallbackInfo& info) {
    std::string channel = info[0].As<Napi::String>().Utf8Value();
    
    auto it = listeners_.find(channel);
    if (it != listeners_.end()) {
        it->second->stop();
        listeners_.erase(it);
    }
    
    return info.Env().Undefined();
}

Napi::Value Connection::Close(const Napi::CallbackInfo& info) {
    for (auto& [_, listener] : listeners_) listener->stop();
    listeners_.clear();
    pool_->close();
    return info.Env().Undefined();
}
