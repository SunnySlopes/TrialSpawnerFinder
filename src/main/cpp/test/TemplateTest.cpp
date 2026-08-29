// ============================================================
// TrialSpawnerFinder 结构模板 / NBT 解析测试
//
// 用手工构造的 big-endian NBT 字节验证 StructureTemplate 的
// palette/blocks 解析、jigsaw 提取、旋转缓存与包围盒计算。
// ============================================================

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "core/Types.hpp"
#include "nbt/NbtParser.hpp"
#include "template/StructureTemplate.hpp"

namespace trialfinder {

namespace {

// 简易 big-endian NBT 写器
class NbtWriter {
public:
    std::vector<uint8_t> data;

    void u8(uint8_t v) { data.push_back(v); }
    void u16(uint16_t v) { data.push_back(static_cast<uint8_t>(v >> 8)); data.push_back(static_cast<uint8_t>(v)); }
    void u32(uint32_t v) {
        data.push_back(static_cast<uint8_t>(v >> 24));
        data.push_back(static_cast<uint8_t>(v >> 16));
        data.push_back(static_cast<uint8_t>(v >> 8));
        data.push_back(static_cast<uint8_t>(v));
    }
    void str(const std::string& s) {
        u16(static_cast<uint16_t>(s.size()));
        data.insert(data.end(), s.begin(), s.end());
    }
    void tagHead(uint8_t type, const std::string& name) { u8(type); str(name); }
    void endCompound() { u8(0); }
    void intList(const std::string& name, std::initializer_list<int> values) {
        tagHead(9, name);          // TAG_List
        u8(3);                     // 元素类型 TAG_Int
        u32(static_cast<uint32_t>(values.size()));
        for (int v : values) u32(static_cast<uint32_t>(v));
    }
};

void jigsawBlock(NbtWriter& w, int x, int y, int z, const std::string& name,
                 bool withJoint) {
    w.tagHead(9, "pos");
    w.u8(3); w.u32(3);
    w.u32(static_cast<uint32_t>(x)); w.u32(static_cast<uint32_t>(y)); w.u32(static_cast<uint32_t>(z));
    w.tagHead(3, "state"); w.u32(0);
    w.tagHead(10, "nbt");
    w.tagHead(8, "name"); w.str(name);
    w.tagHead(8, "target"); w.str("minecraft:target_" + name);
    w.tagHead(8, "pool"); w.str("minecraft:pool_" + name);
    if (withJoint) {
        w.tagHead(8, "joint"); w.str("rollable");
    }
    w.tagHead(3, "placement_priority"); w.u32(2);
    w.tagHead(3, "selection_priority"); w.u32(1);
    w.endCompound();  // nbt
    w.endCompound();  // block
}

std::vector<uint8_t> makeTemplateNbt() {
    NbtWriter w;
    w.u8(10); w.str("");  // 根 compound，空名
    w.intList("size", {5, 3, 4});
    // palette：state 0 = jigsaw(north_up)，state 1 = trial_spawner
    w.tagHead(9, "palette"); w.u8(10); w.u32(2);
    w.tagHead(8, "Name"); w.str("minecraft:jigsaw");
    w.tagHead(10, "Properties");
    w.tagHead(8, "orientation"); w.str("north_up");
    w.endCompound();
    w.endCompound();
    w.tagHead(8, "Name"); w.str("minecraft:trial_spawner");
    w.endCompound();
    // blocks
    w.tagHead(9, "blocks"); w.u8(10); w.u32(3);
    jigsawBlock(w, 0, 0, 0, "foo", true);
    jigsawBlock(w, 2, 1, 1, "qux", false);
    // trial_spawner 方块（带 normal/ominous config）
    w.tagHead(9, "pos"); w.u8(3); w.u32(3);
    w.u32(4); w.u32(2); w.u32(3);
    w.tagHead(3, "state"); w.u32(1);
    w.tagHead(10, "nbt");
    w.tagHead(8, "normal_config"); w.str("minecraft:trial_chamber/breeze/normal");
    w.tagHead(8, "ominous_config"); w.str("minecraft:trial_chamber/breeze/ominous");
    w.endCompound();
    w.endCompound();
    w.endCompound();  // 根
    return w.data;
}

std::vector<uint8_t> makeSimpleCompoundNbt() {
    NbtWriter w;
    w.u8(10); w.str("");
    w.tagHead(8, "name"); w.str("hello");
    w.tagHead(3, "value"); w.u32(5);
    w.tagHead(4, "big"); w.u32(0); w.u32(1);  // TAG_Long 负载 8 字节：高 4 字节 0，低 4 字节 1
    w.endCompound();
    return w.data;
}

// BoundingBox 无 operator==，逐字段比较
void expectBoxEq(const BoundingBox& b, int minX, int minY, int minZ,
                 int maxX, int maxY, int maxZ) {
    EXPECT_EQ(b.minX, minX);
    EXPECT_EQ(b.minY, minY);
    EXPECT_EQ(b.minZ, minZ);
    EXPECT_EQ(b.maxX, maxX);
    EXPECT_EQ(b.maxY, maxY);
    EXPECT_EQ(b.maxZ, maxZ);
}

} // namespace

// ---------------- NbtIo ----------------

TEST(TemplateTest, NbtIoParsesSimpleCompound) {
    std::vector<uint8_t> bytes = makeSimpleCompoundNbt();
    NbtTag tag = NbtIo::read(bytes.data(), bytes.size());
    ASSERT_EQ(tag.type, NbtType::COMPOUND);
    EXPECT_EQ(nbtGetString(tag, "name"), "hello");
    EXPECT_EQ(nbtGetInt(tag, "value"), 5);
    EXPECT_EQ(nbtGetLong(tag, "big"), 1LL);
    EXPECT_EQ(nbtGetString(tag, "missing"), "");
    EXPECT_EQ(nbtGetInt(tag, "missing"), 0);
}

TEST(TemplateTest, NbtIoReadRejectsTruncated) {
    std::vector<uint8_t> bytes = makeSimpleCompoundNbt();
    // 截断一半应抛异常而非越界读
    std::vector<uint8_t> truncated(bytes.begin(), bytes.begin() + bytes.size() / 2);
    EXPECT_THROW(NbtIo::read(truncated.data(), truncated.size()), std::exception);
}

// ---------------- StructureTemplate.load ----------------

TEST(TemplateTest, LoadParsesSizeAndPalette) {
    std::vector<uint8_t> bytes = makeTemplateNbt();
    StructureTemplate tpl;
    tpl.loadBytes(bytes.data(), bytes.size());
    EXPECT_TRUE(tpl.isLoaded());
    EXPECT_EQ(tpl.getSize(), BlockPos(5, 3, 4));
    EXPECT_EQ(tpl.countBlocks("minecraft:jigsaw"), 2);
    EXPECT_EQ(tpl.countBlocks("minecraft:trial_spawner"), 1);
    EXPECT_EQ(tpl.countBlocks("minecraft:air"), 0);
}

TEST(TemplateTest, RotatedSizeSwapsXZForQuarterTurns) {
    std::vector<uint8_t> bytes = makeTemplateNbt();
    StructureTemplate tpl;
    tpl.loadBytes(bytes.data(), bytes.size());
    EXPECT_EQ(tpl.getSize(Rotation::NONE), BlockPos(5, 3, 4));
    EXPECT_EQ(tpl.getSize(Rotation::CLOCKWISE_90), BlockPos(4, 3, 5));
    EXPECT_EQ(tpl.getSize(Rotation::CLOCKWISE_180), BlockPos(5, 3, 4));
    EXPECT_EQ(tpl.getSize(Rotation::COUNTERCLOCKWISE_90), BlockPos(4, 3, 5));
}

TEST(TemplateTest, JigsawExtractionMatchesNbt) {
    std::vector<uint8_t> bytes = makeTemplateNbt();
    StructureTemplate tpl;
    tpl.loadBytes(bytes.data(), bytes.size());
    const std::vector<JigsawBlockInfo>& jigsaws = tpl.getRotatedJigsaws(Rotation::NONE);
    ASSERT_EQ(jigsaws.size(), 2u);

    const JigsawBlockInfo& first = jigsaws[0];  // 按 (Y,X,Z) 排序：foo 在 (0,0,0) 先
    EXPECT_EQ(first.name, "minecraft:foo");
    EXPECT_EQ(first.target, "minecraft:target_foo");
    EXPECT_EQ(first.pool, "minecraft:pool_foo");
    EXPECT_EQ(first.jointType, JointType::ROLLABLE);
    EXPECT_EQ(first.selectionPriority, 1);
    EXPECT_EQ(first.placementPriority, 2);
    EXPECT_EQ(first.info.pos, BlockPos(0, 0, 0));
    EXPECT_EQ(first.info.state.name, "minecraft:jigsaw");
    EXPECT_EQ(first.info.state.frontAndTop.front, Direction::NORTH);
    EXPECT_EQ(first.info.state.frontAndTop.top, Direction::UP);

    const JigsawBlockInfo& second = jigsaws[1];  // qux 在 (2,1,1)
    EXPECT_EQ(second.name, "minecraft:qux");
    // front 水平且无 joint → 默认 ALIGNED
    EXPECT_EQ(second.jointType, JointType::ALIGNED);
    EXPECT_EQ(second.info.pos, BlockPos(2, 1, 1));
}

TEST(TemplateTest, JigsawTranslationOffsetsByPos) {
    std::vector<uint8_t> bytes = makeTemplateNbt();
    StructureTemplate tpl;
    tpl.loadBytes(bytes.data(), bytes.size());
    std::vector<JigsawBlockInfo> jigsaws = tpl.getJigsaws(BlockPos(10, 20, 30), Rotation::NONE);
    ASSERT_EQ(jigsaws.size(), 2u);
    EXPECT_EQ(jigsaws[0].info.pos, BlockPos(10, 20, 30));
    EXPECT_EQ(jigsaws[1].info.pos, BlockPos(12, 21, 31));
    // 平移不改变 name/pool/target
    EXPECT_EQ(jigsaws[1].name, "minecraft:qux");
}

TEST(TemplateTest, QuarterTurnRotatesJigsawPositionAndOrientation) {
    std::vector<uint8_t> bytes = makeTemplateNbt();
    StructureTemplate tpl;
    tpl.loadBytes(bytes.data(), bytes.size());
    const std::vector<JigsawBlockInfo>& jigsaws = tpl.getRotatedJigsaws(Rotation::CLOCKWISE_90);
    ASSERT_EQ(jigsaws.size(), 2u);
    // (2,1,1) 绕 ZERO 顺时针 90°：x → -z, z → +x  ⇒ (-1, 1, 2)
    const JigsawBlockInfo& second = jigsaws[1];
    EXPECT_EQ(second.info.pos, BlockPos(-1, 1, 2));
    // orientation north_up → east_up（front 顺时针）
    EXPECT_EQ(second.info.state.orientation, "east_up");
    EXPECT_EQ(second.info.state.frontAndTop.front, Direction::EAST);
    EXPECT_EQ(second.info.state.frontAndTop.top, Direction::UP);
}

TEST(TemplateTest, SpawnerBlocksExtractedWithConfigs) {
    std::vector<uint8_t> bytes = makeTemplateNbt();
    StructureTemplate tpl;
    tpl.loadBytes(bytes.data(), bytes.size());
    const std::vector<RelativeSpawner>& spawners = tpl.getSpawnerBlocks(Rotation::NONE);
    ASSERT_EQ(spawners.size(), 1u);
    EXPECT_EQ(spawners[0].pos, BlockPos(4, 2, 3));
    EXPECT_EQ(spawners[0].normalConfig, "minecraft:trial_chamber/breeze/normal");
    EXPECT_EQ(spawners[0].ominousConfig, "minecraft:trial_chamber/breeze/ominous");
}

TEST(TemplateTest, FilterBlocksFindsTrialSpawner) {
    std::vector<uint8_t> bytes = makeTemplateNbt();
    StructureTemplate tpl;
    tpl.loadBytes(bytes.data(), bytes.size());
    StructurePlaceSettings settings;
    settings.setRotation(Rotation::NONE);
    std::vector<StructureBlockInfo> blocks =
        tpl.filterBlocks(BlockPos(100, 0, 200), settings, "minecraft:trial_spawner");
    ASSERT_EQ(blocks.size(), 1u);
    EXPECT_EQ(blocks[0].pos, BlockPos(104, 2, 203));
    EXPECT_EQ(blocks[0].normalConfig, "minecraft:trial_chamber/breeze/normal");
}

TEST(TemplateTest, BoundingBoxMatchesRotatedSize) {
    std::vector<uint8_t> bytes = makeTemplateNbt();
    StructureTemplate tpl;
    tpl.loadBytes(bytes.data(), bytes.size());

    StructurePlaceSettings none;
    BoundingBox box0 = tpl.getBoundingBox(none, BlockPos::ZERO);
    expectBoxEq(box0, 0, 0, 0, 4, 2, 3);
    EXPECT_EQ(box0.getXSpan(), 5);
    EXPECT_EQ(box0.getYSpan(), 3);
    EXPECT_EQ(box0.getZSpan(), 4);

    StructurePlaceSettings cw90;
    cw90.setRotation(Rotation::CLOCKWISE_90);
    BoundingBox box1 = tpl.getBoundingBox(cw90, BlockPos::ZERO);
    // (0,0,0)→(0,0,0)，(4,2,3)→(-3,2,4) ⇒ min(-3,0,0) max(0,2,4)
    expectBoxEq(box1, -3, 0, 0, 0, 2, 4);
    EXPECT_EQ(box1.getXSpan(), 4);
    EXPECT_EQ(box1.getYSpan(), 3);
    EXPECT_EQ(box1.getZSpan(), 5);
}

} // namespace trialfinder
