// ============================================================
// TrialSpawnerFinder HandTrialChamberPredictor 逐位一致性测试
//
// 参照值全部来自纯 Java sim oracle（build/oracle/oracle.txt）。
// 覆盖 4 seed × 4 chunk 共 16 组的 spawnerCount / exists，
// 并对 4 组完整 spawner 位置列表、1 组完整 spawnerInfo 列表、
// 1 组完整 vault 列表逐位对拍。任何偏差即 bug。
// ============================================================

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "jigsaw/JigsawPlacement.hpp"
#include "predictor/HandTrialChamberPredictor.hpp"
#include "test/ResourceFixture.hpp"

namespace trialfinder {

using PredictorTest = ResourceTestBase;

namespace {

struct Tri {
    int x, y, z;
};

struct InfoTri {
    int x, y, z;
    std::string normal;
    std::string ominous;
};

struct VaultTri {
    int x, y, z;
    bool ominous;
};

std::vector<Tri> triList(std::initializer_list<Tri> list) {
    return list;
}

} // namespace

// ---------------- 16 组 count / exists 全对拍 ----------------

TEST_F(PredictorTest, SpawnerCountAndExistsMatchOracleForAllCombos) {
    struct Combo {
        int64_t seed;
        int chunkX, chunkZ;
        std::size_t count;
    };
    const Combo combos[] = {
        {0LL, 0, 0, 21},    {0LL, 1, -1, 18},   {0LL, 5, 7, 23},   {0LL, 16, -16, 16},
        {1LL, 0, 0, 15},    {1LL, 1, -1, 20},   {1LL, 5, 7, 14},   {1LL, 16, -16, 17},
        {42LL, 0, 0, 29},   {42LL, 1, -1, 17},  {42LL, 5, 7, 14},  {42LL, 16, -16, 20},
        {9206294873968313284LL, 0, 0, 20}, {9206294873968313284LL, 1, -1, 20},
        {9206294873968313284LL, 5, 7, 22}, {9206294873968313284LL, 16, -16, 16},
    };
    for (const Combo& c : combos) {
        HandTrialChamberPredictor predictor(c.seed, config_, *pools_, *templates_);
        JigsawPlacement::Prediction pred =
            predictor.predict(c.chunkX * 16, c.chunkZ * 16);
        EXPECT_TRUE(pred.exists) << "seed=" << c.seed << " chunk=" << c.chunkX << "," << c.chunkZ;
        EXPECT_EQ(pred.spawners.size(), c.count)
            << "seed=" << c.seed << " chunk=" << c.chunkX << "," << c.chunkZ;
        // spawner info 数量恒等于 spawner 数量
        EXPECT_EQ(pred.spawnerInfos.size(), pred.spawners.size())
            << "seed=" << c.seed << " chunk=" << c.chunkX << "," << c.chunkZ;
    }
}

// ---------------- 完整 spawner 位置列表对拍 ----------------

TEST_F(PredictorTest, FullSpawnerListSeed0Chunk00) {
    const std::vector<Tri> expected = triList({
        {-70, -29, -4}, {-54, -31, -4}, {-48, -31, 1}, {-34, -26, -39},
        {-34, -26, -29}, {-34, -19, -34}, {-34, -19, -29}, {-18, -19, 5},
        {-18, -19, 25}, {-14, -19, -22}, {-13, -19, -9}, {-10, -24, -33},
        {-8, -24, 15}, {-3, -26, -9}, {-3, -22, -44}, {8, -19, -44},
        {8, -19, -22}, {22, -21, 2}, {33, -26, -2}, {44, -24, -9},
        {44, -21, -20},
    });
    HandTrialChamberPredictor predictor(0LL, config_, *pools_, *templates_);
    JigsawPlacement::Prediction pred = predictor.predict(0, 0);
    ASSERT_EQ(pred.spawners.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); i++) {
        EXPECT_EQ(pred.spawners[i].x, expected[i].x) << "index=" << i;
        EXPECT_EQ(pred.spawners[i].y, expected[i].y) << "index=" << i;
        EXPECT_EQ(pred.spawners[i].z, expected[i].z) << "index=" << i;
    }
}

