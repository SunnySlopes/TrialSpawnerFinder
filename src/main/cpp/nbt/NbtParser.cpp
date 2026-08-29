#include "nbt/NbtParser.hpp"

#include <stdexcept>

#ifdef TRIALFINDER_HAVE_ZLIB
#include <zlib.h>
#endif

namespace trialfinder {

// ---------------- ByteReader ----------------

namespace {

class ByteReader {
public:
    ByteReader(const uint8_t* data, std::size_t len)
        : data_(data), len_(len), pos_(0) {}

    std::size_t position() const { return pos_; }

    void ensure(std::size_t n) {
        if (n > len_ - pos_) {
            throw std::runtime_error("NBT 读取越界");
        }
    }

    uint8_t readByte() {
        ensure(1);
        return data_[pos_++];
    }

    uint16_t readUnsignedShort() {
        ensure(2);
        uint16_t v = (uint16_t)((uint16_t)data_[pos_] << 8 | (uint16_t)data_[pos_ + 1]);
        pos_ += 2;
        return v;
    }

    int16_t readShort() {
        return static_cast<int16_t>(readUnsignedShort());
    }

    int32_t readInt() {
        ensure(4);
        int32_t v = ((int32_t)data_[pos_] << 24)
                  | ((int32_t)data_[pos_ + 1] << 16)
                  | ((int32_t)data_[pos_ + 2] << 8)
                  | ((int32_t)data_[pos_ + 3]);
        pos_ += 4;
        return v;
    }

    int64_t readLong() {
        ensure(8);
        int64_t v = 0;
        for (int i = 0; i < 8; i++) {
            v = (v << 8) | (int64_t)data_[pos_ + i];
        }
        pos_ += 8;
        return v;
    }

    float readFloat() {
        int32_t bits = readInt();
        float f;
        std::memcpy(&f, &bits, sizeof(f));
        return f;
    }

    double readDouble() {
        int64_t bits = readLong();
        double d;
        std::memcpy(&d, &bits, sizeof(d));
        return d;
    }

