#pragma once
// ============================================================
// TrialSpawnerFinder C++ 结构模板
//
// 逐位复刻 cn.trialfinder.sim.template 的 StructureTemplate /
// BlockState / JigsawBlockInfo / JigsawBlock / StructurePlaceSettings
// （26.2 语义）。预计算 4 种旋转的 jigsaw / bbox / spawner 缓存。
// ============================================================

#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Types.hpp"
#include "nbt/NbtParser.hpp"

namespace trialfinder {

// ---------------- JointType ----------------

/** 复刻 {@code JointType}（26.2 语义）。 */
enum class JointType : uint8_t { ROLLABLE = 0, ALIGNED = 1 };

inline JointType jointTypeByName(const std::string& name) {
    if (name == "rollable") return JointType::ROLLABLE;
    if (name == "aligned") return JointType::ALIGNED;
    throw std::invalid_argument("未知 joint 类型: " + name);
}

// ---------------- BlockState ----------------

/** 复刻 {@code BlockState} 子集：方块 id + jigsaw orientation。 */
struct BlockState {
    std::string name;
    std::string orientation;  // 空 = 无 orientation
    FrontAndTop frontAndTop;
    bool hasOrientation = false;

    BlockState() = default;
    BlockState(const std::string& name_, const std::string& orientation_)
        : name(name_), orientation(orientation_) {
        if (!orientation_.empty()) {
            frontAndTop = FrontAndTop::parse(orientation_);
            hasOrientation = true;
        }
    }

    static BlockState of(const std::string& name_) {
        return BlockState(name_, "");
    }

    bool isJigsaw() const { return name == "minecraft:jigsaw"; }
    bool isTrialSpawner() const { return name == "minecraft:trial_spawner"; }

    /** 应用旋转：jigsaw orientation 随之旋转。 */
    BlockState rotate(Rotation r) const {
        if (orientation.empty() || r == Rotation::NONE) {
            return *this;
        }
        return BlockState(name, frontAndTop.rotate(r).getSerializedName());
    }

    bool operator==(const BlockState& o) const {
        return name == o.name && orientation == o.orientation;
    }
};

// ---------------- StructureBlockInfo ----------------

/**
 * 复刻 {@code StructureBlockInfo} 子集。保存完整 NBT 以便 jigsaw 提取，
 * 同时缓存 spawner 的 normal/ominous config（仅 trial_spawner 有值）。
 */
struct StructureBlockInfo {
    BlockPos pos;
    BlockState state;
    std::shared_ptr<const NbtTag> nbt;
    std::string normalConfig;
    std::string ominousConfig;
    bool hasNbt = false;

    StructureBlockInfo() = default;
    StructureBlockInfo(BlockPos pos_, BlockState state_,
                       std::shared_ptr<const NbtTag> nbt_ = nullptr)
        : pos(pos_), state(std::move(state_)), nbt(std::move(nbt_)) {
        if (nbt != nullptr && nbt->type == NbtType::COMPOUND) {
            hasNbt = true;
            normalConfig = nbtGetString(*nbt, "normal_config");
            ominousConfig = nbtGetString(*nbt, "ominous_config");
        }
    }
};

// ---------------- JigsawBlockInfo ----------------

/**
 * 复刻 {@code JigsawBlockInfo}（26.2 语义）。
 * name/pool/target 用完整 "namespace:path" 字符串表示 Identifier / ResourceKey。
 */
struct JigsawBlockInfo {
    StructureBlockInfo info;
    JointType jointType = JointType::ROLLABLE;
    std::string name;
    std::string pool;
    std::string target;
    int placementPriority = 0;
    int selectionPriority = 0;

    static JigsawBlockInfo of(const StructureBlockInfo& info);

