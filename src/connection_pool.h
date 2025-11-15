#pragma once
#include <pqxx/pqxx>
#include <vector>
#include <mutex>
#include <memory>
#include <string>

class ConnectionPool {
public:
    ConnectionPool(const std::string& connStr, size_t poolSize);
    ~ConnectionPool();
    std::shared_ptr<pqxx::connection> acquire();
    void release(std::shared_ptr<pqxx::connection> conn);
    void close();

private:
    std::string connStr_;
    size_t poolSize_;
    size_t currentSize_;
    std::vector<std::shared_ptr<pqxx::connection>> available_;
    std::mutex mutex_;
    bool closed_ = false;
};
