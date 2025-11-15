#include "connection_pool.h"
#include <thread>

ConnectionPool::ConnectionPool(const std::string& connStr, size_t poolSize)
    : connStr_(connStr), poolSize_(poolSize), currentSize_(0) {
    try {
        auto conn = std::make_shared<pqxx::connection>(connStr_);
        if (conn->is_open()) {
            available_.push_back(conn);
            currentSize_ = 1;
        }
    } catch (...) {}
    
    if (poolSize > 1) {
        std::thread([this, connStr, poolSize]() {
            for (size_t i = 1; i < poolSize; ++i) {
                try {
                    auto conn = std::make_shared<pqxx::connection>(connStr);
                    if (conn->is_open()) {
                        std::lock_guard<std::mutex> lock(mutex_);
                        if (!closed_) {
                            available_.push_back(conn);
                            currentSize_++;
                        }
                    }
                } catch (...) {
                    break;
                }
            }
        }).detach();
    }
}

ConnectionPool::~ConnectionPool() {
    close();
}

std::shared_ptr<pqxx::connection> ConnectionPool::acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!available_.empty()) {
        auto conn = available_.back();
        available_.pop_back();
        return conn;
    }
    
    if (currentSize_ < poolSize_ && !closed_) {
        currentSize_++;
        mutex_.unlock();
        try {
            return std::make_shared<pqxx::connection>(connStr_);
        } catch (...) {
            std::lock_guard<std::mutex> lock2(mutex_);
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
        available_.push_back(conn);
    }
}

void ConnectionPool::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
    available_.clear();
}
