#include "connection_pool.h"

ConnectionPool::ConnectionPool(const std::string& connStr, size_t poolSize)
    : connStr_(connStr), poolSize_(poolSize), currentSize_(0) {
    // Don't create connections upfront - create on demand
}

ConnectionPool::~ConnectionPool() {
    close();
}

std::shared_ptr<pqxx::connection> ConnectionPool::acquire() {
    std::unique_lock<std::mutex> lock(mutex_);
    
    // If available connection exists, return it
    if (!available_.empty()) {
        auto conn = available_.front();
        available_.pop();
        return conn;
    }
    
    // If we can create more connections, create one
    if (currentSize_ < poolSize_ && !closed_) {
        currentSize_++;
        lock.unlock();
        return std::make_shared<pqxx::connection>(connStr_);
    }
    
    // Wait for a connection to become available
    cv_.wait(lock, [this] { return !available_.empty() || closed_; });
    if (closed_) return nullptr;
    auto conn = available_.front();
    available_.pop();
    return conn;
}

void ConnectionPool::release(std::shared_ptr<pqxx::connection> conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!closed_ && conn && conn->is_open()) {
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
