#include "template/StructureTemplate.hpp"

#include <algorithm>
#include <stdexcept>

namespace trialfinder {

std::vector<JigsawBlockInfo> StructureTemplate::getJigsaws(
    const BlockPos& pos, Rotation rotation) const {
    if (palettes_.empty()) {
        return {};
    }
    const std::vector<JigsawBlockInfo>& base = getRotatedJigsaws(rotation);
    if (pos == BlockPos::ZERO) {
        return base;
    }
    std::vector<JigsawBlockInfo> result;
    result.reserve(base.size());
    for (const JigsawBlockInfo& jigsaw : base) {
        const StructureBlockInfo& info = jigsaw.info;
        BlockPos offsetPos = info.pos.offset(pos);
        StructureBlockInfo newInfo(offsetPos, info.state, info.nbt);
        result.push_back(jigsaw.withInfo(newInfo));
    }
    return result;
}

std::vector<StructureBlockInfo> StructureTemplate::filterBlocks(
    const BlockPos& pos, const StructurePlaceSettings& settings,
    const std::string& blockName) const {
    if (palettes_.empty()) {
        return {};
    }
    const Palette* palette = palette0();
    auto it = palette->byName.find(blockName);
    if (it == palette->byName.end()) {
        return {};
    }
    std::vector<StructureBlockInfo> result;
    result.reserve(it->second.size());
    for (std::size_t index : it->second) {
        const BlockRecord& record = palette->blocks[index];
        BlockPos transformed = calculateRelativePosition(settings, record.pos).offset(pos);
        result.emplace_back(transformed, record.state.rotate(settings.getRotation()), record.nbt);
    }
    return result;
}

int StructureTemplate::countBlocks(const std::string& blockName) const {
    if (palettes_.empty()) {
        return 0;
    }
    const Palette* palette = palette0();
    auto it = palette->byName.find(blockName);
    return it == palette->byName.end() ? 0 : static_cast<int>(it->second.size());
}

BlockPos StructureTemplate::transform(
    const BlockPos& pos, Mirror mirror, Rotation rotation, const BlockPos& pivot) {
    int x = pos.x;
    int y = pos.y;
    int z = pos.z;
    bool mirrored = true;
    switch (mirror) {
        case Mirror::FRONT_BACK: z = -z; break;
        case Mirror::LEFT_RIGHT: x = -x; break;
        default: mirrored = false; break;
    }
    int pivotX = pivot.x;
    int pivotZ = pivot.z;
    switch (rotation) {
        case Rotation::CLOCKWISE_90:
            return BlockPos(pivotX + pivotZ - z, y, pivotZ - pivotX + x);
        case Rotation::CLOCKWISE_180:
            return BlockPos(pivotX + pivotX - x, y, pivotZ + pivotZ - z);
        case Rotation::COUNTERCLOCKWISE_90:
            return BlockPos(pivotX - pivotZ + z, y, pivotX + pivotZ - x);
        default:
            return mirrored ? BlockPos(x, y, z) : pos;
    }
}

BoundingBox StructureTemplate::getBoundingBox(
    const BlockPos& pos, Rotation rotation, const BlockPos& pivot, Mirror mirror) const {
    if (pivot == BlockPos::ZERO && mirror == Mirror::NONE) {
        ensureRotated(rotation);
        return rotatedBounds_[(int)rotation].moved(pos.x, pos.y, pos.z);
    }
    // 非默认 pivot/mirror：直接用原始 size 计算
    BlockPos size2 = BlockPos(size_.x - 1, size_.y - 1, size_.z - 1);
    BlockPos p1 = transform(BlockPos::ZERO, mirror, rotation, pivot);
    BlockPos p2 = transform(BlockPos::ZERO.offset(size2), mirror, rotation, pivot);
    return BoundingBox::fromCorners(p1, p2).moved(pos.x, pos.y, pos.z);
}

void StructureTemplate::load(const NbtTag& tag) {
    palettes_.clear();
    rotationComputed_ = {false, false, false, false};
    if (tag.type != NbtType::COMPOUND) {
        throw std::runtime_error("结构模板根标签必须是 COMPOUND");
    }
    const std::vector<NbtTag>& sizeList = nbtGetList(tag, "size");
    if (sizeList.size() < 3) {
        throw std::runtime_error("结构模板缺少 size");
    }
    // size 是 int list，元素本身是 INT tag
    size_ = BlockPos(sizeList[0].i, sizeList[1].i, sizeList[2].i);

    const std::vector<NbtTag>& blocksList = nbtGetList(tag, "blocks");
    const std::vector<NbtTag>& paletteList = nbtGetList(tag, "palette");
    loadPalette(paletteList, blocksList);
}

void StructureTemplate::loadPalette(
    const std::vector<NbtTag>& paletteList, const std::vector<NbtTag>& blocksList) {
    // 1. 解析 palette -> states
    std::vector<BlockState> states;
    states.reserve(paletteList.size());
    for (const NbtTag& stateTag : paletteList) {
        std::string name = nbtGetString(stateTag, "Name");
        std::string orientation;
        const NbtTag* properties = nbtGetCompound(stateTag, "Properties");
        if (properties != nullptr && nbtContains(*properties, "orientation")) {
            orientation = nbtGetString(*properties, "orientation");
        }
        states.emplace_back(name, orientation);
    }
    if (states.empty()) {
        throw std::runtime_error("结构模板 palette 为空");
    }

    // 2. 解析 blocks，分组
    struct Grouped {
        std::vector<StructureBlockInfo> full;    // jigsaw 且无 nbt
        std::vector<StructureBlockInfo> withNbt; // 有 nbt
        std::vector<StructureBlockInfo> other;   // 非 jigsaw 且无 nbt
    } grouped;

    for (const NbtTag& blockTag : blocksList) {
        const std::vector<NbtTag>& posList = nbtGetList(blockTag, "pos");
        if (posList.size() < 3) {
            continue;
        }
        BlockPos pos(posList[0].i, posList[1].i, posList[2].i);
        int stateIndex = nbtGetInt(blockTag, "state");
        int clamped = std::min(std::max(stateIndex, 0),
                               static_cast<int>(states.size()) - 1);
        std::shared_ptr<const NbtTag> nbt;
        const NbtTag* nbtTag = nbtGetCompound(blockTag, "nbt");
        if (nbtTag != nullptr) {
            nbt = std::make_shared<NbtTag>(*nbtTag);
        }
        StructureBlockInfo info(pos, states[clamped], nbt);
        if (info.hasNbt) {
            grouped.withNbt.push_back(std::move(info));
        } else if (info.state.isJigsaw()) {
            grouped.full.push_back(std::move(info));
        } else {
            grouped.other.push_back(std::move(info));
        }
    }

    // 3. 每组按 (Y, X, Z) 排序
    auto byYxz = [](const StructureBlockInfo& a, const StructureBlockInfo& b) {
        if (a.pos.y != b.pos.y) return a.pos.y < b.pos.y;
        if (a.pos.x != b.pos.x) return a.pos.x < b.pos.x;
        return a.pos.z < b.pos.z;
    };
    std::sort(grouped.full.begin(), grouped.full.end(), byYxz);
    std::sort(grouped.withNbt.begin(), grouped.withNbt.end(), byYxz);
    std::sort(grouped.other.begin(), grouped.other.end(), byYxz);

    // 4. 合并 full + other + withNbt
    Palette palette;
    palette.blocks.reserve(grouped.full.size() + grouped.withNbt.size() + grouped.other.size());
    auto appendToPalette = [&palette](const StructureBlockInfo& info) {
        BlockRecord record;
        record.pos = info.pos;
        record.state = info.state;
        record.nbt = info.nbt;
        palette.blocks.push_back(std::move(record));
    };
    for (const auto& info : grouped.full) appendToPalette(info);
    for (const auto& info : grouped.other) appendToPalette(info);
    for (const auto& info : grouped.withNbt) appendToPalette(info);

    // 5. 构建 jigsaws + byName 索引
    palette.jigsaws.reserve(palette.blocks.size());
    for (std::size_t i = 0; i < palette.blocks.size(); i++) {
        const BlockRecord& record = palette.blocks[i];
        palette.byName[record.state.name].push_back(i);
        if (record.state.isJigsaw()) {
            StructureBlockInfo info(record.pos, record.state, record.nbt);
            palette.jigsaws.push_back(JigsawBlockInfo::of(info));
        }
    }

    palettes_.push_back(std::move(palette));
    rotationComputed_ = {false, false, false, false};
}

void StructureTemplate::ensureRotated(Rotation rotation) const {
    int index = (int)rotation;
    if (!rotationComputed_[index]) {
        precomputeRotation(rotation);
        rotationComputed_[index] = true;
    }
}

void StructureTemplate::precomputeRotation(Rotation rotation) const {
    int index = (int)rotation;
    const Palette* palette = palette0();
    StructurePlaceSettings settings;
    settings.setRotation(rotation);

    // jigsaws
    std::vector<JigsawBlockInfo>& jigsaws = rotatedJigsaws_[index];
    jigsaws.clear();
    if (palette != nullptr) {
        jigsaws.reserve(palette->jigsaws.size());
        for (const JigsawBlockInfo& jigsaw : palette->jigsaws) {
            const StructureBlockInfo& info = jigsaw.info;
            BlockPos transformed = calculateRelativePosition(settings, info.pos);
            StructureBlockInfo newInfo(transformed, info.state.rotate(rotation), info.nbt);
            jigsaws.push_back(jigsaw.withInfo(newInfo));
        }
    }

    // bounding box（用原始 size）
    BlockPos size2 = BlockPos(size_.x - 1, size_.y - 1, size_.z - 1);
    BlockPos p1 = transform(BlockPos::ZERO, Mirror::NONE, rotation, BlockPos::ZERO);
    BlockPos p2 = transform(BlockPos::ZERO.offset(size2), Mirror::NONE, rotation, BlockPos::ZERO);
    rotatedBounds_[index] = BoundingBox::fromCorners(p1, p2);

    // spawners
    std::vector<RelativeSpawner>& spawners = rotatedSpawners_[index];
    spawners.clear();
    if (palette != nullptr) {
        auto it = palette->byName.find("minecraft:trial_spawner");
        if (it != palette->byName.end()) {
            spawners.reserve(it->second.size());
            for (std::size_t blockIndex : it->second) {
                const BlockRecord& record = palette->blocks[blockIndex];
                BlockPos transformed = calculateRelativePosition(settings, record.pos);
                RelativeSpawner spawner;
                spawner.pos = transformed;
                if (record.nbt != nullptr) {
                    spawner.normalConfig = nbtGetString(*record.nbt, "normal_config");
                    spawner.ominousConfig = nbtGetString(*record.nbt, "ominous_config");
                }
                spawners.push_back(std::move(spawner));
            }
        }
    }
}

} // namespace trialfinder
