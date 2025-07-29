import std;
import kvdb.core;

int main() {
    // Create a database instance in the "my_db" directory
    kvdb::core::Database db("my_db");

    // Put some key-value pairs
    db.put("hello", "world");
    db.put("gemini", "is awesome");

    // Get a value
    if (auto value = db.get("hello")) {
        std::cout << "Found key 'hello', value: " << *value << std::endl;
    }

    // Remove a key
    db.remove("gemini");

    // Check size
    std::cout << "Database size: " << db.size() << std::endl;

    return 0;
}
