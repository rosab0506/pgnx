#pragma once
#include <napi.h>
#include <pqxx/pqxx>
#include <thread>
#include <chrono>
#include <atomic>
#include <string>
#include <memory>

class Listener {
public:
    Listener(const std::string& connStr, const std::string& channel, Napi::Function callback);
    ~Listener();
    void start();
    void stop();

private:
    void listen();
    std::string connStr_;
    std::string channel_;
    Napi::ThreadSafeFunction tsfn_;
    std::thread thread_;
    std::atomic<bool> running_{false};
};
