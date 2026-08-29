#pragma once
// ============================================================
// TrialSpawnerFinder C++ 核心 —— 基础类型
//
// 逐位复刻 cn.trialfinder.sim.math 的 BlockPos / BoundingBox /
// Direction / Rotation / FrontAndTop / Mth（26.2 语义）。
// 所有整数运算与 Java 逐位一致：int32 回绕用显式 uint32 截断，
// 有符号右移依赖算术右移（MSVC/GCC/Clang 均保证）。
// ============================================================

#include <cstdint>
#include <string>
#include <vector>

namespace trialfinder {

using std::int32_t;
using std::int64_t;
using std::uint8_t;
using std::uint32_t;
using std::uint64_t;

// ---------------- Vec3i / BlockPos ----------------

/** 复刻 {@code Vec3i} + {@code BlockPos} 子集。 */
struct BlockPos {
    int x, y, z;

    static const BlockPos ZERO;

    BlockPos() : x(0), y(0), z(0) {}
    BlockPos(int x_, int y_, int z_) : x(x_), y(y_), z(z_) {}

    int getX() const { return x; }
    int getY() const { return y; }
    int getZ() const { return z; }

    BlockPos offset(int dx, int dy, int dz) const {
        return BlockPos(x + dx, y + dy, z + dz);
    }
    BlockPos offset(const BlockPos& other) const {
        return BlockPos(x + other.x, y + other.y, z + other.z);
    }
    BlockPos subtract(const BlockPos& other) const {
        return BlockPos(x - other.x, y - other.y, z - other.z);
    }

    bool operator==(const BlockPos& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
    bool operator!=(const BlockPos& o) const { return !(*this == o); }
};

// ---------------- BoundingBox ----------------

/**
 * 复刻 {@code BoundingBox} —— 闭区间整数盒。
 */
struct BoundingBox {
    int minX, minY, minZ;
    int maxX, maxY, maxZ;

    BoundingBox() : minX(0), minY(0), minZ(0), maxX(-1), maxY(-1), maxZ(-1) {}
    BoundingBox(int minX_, int minY_, int minZ_, int maxX_, int maxY_, int maxZ_)
        : minX(minX_), minY(minY_), minZ(minZ_),
          maxX(maxX_), maxY(maxY_), maxZ(maxZ_) {}

    int minX_() const { return minX; }
    int minY_() const { return minY; }
    int minZ_() const { return minZ; }
    int maxX_() const { return maxX; }
    int maxY_() const { return maxY; }
    int maxZ_() const { return maxZ; }

    int getXSpan() const { return maxX - minX + 1; }
    int getYSpan() const { return maxY - minY + 1; }
    int getZSpan() const { return maxZ - minZ + 1; }

    bool isInside(int x_, int y_, int z_) const {
        return x_ >= minX && x_ <= maxX && z_ >= minZ && z_ <= maxZ
            && y_ >= minY && y_ <= maxY;
    }
    bool isInside(const BlockPos& pos) const {
        return isInside(pos.x, pos.y, pos.z);
    }

    bool intersects(const BoundingBox& o) const {
        return minX <= o.maxX && maxX >= o.minX
            && minZ <= o.maxZ && maxZ >= o.minZ
            && minY <= o.maxY && maxY >= o.minY;
    }

    BoundingBox moved(int dx, int dy, int dz) const {
        return BoundingBox(minX + dx, minY + dy, minZ + dz,
                           maxX + dx, maxY + dy, maxZ + dz);
    }

    BoundingBox encapsulate(const BlockPos& pos) const {
        return BoundingBox(
            std::min(minX, pos.x), std::min(minY, pos.y), std::min(minZ, pos.z),
            std::max(maxX, pos.x), std::max(maxY, pos.y), std::max(maxZ, pos.z));
    }

    BoundingBox encapsulate(const BoundingBox& o) const {
        return BoundingBox(
            std::min(minX, o.minX), std::min(minY, o.minY), std::min(minZ, o.minZ),
            std::max(maxX, o.maxX), std::max(maxY, o.maxY), std::max(maxZ, o.maxZ));
    }

