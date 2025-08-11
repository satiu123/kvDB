export module kvdb.core.binary;

import std;

export namespace kvdb::core::binary {

// --- 拥有所有权的缓冲区 (用于构建数据) ---
class BytesBuffer {
  public:
    BytesBuffer() = default;
    explicit BytesBuffer(std::vector<std::byte> data) : data_(std::move(data)) {}

    void push(const std::byte* bytes, std::size_t size);
    void push_string(std::string_view str);

    auto get_data() const -> const std::vector<std::byte>& {
        return data_;
    }
    auto get_span() const -> std::span<const std::byte> {
        return data_;
    }
    std::size_t get_offset() const {
        return offset_;
    }

  private:
    std::vector<std::byte> data_;
    std::size_t offset_ = 0;
};

// --- 非拥有所有权的缓冲区视图 (用于零拷贝读写) ---
class BytesBufferView {
  public:
    explicit BytesBufferView(std::span<const std::byte> data) : r_span_(data) {}
    explicit BytesBufferView(std::span<std::byte> data) : r_span_(data), w_span_(data) {}
    // 写入 API
    auto write_uint8(std::uint8_t value) -> bool;
    auto write_uint32(std::uint32_t value) -> bool;
    auto write_uint64(std::uint64_t value) -> bool;
    auto write_string(std::string_view str) -> bool;
    template <typename T>
    static bool write_object_view(BytesBufferView& view, const T& value);
    // 读取 API
    auto read_uint8() -> std::expected<std::uint8_t, std::string>;
    auto read_uint32() -> std::expected<std::uint32_t, std::string>;
    auto read_uint64() -> std::expected<std::uint64_t, std::string>;
    auto read_string_view() -> std::expected<std::string_view, std::string>;
    template <typename T>
    static auto read_object_view(BytesBufferView& view) -> std::expected<T, std::string>;
    // 访问器
    std::size_t get_offset() const {
        return offset_;
    }
    std::span<const std::byte> get_written_span() const {
        return r_span_.first(offset_);
    }

  private:
    std::span<const std::byte> r_span_;  // 用于读取的视图
    std::span<std::byte> w_span_;        // 用于写入的视图
    std::size_t offset_ = 0;
};

// --- CRC32 校验和计算 ---
std::uint32_t calculate_crc32(std::span<const std::byte> data);

template <typename T>
bool BytesBufferView::write_object_view(BytesBufferView& view, const T& value) {
    auto bytes = std::as_bytes(std::span{std::addressof(value), 1});
    if (view.get_offset() + bytes.size() > view.w_span_.size())
        return false;
    std::memcpy(view.w_span_.data() + view.get_offset(), bytes.data(), bytes.size());
    view.offset_ += bytes.size();
    return true;
}

template <typename T>
auto BytesBufferView::read_object_view(BytesBufferView& view) -> std::expected<T, std::string> {
    if (view.get_offset() + sizeof(T) > view.r_span_.size()) {
        return std::unexpected("Read out of bounds");
    }
    T value;
    std::memcpy(&value, view.r_span_.data() + view.get_offset(), sizeof(T));
    view.offset_ += sizeof(T);
    return value;
}
}  // namespace kvdb::core::binary