TEST_F(PredictorTest, FullSpawnerListSeed42Chunk00) {
    const std::vector<Tri> expected = triList({
        {-26, -18, -57}, {-21, -28, -57}, {-21, -23, -51}, {-16, -30, -33},
        {-16, -18, -57}, {-16, 4, -28}, {-11, -6, -28}, {-11, -1, -34},
        {-6, 4, -28}, {-1, -20, 23}, {-1, -13, -52}, {4, -23, -52},
        {4, -18, -46}, {9, -30, -18}, {9, -30, -3}, {9, -25, 33},
        {9, -23, -18}, {9, -23, -13}, {9, -23, -8}, {9, -23, -3},
        {9, -18, -18}, {9, -18, -3}, {9, -13, -52}, {19, -20, 23},
        {22, -10, -3}, {23, -3, 12}, {23, -1, 28}, {33, -28, -18},
        {43, -23, -28},
    });
    HandTrialChamberPredictor predictor(42LL, config_, *pools_, *templates_);
    JigsawPlacement::Prediction pred = predictor.predict(0, 0);
    ASSERT_EQ(pred.spawners.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); i++) {
        EXPECT_EQ(pred.spawners[i].x, expected[i].x) << "index=" << i;
        EXPECT_EQ(pred.spawners[i].y, expected[i].y) << "index=" << i;
        EXPECT_EQ(pred.spawners[i].z, expected[i].z) << "index=" << i;
    }
}

TEST_F(PredictorTest, FullSpawnerListSeed9206294873968313284Chunk57) {
    const std::vector<Tri> expected = triList({
        {54, -39, 89}, {64, -41, 89}, {64, -39, 99}, {69, -41, 100},
        {74, -39, 89}, {74, -23, 79}, {75, -41, 84}, {78, -13, 66},
        {79, -25, 150}, {82, -18, 55}, {84, -6, 94}, {84, 4, 89},
        {84, 4, 99}, {89, -30, 140}, {89, -23, 94}, {89, -23, 99},
        {89, -23, 104}, {89, -18, 48}, {90, -1, 94}, {99, -25, 150},
        {100, -16, 55}, {100, -13, 44},
    });
    HandTrialChamberPredictor predictor(9206294873968313284LL, config_, *pools_, *templates_);
    JigsawPlacement::Prediction pred = predictor.predict(5 * 16, 7 * 16);
    ASSERT_EQ(pred.spawners.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); i++) {
        EXPECT_EQ(pred.spawners[i].x, expected[i].x) << "index=" << i;
        EXPECT_EQ(pred.spawners[i].y, expected[i].y) << "index=" << i;
        EXPECT_EQ(pred.spawners[i].z, expected[i].z) << "index=" << i;
    }
}

TEST_F(PredictorTest, FullSpawnerListSeed1Chunk16m16) {
    const std::vector<Tri> expected = triList({
        {194, -10, -260}, {199, -10, -265}, {223, -22, -250}, {224, 2, -225},
        {228, 2, -235}, {228, 4, -245}, {238, 4, -235}, {246, -38, -250},
        {248, 0, -251}, {253, -22, -265}, {253, -10, -251}, {253, -5, -245},
        {258, 0, -251}, {262, -40, -250}, {279, -10, -265}, {289, -12, -265},
        {289, -10, -255},
    });
    HandTrialChamberPredictor predictor(1LL, config_, *pools_, *templates_);
    JigsawPlacement::Prediction pred = predictor.predict(16 * 16, -16 * 16);
    ASSERT_EQ(pred.spawners.size(), expected.size());
    for (std::size_t i = 0; i < expected.size(); i++) {
        EXPECT_EQ(pred.spawners[i].x, expected[i].x) << "index=" << i;
        EXPECT_EQ(pred.spawners[i].y, expected[i].y) << "index=" << i;
        EXPECT_EQ(pred.spawners[i].z, expected[i].z) << "index=" << i;
    }
}

// ---------------- 完整 spawnerInfo / vault 列表对拍 ----------------