    BoundingBox min(int x_, int y_, int z_, int dx, int dy, int dz) const {
        return BoundingBox(
            std::max(minX, x_), std::max(minY, y_), std::max(minZ, z_),
            std::min(maxX, dx), std::min(maxY, dy), std::min(maxZ, dz));
    }

    static BoundingBox fromCorners(const BlockPos& first, const BlockPos& second) {
        return BoundingBox(
            std::min(first.x, second.x), std::min(first.y, second.y), std::min(first.z, second.z),
            std::max(first.x, second.x), std::max(first.y, second.y), std::max(first.z, second.z));
    }

    static BoundingBox infinite() {
        return BoundingBox(INT32_MIN, INT32_MIN, INT32_MIN,
                           INT32_MAX, INT32_MAX, INT32_MAX);
    }

    std::string toString() const;
};

// ---------------- Direction ----------------

/** 复刻 {@code Direction}（26.2 语义）。id 与 Java 枚举序一致。 */
enum class Direction : uint8_t {
    DOWN = 0, UP = 1, NORTH = 2, SOUTH = 3, WEST = 4, EAST = 5
};

enum class Axis : uint8_t { X, Y, Z };

constexpr uint8_t directionCount() { return 6; }

constexpr int directionStepX(Direction d) {
    switch (d) {
        case Direction::WEST: return -1;
        case Direction::EAST: return 1;
        default: return 0;
    }
}
constexpr int directionStepY(Direction d) {
    switch (d) {
        case Direction::DOWN: return -1;
        case Direction::UP: return 1;
        default: return 0;
    }
}
constexpr int directionStepZ(Direction d) {
    switch (d) {
        case Direction::NORTH: return -1;
        case Direction::SOUTH: return 1;
        default: return 0;
    }
}

constexpr Axis directionAxis(Direction d) {
    switch (d) {
        case Direction::DOWN: case Direction::UP: return Axis::Y;
        case Direction::NORTH: case Direction::SOUTH: return Axis::Z;
        default: return Axis::X;
    }
}

constexpr bool directionIsHorizontal(Direction d) {
    return d != Direction::DOWN && d != Direction::UP;
}

constexpr Direction directionOpposite(Direction d) {
    switch (d) {
        case Direction::DOWN: return Direction::UP;
        case Direction::UP: return Direction::DOWN;
        case Direction::NORTH: return Direction::SOUTH;
        case Direction::SOUTH: return Direction::NORTH;
        case Direction::WEST: return Direction::EAST;
        case Direction::EAST: return Direction::WEST;
    }
    return Direction::DOWN;
}

constexpr Direction directionClockWise(Direction d) {
    switch (d) {
        case Direction::NORTH: return Direction::EAST;
        case Direction::EAST: return Direction::SOUTH;
        case Direction::SOUTH: return Direction::WEST;
        case Direction::WEST: return Direction::NORTH;
        default: return d;  // Y 轴无顺时针
    }
}

constexpr Direction directionCounterClockWise(Direction d) {
    switch (d) {
        case Direction::NORTH: return Direction::WEST;
        case Direction::WEST: return Direction::SOUTH;
        case Direction::SOUTH: return Direction::EAST;
        case Direction::EAST: return Direction::NORTH;
        default: return d;  // Y 轴无逆时针
    }
}

constexpr Direction directionById(int id) {
    return static_cast<Direction>(id);
}

const char* directionSerializedName(Direction d);

// ---------------- Rotation ----------------

/** 复刻 {@code Rotation}（26.2 语义）。index 与 Java 枚举序一致。 */
enum class Rotation : uint8_t {
    NONE = 0, CLOCKWISE_90 = 1, CLOCKWISE_180 = 2, COUNTERCLOCKWISE_90 = 3
};

constexpr int rotationCount() { return 4; }

class RandomSource;

/** 旋转 90/180/270 后的方向（绕 Y 轴；Y 轴方向不变）。 */
inline Direction rotationRotate(Rotation r, Direction d) {
    if (directionAxis(d) == Axis::Y) {
        return d;
    }
    switch (r) {
        case Rotation::CLOCKWISE_90: return directionClockWise(d);
        case Rotation::CLOCKWISE_180: return directionOpposite(d);
        case Rotation::COUNTERCLOCKWISE_90: return directionCounterClockWise(d);
        default: return d;
    }
}

/** 旋转叠加。 */
inline Rotation rotationGetRotated(Rotation self, Rotation r) {
    if (r == Rotation::NONE) return self;
    int idx = (static_cast<int>(self) + static_cast<int>(r)) % 4;
    return static_cast<Rotation>(idx);
}

/** 取数组/列表元素（0..length），索引按旋转平移。 */
inline int rotationRotateValue(Rotation r, int value, int length) {
    switch (r) {
        case Rotation::CLOCKWISE_90: return (value + length / 4) % length;
        case Rotation::CLOCKWISE_180: return (value + length / 2) % length;
        case Rotation::COUNTERCLOCKWISE_90: return (value + length * 3 / 4) % length;
        default: return value;
    }
}

Rotation rotationByIndex(int index);
Rotation rotationGetRandom(RandomSource& random);
std::vector<Rotation> rotationGetShuffled(RandomSource& random);

// ---------------- FrontAndTop ----------------

/**
 * 复刻 {@code FrontAndTop} —— jigsaw 方块 "orientation" 属性。
 * 序列化名 "{front}_{top}"。
 */
struct FrontAndTop {
    Direction front;
    Direction top;

