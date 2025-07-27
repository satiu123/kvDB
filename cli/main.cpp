import std;
import std.compat;

import kvdb.core.database;
import kvdb.logging.log.log_impl;
import kvdb.logging.log.logger;
import kvdb.logging.log.log_record;


import kvdb.logging.sinks.file_sink;
import kvdb.logging.sinks.console_sink;


void print_usage() {
    std::cout << "Commands:" << std::endl;
    std::cout << "  get <key>        Get the value of a key" << std::endl;
    std::cout << "  set <key> <value>  Set the value of a key" << std::endl;
    std::cout << "  rm <key>         Remove a key" << std::endl;
    std::cout << "  ls               List all keys (not implemented)" << std::endl;
    std::cout << "  size             Show number of entries in database" << std::endl;
    std::cout << "  clear            Clear all data from database" << std::endl;
    std::cout << "  help             Show this help message" << std::endl;
    std::cout << "  exit/quit        Exit the CLI" << std::endl;
    std::cout << std::endl;
}

int main() {
    kvdb::Database db("kvdb.wal");
    kvdb::logging::Logger::getInstance().setLevel(kvdb::logging::LogLevel::DEBUG);
    kvdb::logging::Logger::getInstance().addSink(std::make_shared<kvdb::FileSink>("kvdb.log"));
    std::cout << "Welcome to kvDB CLI!" << std::endl;
    std::cout << "Type 'help' for available commands." << std::endl;
    std::cout << std::endl;

    std::string input;
    while (true) {
        std::cout << "kvdb> ";
        std::cout.flush();

        if (!std::getline(std::cin, input)) {
            // EOF reached (Ctrl+D)
            std::cout << std::endl;
            break;
        }

        // Trim whitespace
        size_t start = input.find_first_not_of(" \t");
        if (start == std::string::npos) {
            continue;  // Empty line
        }
        size_t end = input.find_last_not_of(" \t");
        input = input.substr(start, end - start + 1);

        if (input == "exit" || input == "quit") {
            break;
        }

        std::vector<std::string> args;
        std::string current_arg;
        for (char c : input) {
            if (c == ' ') {
                if (!current_arg.empty()) {
                    args.push_back(current_arg);
                    current_arg.clear();
                }
            } else {
                current_arg += c;
            }
        }
        if (!current_arg.empty()) {
            args.push_back(current_arg);
        }

        if (args.empty()) {
            continue;
        }

        const std::string& command = args[0];

        if (command == "get") {
            if (args.size() != 2) {
                print_usage();
                continue;
            }
            const std::string& key = args[1];
            auto value = db.get(key);
            if (value) {
                std::cout << *value << std::endl;
            } else {
                std::cerr << "Key not found" << std::endl;
            }
        } else if (command == "set") {
            if (args.size() != 3) {
                print_usage();
                continue;
            }
            const std::string& key = args[1];
            const std::string& value = args[2];
            if (db.put(key, value)) {
                std::cout << "OK" << std::endl;
            } else {
                std::cerr << "Failed to set value" << std::endl;
            }
        } else if (command == "rm") {
            if (args.size() != 2) {
                print_usage();
                continue;
            }
            const std::string& key = args[1];
            if (db.remove(key)) {
                std::cout << "OK" << std::endl;
            } else {
                std::cerr << "Key not found" << std::endl;
            }
        } else if (command == "ls") {
            if (args.size() == 2 and args[1] == "wal") {
                db.printWALRecords();
                continue;
            }
            if (args.size() > 2) {
                print_usage();
                continue;
            }
            auto keys = db.keys();
            for (const auto& key : keys) {
                std::cout << key << std::endl;
            }
        } else if (command == "help") {
            print_usage();
        } else if (command == "size") {
            std::cout << "Database contains " << db.size() << " entries" << std::endl;
        } else if (command == "clear") {
            std::cout << "Are you sure you want to clear all data? (y/N): ";
            std::string confirm;
            std::getline(std::cin, confirm);
            if (confirm == "y" || confirm == "Y" || confirm == "yes") {
                db.clear();
                std::cout << "Database cleared." << std::endl;
            } else {
                std::cout << "Operation cancelled." << std::endl;
            }
        } else {
            std::cout << "Unknown command: " << command << std::endl;
            print_usage();
        }
    }
    std::cout << "Goodbye!" << std::endl;
    return 0;
}
