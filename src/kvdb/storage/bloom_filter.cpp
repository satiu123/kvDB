module kvdb.storage.bloom_filter;

import std;
import std.compat;

// MurmurHash3, 64-bit versions, by Austin Appleby
// https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
static inline std::uint64_t fmix64(std::uint64_t k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdULL;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ULL;
    k ^= k >> 33;
    return k;
}

static void MurmurHash3_x64_128(const void* key, const int len, const std::uint32_t seed,
                                void* out) {
    const auto* data = static_cast<const std::uint8_t*>(key);
    const int nblocks = len / 16;

    std::uint64_t h1 = seed;
    std::uint64_t h2 = seed;

    const auto* blocks = reinterpret_cast<const std::uint64_t*>(data);

    for (int i = 0; i < nblocks; i++) {
        std::uint64_t k1 = blocks[i * 2 + 0];
        std::uint64_t k2 = blocks[i * 2 + 1];

        k1 *= 0x87c37b91114253d5ULL;
        k1 = (k1 << 31) | (k1 >> (64 - 31));
        k1 *= 0x4cf5ad432745937fULL;
        h1 ^= k1;

        h1 = (h1 << 27) | (h1 >> (64 - 27));
        h1 += h2;
        h1 = h1 * 5 + 0x52dce729;

        k2 *= 0x4cf5ad432745937fULL;
        k2 = (k2 << 33) | (k2 >> (64 - 33));
        k2 *= 0x87c37b91114253d5ULL;
        h2 ^= k2;

        h2 = (h2 << 31) | (h2 >> (64 - 31));
        h2 += h1;
        h2 = h2 * 5 + 0x38495ab5;
    }

    const auto* tail = static_cast<const std::uint8_t*>(key) + nblocks * 16;

    std::uint64_t k1 = 0;
    std::uint64_t k2 = 0;

    switch (len & 15) {
        case 15:
            k2 ^= static_cast<std::uint64_t>(tail[14]) << 48;
        case 14:
            k2 ^= static_cast<std::uint64_t>(tail[13]) << 40;
        case 13:
            k2 ^= static_cast<std::uint64_t>(tail[12]) << 32;
        case 12:
            k2 ^= static_cast<std::uint64_t>(tail[11]) << 24;
        case 11:
            k2 ^= static_cast<std::uint64_t>(tail[10]) << 16;
        case 10:
            k2 ^= static_cast<std::uint64_t>(tail[9]) << 8;
        case 9:
            k2 ^= static_cast<std::uint64_t>(tail[8]) << 0;
            k2 *= 0x4cf5ad432745937fULL;
            k2 = (k2 << 33) | (k2 >> (64 - 33));
            k2 *= 0x87c37b91114253d5ULL;
            h2 ^= k2;

        case 8:
            k1 ^= static_cast<std::uint64_t>(tail[7]) << 56;
        case 7:
            k1 ^= static_cast<std::uint64_t>(tail[6]) << 48;
        case 6:
            k1 ^= static_cast<std::uint64_t>(tail[5]) << 40;
        case 5:
            k1 ^= static_cast<std::uint64_t>(tail[4]) << 32;
        case 4:
            k1 ^= static_cast<std::uint64_t>(tail[3]) << 24;
        case 3:
            k1 ^= static_cast<std::uint64_t>(tail[2]) << 16;
        case 2:
            k1 ^= static_cast<std::uint64_t>(tail[1]) << 8;
        case 1:
            k1 ^= static_cast<std::uint64_t>(tail[0]) << 0;
            k1 *= 0x87c37b91114253d5ULL;
            k1 = (k1 << 31) | (k1 >> (64 - 31));
            k1 *= 0x4cf5ad432745937fULL;
            h1 ^= k1;
    };

    h1 ^= len;
    h2 ^= len;

    h1 += h2;
    h2 += h1;

    h1 = fmix64(h1);
    h2 = fmix64(h2);

    h1 += h2;
    h2 += h1;

    reinterpret_cast<std::uint64_t*>(out)[0] = h1;
    reinterpret_cast<std::uint64_t*>(out)[1] = h2;
}

