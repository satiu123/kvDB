import std;
import kvdb;

// 函数声明
void print_usage();
void process_command(kvdb::core::Database& db, const std::string& line);
std::vector<std::string> split_input(const std::string& input);


// 打印使用帮助
void print_usage() {
    std::cout << "命令:\n"
              << "  put <key> <value>    - 插入或更新一个键值对。\n"
              << "  get <key>            - 检索一个键的值。\n"
              << "  remove <key>         - 删除一个键。\n"
              << "  exists <key>         - 检查一个键是否存在。\n"
              << "  size                 - 获取键的数量。\n"
              << "  keys                 - 列出所有的键。\n"
              << "  clear                - 清空数据库。\n"
              << "  compact              - 压缩数据库。\n"
              << "  wal                  - 打印WAL记录。\n"
              << "  exit/quit            - 退出CLI。\n"
              << "  help                 - 显示此帮助信息。\n";
}

// 主函数
int main(int argc, char* argv[]) {
    std::string db_path = ".";  // 默认数据库路径
    if (argc > 1) {
        db_path = argv[1];
    }
    kvdb::core::Database db(db_path);
    db.setMemtableFlushThreshold(4);
    if (auto sink = kvdb::logging::FileSink::create(db_path + "/data/kvdb.log")) {
        kvdb::logging::Logger::getInstance().addSink(*sink);
    } else {
        std::cerr << "创建文件日志接收器失败: " << sink.error() << std::endl;
    }
    std::cout << "欢迎来到 kvDB 命令行界面!\n";
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

    std::cout << "\n再见!" << std::endl;
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
            std::cerr << "错误: 存入键失败。\n";
        }
    } else if (command == "get" && args.size() == 2) {
        auto value = db.get(args[1]);
        if (value) {
            std::cout << *value << '\n';
        } else {
            std::cout << "(空)\n";
        }
    } else if (command == "remove" && args.size() == 2) {
        if (db.remove(args[1])) {
            std::cout << "OK\n";
        } else {
            std::cerr << "错误: 键未找到或删除失败。\n";
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
        // 这个情况在主循环中处理，但为了清晰起见保留在此处
    } else {
        std::cout << "未知命令: " << command << std::endl;
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