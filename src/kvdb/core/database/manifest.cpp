module kvdb.core.database.manifest;

import std;
import kvdb.core.binary;
import kvdb.logging.log;

using kvdb::logging::LOG_ERROR;

namespace kvdb::core::database {

// --- Manifest 实现 ---

std::expected<void, std::string> Manifest::serialize(std::ostream& os) const {
    std::stringstream buffer;

    if (auto res = binary::write_uint64(buffer, last_wal_sequence_number); !res)
        return std::unexpected(res.error());
    if (auto res = binary::write_uint32(buffer, static_cast<std::uint32_t>(sstables.size())); !res)
        return std::unexpected(res.error());

    for (const auto& [level, files] : sstables) {
        if (auto res = binary::write_uint32(buffer, static_cast<std::uint32_t>(level)); !res)
            return std::unexpected(res.error());
        if (auto res = binary::write_uint32(buffer, static_cast<std::uint32_t>(files.size())); !res)
            return std::unexpected(res.error());
        for (const auto& file : files) {
            if (auto res = binary::write_string(buffer, file); !res)
                return std::unexpected(res.error());
        }
    }

    std::string content = buffer.str();
    std::vector<std::uint8_t> data_vec(content.begin(), content.end());
    std::uint32_t crc = binary::calculate_crc32(data_vec);
    if (auto res = binary::write_uint32(os, crc); !res)
        return std::unexpected(res.error());

    os.write(content.c_str(), content.size());
    if (!os)
        return std::unexpected("Failed to write manifest content to output stream");

    return {};
}

std::expected<void, std::string> Manifest::deserialize(std::istream& is) {
    auto crc_res = binary::read_uint32(is);
    if (!crc_res)
        return std::unexpected(crc_res.error());
    std::uint32_t stored_crc = *crc_res;

    std::string content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    if (is.bad()) {
        return std::unexpected("Failed to read manifest content");
    }

    std::vector<std::uint8_t> data_vec(content.begin(), content.end());
    std::uint32_t calculated_crc = binary::calculate_crc32(data_vec);

    if (stored_crc != calculated_crc) {
        return std::unexpected("Manifest checksum mismatch. The file may be corrupted.");
    }

    std::stringstream buffer(content);
    auto seq_num_res = binary::read_uint64(buffer);
    if (!seq_num_res)
        return std::unexpected(seq_num_res.error());
    last_wal_sequence_number = *seq_num_res;

    auto levels_res = binary::read_uint32(buffer);
    if (!levels_res)
        return std::unexpected(levels_res.error());
    std::uint32_t sstable_levels = *levels_res;

    sstables.clear();
    for (std::uint32_t i = 0; i < sstable_levels; ++i) {
        auto level_res = binary::read_uint32(buffer);
        if (!level_res)
            return std::unexpected(level_res.error());
        std::uint32_t level = *level_res;

        auto num_files_res = binary::read_uint32(buffer);
        if (!num_files_res)
            return std::unexpected(num_files_res.error());
        std::uint32_t num_files = *num_files_res;

        std::vector<std::string> files;
        files.reserve(num_files);
        for (std::uint32_t j = 0; j < num_files; ++j) {
            auto file_res = binary::read_string(buffer);
            if (!file_res)
                return std::unexpected(file_res.error());
            files.push_back(*file_res);
        }
        sstables[static_cast<int>(level)] = std::move(files);
    }
    return {};
}

// --- ManifestFile 实现 ---

ManifestFile::ManifestFile(std::string_view path)
    : path_(path), current_path_(std::string(path) + "/CURRENT") {}

std::expected<Manifest, std::string> ManifestFile::load() {
    std::ifstream current_file(current_path_);
    if (!current_file.is_open()) {
        return Manifest{};  // 首次启动，没有CURRENT文件，是正常情况
    }

    std::string manifest_filename;
    current_file >> manifest_filename;
    current_file.close();

    if (manifest_filename.empty()) {
        return Manifest{};  // CURRENT文件为空，也算正常
    }

    std::ifstream manifest_file(std::string(path_) + "/" + manifest_filename, std::ios::binary);
    if (!manifest_file.is_open()) {
        return std::unexpected("Failed to open MANIFEST file: " + manifest_filename);
    }

    Manifest manifest;
    if (auto res = manifest.deserialize(manifest_file); !res) {
        LOG_ERROR()("Failed to load and parse MANIFEST file '{}': {}", manifest_filename,
                    res.error());
        return std::unexpected("Failed to parse MANIFEST file: " + res.error());
    }
    return manifest;
}

std::expected<void, std::string> ManifestFile::store(const Manifest& manifest) {
    std::string new_manifest_filename = get_new_manifest_filename();
    std::string new_manifest_path = std::string(path_) + "/" + new_manifest_filename;
    std::string temp_path = new_manifest_path + ".tmp";

    // 1. 写入到临时文件
    std::ofstream temp_file(temp_path, std::ios::binary);
    if (!temp_file.is_open()) {
        return std::unexpected("Failed to open temporary MANIFEST file for writing.");
    }
    if (auto res = manifest.serialize(temp_file); !res) {
        temp_file.close();
        return std::unexpected("Failed to serialize manifest: " + res.error());
    }
    temp_file.close();

    // 2. 原子性地重命名临时文件
    if (std::rename(temp_path.c_str(), new_manifest_path.c_str()) != 0) {
        return std::unexpected("Failed to rename temporary MANIFEST file.");
    }

    // 3. 更新CURRENT文件指向新的Manifest文件
    std::ofstream current_file(current_path_, std::ios::trunc);
    if (!current_file.is_open()) {
        // 这是一个严重问题，可能导致数据库状态不一致
        // 此时新Manifest已生成，但CURRENT未更新
        return std::unexpected("CRITICAL: Failed to open CURRENT file for writing.");
    }
    current_file << new_manifest_filename;
    current_file.close();

    return {};
}

std::string ManifestFile::get_new_manifest_filename() {
    int max_num = 0;
    if (!std::filesystem::exists(path_)) {
        std::filesystem::create_directories(path_);
    }
    for (const auto& entry : std::filesystem::directory_iterator(path_)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            if (filename.starts_with("MANIFEST-")) {
                int num = std::stoi(filename.substr(9));
                max_num = std::max(num, max_num);
            }
        }
    }
    return std::format("MANIFEST-{:06d}", max_num + 1);
}

}  // namespace kvdb::core::database