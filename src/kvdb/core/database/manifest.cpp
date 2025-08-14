module kvdb.core.database.manifest;

import std;
import kvdb.core.binary;
import kvdb.core.types;

using kvdb::core::binary::BytesBuffer;
using kvdb::core::binary::BytesBufferView;
using kvdb::core::types::ConstByteSpan;
using kvdb::core::types::Result;

namespace kvdb::core::database {

auto Manifest::serialize(std::ostream& os) const -> Result<void> {
    BytesBuffer buffer;

    // 使用 BytesBuffer 构建负载
    buffer.push(std::bit_cast<const std::byte*>(&last_wal_sequence_number),
                sizeof(last_wal_sequence_number));
    auto sstables_size = static_cast<std::uint32_t>(sstables.size());
    buffer.push(std::bit_cast<const std::byte*>(&sstables_size), sizeof(sstables_size));

    for (const auto& [level, files] : sstables) {
        auto level_u32 = static_cast<std::uint32_t>(level);
        auto files_size = static_cast<std::uint32_t>(files.size());
        buffer.push(std::bit_cast<const std::byte*>(&level_u32), sizeof(level_u32));
        buffer.push(std::bit_cast<const std::byte*>(&files_size), sizeof(files_size));
        for (const auto& file : files) {
            buffer.push_string(file);
        }
    }

    // 计算负载的 CRC
    auto payload_span = buffer.get_span();
    std::uint32_t crc = binary::calculate_crc32(payload_span);

    // 写入 CRC 和负载到输出流
    os.write(std::bit_cast<const char*>(&crc), sizeof(crc));
    os.write(std::bit_cast<const char*>(payload_span.data()), payload_span.size());

    if (!os) {
        return std::unexpected("无法将manifest写入输出流");
    }

    return {};
}

auto Manifest::deserialize(std::istream& is) -> Result<void> {
    // 读取 CRC
    std::uint32_t stored_crc;
    is.read(std::bit_cast<char*>(&stored_crc), sizeof(stored_crc));
    if (!is) {
        return std::unexpected("无法读取manifest校验和");
    }

    // 读取剩余的负载
    std::string payload_str((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
    if (is.bad()) {
        return std::unexpected("无法读取manifest负载");
    }
    ConstByteSpan payload_span(std::bit_cast<const std::byte*>(payload_str.data()),
                               payload_str.size());

    // 校验 CRC
    if (binary::calculate_crc32(payload_span) != stored_crc) {
        return std::unexpected("Manifest校验和不匹配。文件可能已损坏。");
    }

    // 使用 BytesBufferView 从负载中解析数据
    BytesBufferView buffer(payload_span);

    auto seq_num_res = buffer.read_uint64();
    if (!seq_num_res)
        return std::unexpected(seq_num_res.error());
    last_wal_sequence_number = *seq_num_res;

    auto levels_res = buffer.read_uint32();
    if (!levels_res)
        return std::unexpected(levels_res.error());
    std::uint32_t sstable_levels = *levels_res;

    sstables.clear();
    for (std::uint32_t i = 0; i < sstable_levels; ++i) {
        auto level_res = buffer.read_uint32();
        if (!level_res)
            return std::unexpected(level_res.error());
        std::uint32_t level = *level_res;

        auto num_files_res = buffer.read_uint32();
        if (!num_files_res)
            return std::unexpected(num_files_res.error());
        std::uint32_t num_files = *num_files_res;

        std::vector<std::string> files;
        files.reserve(num_files);
        for (std::uint32_t j = 0; j < num_files; ++j) {
            auto file_res = buffer.read_string_view();  // 使用 read_string_view
            if (!file_res)
                return std::unexpected(file_res.error());
            files.emplace_back(*file_res);
        }
        sstables[static_cast<int>(level)] = std::move(files);
    }

    return {};
}

}  // namespace kvdb::core::database