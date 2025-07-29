import std;
import kvdb.core;

int main() {
    // Create a database instance in the "structure_test_db" directory
    kvdb::core::Database db("structure_test_db");

    // Set a low threshold to observe flushing
    db.setMemtableFlushThreshold(3);

    // Add some data
    db.put("key1", "value1");
    db.put("key2", "value2");
    db.put("key3", "value3");  // This should trigger a flush

    std::cout << "Database structure after first flush." << std::endl;

    db.put("key4", "value4");

    std::cout << "Database structure after adding more data." << std::endl;

    return 0;
}
