#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <string>

import kvdb.core.types;
import kvdb.core.io.io_uring;
import kvdb.core.coro.task;
import kvdb.core.database.manifest;
import kvdb.core.database.async_manifest;

using kvdb::core::coro::Task;
using kvdb::core::database::AsyncManifestFile;
using kvdb::core::database::Manifest;
using kvdb::core::io::IOUring;
using kvdb::core::types::Result;

TEST(AsyncManifest, StoreThenLoad) {
    const std::filesystem::path base{"./test_manifest_async"};
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);

    IOUring ring(64);
    AsyncManifestFile mf(ring, base);

    // prepare manifest
    Manifest m;
    m.last_wal_sequence_number = 42;
    m.sstables[0] = {(base / "sstables" / "sstable-000001.sst").string()};

    // store
    auto st = mf.async_store(m);
    st.resume();
    while (!st.done()) {
        ring.wait_for_completion();
    }
    auto sres = st.get();
    ASSERT_TRUE(sres.has_value()) << (sres ? "" : sres.error());

    // load
    auto lt = mf.async_load();
    lt.resume();
    while (!lt.done()) {
        ring.wait_for_completion();
    }
    auto lres = lt.get();
    ASSERT_TRUE(lres.has_value()) << (lres ? "" : lres.error());
    auto m2 = *lres;

    EXPECT_EQ(m2.last_wal_sequence_number, 42u);
    ASSERT_FALSE(m2.sstables.empty());
    ASSERT_FALSE(m2.sstables[0].empty());
}
