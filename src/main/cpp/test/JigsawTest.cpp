// ============================================================
// TrialSpawnerFinder B 流拼接集成测试（读磁盘真实资源）
//
// 从 minecraft-26.2-runtime/build/resources/main 加载
// 47 个 pool JSON + 191 个结构 NBT + worldgen/structure/
// trial_chambers.json，与 Java oracle 的 predict 结果逐位对拍。
// 缺资源时跳过（GTEST_SKIP）。
// ============================================================

#include <gtest/gtest.h>

#include "jigsaw/JigsawPlacement.hpp"
#include "pool/Pool.hpp"
#include "predictor/HandTrialChamberPredictor.hpp"
#include "template/StructureTemplateManager.hpp"
#include "test/ResourceFixture.hpp"
#include "test/TestResources.hpp"

namespace trialfinder {

using JigsawResourceTest = ResourceTestBase;

// ---------------- 配置装配 ----------------

TEST_F(JigsawResourceTest, TrialChambersConfigFieldsMatch26Point2) {
    EXPECT_EQ(config_.startPool, "trial_chambers/chamber/end");
    EXPECT_EQ(config_.startJigsawName, "");
    EXPECT_EQ(config_.size, 20);
    EXPECT_EQ(config_.startHeightMin, -40);
    EXPECT_EQ(config_.startHeightMax, -20);
    EXPECT_FALSE(config_.useExpansionHack);
    EXPECT_FALSE(config_.projectStartToHeightmap);
    EXPECT_EQ(config_.maxDistance.horizontal, 116);
    EXPECT_EQ(config_.maxDistance.vertical, 116);
    EXPECT_EQ(config_.dimensionPadding.bottom, 10);
    EXPECT_EQ(config_.dimensionPadding.top, 10);
    EXPECT_EQ(config_.minY, -64);
    EXPECT_EQ(config_.maxY, 320);
}

TEST_F(JigsawResourceTest, TrialChambersAliasesParsedFromJson) {
    ASSERT_FALSE(config_.poolAliases.empty());
    // pool_aliases 至少含 ranged/slow_ranged 等别名，非空即可
    EXPECT_EQ(config_.startPool, "trial_chambers/chamber/end");
}

TEST_F(JigsawResourceTest, PoolRegistryLoadedFromDisk) {
    // 核心池必须可用；别名解析后 start 池应存在。
    const StructureTemplatePool* start = pools_->get("trial_chambers/chamber/end");
    ASSERT_NE(start, nullptr);
    EXPECT_GT(start->size(), 0);
    EXPECT_EQ(pools_->get("minecraft:empty"), pools_->emptyPool());
}

TEST_F(JigsawResourceTest, TemplateManagerLazyLoadsGzipNbt) {
    // trial_chambers 的任意模板 id（start 池第一个元素）
    StructureTemplatePool& start = pools_->getOrThrow("trial_chambers/chamber/end");
    ASSERT_GT(start.getTemplates().size(), 0u);
    const auto& element = start.getTemplates()[0].first;
    // 触发磁盘 gzip NBT 加载
    BlockPos size = element->getSize(*templates_, Rotation::NONE);
    EXPECT_GT(size.y, 0);
    EXPECT_GT(templates_->cacheSize(), 0u);
}

// ---------------- 预测对拍（块坐标 = chunk * 16） ----------------

TEST_F(JigsawResourceTest, PredictSeed0Chunk00MatchesOracle) {
    HandTrialChamberPredictor predictor(0LL, config_, *pools_, *templates_);
    JigsawPlacement::Prediction pred = predictor.predict(0, 0);  // chunk (0,0)
    EXPECT_TRUE(pred.exists);
    ASSERT_EQ(pred.spawners.size(), 21u);
    // oracle: 首 spawner -70,-29,-4，末 spawner 44,-21,-20
    EXPECT_EQ(pred.spawners.front().x, -70);
    EXPECT_EQ(pred.spawners.front().y, -29);
    EXPECT_EQ(pred.spawners.front().z, -4);
    EXPECT_EQ(pred.spawners.back().x, 44);
    EXPECT_EQ(pred.spawners.back().y, -21);
    EXPECT_EQ(pred.spawners.back().z, -20);
}

TEST_F(JigsawResourceTest, PredictSeed42Chunk00CountMatchesOracle) {
    HandTrialChamberPredictor predictor(42LL, config_, *pools_, *templates_);
    JigsawPlacement::Prediction pred = predictor.predict(0, 0);
    EXPECT_TRUE(pred.exists);
    EXPECT_EQ(pred.spawners.size(), 29u);  // oracle spawnerCount=29
}

TEST_F(JigsawResourceTest, PredictBlockCoordinatesConvertToChunk) {
    // oracle：seed 0 chunk (1,-1) → 18，(5,7) → 23，(16,-16) → 16
    HandTrialChamberPredictor predictor(0LL, config_, *pools_, *templates_);
    EXPECT_EQ(predictor.predict(16, -16).spawners.size(), 18u);    // chunk (1,-1)
    EXPECT_EQ(predictor.predict(80, 112).spawners.size(), 23u);    // chunk (5,7)
    EXPECT_EQ(predictor.predict(256, -256).spawners.size(), 16u);  // chunk (16,-16)
}

TEST_F(JigsawResourceTest, PredictIsDeterministic) {
    HandTrialChamberPredictor predictor(42LL, config_, *pools_, *templates_);
    JigsawPlacement::Prediction a = predictor.predict(16, -16);
    JigsawPlacement::Prediction b = predictor.predict(16, -16);
    ASSERT_EQ(a.spawners.size(), b.spawners.size());
    ASSERT_EQ(a.spawners.size(), 17u);  // oracle：seed 42 chunk (1,-1) = 17
    for (std::size_t i = 0; i < a.spawners.size(); i++) {
        EXPECT_EQ(a.spawners[i].x, b.spawners[i].x);
        EXPECT_EQ(a.spawners[i].y, b.spawners[i].y);
        EXPECT_EQ(a.spawners[i].z, b.spawners[i].z);
    }
    // spawner infos 与 vaults 同样确定
    ASSERT_EQ(a.spawnerInfos.size(), b.spawnerInfos.size());
    ASSERT_EQ(a.vaults.size(), b.vaults.size());
}

TEST_F(JigsawResourceTest, PredictorReusedAcrossCallsKeepsLocalCache) {
    // 复用同一 predictor 多次预测：RNG 每次重置，结果稳定
    HandTrialChamberPredictor predictor(0LL, config_, *pools_, *templates_);
    EXPECT_EQ(predictor.predict(0, 0).spawners.size(), 21u);
    EXPECT_EQ(predictor.predict(80, 112).spawners.size(), 23u);
    EXPECT_EQ(predictor.predict(0, 0).spawners.size(), 21u);
}

} // namespace trialfinder
