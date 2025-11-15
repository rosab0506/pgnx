#include "connection.h"

struct QueryWorker : Napi::AsyncWorker {
    std::shared_ptr<ConnectionPool> pool;
    std::string sql;
    std::vector<std::string> params;
    pqxx::result result;
    Napi::Promise::Deferred deferred;
    std::shared_ptr<pqxx::connection> conn;
    
    QueryWorker(Napi::Env env, std::shared_ptr<ConnectionPool> p, std::string s, std::vector<std::string> prm, Napi::Promise::Deferred d)
        : AsyncWorker(env), pool(p), sql(std::move(s)), params(std::move(prm)), deferred(d) {}
    
    void Execute() override {
        conn = pool->acquire();
        if (!conn) return;
        pqxx::nontransaction txn(*conn);
        result = params.empty() ? txn.exec(sql) : txn.exec_params(sql, pqxx::prepare::make_dynamic_params(params));
    }
    
    void OnOK() override {
        pool->release(conn);
        
        auto env = Env();
        size_t rowCount = result.size();
        
        if (rowCount == 0) {
            deferred.Resolve(Napi::Array::New(env, 0));
            return;
        }
        
        size_t colCount = result.columns();
        auto rows = Napi::Array::New(env, rowCount);
        
        // Cache column metadata
        std::vector<const char*> colNames(colCount);
        std::vector<int> colTypes(colCount);
        for (size_t j = 0; j < colCount; ++j) {
            colNames[j] = result.column_name(j);
            colTypes[j] = result.column_type(j);
        }
        
        // Process rows
        for (size_t i = 0; i < rowCount; ++i) {
            auto row = Napi::Object::New(env);
            const auto& dbRow = result[i];
            
            for (size_t j = 0; j < colCount; ++j) {
                const auto& field = dbRow[j];
                
                if (field.is_null()) {
                    row.Set(colNames[j], env.Null());
                } else {
                    int type = colTypes[j];
                    if (type == 23 || type == 20 || type == 21) {
                        row.Set(colNames[j], Napi::Number::New(env, field.as<long long>()));
                    } else if (type == 16) {
                        row.Set(colNames[j], Napi::Boolean::New(env, field.as<bool>()));
                    } else if (type == 700 || type == 701) {
                        row.Set(colNames[j], Napi::Number::New(env, field.as<double>()));
                    } else {
                        row.Set(colNames[j], Napi::String::New(env, field.c_str(), field.size()));
                    }
                }
            }
            rows[i] = row;
        }
        deferred.Resolve(rows);
    }
    
    void OnError(const Napi::Error& e) override {
        if (conn) pool->release(conn);
        deferred.Reject(e.Value());
    }
};

struct PipelineWorker : Napi::AsyncWorker {
    std::shared_ptr<ConnectionPool> pool;
    std::vector<std::string> queries;
    std::vector<size_t> affected;
    Napi::Promise::Deferred deferred;
    std::shared_ptr<pqxx::connection> conn;
    
    PipelineWorker(Napi::Env env, std::shared_ptr<ConnectionPool> p, std::vector<std::string> q, Napi::Promise::Deferred d)
        : AsyncWorker(env), pool(p), queries(std::move(q)), deferred(d) {}
    
    void Execute() override {
        conn = pool->acquire();
        if (!conn) return;
        
        pqxx::nontransaction txn(*conn);
        affected.reserve(queries.size());
        for (const auto& sql : queries) {
            affected.push_back(txn.exec(sql).affected_rows());
        }
    }
    
    void OnOK() override {
        pool->release(conn);
        
        auto env = Env();
        auto results = Napi::Array::New(env, affected.size());
        for (size_t i = 0; i < affected.size(); ++i) {
            results[i] = Napi::Number::New(env, affected[i]);
        }
        deferred.Resolve(results);
    }
    
    void OnError(const Napi::Error& e) override {
        if (conn) pool->release(conn);
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
    prepared_[info[0].As<Napi::String>().Utf8Value()] = info[1].As<Napi::String>().Utf8Value();
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