TEST_F(PredictorTest, FullSpawnerInfoListSeed0Chunk00) {
    const std::string pc = "minecraft:trial_chamber/ranged/poison_skeleton/";
    const std::string bc = "minecraft:trial_chamber/breeze/";
    const std::string ms = "minecraft:trial_chamber/melee/spider/";
    const std::string bz = "minecraft:trial_chamber/small_melee/baby_zombie/";
    const InfoTri expected[] = {
        {-70, -29, -4, pc + "normal", pc + "ominous"},
        {-54, -31, -4, bc + "normal", bc + "ominous"},
        {-48, -31, 1, ms + "normal", ms + "ominous"},
        {-34, -26, -39, bz + "normal", bz + "ominous"},
        {-34, -26, -29, ms + "normal", ms + "ominous"},
        {-34, -19, -34, ms + "normal", ms + "ominous"},
        {-34, -19, -29, ms + "normal", ms + "ominous"},
        {-18, -19, 5, pc + "normal", pc + "ominous"},
        {-18, -19, 25, pc + "normal", pc + "ominous"},
        {-14, -19, -22, pc + "normal", pc + "ominous"},
        {-13, -19, -9, ms + "normal", ms + "ominous"},
        {-10, -24, -33, ms + "normal", ms + "ominous"},
        {-8, -24, 15, pc + "normal", pc + "ominous"},
        {-3, -26, -9, pc + "normal", pc + "ominous"},
        {-3, -22, -44, bc + "normal", bc + "ominous"},
        {8, -19, -44, pc + "normal", pc + "ominous"},
        {8, -19, -22, pc + "normal", pc + "ominous"},
        {22, -21, 2, pc + "normal", pc + "ominous"},
        {33, -26, -2, ms + "normal", ms + "ominous"},
        {44, -24, -9, bc + "normal", bc + "ominous"},
        {44, -21, -20, pc + "normal", pc + "ominous"},
    };
    HandTrialChamberPredictor predictor(0LL, config_, *pools_, *templates_);
    JigsawPlacement::Prediction pred = predictor.predict(0, 0);
    ASSERT_EQ(pred.spawnerInfos.size(), 21u);
    for (int i = 0; i < 21; i++) {
        EXPECT_EQ(pred.spawnerInfos[i].x, expected[i].x) << "index=" << i;
        EXPECT_EQ(pred.spawnerInfos[i].y, expected[i].y) << "index=" << i;
        EXPECT_EQ(pred.spawnerInfos[i].z, expected[i].z) << "index=" << i;
        EXPECT_EQ(pred.spawnerInfos[i].normalConfig, expected[i].normal) << "index=" << i;
        EXPECT_EQ(pred.spawnerInfos[i].ominousConfig, expected[i].ominous) << "index=" << i;
    }
}

TEST_F(PredictorTest, FullVaultListSeed0Chunk00) {
    const VaultTri expected[] = {
        {-75, -28, -9, false}, {-75, -28, 1, false}, {-66, -30, -4, false},
        {-58, -24, -16, true}, {-48, -35, -8, false}, {-42, -20, -4, true},
        {-34, -14, -61, true}, {-29, -8, -65, false}, {-15, -18, -38, false},
        {-7, -28, 15, true}, {-3, -18, -27, false}, {-3, -17, -45, true},
        {11, -15, -9, true}, {21, -18, -14, false}, {45, -20, -4, false},
        {45, -19, -9, true},
    };
    HandTrialChamberPredictor predictor(0LL, config_, *pools_, *templates_);
    JigsawPlacement::Prediction pred = predictor.predict(0, 0);
    ASSERT_EQ(pred.vaults.size(), 16u);
    for (int i = 0; i < 16; i++) {
        EXPECT_EQ(pred.vaults[i].x, expected[i].x) << "index=" << i;
        EXPECT_EQ(pred.vaults[i].y, expected[i].y) << "index=" << i;
        EXPECT_EQ(pred.vaults[i].z, expected[i].z) << "index=" << i;
        EXPECT_EQ(pred.vaults[i].ominous, expected[i].ominous) << "index=" << i;
    }
}

} // namespace trialfinder
