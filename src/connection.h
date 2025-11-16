#pragma once
#include <napi.h>
#include "connection_pool.h"
#include "listener.h"
#include <memory>
#include <unordered_map>

class Connection : public Napi::ObjectWrap<Connection> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);
    Connection(const Napi::CallbackInfo& info);
    ~Connection();

private:
    Napi::Value Query(const Napi::CallbackInfo& info);
    Napi::Value QuerySync(const Napi::CallbackInfo& info);
    Napi::Value Prepare(const Napi::CallbackInfo& info);
    Napi::Value Execute(const Napi::CallbackInfo& info);
    Napi::Value Pipeline(const Napi::CallbackInfo& info);
    Napi::Value Begin(const Napi::CallbackInfo& info);
    Napi::Value Commit(const Napi::CallbackInfo& info);
    Napi::Value Rollback(const Napi::CallbackInfo& info);
    Napi::Value Listen(const Napi::CallbackInfo& info);
    Napi::Value Unlisten(const Napi::CallbackInfo& info);
    Napi::Value Close(const Napi::CallbackInfo& info);
    Napi::Value PoolStatus(const Napi::CallbackInfo& info);

    std::shared_ptr<ConnectionPool> pool_;
    std::unordered_map<std::string, std::unique_ptr<Listener>> listeners_;
    std::unordered_map<std::string, std::string> prepared_;
};
