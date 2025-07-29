import std;
import kvdb;

// 函数声明
void print_usage();
void process_command(kvdb::core::Database& db, const std::string& line);
std::vector<std::string> split_input(const std::string& input);


// 打印使用帮助
void print_usage() {
    std::cout << "Commands:\n"
              << "  put <key> <value>    - Insert or update a key-value pair.\n"
              << "  get <key>            - Retrieve the value for a key.\n"
              << "  remove <key>         - Delete a key.\n"
              << "  exists <key>         - Check if a key exists.\n"
              << "  size                 - Get the number of keys.\n"
              << "  keys                 - List all keys.\n"
              << "  clear                - Clear the database.\n"
              << "  compact              - Compact the database.\n"
              << "  wal                  - Print the WAL records.\n"
              << "  exit/quit            - Exit the CLI.\n"
              << "  help                 - Show this help message.\n";
}

// 主函数
int main(int argc, char* argv[]) {
    std::string db_path = ".";  // 默认数据库路径
    if (argc > 1) {
        db_path = argv[1];
    }
    kvdb::core::Database db(db_path);
    kvdb::logging::Logger::getInstance().addSink(
        std::make_shared<kvdb::logging::FileSink>(db_path + "/kvdb.log"));
    std::cout << "Welcome to kvDB CLI!\n";
    std::string line;
    std::cout << "kvdb> ";
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            std::cout << "kvdb> ";
            continue;
        }
        if (line == "quit" || line == "exit") {
            break;
        }
        process_command(db, line);
        std::cout << "kvdb> ";
    }

    std::cout << "\nGoodbye!" << std::endl;
    return 0;
}

// 处理命令
void process_command(kvdb::core::Database& db, const std::string& line) {
    auto args = split_input(line);
    if (args.empty()) {
        return;
    }

    std::string command = args[0];

    if (command == "put" && args.size() == 3) {
        if (db.put(args[1], args[2])) {
            std::cout << "OK\n";
        } else {
            std::cerr << "Error: Failed to put key.\n";
        }
    } else if (command == "get" && args.size() == 2) {
        auto value = db.get(args[1]);
        if (value) {
            std::cout << *value << '\n';
        } else {
            std::cout << "(nil)\n";
        }
    } else if (command == "remove" && args.size() == 2) {
        if (db.remove(args[1])) {
            std::cout << "OK\n";
        } else {
            std::cerr << "Error: Key not found or failed to remove.\n";
        }
    } else if (command == "exists" && args.size() == 2) {
        if (db.exists(args[1])) {
            std::cout << "true\n";
        } else {
            std::cout << "false\n";
        }
    } else if (command == "size" && args.size() == 1) {
        std::cout << db.size() << '\n';
    } else if (command == "keys" && args.size() == 1) {
        for (const auto& key : db.keys()) {
            std::cout << key << '\n';
        }
    } else if (command == "clear" && args.size() == 1) {
        db.clear();
        std::cout << "OK\n";
    } else if (command == "compact" && args.size() == 1) {
        db.compact();
        std::cout << "OK\n";
    } else if (command == "wal" && args.size() == 1) {
        db.printWALRecords();
    } else if (command == "help") {
        print_usage();
    } else if (command == "quit" || command == "exit") {
        // This case is handled in the main loop, but we keep it here for clarity
    } else {
        std::cout << "Unknown command: " << command << std::endl;
        print_usage();
    }
}

// 分割输入字符串
std::vector<std::string> split_input(const std::string& input) {
    std::istringstream iss(input);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}