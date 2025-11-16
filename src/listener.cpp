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
    running_ = false;
    if (thread_.joinable()) thread_.join();
    tsfn_.Release();
}

void Listener::listen() {
    try {
        pqxx::connection conn(connStr_);
        pqxx::work txn(conn);
        txn.exec("LISTEN " + conn.quote_name(channel_));
        txn.commit();

        while (running_) {
            conn.await_notification(100, 0);
            
            int count = conn.get_notifs();
            if (count > 0) {
                tsfn_.BlockingCall([](Napi::Env env, Napi::Function jsCallback) {
                    jsCallback.Call({Napi::String::New(env, "notification")});
                });
            }
        }
    } catch (...) {}
}
