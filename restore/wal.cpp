module kvdb.storage.wal;

import std;
import kvdb.logging.log;

using kvdb::logging::LOG_ERROR, kvdb::logging::LOG_INFO;

namespace kvdb::storage {

// --- Private Helper Functions (No Locking) ---

void Wal::sync_locked() {
    if (is_open_) {
        file_.flush();
    }
}

bool Wal::isEmpty_locked() {
    if (!is_open_) {
        return true;
    }
    auto current_pos = file_.tellg();
    file_.seekg(0, std::ios::beg);
    bool is_empty = (file_.peek() == std::ios::traits_type::eof());
    file_.seekg(current_pos);
    return is_empty;
}

// --- Public API ---

Wal::Wal(const std::filesystem::path& path) : path_(path.string()) {
    open(false);
    sync_thread_ = std::jthread(&Wal::syncLoop, this);
    LOG_INFO()("WAL (sync) started for path: {}", path_);
}

Wal::~Wal() {
    stop_sync_ = true;
    cv_.notify_one();
    if (sync_thread_.joinable()) {
        sync_thread_.join();
    }
    close();
}

bool Wal::appendPut(std::string_view key, std::string_view value) {
    WalRecord record(WalOpType::PUT, key, value, ++sequence_number_);
    return appendRecord(record);
}

bool Wal::appendRemove(std::string_view key) {
    WalRecord record(WalOpType::REMOVE, key, "", ++sequence_number_);
    return appendRecord(record);
}

bool Wal::appendClear() {
    WalRecord record(WalOpType::CLEAR, "", "", ++sequence_number_);
    return appendRecord(record);
}

bool Wal::appendRecord(const WalRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_open_ && !open(false)) {
        LOG_ERROR()("Failed to open WAL file: {}", path_);
        return false;
    }

    auto data = record.serialize();
    file_.seekp(0, std::ios::end);
    file_.write(reinterpret_cast<const char*>(data.data()), data.size());

    if (file_.fail()) {
        LOG_ERROR()("Failed to write WAL record: {}", path_);
        return false;
    }

    has_new_data_ = true;
    cv_.notify_one();
    return true;
}

void Wal::syncLoop() {
    while (true) {
        std::unique_lock<std::mutex> lock(sync_mutex_);
        cv_.wait_for(lock, std::chrono::milliseconds(10),
                     [this] { return stop_sync_.load() || has_new_data_.load(); });

        if (has_new_data_.load()) {
            has_new_data_ = false;
            sync();
        }

        if (stop_sync_.load()) {
            break;
        }
    }
}

bool Wal::sync() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_open_)
        return false;
    sync_locked();
    return !file_.fail();
}

bool Wal::open(bool truncate) {
    if (is_open_) {
        file_.close();
    }
    std::ios_base::openmode mode = std::ios::binary | std::ios::in | std::ios::out | std::ios::ate;
    if (truncate) {
        mode |= std::ios::trunc;
    } else if (!std::filesystem::exists(path_)) {
        mode |= std::ios::trunc;
    }
    file_.open(path_, mode);
    is_open_ = file_.is_open();
    if (!is_open_) {
        LOG_ERROR()("Failed to open WAL file: {}", path_);
    }
    return is_open_;
}

bool Wal::replay(const std::function<bool(const WalRecord&)>& handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_open_ && !open(false))
        return false;

    auto current_pos = file_.tellg();
    file_.seekg(0, std::ios::beg);
    std::uint64_t max_seq = sequence_number_.load();
    while (true) {
        auto record_result = readNextRecord();
        if (!record_result)
            break;
        if (!handler(*record_result.value())) {
            LOG_ERROR()("WAL replay handler failed.");
            file_.seekg(current_pos);
            return false;
        }
        max_seq = std::max(max_seq, record_result.value()->getSequenceNumber());
    }
    if (file_.bad()) {
        LOG_ERROR()("Error reading WAL file: {}", path_);
        file_.seekg(current_pos);
        return false;
    }
    file_.clear();
    file_.seekg(current_pos);
    sequence_number_ = max_seq;
    return true;
}

auto Wal::readNextRecord() -> std::expected<std::unique_ptr<WalRecord>, std::string> {
    if (!is_open_ || file_.peek() == std::ios::traits_type::eof()) {
        return std::unexpected("WAL file not open or EOF");
    }
    std::uint32_t total_size = 0;
    file_.read(reinterpret_cast<char*>(&total_size), sizeof(total_size));
    if (file_.gcount() == 0)
        return std::unexpected("Reached WAL EOF");
    if (!file_)
        return std::unexpected("Failed to read record length");
    if (total_size < sizeof(total_size))
        return std::unexpected("Invalid record length");

    std::vector<std::byte> record_data(total_size);
    std::memcpy(record_data.data(), &total_size, sizeof(total_size));
    file_.read(reinterpret_cast<char*>(record_data.data() + sizeof(total_size)),
               total_size - sizeof(total_size));
    if (!file_)
        return std::unexpected("Failed to read record data");

    return WalRecord::deserialize(record_data);
}

bool Wal::truncate() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_open_) {
        file_.close();
        is_open_ = false;
    }
    return open(true);
}

bool Wal::isEmpty() {
    std::lock_guard<std::mutex> lock(mutex_);
    return isEmpty_locked();
}

void Wal::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_open_) {
        sync_locked();
        file_.close();
        is_open_ = false;
    }
}

auto Wal::getFormattedContent() -> std::expected<std::vector<std::string>, std::string> {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isEmpty_locked())
        return std::vector<std::string>{};

    std::vector<std::string> formatted_lines;
    auto current_pos = file_.tellg();
    file_.seekg(0, std::ios::beg);
    while (true) {
        auto record = readNextRecord();
        if (!record)
            break;
        formatted_lines.emplace_back(record.value()->toString());
    }
    file_.clear();
    file_.seekg(current_pos);
    return formatted_lines;
}

std::uint64_t Wal::getLastSequenceNumber() const {
    return sequence_number_.load();
}

void Wal::setCurrentSequenceNumber(std::uint64_t seq) {
    sequence_number_ = seq;
}

}  // namespace kvdb::storage
