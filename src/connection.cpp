#include "connection.h"
#include <pqxx/params.hxx>
#include <thread>
#include <cmath>
#include <optional>

// --- Type conversion utilities ---

inline Napi::Value FastConvert(Napi::Env env, const pqxx::field& field, int type) {
    if (field.is_null()) return env.Null();

    switch (type) {
        case 20:  // int8 (bigint)
        case 21:  // int2 (smallint)
        case 23:  // int4 (integer)
            return Napi::Number::New(env, field.as<long long>());
        case 16:  // bool
            return Napi::Boolean::New(env, field.as<bool>());
        case 700: // float4
        case 701: // float8
            return Napi::Number::New(env, field.as<double>());
        default:
            return Napi::String::New(env, field.c_str(), field.size());
    }
}

struct ColInfo {
    std::string name;
    int type;
};

inline Napi::Array ConvertResult(Napi::Env env, const pqxx::result& result) {
    size_t rowCount = result.size();
    auto rows = Napi::Array::New(env, rowCount);

    if (rowCount == 0) return rows;

    size_t colCount = result.columns();

    // Cache column metadata once per result set
    std::vector<ColInfo> cols(colCount);
    for (size_t j = 0; j < colCount; ++j) {
        cols[j].name = result.column_name(j);
        cols[j].type = result.column_type(j);
    }

    for (size_t i = 0; i < rowCount; ++i) {
        auto row = Napi::Object::New(env);
        for (size_t j = 0; j < colCount; ++j) {
            row.Set(cols[j].name, FastConvert(env, result[i][j], cols[j].type));
        }
        rows[i] = row;
    }
    return rows;
}

// --- Shared parameter conversion ---

struct ConvertedParams {
    pqxx::params parms;
    bool empty = true;
};

inline ConvertedParams ConvertParams(const Napi::CallbackInfo& info, size_t paramIndex) {
    ConvertedParams result;
    if (info.Length() <= paramIndex || !info[paramIndex].IsArray()) return result;

    auto arr = info[paramIndex].As<Napi::Array>();
    uint32_t len = arr.Length();
    if (len == 0) return result;

    result.empty = false;
    result.parms.reserve(len);

    for (uint32_t i = 0; i < len; ++i) {
        auto val = arr.Get(i);

        if (val.IsNull() || val.IsUndefined()) {
            result.parms.append();  // SQL NULL
        } else if (val.IsString()) {
            result.parms.append(val.As<Napi::String>().Utf8Value());
        } else if (val.IsNumber()) {
            double num = val.As<Napi::Number>().DoubleValue();
            if (num == std::floor(num) && std::abs(num) < 9007199254740992.0) {
                result.parms.append(std::to_string(static_cast<long long>(num)));
            } else {
                result.parms.append(std::to_string(num));
            }
        } else if (val.IsBoolean()) {
            result.parms.append(val.As<Napi::Boolean>().Value() ? "true" : "false");
        } else if (val.IsBigInt()) {
            bool lossless;
            int64_t bigint = val.As<Napi::BigInt>().Int64Value(&lossless);
            result.parms.append(std::to_string(bigint));
        } else {
            // Fallback: convert to string via .toString()
            result.parms.append(val.ToString().Utf8Value());
        }
    }
    return result;
}

// --- Async workers ---

struct QueryWorker : Napi::AsyncWorker {
    std::shared_ptr<ConnectionPool> pool;
    std::string sql;
    pqxx::params parms;
    bool hasParams;
    pqxx::result result;
    Napi::Promise::Deferred deferred;
    std::shared_ptr<pqxx::connection> conn;

    QueryWorker(Napi::Env env, std::shared_ptr<ConnectionPool> p, std::string s, ConvertedParams cp, Napi::Promise::Deferred d)
        : AsyncWorker(env), pool(p), sql(std::move(s)), parms(std::move(cp.parms)), hasParams(!cp.empty), deferred(d) {}

    void Execute() override {
        conn = pool->acquire();
        if (!conn) {
            SetError("Failed to acquire connection from pool");
            return;
        }
        try {
            pqxx::nontransaction txn(*conn);
            result = hasParams
                ? txn.exec(sql, parms)
                : txn.exec(sql);
        } catch (const std::exception& e) {
            SetError(e.what());
        }
    }

    void OnOK() override {
        if (conn) pool->release(conn);
        deferred.Resolve(ConvertResult(Env(), result));
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
        if (!conn) {
            SetError("Failed to acquire connection from pool");
            return;
        }

        try {
            pqxx::work txn(*conn);
            affected.reserve(queries.size());
            for (const auto& sql : queries) {
                affected.push_back(txn.exec(sql).affected_rows());
            }
            txn.commit();
        } catch (const std::exception& e) {
            SetError(e.what());
        }
    }

