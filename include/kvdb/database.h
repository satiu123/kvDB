#ifndef KVDB_DATABASE_H
#define KVDB_DATABASE_H

#include <string>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <fstream>

namespace kvdb {

class Database {
public:
    Database();
    ~Database();
    
    // 禁用拷贝和移动构造函数
    // 禁用拷贝和移动赋值运算符
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) = delete;
    Database& operator=(Database&&) = delete;

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
    std::string walPath_;
    std::ofstream walFile_;
    enum class OpType : uint8_t { PUT, REMOVE, CLEAR };
    bool openWalFile();
    bool applyWalLog();
};

} // namespace kvdb

#endif // KVDB_DATABASE_H
