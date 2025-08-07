export module kvdb.core.binary;

import std;

export namespace kvdb::core::binary {

// --- 新增：基于 std::byte 的缓冲区 ---
class BytesBuffer {
  public:
    BytesBuffer() = default;
    explicit BytesBuffer(std::vector<std::byte> data) : data_(std::move(data)) {}

    // 写入数据
    void push(const std::byte* bytes, std::size_t size);
    void push_string(std::string_view str);

    // 读取数据
    auto read(std::size_t size) -> std::expected<std::span<const std::byte>, std::string>;
    auto read_string() -> std::expected<std::string, std::string>;

    // 访问底层数据
    auto get_data() const -> const std::vector<std::byte>& {
        return data_;
    }
    auto get_span() const -> std::span<const std::byte> {
        return data_;
    }
    bool eof() const {
        return offset_ >= data_.size();
    }
    std::size_t get_offset() const { return offset_; }

  private:
    std::vector<std::byte> data_;
    std::size_t offset_ = 0;
};

// --- 类型写入 (新：使用 BytesBuffer) ---
auto write_uint8(BytesBuffer& buf, std::uint8_t value) -> std::expected<void, std::string>;
auto write_uint32(BytesBuffer& buf, std::uint32_t value) -> std::expected<void, std::string>;
auto write_uint64(BytesBuffer& buf, std::uint64_t value) -> std::expected<void, std::string>;
auto write_string(BytesBuffer& buf, std::string_view str) -> std::expected<void, std::string>;

// --- 类型读取 (新：使用 BytesBuffer) ---
auto read_uint8(BytesBuffer& buf) -> std::expected<std::uint8_t, std::string>;
auto read_uint32(BytesBuffer& buf) -> std::expected<std::uint32_t, std::string>;
auto read_uint64(BytesBuffer& buf) -> std::expected<std::uint64_t, std::string>;
auto read_string(BytesBuffer& buf) -> std::expected<std::string, std::string>;

// --- CRC32 校验和计算 (新：使用 std::span<const std::byte>) ---
std::uint32_t calculate_crc32(std::span<const std::byte> data);


// --- 旧的 iostream API (保持兼容) ---
auto write_uint8(std::ostream& os, std::uint8_t value) -> std::expected<void, std::string>;
auto write_uint32(std::ostream& os, std::uint32_t value) -> std::expected<void, std::string>;
auto write_uint64(std::ostream& os, std::uint64_t value) -> std::expected<void, std::string>;
auto write_string(std::ostream& os, std::string_view str) -> std::expected<void, std::string>;

auto read_uint8(std::istream& is) -> std::expected<std::uint8_t, std::string>;
auto read_uint32(std::istream& is) -> std::expected<std::uint32_t, std::string>;
auto read_uint64(std::istream& is) -> std::expected<std::uint64_t, std::string>;
auto read_string(std::istream& is) -> std::expected<std::string, std::string>;

std::uint32_t calculate_crc32(const std::vector<std::uint8_t>& data);

}  // namespace kvdb::core::binary