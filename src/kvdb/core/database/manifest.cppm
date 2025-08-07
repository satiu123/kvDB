export module kvdb.core.database.manifest;

import std;

import kvdb.core.binary;
import kvdb.core.coro.task;
import kvdb.core.io.io_uring;

export namespace kvdb::core::database {

struct Manifest {
    std::uint64_t last_wal_sequence_number = 0;
    std::map<int, std::vector<std::string>> sstables;

    // 将Manifest序列化到输出流
    std::expected<void, std::string> serialize(std::ostream& os) const;
    // 从输入流反序列化Manifest
    std::expected<void, std::string> deserialize(std::istream& is);
};

class ManifestFile {
  public:
    explicit ManifestFile(std::string_view path);
    explicit ManifestFile(kvdb::core::io::IOUring& ring, std::string_view path);

    auto load() -> std::expected<Manifest, std::string>;
    auto store(const Manifest& manifest) -> std::expected<void, std::string>;

    auto async_load() -> kvdb::core::coro::Task<std::expected<Manifest, std::string>>;
    auto async_store(const Manifest& manifest)
        -> kvdb::core::coro::Task<std::expected<void, std::string>>;

  private:
    kvdb::core::io::IOUring* ring_{nullptr};
    std::string path_;
    std::string current_path_;

    std::string get_new_manifest_filename();
};

}  // namespace kvdb::core::database