    std::string readString() {
        uint16_t length = readUnsignedShort();
        ensure(length);
        std::string s(reinterpret_cast<const char*>(data_ + pos_), length);
        pos_ += length;
        return s;
    }

private:
    const uint8_t* data_;
    std::size_t len_;
    std::size_t pos_;
};

} // namespace

// ---------------- NBT 读取 ----------------

namespace {

NbtTag readTagPayload(ByteReader& input, NbtType type);

NbtTag readCompoundPayload(ByteReader& input) {
    NbtTag compound;
    compound.type = NbtType::COMPOUND;
    while (true) {
        uint8_t typeId = input.readByte();
        if (typeId == static_cast<uint8_t>(NbtType::END)) {
            break;
        }
        if (typeId > static_cast<uint8_t>(NbtType::LONG_ARRAY)) {
            throw std::runtime_error("未知 NBT 类型 id: " + std::to_string(typeId));
        }
        std::string name = input.readString();
        NbtTag tag = readTagPayload(input, static_cast<NbtType>(typeId));
        compound.compound[std::move(name)] = std::move(tag);
    }
    return compound;
}

NbtTag readTagPayload(ByteReader& input, NbtType type) {
    NbtTag tag;
    switch (type) {
        case NbtType::BYTE:
            tag.type = NbtType::BYTE;
            tag.b = static_cast<int8_t>(input.readByte());
            break;
        case NbtType::SHORT:
            tag.type = NbtType::SHORT;
            tag.s = input.readShort();
            break;
        case NbtType::INT:
            tag.type = NbtType::INT;
            tag.i = input.readInt();
            break;
        case NbtType::LONG:
            tag.type = NbtType::LONG;
            tag.l = input.readLong();
            break;
        case NbtType::FLOAT:
            tag.type = NbtType::FLOAT;
            tag.f = input.readFloat();
            break;
        case NbtType::DOUBLE:
            tag.type = NbtType::DOUBLE;
            tag.d = input.readDouble();
            break;
        case NbtType::BYTE_ARRAY: {
            tag.type = NbtType::BYTE_ARRAY;
            int32_t length = input.readInt();
            if (length < 0) {
                throw std::runtime_error("负的字节数组长度: " + std::to_string(length));
            }
            tag.byteArray.resize(static_cast<std::size_t>(length));
            for (int32_t i = 0; i < length; i++) {
                tag.byteArray[i] = static_cast<int8_t>(input.readByte());
            }
            break;
        }
        case NbtType::STRING:
            tag.type = NbtType::STRING;
            tag.str = input.readString();
            break;
        case NbtType::LIST: {
            tag.type = NbtType::LIST;
            uint8_t elementTypeId = input.readByte();
            tag.listType = static_cast<NbtType>(elementTypeId);
            int32_t length = input.readInt();
            if (length < 0) {
                throw std::runtime_error("负的列表长度: " + std::to_string(length));
            }
            tag.list.reserve(static_cast<std::size_t>(length));
            for (int32_t i = 0; i < length; i++) {
                tag.list.push_back(readTagPayload(input, tag.listType));
            }
            break;
        }
        case NbtType::COMPOUND:
            tag = readCompoundPayload(input);
            break;
        case NbtType::INT_ARRAY: {
            tag.type = NbtType::INT_ARRAY;
            int32_t length = input.readInt();
            if (length < 0) {
                throw std::runtime_error("负的整数数组长度: " + std::to_string(length));
            }
            tag.intArray.resize(static_cast<std::size_t>(length));
            for (int32_t i = 0; i < length; i++) {
                tag.intArray[i] = input.readInt();
            }
            break;
        }
        case NbtType::LONG_ARRAY: {
            tag.type = NbtType::LONG_ARRAY;
            int32_t length = input.readInt();
            if (length < 0) {
                throw std::runtime_error("负的长整数数组长度: " + std::to_string(length));
            }
            tag.longArray.resize(static_cast<std::size_t>(length));
            for (int32_t i = 0; i < length; i++) {
                tag.longArray[i] = input.readLong();
            }
            break;
        }
        default:
            throw std::runtime_error("不支持的 NBT 类型: " + std::to_string(static_cast<int>(type)));
    }
    return tag;
}

} // namespace

NbtTag NbtIo::read(const uint8_t* data, std::size_t len) {
    ByteReader input(data, len);
    uint8_t rootType = input.readByte();
    if (rootType != static_cast<uint8_t>(NbtType::COMPOUND)) {
        throw std::runtime_error("根 NBT 标签必须是 COMPOUND，实际是 " + std::to_string(rootType));
    }
    input.readString();  // 根 tag 名，忽略
    return readCompoundPayload(input);
}

NbtTag NbtIo::readCompressed(const uint8_t* data, std::size_t len) {
#ifndef TRIALFINDER_HAVE_ZLIB
    (void)data;
    (void)len;
    throw std::runtime_error(
        "NbtIo::readCompressed 需要 zlib（TRIALFINDER_HAVE_ZLIB 未定义）");
#else
    z_stream zs;
    std::memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, 16 + MAX_WBITS) != Z_OK) {  // 16+ = gzip 头
        throw std::runtime_error("inflateInit2 失败");
    }
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<uint8_t*>(data));
    zs.avail_in = static_cast<uInt>(len);

    std::vector<uint8_t> out;
    out.reserve(len * 4);
    uint8_t buffer[1 << 16];
    int ret;
    do {
        zs.next_out = buffer;
        zs.avail_out = sizeof(buffer);
        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&zs);
            throw std::runtime_error("gzip inflate 失败: " + std::to_string(ret));
        }
        std::size_t produced = sizeof(buffer) - zs.avail_out;
        out.insert(out.end(), buffer, buffer + produced);
    } while (ret != Z_STREAM_END);
    inflateEnd(&zs);

    return read(out.data(), out.size());
#endif
}

} // namespace trialfinder