    JigsawBlockInfo withInfo(const StructureBlockInfo& newInfo) const {
        JigsawBlockInfo copy = *this;
        copy.info = newInfo;
        return copy;
    }
};

// ---------------- JigsawBlock ----------------

/** 复刻 {@code JigsawBlock} 静态助手（26.2 语义）。 */
namespace JigsawBlock {

inline bool canAttach(const JigsawBlockInfo& jigsaw, const JigsawBlockInfo& candidate) {
    const FrontAndTop& a = jigsaw.info.state.frontAndTop;
    const FrontAndTop& b = candidate.info.state.frontAndTop;
    Direction front = a.front;
    Direction candidateFront = b.front;
    Direction top = a.top;
    Direction candidateTop = b.top;
    bool rollable = jigsaw.jointType == JointType::ROLLABLE;
    return front == directionOpposite(candidateFront)
        && (rollable || top == candidateTop)
        && jigsaw.target == candidate.name;
}

inline Direction getFrontFacing(const BlockState& state) {
    return state.frontAndTop.front;
}

inline Direction getTopFacing(const BlockState& state) {
    return state.frontAndTop.top;
}

} // namespace JigsawBlock

// ---------------- 相对刷怪笼 ----------------

/** 模板内相对坐标的 trial_spawner（对应 Java {@code RelativeSpawner}）。 */
struct RelativeSpawner {
    BlockPos pos;
    std::string normalConfig;
    std::string ominousConfig;
};

// ---------------- StructurePlaceSettings ----------------

/** 复刻 {@code StructurePlaceSettings} 子集。 */
class StructurePlaceSettings {
public:
    StructurePlaceSettings& setRotation(Rotation r) {
        rotation_ = r;
        return *this;
    }
    Rotation getRotation() const { return rotation_; }

    StructurePlaceSettings& setRotationPivot(BlockPos pivot) {
        rotationPivot_ = pivot;
        return *this;
    }
    BlockPos getRotationPivot() const { return rotationPivot_; }

private:
    Rotation rotation_ = Rotation::NONE;
    BlockPos rotationPivot_ = BlockPos::ZERO;
};

// ---------------- StructureTemplate ----------------

/**
 * 复刻 {@code StructureTemplate} 子集——枚举 jigsaw 连接块、
 * trial_spawner 与 vault 方块所需的模板数据。
 */
class StructureTemplate {
public:
    /** vanilla Mirror，jigsaw piece 恒为 NONE。 */
    enum class Mirror : uint8_t { NONE, LEFT_RIGHT, FRONT_BACK };

    BlockPos getSize() const { return size_; }

    /** 返回按 rotation 旋转、原点在 ZERO 的 jigsaw 列表（平移留给调用方）。 */
    const std::vector<JigsawBlockInfo>& getRotatedJigsaws(Rotation rotation) const {
        ensureRotated(rotation);
        return rotatedJigsaws_[(int)rotation];
    }

    /** Java {@code getJigsaws(pos, rotation)}：旋转并平移到 pos。 */
    std::vector<JigsawBlockInfo> getJigsaws(const BlockPos& pos, Rotation rotation) const;

    /** Java {@code filterBlocks(pos, settings, blockName)}。 */
    std::vector<StructureBlockInfo> filterBlocks(
        const BlockPos& pos, const StructurePlaceSettings& settings,
        const std::string& blockName) const;

    /** 返回按 rotation 旋转、原点在 ZERO 的 trial_spawner 方块。 */
    const std::vector<RelativeSpawner>& getSpawnerBlocks(Rotation rotation) const {
        ensureRotated(rotation);
        return rotatedSpawners_[(int)rotation];
    }

    /** 第一 palette 中指定状态名的方块数（O(1)）。 */
    int countBlocks(const std::string& blockName) const;

    BlockPos getSize(Rotation rotation) const {
        if (rotation == Rotation::CLOCKWISE_90 || rotation == Rotation::COUNTERCLOCKWISE_90) {
            return BlockPos(size_.z, size_.y, size_.x);
        }
        return size_;
    }

    /** 复刻 {@code StructureTemplate.transform}（含 Mirror）。 */
    static BlockPos transform(const BlockPos& pos, Mirror mirror,
                              Rotation rotation, const BlockPos& pivot);