    static FrontAndTop parse(const std::string& serializedName);
    static FrontAndTop fromFrontAndTop(Direction front, Direction top);

    FrontAndTop rotate(Rotation r) const {
        switch (r) {
            case Rotation::CLOCKWISE_90:
                return FrontAndTop{rotYneg(front), rotYneg(top)};
            case Rotation::CLOCKWISE_180:
                return FrontAndTop{rot180Xz(front), rot180Xz(top)};
            case Rotation::COUNTERCLOCKWISE_90:
                return FrontAndTop{rotYpos(front), rotYpos(top)};
            default:
                return *this;
        }
    }

    std::string getSerializedName() const {
        return std::string(directionSerializedName(front)) + "_"
             + std::string(directionSerializedName(top));
    }

    bool operator==(const FrontAndTop& o) const { return front == o.front && top == o.top; }
    bool operator!=(const FrontAndTop& o) const { return !(*this == o); }

private:
    static Direction rotYneg(Direction d) {
        return directionAxis(d) == Axis::Y ? d : directionClockWise(d);
    }
    static Direction rotYpos(Direction d) {
        return directionAxis(d) == Axis::Y ? d : directionCounterClockWise(d);
    }
    static Direction rot180Xz(Direction d) {
        return directionAxis(d) == Axis::Y ? d : directionOpposite(d);
    }
};

// ---------------- Mth ----------------

/** 复刻 {@code Mth} 子集（26.2 语义）。 */
namespace Mth {

/** 原版 {@code Mth.getSeed(int,int,int)}。int 乘法按 32 位回绕。 */
inline int64_t getSeed(int x, int y, int z) {
    int64_t l = (int64_t)(int32_t)((uint32_t)x * 3129871u)
              ^ (int64_t)z * 116129781LL
              ^ (int64_t)y;
    l = l * l * 42317861LL + l * 11LL;
    return l >> 16;
}

inline int64_t getSeed(const BlockPos& pos) {
    return getSeed(pos.x, pos.y, pos.z);
}

inline int floorDiv(int a, int b) {
    int q = a / b;
    if ((a % b != 0) && ((a ^ b) < 0)) {
        q--;
    }
    return q;
}

inline int64_t floorDiv(int64_t a, int64_t b) {
    int64_t q = a / b;
    if ((a % b != 0) && ((a ^ b) < 0)) {
        q--;
    }
    return q;
}

inline int ceilDiv(int a, int b) {
    return -floorDiv(-a, b);
}

inline int clamp(int value, int min, int max) {
    return value < min ? min : value > max ? max : value;
}

} // namespace Mth

} // namespace trialfinder