    void OnOK() override {
        if (conn) pool->release(conn);
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

// --- Connection class ---

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
    auto env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "Connection string must be a string").ThrowAsJavaScriptException();
        return;
    }

    std::string connStr = info[0].As<Napi::String>().Utf8Value();
    if (connStr.empty()) {
        Napi::Error::New(env, "Connection string cannot be empty").ThrowAsJavaScriptException();
        return;
    }

    size_t poolSize = 10;
    if (info.Length() > 1) {
        if (!info[1].IsNumber()) {
            Napi::TypeError::New(env, "Pool size must be a number").ThrowAsJavaScriptException();
            return;
        }
        poolSize = info[1].As<Napi::Number>().Uint32Value();
        if (poolSize == 0) {
            Napi::RangeError::New(env, "Pool size must be at least 1").ThrowAsJavaScriptException();
            return;
        }
    }

    try {
        pool_ = std::make_shared<ConnectionPool>(connStr, poolSize);
    } catch (const std::exception& e) {
        Napi::Error::New(env, std::string("Failed to create connection pool: ") + e.what()).ThrowAsJavaScriptException();
    }
}

Connection::~Connection() {
    for (auto& [_, listener] : listeners_) listener->stop();
    if (pool_) pool_->close();
}

Napi::Value Connection::QuerySync(const Napi::CallbackInfo& info) {
    auto env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "SQL query must be a string").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    try {
        auto conn = pool_->acquire();
        if (!conn) {
            Napi::Error::New(env, "Failed to acquire connection").ThrowAsJavaScriptException();
            return env.Undefined();
        }

        auto cp = ConvertParams(info, 1);

        pqxx::nontransaction txn(*conn);
        auto sql = info[0].As<Napi::String>().Utf8Value();
        auto result = cp.empty
            ? txn.exec(sql)
            : txn.exec(sql, cp.parms);

        pool_->release(conn);
        return ConvertResult(env, result);

    } catch (const std::exception& e) {
        Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
        return env.Undefined();
    }
}

Napi::Value Connection::Query(const Napi::CallbackInfo& info) {
    auto env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "SQL query must be a string").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    auto deferred = Napi::Promise::Deferred::New(env);
    auto cp = ConvertParams(info, 1);

    auto* worker = new QueryWorker(env, pool_, info[0].As<Napi::String>().Utf8Value(), std::move(cp), deferred);
    worker->Queue();
    return deferred.Promise();
}

Napi::Value Connection::Prepare(const Napi::CallbackInfo& info) {
    auto env = info.Env();

    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsString()) {
        Napi::TypeError::New(env, "prepare() requires (name: string, sql: string)").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    prepared_[info[0].As<Napi::String>().Utf8Value()] = info[1].As<Napi::String>().Utf8Value();
    return env.Undefined();
}

Napi::Value Connection::Execute(const Napi::CallbackInfo& info) {
    auto env = info.Env();

    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "execute() requires a prepared statement name").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    std::string name = info[0].As<Napi::String>().Utf8Value();
    auto it = prepared_.find(name);
    if (it == prepared_.end()) {
        Napi::Error::New(env, "Prepared statement not found: " + name).ThrowAsJavaScriptException();
        return env.Undefined();
    }

    auto deferred = Napi::Promise::Deferred::New(env);
    auto cp = ConvertParams(info, 1);

    auto* worker = new QueryWorker(env, pool_, it->second, std::move(cp), deferred);
    worker->Queue();
    return deferred.Promise();
}

Napi::Value Connection::Pipeline(const Napi::CallbackInfo& info) {
    auto env = info.Env();

    if (info.Length() < 1 || !info[0].IsArray()) {
        Napi::TypeError::New(env, "pipeline() requires an array of SQL queries").ThrowAsJavaScriptException();
        return env.Undefined();
    }

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
    auto* worker = new QueryWorker(env, pool_, "BEGIN", ConvertedParams{}, deferred);
    worker->Queue();
    return deferred.Promise();
}

Napi::Value Connection::Commit(const Napi::CallbackInfo& info) {
    auto env = info.Env();
    auto deferred = Napi::Promise::Deferred::New(env);
    auto* worker = new QueryWorker(env, pool_, "COMMIT", ConvertedParams{}, deferred);
    worker->Queue();
    return deferred.Promise();
}

Napi::Value Connection::Rollback(const Napi::CallbackInfo& info) {
    auto env = info.Env();
    auto deferred = Napi::Promise::Deferred::New(env);
    auto* worker = new QueryWorker(env, pool_, "ROLLBACK", ConvertedParams{}, deferred);
    worker->Queue();
    return deferred.Promise();
}

Napi::Value Connection::Listen(const Napi::CallbackInfo& info) {
    auto env = info.Env();

    if (info.Length() < 2 || !info[0].IsString() || !info[1].IsFunction()) {
        Napi::TypeError::New(env, "listen() requires (channel: string, callback: function)").ThrowAsJavaScriptException();
        return env.Undefined();
    }

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
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(info.Env(), "unlisten() requires a channel name").ThrowAsJavaScriptException();
        return info.Env().Undefined();
    }

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
    if (pool_) pool_->close();
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
