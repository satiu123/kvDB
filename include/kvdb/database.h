#ifndef KVDB_DATABASE_H
#define KVDB_DATABASE_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <optional>

namespace kvdb {

class Database {
public:
    Database();
    ~Database();

    // 基本操作
    bool put(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool remove(const std::string& key);
    
    // 高级操作
    size_t size() const;
    void clear();
    bool exists(const std::string& key) const;

private:
    std::unordered_map<std::string, std::string> data_;
    mutable std::mutex mutex_;
};

} // namespace kvdb

#endif // KVDB_DATABASE_H
