#include "connection_pool.h"
#include <thread>

ConnectionPool::ConnectionPool(const std::string& connStr, size_t poolSize)
    : connStr_(connStr), poolSize_(poolSize), currentSize_(0) {
    try {
        auto conn = createConnection();
        if (conn) {
            available_.push_back({conn, std::chrono::steady_clock::now()});
            currentSize_ = 1;
        }
    } catch (...) {}
    
    if (poolSize > 1) {
        std::thread([this, connStr, poolSize]() {
            for (size_t i = 1; i < poolSize; ++i) {
                try {
                    auto conn = std::make_shared<pqxx::connection>(connStr);
                    if (conn && conn->is_open()) {
                        std::lock_guard<std::mutex> lock(mutex_);
                        if (!closed_) {
                            available_.push_back({conn, std::chrono::steady_clock::now()});
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

bool ConnectionPool::isHealthy(const std::shared_ptr<pqxx::connection>& conn) {
    if (!conn || !conn->is_open()) return false;
    try {
        pqxx::nontransaction txn(*conn);
        txn.exec("SELECT 1");
        return true;
    } catch (...) {
        return false;
    }
}

std::shared_ptr<pqxx::connection> ConnectionPool::createConnection() {
    try {
        auto conn = std::make_shared<pqxx::connection>(connStr_);
        if (conn->is_open()) return conn;
    } catch (...) {}
    return nullptr;
}

std::shared_ptr<pqxx::connection> ConnectionPool::acquire() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    while (!available_.empty()) {
        auto pooled = available_.back();
        available_.pop_back();
        
        auto idleSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - pooled.lastUsed).count();
        
        if (idleSeconds > MAX_IDLE_SECONDS || !isHealthy(pooled.conn)) {
            currentSize_--;
            continue;
        }
        
        return pooled.conn;
    }
    
    if (currentSize_ < poolSize_ && !closed_) {
        currentSize_++;
        auto conn = createConnection();
        if (conn) return conn;
        currentSize_--;
    }
    
    return nullptr;
}

void ConnectionPool::release(std::shared_ptr<pqxx::connection> conn) {
    if (!conn || !conn->is_open()) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (!closed_) {
        available_.push_back({conn, std::chrono::steady_clock::now()});
    }
}

void ConnectionPool::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    closed_ = true;
    available_.clear();
    currentSize_ = 0;
}

size_t ConnectionPool::availableCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    return available_.size();
}

size_t ConnectionPool::currentCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentSize_;
}

size_t ConnectionPool::maxSize() {
    return poolSize_;
}

bool ConnectionPool::closed() {
    std::lock_guard<std::mutex> lock(mutex_);
    return closed_;
}
