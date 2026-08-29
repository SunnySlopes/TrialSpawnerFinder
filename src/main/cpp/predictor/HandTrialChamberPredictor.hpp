#pragma once
// ============================================================
// TrialSpawnerFinder C++ HandTrialChamberPredictor（B 流顶层装配）
//
// 复刻 cn.trialfinder.sim.world.TrialChamberPredictor 的对外面
// （26.2 语义）：持有固定 seed 的 JigsawPlacement，按 chunk 块坐标
// 预测试炼密室布局。与 Java 版一致，预测器可跨 predict 调用复用
// （本地元数据缓存 + RNG 每次 predict 重置）。
//
// 本文件还提供 trial_chambers 数据装配：
//  - parseTrialChamberAliases：从 worldgen/structure/trial_chambers.json 文本
//    解析 pool_aliases（复刻 TrialChambersData.loadAliases）；
//  - trialChambersConfig：26.2 固定配置面（复刻 SimStructureConfig.trialChambers）。
// ============================================================

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "jigsaw/JigsawPlacement.hpp"
#include "pool/Pool.hpp"
#include "template/StructureTemplateManager.hpp"

namespace trialfinder {

/**
 * 从 {@code structure/trial_chambers.json} 文本解析 pool_aliases。
 * 复刻 Java {@code TrialChambersData.loadAliases()} / {@code parseAlias(Json.Object)}。
 */
std::vector<PoolAliasBinding> parseTrialChambersAliases(const std::string& jsonText);

/**
 * 26.2 试炼密室固定配置面。
 * 复刻 Java {@code SimStructureConfig.trialChambers()}。
 */
JigsawPlacement::Config trialChambersConfig(const std::vector<PoolAliasBinding>& aliases);

/**
 * B 流预测器（复刻 HandTrialChamberPredictor，26.2 语义）。
 * 单线程使用；JNI 引擎按 seed 缓存实例以复用本地元数据。
 */
class HandTrialChamberPredictor {
public:
    HandTrialChamberPredictor(int64_t seed, JigsawPlacement::Config config,
                              PoolRegistry& pools, StructureTemplateManager& templates)
        : placement_(seed, std::move(config), pools, templates) {}

    /** 复刻 {@code predict(BlockPoint)}。 */
    JigsawPlacement::Prediction predict(int x, int z) { return placement_.predict(x, z); }

    /** 复刻 {@code Prediction.actualSpawners().size()}。 */
    int getSpawnerCount(int x, int z) {
        JigsawPlacement::Prediction result = predict(x, z);
        return static_cast<int>(result.spawners.size());
    }

    /** 复刻 {@code Prediction.actualSpawners()}。 */
    std::vector<SpawnerPoint> getSpawnerPositions(int x, int z) {
        JigsawPlacement::Prediction result = predict(x, z);
        return std::move(result.spawners);
    }

private:
    JigsawPlacement placement_;
};

} // namespace trialfinder
