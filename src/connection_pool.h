#pragma once
#include <pqxx/pqxx>
#include <vector>
#include <mutex>
#include <memory>
#include <string>
#include <chrono>

struct PooledConnection {
    std::shared_ptr<pqxx::connection> conn;
    std::chrono::steady_clock::time_point lastUsed;
};

class ConnectionPool {
public:
    ConnectionPool(const std::string& connStr, size_t poolSize);
    ~ConnectionPool();
    std::shared_ptr<pqxx::connection> acquire();
    void release(std::shared_ptr<pqxx::connection> conn);
    void close();

    size_t availableCount();
    size_t currentCount();
    size_t maxSize();
    bool closed();

private:
    bool isHealthy(const std::shared_ptr<pqxx::connection>& conn);
    std::shared_ptr<pqxx::connection> createConnection();
    
    std::string connStr_;
    size_t poolSize_;
    size_t currentSize_;
    std::vector<PooledConnection> available_;
    std::mutex mutex_;
    bool closed_ = false;
    static constexpr int MAX_IDLE_SECONDS = 300;
};
