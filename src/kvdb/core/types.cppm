export module kvdb.core.types;

import std;

// 统一的类型别名集中定义，便于跨模块复用与阅读
export namespace kvdb::core::types {
// Key/Value 语义别名
using Key = std::string;
using KeyView = std::string_view;
using Value = std::string;
using ValueView = std::string_view;

// 更明确的数值语义
using SeqNo = std::uint64_t;   // 序列号
using Offset = std::uint64_t;  // 文件偏移
using Size = std::uint64_t;    // 尺寸/长度
using Level = int;             // LSM 层级

// 二进制缓冲区相关
using Byte = std::byte;
using Bytes = std::vector<Byte>;
using ByteSpan = std::span<Byte>;
using ConstByteSpan = std::span<const Byte>;

// 常用容器别名
// Note: map 使用 transparent comparator 以便用 string_view 查找时避免临时构造
using OrderedKVMap = std::map<std::string, std::string, std::less<>>;

// 统一 Result 类型（错误为 std::string）
template <class T>
using Result = std::expected<T, std::string>;

}  // namespace kvdb::core::types
