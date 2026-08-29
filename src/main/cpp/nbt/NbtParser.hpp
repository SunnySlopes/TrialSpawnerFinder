#pragma once
// ============================================================
// TrialSpawnerFinder C++ NBT 解析器
//
// 逐位复刻 cn.trialfinder.sim.nbt.NbtIo 的最小只读 NBT 解析器。
// 模板文件为 big-endian NBT；gzip 解压依赖 zlib（可选）。
//
// 访问语义与 Java NbtTag.Compound / NbtTag.List 对齐：
//  - getString(key)：不存在或类型不符返回 ""；
//  - getInt(key)：Int/Short/Byte/Long 均可读，否则 0；
//  - getList(key)：不存在返回空 list。
// ============================================================

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace trialfinder {

using std::int32_t;
using std::int64_t;
using std::uint8_t;

enum class NbtType : uint8_t {
    END = 0, BYTE = 1, SHORT = 2, INT = 3, LONG = 4, FLOAT = 5,
    DOUBLE = 6, BYTE_ARRAY = 7, STRING = 8, LIST = 9,
    COMPOUND = 10, INT_ARRAY = 11, LONG_ARRAY = 12
};

/**
 * 递归 NBT 值。字段按类型复用；type 决定哪个字段有效。
 */
struct NbtTag {
    NbtType type = NbtType::END;

    int8_t b = 0;
    int16_t s = 0;
    int32_t i = 0;
    int64_t l = 0;
    float f = 0;
    double d = 0;
    std::string str;
    std::vector<int8_t> byteArray;
    std::vector<int32_t> intArray;
    std::vector<int64_t> longArray;

    // LIST
    NbtType listType = NbtType::END;
    std::vector<NbtTag> list;

    // COMPOUND
    std::unordered_map<std::string, NbtTag> compound;
};

// ---- Compound 访问辅助（对齐 Java NbtTag.Compound） ----

inline bool nbtContains(const NbtTag& tag, const std::string& key) {
    return tag.type == NbtType::COMPOUND
        && tag.compound.find(key) != tag.compound.end();
}

inline const NbtTag* nbtGet(const NbtTag& tag, const std::string& key) {
    if (tag.type != NbtType::COMPOUND) {
        return nullptr;
    }
    auto it = tag.compound.find(key);
    return it == tag.compound.end() ? nullptr : &it->second;
}

inline std::string nbtGetString(const NbtTag& tag, const std::string& key) {
    const NbtTag* entry = nbtGet(tag, key);
    if (entry == nullptr || entry->type != NbtType::STRING) {
        return "";
    }
    return entry->str;
}

inline int32_t nbtGetInt(const NbtTag& tag, const std::string& key) {
    const NbtTag* entry = nbtGet(tag, key);
    if (entry == nullptr) {
        return 0;
    }
    switch (entry->type) {
        case NbtType::INT: return entry->i;
        case NbtType::SHORT: return entry->s;
        case NbtType::BYTE: return entry->b;
        case NbtType::LONG: return static_cast<int32_t>(entry->l);
        default: return 0;
    }
}

inline int64_t nbtGetLong(const NbtTag& tag, const std::string& key) {
    const NbtTag* entry = nbtGet(tag, key);
    if (entry == nullptr) {
        return 0;
    }
    return entry->type == NbtType::LONG ? entry->l : nbtGetInt(tag, key);
}

inline const std::vector<NbtTag>& nbtGetList(const NbtTag& tag, const std::string& key) {
    static const std::vector<NbtTag> EMPTY;
    const NbtTag* entry = nbtGet(tag, key);
    if (entry == nullptr || entry->type != NbtType::LIST) {
        return EMPTY;
    }
    return entry->list;
}

inline const NbtTag* nbtGetCompound(const NbtTag& tag, const std::string& key) {
    const NbtTag* entry = nbtGet(tag, key);
    if (entry == nullptr || entry->type != NbtType::COMPOUND) {
        return nullptr;
    }
    return entry;
}

// ---- List 访问辅助（对齐 Java NbtTag.List） ----

inline int32_t nbtListGetInt(const NbtTag& listTag, std::size_t index) {
    if (listTag.type != NbtType::LIST || index >= listTag.list.size()) {
        return 0;
    }
    const NbtTag& entry = listTag.list[index];
    switch (entry.type) {
        case NbtType::INT: return entry.i;
        case NbtType::LONG: return static_cast<int32_t>(entry.l);
        default: return 0;
    }
}

// ---- NbtIo ----

class NbtIo {
public:
    /** 读取未压缩 big-endian NBT（Java {@code NbtIo.read}）。 */
    static NbtTag read(const uint8_t* data, std::size_t len);

    /** 读取 gzip 压缩 NBT（Java {@code NbtIo.readCompressed}）；需 zlib。 */
    static NbtTag readCompressed(const uint8_t* data, std::size_t len);
};

} // namespace trialfinder
