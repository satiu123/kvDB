import std;
import kvdb.core;

int main() {
    // 在 "my_db" 目录中创建一个数据库实例
    kvdb::core::Database db("my_db");

    // 放入一些键值对
    db.put("hello", "world");
    db.put("gemini", "is awesome");

    // 获取一个值
    if (auto value = db.get("hello")) {
        std::cout << "找到键 'hello', 值为: " << *value << std::endl;
    }

    // 删除一个键
    db.remove("gemini");

    // 检查大小
    std::cout << "数据库大小: " << db.size() << std::endl;

    return 0;
}
