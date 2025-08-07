module kvdb.core.database.manifest;

import std;
import kvdb.core.binary;
namespace kvdb::core::database {

auto Manifest::serialize(std::ostream& os) const -> std::expected<void, std::string> {
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

auto Manifest::deserialize(std::istream& is) -> std::expected<void, std::string> {
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

}  // namespace kvdb::core::database