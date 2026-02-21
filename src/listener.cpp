#include "listener.h"

Listener::Listener(const std::string& connStr, const std::string& channel, Napi::Function callback)
    : connStr_(connStr), channel_(channel) {
    tsfn_ = Napi::ThreadSafeFunction::New(callback.Env(), callback, "Listener", 0, 1);
}

Listener::~Listener() {
    stop();
}

void Listener::start() {
    running_ = true;
    thread_ = std::thread(&Listener::listen, this);
}

void Listener::stop() {
    if (stopped_.exchange(true)) return;
    running_ = false;
    if (thread_.joinable()) thread_.join();
    tsfn_.Release();
}

void Listener::listen() {
    try {
        pqxx::connection conn(connStr_);

        // Use libpqxx 7.10+ listen() API with notification handler
        conn.listen(channel_,
            [this](pqxx::notification n) {
                if (!running_) return;
                std::string payload{n.payload};
                tsfn_.BlockingCall([payload](Napi::Env env, Napi::Function jsCallback) {
                    jsCallback.Call({Napi::String::New(env, payload)});
                });
            });

        while (running_) {
            conn.await_notification(1);
        }
    } catch (const std::exception& e) {
        if (!running_) return;
        std::string errMsg = e.what();
        tsfn_.BlockingCall([errMsg](Napi::Env env, Napi::Function jsCallback) {
            auto error = Napi::Error::New(env, errMsg);
            jsCallback.Call({error.Value()});
        });
    }
}
