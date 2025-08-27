#include <gtest/gtest.h>

#include <coroutine>
#include <filesystem>
#include <string>
#include <vector>

import kvdb.core.types;
import kvdb.core.io.io_uring;
import kvdb.core.io.file;
import kvdb.core.coro.task;
import kvdb.storage.wal.async_wal;
import kvdb.storage.wal.wal_record;

using kvdb::core::coro::Task;
using kvdb::core::io::IOUring;
using kvdb::core::types::KeyView;
using kvdb::core::types::ValueView;
using kvdb::storage::AsyncWal;
using kvdb::storage::WalOpType;
using kvdb::storage::WalRecord;

namespace {
static Task<bool> append_sequence(AsyncWal& wal) {
    bool ok = true;
    ok = co_await wal.async_append_put("k1", "v1");
    if (!ok)
        co_return false;
    ok = co_await wal.async_append_put("k2", "v2");
    if (!ok)
        co_return false;
    ok = co_await wal.async_append_remove("k1");
    if (!ok)
        co_return false;
    ok = co_await wal.async_append_clear();
    co_return ok;
}
}  // namespace

TEST(AsyncWal, AppendAndReplay) {
    std::filesystem::remove_all("./test_wal");
    std::filesystem::create_directories("./test_wal");
    IOUring ring(64);
    AsyncWal wal(ring, std::filesystem::path{"./test_wal"});

    // append
    auto t = append_sequence(wal);
    t.resume();
    while (!t.done()) {
        ring.wait_for_completion();
    }
    ASSERT_TRUE(t.get());

    // replay
    std::vector<WalRecord> replayed;
    auto handler = [&replayed](const WalRecord& rec) {
        replayed.push_back(rec);
        return true;
    };
    auto rt = wal.async_replay(handler);
    rt.resume();
    while (!rt.done()) {
        ring.wait_for_completion();
    }
    ASSERT_TRUE(rt.get());

    ASSERT_EQ(replayed.size(), 4u);
    EXPECT_EQ(replayed[0].getOpType(), WalOpType::PUT);
    EXPECT_EQ(replayed[1].getOpType(), WalOpType::PUT);
    EXPECT_EQ(replayed[2].getOpType(), WalOpType::REMOVE);
    EXPECT_EQ(replayed[3].getOpType(), WalOpType::CLEAR);

    // monotonic seq
    std::uint64_t last = 0;
    for (auto& r : replayed) {
        EXPECT_GT(r.getSequenceNumber(), last);
        last = r.getSequenceNumber();
    }
}