namespace kvdb::storage {

BloomFilter::BloomFilter(std::uint64_t num_items, double false_positive_rate) {
    double m = -static_cast<double>(num_items) * std::log(false_positive_rate) /
               (std::log(2) * std::log(2));
    std::size_t num_bits = static_cast<std::size_t>(std::ceil(m));
    num_hash_functions_ = static_cast<std::uint32_t>(
        std::round((static_cast<double>(num_bits) / num_items) * std::log(2)));
    bit_set_.resize(num_bits);
}

BloomFilter::BloomFilter(std::vector<bool> bit_set, std::uint32_t num_hash_functions)
    : num_hash_functions_(num_hash_functions), bit_set_(std::move(bit_set)) {}

void BloomFilter::add(std::string_view key) {
    auto hashes = hash(key);
    for (std::uint32_t i = 0; i < num_hash_functions_; ++i) {
        std::uint64_t h = hashes[0] + i * hashes[1];
        bit_set_[h % bit_set_.size()] = true;
    }
}

bool BloomFilter::contains(std::string_view key) const {
    auto hashes = hash(key);
    for (std::uint32_t i = 0; i < num_hash_functions_; ++i) {
        std::uint64_t h = hashes[0] + i * hashes[1];
        if (!bit_set_[h % bit_set_.size()]) {
            return false;
        }
    }
    return true;
}

const std::vector<bool>& BloomFilter::getBitSet() const {
    return bit_set_;
}

std::uint32_t BloomFilter::getNumHashFunctions() const {
    return num_hash_functions_;
}

std::size_t BloomFilter::getBitSetSizeInBytes() const {
    return (bit_set_.size() + 7) / 8;
}

void BloomFilter::serialize(std::ostream& os) const {
    std::uint32_t num_hash_functions = num_hash_functions_;
    os.write(reinterpret_cast<const char*>(&num_hash_functions), sizeof(num_hash_functions));

    std::uint64_t bit_set_size = bit_set_.size();
    os.write(reinterpret_cast<const char*>(&bit_set_size), sizeof(bit_set_size));

    std::vector<std::uint8_t> byte_vector((bit_set_size + 7) / 8, 0);
    for (std::size_t i = 0; i < bit_set_size; ++i) {
        if (bit_set_[i]) {
            byte_vector[i / 8] |= (1 << (i % 8));
        }
    }
    os.write(reinterpret_cast<const char*>(byte_vector.data()), byte_vector.size());
}

std::optional<BloomFilter> BloomFilter::deserialize(std::istream& is) {
    std::uint32_t num_hash_functions;
    is.read(reinterpret_cast<char*>(&num_hash_functions), sizeof(num_hash_functions));
    if (is.gcount() != sizeof(num_hash_functions)) {
        return std::nullopt;
    }

    std::uint64_t bit_set_size;
    is.read(reinterpret_cast<char*>(&bit_set_size), sizeof(bit_set_size));
    if (is.gcount() != sizeof(bit_set_size)) {
        return std::nullopt;
    }

    std::vector<std::uint8_t> byte_vector((bit_set_size + 7) / 8);
    is.read(reinterpret_cast<char*>(byte_vector.data()), byte_vector.size());
    if (static_cast<std::uint64_t>(is.gcount()) != byte_vector.size()) {
        return std::nullopt;
    }

    std::vector<bool> bit_set(bit_set_size);
    for (std::size_t i = 0; i < bit_set_size; ++i) {
        if ((byte_vector[i / 8] >> (i % 8)) & 1) {
            bit_set[i] = true;
        }
    }

    return BloomFilter(std::move(bit_set), num_hash_functions);
}

std::array<std::uint64_t, 2> BloomFilter::hash(std::string_view key) const {
    std::array<std::uint64_t, 2> hashes;
    MurmurHash3_x64_128(key.data(), static_cast<int>(key.length()), 0, hashes.data());
    return hashes;
}

}  // namespace kvdb::storage
