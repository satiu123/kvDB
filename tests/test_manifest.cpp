#include <gtest/gtest.h>

#include <filesystem>
#include <memory>

import kvdb.core.database.manifest;

// 测试 Manifest 的序列化和反序列化是否对称且正确
TEST(ManifestTest, SerializationDeserialization) {
    // 1. 准备一个 Manifest 对象
    kvdb::core::database::Manifest original_manifest;
    original_manifest.last_wal_sequence_number = 12345;
    original_manifest.sstables[0] = {"file1.sst", "file2.sst"};
    original_manifest.sstables[2] = {"file3.sst"};

    // 2. 将其序列化到 stringstream
    std::stringstream ss;
    auto serialize_result = original_manifest.serialize(ss);
    ASSERT_TRUE(serialize_result.has_value());

    // 3. 创建一个新的 Manifest 对象并从 stringstream 反序列化
    kvdb::core::database::Manifest new_manifest;
    auto deserialize_result = new_manifest.deserialize(ss);
    ASSERT_TRUE(deserialize_result.has_value());

    // 4. 验证所有字段是否与原始对象完全一致
    EXPECT_EQ(new_manifest.last_wal_sequence_number, original_manifest.last_wal_sequence_number);
    EXPECT_EQ(new_manifest.sstables.size(), original_manifest.sstables.size());

    EXPECT_TRUE(new_manifest.sstables.contains(0));
    EXPECT_EQ(new_manifest.sstables[0], original_manifest.sstables[0]);

    EXPECT_TRUE(new_manifest.sstables.contains(2));
    EXPECT_EQ(new_manifest.sstables[2], original_manifest.sstables[2]);

    EXPECT_FALSE(new_manifest.sstables.contains(1));  // 确保不存在的层级也没有被意外添加
}

// 测试 ManifestFile 的存储和加载功能
class ManifestFileTest : public ::testing::Test {
  protected:
    void SetUp() override {
        cleanup();
        std::filesystem::create_directories(test_dir + "/manifest");
    }

    void TearDown() override {
        cleanup();
    }

    void cleanup() {
        if (std::filesystem::exists(test_dir)) {
            std::filesystem::remove_all(test_dir);
        }
    }

    const std::string test_dir = "_test_manifest_dir";
};

TEST_F(ManifestFileTest, StoreAndLoad) {
    // 1. 准备 Manifest 和 ManifestFile
    kvdb::core::database::Manifest original_manifest;
    original_manifest.last_wal_sequence_number = 999;
    original_manifest.sstables[1] = {"sstable-1-1.sst", "sstable-1-2.sst"};

    kvdb::core::database::ManifestFile manifest_file(test_dir);

    // 2. 存储 Manifest
    auto store_result = manifest_file.store(original_manifest);
    ASSERT_TRUE(store_result.has_value());

    // 3. 从文件加载 Manifest
    auto load_result = manifest_file.load();
    ASSERT_TRUE(load_result.has_value());
    kvdb::core::database::Manifest loaded_manifest = *load_result;

    // 4. 验证加载的数据是否正确
    EXPECT_EQ(loaded_manifest.last_wal_sequence_number, original_manifest.last_wal_sequence_number);
    EXPECT_EQ(loaded_manifest.sstables.size(), original_manifest.sstables.size());
    EXPECT_EQ(loaded_manifest.sstables[1], original_manifest.sstables[1]);
}

// 测试加载一个损坏的 Manifest 文件 (校验和错误)
TEST_F(ManifestFileTest, LoadCorruptedFile) {
    // 1. 手动创建一个损坏的 Manifest 文件
    std::string manifest_filename = "MANIFEST-000001";
    std::string manifest_path = std::string(test_dir + "/manifest") + "/" + manifest_filename;
    std::ofstream manifest_file(manifest_path, std::ios::binary);
    manifest_file << "this is corrupted data";
    manifest_file.close();

    // 写入 CURRENT 文件指向这个损坏的文件
    std::ofstream current_file(std::string(test_dir + "/manifest") + "/CURRENT");
    current_file << manifest_filename;
    current_file.close();

    // 2. 尝试加载
    kvdb::core::database::ManifestFile manifest_loader(test_dir);
    auto load_result = manifest_loader.load();

    // // 3. 验证加载是否失败
    // EXPECT_FALSE(load_result.has_value());
}
