import std;
import kvdb.core;

int main() {
    // 在 "structure_test_db" 目录中创建一个数据库实例
    kvdb::core::Database db("structure_test_db");

    // 设置一个较低的阈值以观察刷写过程
    db.setMemtableFlushThreshold(3);

    // 添加一些数据
    db.put("key1", "value1");
    db.put("key2", "value2");
    db.put("key3", "value3");  // 这里应该会触发一次刷写

    std::cout << "第一次刷写后的数据库结构。" << std::endl;

    db.put("key4", "value4");

    std::cout << "添加更多数据后的数据库结构。" << std::endl;

    return 0;
}
