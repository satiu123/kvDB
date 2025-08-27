export module kvdb.core.io.ring_api;

import std;
import kvdb.core.types;

export namespace kvdb::core::io {
using kvdb::core::types::ByteSpan;
using kvdb::core::types::ConstByteSpan;

// 最小 I/O 提交接口：由 IOUring 等实现，供 Awaiter 调用
struct ISubmitter {
    virtual ~ISubmitter() = default;
    virtual void submit_read_request(int fd, ByteSpan buffer, std::uint64_t offset,
                                     void* user_data) = 0;
    virtual void submit_write_request(int fd, ConstByteSpan buffer, std::uint64_t offset,
                                      void* user_data) = 0;
    virtual void submit_nop_request(void* user_data) = 0;
};
}  // namespace kvdb::core::io
