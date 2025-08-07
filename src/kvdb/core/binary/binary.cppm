export module kvdb.core.binary;

import std;

export namespace kvdb::core::binary {

// 基础类型写入
// 成功返回 void, 失败返回错误信息
auto write_uint8(std::ostream& os, std::uint8_t value) -> std::expected<void, std::string>;
auto write_uint32(std::ostream& os, std::uint32_t value) -> std::expected<void, std::string>;
auto write_uint64(std::ostream& os, std::uint64_t value) -> std::expected<void, std::string>;
auto write_string(std::ostream& os, std::string_view str) -> std::expected<void, std::string>;

// 基础类型读取
// 成功返回值, 失败返回错误信息
auto read_uint8(std::istream& is) -> std::expected<std::uint8_t, std::string>;
auto read_uint32(std::istream& is) -> std::expected<std::uint32_t, std::string>;
auto read_uint64(std::istream& is) -> std::expected<std::uint64_t, std::string>;
auto read_string(std::istream& is) -> std::expected<std::string, std::string>;

// CRC32 校验和计算
std::uint32_t calculate_crc32(const std::vector<std::uint8_t>& data);

}  // namespace kvdb::core::binary
