#include <cstddef>  // for size_t
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include "kvdb/database.h"

namespace kvdb {

Database::Database() = default;
Database::~Database() = default;

bool Database::put(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_[key] = value;
    return true;
}

std::optional<std::string> Database::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(key);
    if (it != data_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Database::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.erase(key) > 0;
}

size_t Database::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.size();
}

void Database::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.clear();
}

bool Database::exists(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.contains(key);
}
bool Database::openWalFile() {
    // Implementation for opening the Write-Ahead Log file
    walFile_.open(walPath_, std::ios::app);
    if (!walFile_) {
        return false;  // Failed to open WAL file
    }
    return true;  // WAL file opened successfully
}
bool Database::applyWalLog() {
    // Implementation for applying the Write-Ahead Log
    return true;
}
} // namespace kvdb
