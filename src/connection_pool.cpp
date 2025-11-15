#include "connection_pool.h"

ConnectionPool::ConnectionPool(const std::string& connStr, size_t poolSize)
    : connStr_(connStr), poolSize_(poolSize), currentSize_(0) {
    // Create initial connection
    try {
        auto conn = std::make_shared<pqxx::connection>(connStr_);
        if (conn->is_open()) {
            available_.push(conn);
            currentSize_ = 1;
        }
    } catch (...) {}
}

ConnectionPool::~ConnectionPool() {
    close();
}

std::shared_ptr<pqxx::connection> ConnectionPool::acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!available_.empty()) {
        auto conn = available_.front();
        available_.pop();
        if (conn && conn->is_open()) {
            return conn;
        }
    }
    
    if (currentSize_ < poolSize_ && !closed_) {
        currentSize_++;
        try {
            return std::make_shared<pqxx::connection>(connStr_);
        } catch (...) {
            currentSize_--;
            return nullptr;
        }
    }
    
    return nullptr;
}

void ConnectionPool::release(std::shared_ptr<pqxx::connection> conn) {
    if (!conn || !conn->is_open()) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (!closed_) {
        available_.push(conn);
    }
}

void ConnectionPool::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
    while (!available_.empty()) available_.pop();
}