    static BlockPos calculateRelativePosition(
        const StructurePlaceSettings& settings, const BlockPos& pos) {
        return transform(pos, Mirror::NONE, settings.getRotation(), settings.getRotationPivot());
    }

    BoundingBox getBoundingBox(const StructurePlaceSettings& settings, const BlockPos& pos) const {
        return getBoundingBox(pos, settings.getRotation(), settings.getRotationPivot(), Mirror::NONE);
    }

    BoundingBox getBoundingBox(const BlockPos& pos, Rotation rotation,
                               const BlockPos& pivot, Mirror mirror) const;

    /** 从 NBT compound 加载（对应 Java {@code StructureTemplate.load}）。 */
    void load(const NbtTag& tag);

    /** 从解压后的 NBT 字节加载。 */
    void loadBytes(const uint8_t* data, std::size_t len) {
        NbtTag tag = NbtIo::read(data, len);
        load(tag);
    }

    bool isLoaded() const { return !palettes_.empty(); }

private:
    struct BlockRecord {
        BlockPos pos;
        BlockState state;
        std::shared_ptr<const NbtTag> nbt;
    };

    struct Palette {
        std::vector<BlockRecord> blocks;   // 排序后全量（jigsaw+other+withNbt）
        std::vector<JigsawBlockInfo> jigsaws;
        std::unordered_map<std::string, std::vector<std::size_t>> byName;
    };

    void loadPalette(const std::vector<NbtTag>& paletteList, const std::vector<NbtTag>& blocksList);
    const Palette* palette0() const { return palettes_.empty() ? nullptr : &palettes_[0]; }

    void ensureRotated(Rotation rotation) const;
    void precomputeRotation(Rotation rotation) const;

    BlockPos size_ = BlockPos::ZERO;
    std::vector<Palette> palettes_;

    mutable std::array<std::vector<JigsawBlockInfo>, 4> rotatedJigsaws_;
    mutable std::array<BoundingBox, 4> rotatedBounds_;
    mutable std::array<std::vector<RelativeSpawner>, 4> rotatedSpawners_;
    mutable std::array<bool, 4> rotationComputed_ = {false, false, false, false};
};

// ---------------- 内联实现 ----------------

// 复刻 Java Identifier.withDefaultNamespace：无 ':' 时补 "minecraft:" 前缀。
inline std::string identifierWithDefaultNamespace(std::string value) {
    return value.find(':') == std::string::npos ? "minecraft:" + value : value;
}

inline JigsawBlockInfo JigsawBlockInfo::of(const StructureBlockInfo& info) {
    if (info.nbt == nullptr || info.nbt->type != NbtType::COMPOUND) {
        throw std::runtime_error("jigsaw 方块缺少 NBT");
    }
    const NbtTag& nbt = *info.nbt;
    JointType jointType;
    if (nbtContains(nbt, "joint")) {
        jointType = jointTypeByName(nbtGetString(nbt, "joint"));
    } else {
        // getDefaultJointType：front 水平 => ALIGNED
        jointType = info.state.hasOrientation
                && directionIsHorizontal(info.state.frontAndTop.front)
            ? JointType::ALIGNED : JointType::ROLLABLE;
    }
    std::string name = nbtGetString(nbt, "name");
    std::string pool = nbtGetString(nbt, "pool");
    std::string target = nbtGetString(nbt, "target");
    JigsawBlockInfo result;
    result.info = info;
    result.jointType = jointType;
    result.name = name.empty() ? "minecraft:empty"
                               : identifierWithDefaultNamespace(name);
    result.pool = pool.empty() ? "minecraft:empty"
                               : identifierWithDefaultNamespace(pool);
    result.target = target.empty() ? "minecraft:empty"
                                   : identifierWithDefaultNamespace(target);
    result.placementPriority = nbtGetInt(nbt, "placement_priority");
    result.selectionPriority = nbtGetInt(nbt, "selection_priority");
    return result;
}

} // namespace trialfinder
