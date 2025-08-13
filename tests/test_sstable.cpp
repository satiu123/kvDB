#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <string>

import kvdb.core.types;
import kvdb.core.io.io_uring;
import kvdb.core.coro.task;
import kvdb.storage.sstable;

using kvdb::core::coro::Task;
using kvdb::core::io::IOUring;
using kvdb::core::types::KeyView;
using kvdb::core::types::OrderedKVMap;
using kvdb::storage::SSTable;

TEST(SSTable, BuildOpenFindReadAll) {
    const std::filesystem::path base{"./test_sstable"};
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(base);
    const std::string sst_path = (base / "sstable-000001.sst").string();

    IOUring ring(64);

    // prepare data
    OrderedKVMap data;
    data["a"] = "1";
    data["b"] = "2";
    data["c"] = "3";

    // build
    auto bt = SSTable::buildFrom(ring, sst_path, data);
    bt.resume();
    while (!bt.done()) {
        ring.wait_for_completion();
    }
    ASSERT_TRUE(bt.get());

    // open
    SSTable sst(ring);
    auto ot = sst.open(sst_path);
    ot.resume();
    while (!ot.done()) {
        ring.wait_for_completion();
    }
    ASSERT_TRUE(ot.get());

    // find existing
    auto ft1 = sst.find(KeyView{"a"});
    ft1.resume();
    while (!ft1.done()) {
        ring.wait_for_completion();
    }
    auto v1 = ft1.get();
    ASSERT_TRUE(v1.has_value());
    EXPECT_EQ(*v1, "1");

    // find non-existing
    auto ft2 = sst.find(KeyView{"z"});
    ft2.resume();
    while (!ft2.done()) {
        ring.wait_for_completion();
    }
    auto vz = ft2.get();
    EXPECT_FALSE(vz.has_value());

    // readAll
    auto rt = sst.readAll();
    rt.resume();
    while (!rt.done()) {
        ring.wait_for_completion();
    }
    auto all = rt.get();
    ASSERT_EQ(all.size(), 3u);
    EXPECT_EQ(all["a"], "1");
    EXPECT_EQ(all["b"], "2");
    EXPECT_EQ(all["c"], "3");
}
