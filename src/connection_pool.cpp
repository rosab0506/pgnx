#include "connection_pool.h"

ConnectionPool::ConnectionPool(const std::string& connStr, size_t poolSize)
    : connStr_(connStr), poolSize_(poolSize), currentSize_(0) {
    // Pre-create one connection for faster first query
    available_.push(std::make_shared<pqxx::connection>(connStr_));
    currentSize_ = 1;
}

ConnectionPool::~ConnectionPool() {
    close();
}

std::shared_ptr<pqxx::connection> ConnectionPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (!available_.empty()) {
        auto conn = available_.front();
        available_.pop();
        return conn;
    }
    
    if (currentSize_ < poolSize_ && !closed_) {
        currentSize_++;
        lock.unlock();
        return std::make_shared<pqxx::connection>(connStr_);
    }
    
    cv_.wait(lock, [this] { return !available_.empty() || closed_; });
    if (closed_) return nullptr;
    auto conn = available_.front();
    available_.pop();
    return conn;
}

void ConnectionPool::release(std::shared_ptr<pqxx::connection> conn) {
    if (!conn || !conn->is_open()) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (!closed_) {
        available_.push(conn);
        cv_.notify_one();
    }
}

void ConnectionPool::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
    while (!available_.empty()) available_.pop();
    cv_.notify_all();
}
