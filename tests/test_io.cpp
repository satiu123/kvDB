#include <gtest/gtest.h>

#include <coroutine>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

import kvdb.core.io.io_uring;
import kvdb.core.io.file;
import kvdb.core.coro.task;
import kvdb.core.types;

using kvdb::core::coro::Task;
using kvdb::core::io::File;
using kvdb::core::io::FileMode;
using kvdb::core::io::IOUring;
using kvdb::core::types::ByteSpan;
using kvdb::core::types::ConstByteSpan;

static Task<int> write_file(File& f, ConstByteSpan data) {
    co_return co_await f.write(data, -1);
}

static Task<int> read_file(File& f, ByteSpan buf, std::uint64_t off) {
    co_return co_await f.read(buf, off);
}

TEST(IO, FileReadWrite) {
    std::filesystem::create_directories("./test_io");
    std::string path = "./test_io/async_io_test.bin";
    IOUring ring(64);

    // Write some bytes
    File wf(ring, path, FileMode::Write);
    std::string payload = "hello-io_uring";
    std::vector<std::byte> w(payload.size());
    std::memcpy(w.data(), payload.data(), payload.size());
    auto wt = write_file(wf, ConstByteSpan{w.data(), w.size()});
    wt.resume();

    while (!wt.done()) {
        ring.wait_for_completion();
    }
    auto wres = wt.get();
    ASSERT_GE(wres, 0);

    // Read back
    File rf(ring, path, FileMode::Read);
    std::vector<std::byte> r(payload.size());
    auto rt = read_file(rf, ByteSpan{r.data(), r.size()}, 0);
    rt.resume();

    while (!rt.done()) {
        ring.wait_for_completion();
    }
    auto rres = rt.get();
    ASSERT_EQ(rres, static_cast<int>(payload.size()));
    std::string roundtrip(std::bit_cast<char*>(r.data()), r.size());
    EXPECT_EQ(roundtrip, payload);
}
