import std;
import kvdb.core;    // AsyncDatabase
import kvdb.logging; // Logger & sinks

namespace {
// 简单分词
auto split(std::string_view s) -> std::vector<std::string> {
    std::vector<std::string> out;
    std::string token;
    std::istringstream iss(std::string{s});
    while (iss >> token)
        out.push_back(std::move(token));
    return out;
}

void print_usage() {
    std::cout << "命令:\n"
              << "  put <key> <value>   插入/更新\n"
              << "  get <key>           查询\n"
              << "  del <key>           删除\n"
              << "  compact             压缩合并 SSTables\n"
              << "  wal                 打印 WAL 记录\n"
              << "  sstables            打印 SSTable 列表\n"
              << "  manifest            打印 MANIFEST\n"
              << "  help                显示帮助\n"
              << "  exit|quit           退出\n";
}
}  // namespace

int main(int argc, char* argv[]) {
    std::string db_path = argc > 1 ? argv[1] : std::string{"./data/performance_test_db_async"};
    std::filesystem::create_directories(db_path);

    // 配置日志到控制台（可选）
    auto& logger = kvdb::logging::Logger::getInstance();
    logger.addSink(std::make_shared<kvdb::logging::ConsoleSink>());

    kvdb::core::AsyncDatabase db(db_path);
    db.set_flush_threshold(static_cast<std::uint64_t>(4) * 1024);  // 4KB 阈值
    db.run(db.init());                                             // 初始化
    // 等待日志线程把初始化期间的日志全部输出完毕
    logger.flush();

    std::cout << "kvDB CLI 已就绪，数据目录: " << db_path << "\n";
    print_usage();

    std::string line;
    std::cout << "kvdb> " << std::flush;
    while (std::getline(std::cin, line)) {
        auto args = split(line);
        if (args.empty()) {
            std::cout << "kvdb> " << std::flush;
            continue;
        }
        const auto& cmd = args[0];
        if (cmd == "exit" || cmd == "quit")
            break;
        if (cmd == "help") {
            print_usage();
        } else if (cmd == "put" && args.size() >= 3) {
            bool ok = db.run(db.async_put(args[1], args[2]));
            std::cout << (ok ? "OK" : "ERR") << "\n";
        } else if (cmd == "get" && args.size() >= 2) {
            auto val = db.run(db.async_get(args[1]));
            if (val)
                std::cout << *val << "\n";
            else
                std::cout << "(nil)\n";
        } else if (cmd == "del" && args.size() >= 2) {
            bool ok = db.run(db.async_remove(args[1]));
            std::cout << (ok ? "OK" : "ERR") << "\n";
        } else if (cmd == "compact") {
            db.run(db.compact_sstables());
            std::cout << "OK\n";
        } else if (cmd == "wal") {
            db.run(db.printWALRecords());
        } else if (cmd == "sstables") {
            db.run(db.printSSTables());
        } else if (cmd == "manifest") {
            db.printManifest();
        } else {
            std::cout << "未知命令，输入 help 查看可用命令。\n";
        }
        std::cout << "kvdb> " << std::flush;
    }
    return 0;
}