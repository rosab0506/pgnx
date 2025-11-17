#include "connection.h"
#include <thread>

inline Napi::Value FastConvert(Napi::Env env, const pqxx::field& field, int type) {
    if (field.is_null()) return env.Null();
    
    if (type == 23 || type == 20 || type == 21) {
        return Napi::Number::New(env, field.as<long long>());
    } else if (type == 16) {
        return Napi::Boolean::New(env, field.as<bool>());
    } else if (type == 700 || type == 701) {
        return Napi::Number::New(env, field.as<double>());
    }
    return Napi::String::New(env, field.c_str(), field.size());
}

inline Napi::Array ConvertResult(Napi::Env env, const pqxx::result& result) {
    size_t rowCount = result.size();
    auto rows = Napi::Array::New(env, rowCount);
    
    if (rowCount == 0) return rows;
    
    size_t colCount = result.columns();
    
    for (size_t i = 0; i < rowCount; ++i) {
        auto row = Napi::Object::New(env);
        
        for (size_t j = 0; j < colCount; ++j) {
            row.Set(result.column_name(j), FastConvert(env, result[i][j], result.column_type(j)));
        }
        rows[i] = row;
    }
    return rows;
}

struct QueryWorker : Napi::AsyncWorker {
    std::shared_ptr<ConnectionPool> pool;
    std::string sql;
    std::vector<std::string> params;
    pqxx::result result;
    Napi::Promise::Deferred deferred;
    std::shared_ptr<pqxx::connection> conn;
    std::string errorMsg;
    
    QueryWorker(Napi::Env env, std::shared_ptr<ConnectionPool> p, std::string s, std::vector<std::string> prm, Napi::Promise::Deferred d)
        : AsyncWorker(env), pool(p), sql(std::move(s)), params(std::move(prm)), deferred(d) {}
    
    void Execute() override {
        conn = pool->acquire();
        if (!conn) {
            errorMsg = "Failed to acquire connection from pool";
            return;
        }
        try {
            pqxx::nontransaction txn(*conn);
            result = params.empty() ? txn.exec(sql) : txn.exec_params(sql, pqxx::prepare::make_dynamic_params(params));
        } catch (const std::exception& e) {
            errorMsg = e.what();
            throw;
        }
    }
    
    void OnOK() override {
        if (conn) pool->release(conn);
        if (!errorMsg.empty()) {
            deferred.Reject(Napi::Error::New(Env(), errorMsg).Value());
        } else {
            deferred.Resolve(ConvertResult(Env(), result));
        }
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
    std::string errorMsg;
    
    PipelineWorker(Napi::Env env, std::shared_ptr<ConnectionPool> p, std::vector<std::string> q, Napi::Promise::Deferred d)
        : AsyncWorker(env), pool(p), queries(std::move(q)), deferred(d) {}
    
    void Execute() override {
        conn = pool->acquire();
        if (!conn) {
            errorMsg = "Failed to acquire connection from pool";
            return;
        }
        
        try {
            pqxx::nontransaction txn(*conn);
            affected.reserve(queries.size());
            for (const auto& sql : queries) {
                affected.push_back(txn.exec(sql).affected_rows());
            }
        } catch (const std::exception& e) {
            errorMsg = e.what();
            throw;
        }
    }
    
    void OnOK() override {
        if (conn) pool->release(conn);
        
        if (!errorMsg.empty()) {
            deferred.Reject(Napi::Error::New(Env(), errorMsg).Value());
            return;
        }
        
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
        InstanceMethod("querySync", &Connection::QuerySync),
        InstanceMethod("prepare", &Connection::Prepare),
        InstanceMethod("execute", &Connection::Execute),
        InstanceMethod("pipeline", &Connection::Pipeline),
        InstanceMethod("begin", &Connection::Begin),
        InstanceMethod("commit", &Connection::Commit),
        InstanceMethod("rollback", &Connection::Rollback),
        InstanceMethod("listen", &Connection::Listen),
        InstanceMethod("unlisten", &Connection::Unlisten),
        InstanceMethod("poolStatus", &Connection::PoolStatus),
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

Napi::Value Connection::QuerySync(const Napi::CallbackInfo& info) {
    auto env = info.Env();
    
    try {
        auto conn = pool_->acquire();
        if (!conn) {
            Napi::Error::New(env, "Failed to acquire connection").ThrowAsJavaScriptException();
            return env.Undefined();
        }
        
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
        
        pqxx::nontransaction txn(*conn);
        auto result = params.empty() ? 
            txn.exec(info[0].As<Napi::String>().Utf8Value()) : 
            txn.exec_params(info[0].As<Napi::String>().Utf8Value(), pqxx::prepare::make_dynamic_params(params));
        
        pool_->release(conn);
        return ConvertResult(env, result);
        
    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Undefined();
    }
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

Napi::Value Connection::Begin(const Napi::CallbackInfo& info) {
    auto env = info.Env();
    auto deferred = Napi::Promise::Deferred::New(env);
    auto* worker = new QueryWorker(env, pool_, "BEGIN", {}, deferred);
    worker->Queue();
    return deferred.Promise();
}

Napi::Value Connection::Commit(const Napi::CallbackInfo& info) {
    auto env = info.Env();
    auto deferred = Napi::Promise::Deferred::New(env);
    auto* worker = new QueryWorker(env, pool_, "COMMIT", {}, deferred);
    worker->Queue();
    return deferred.Promise();
}

Napi::Value Connection::Rollback(const Napi::CallbackInfo& info) {
    auto env = info.Env();
    auto deferred = Napi::Promise::Deferred::New(env);
    auto* worker = new QueryWorker(env, pool_, "ROLLBACK", {}, deferred);
    worker->Queue();
    return deferred.Promise();
}

Napi::Value Connection::Listen(const Napi::CallbackInfo& info) {
    auto env = info.Env();
    std::string channel = info[0].As<Napi::String>().Utf8Value();
    auto callback = info[1].As<Napi::Function>();
    
    auto conn = pool_->acquire();
    if (!conn) {
        Napi::Error::New(env, "Failed to acquire connection for listener").ThrowAsJavaScriptException();
        return env.Undefined();
    }
    
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

Napi::Value Connection::PoolStatus(const Napi::CallbackInfo& info) {
    auto env = info.Env();
    Napi::Object stats = Napi::Object::New(env);
    stats.Set("available", Napi::Number::New(env, pool_->availableCount()));
    stats.Set("current", Napi::Number::New(env, pool_->currentCount()));
    stats.Set("max", Napi::Number::New(env, pool_->maxSize()));
    stats.Set("closed", Napi::Boolean::New(env, pool_->closed()));
    return stats;
}
