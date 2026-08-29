// ============================================================
// TrialSpawnerFinder C++ 性能基准
//
// 用法：trialfinder_bench [资源根目录]
// 缺省资源根时使用 TRIALFINDER_TEST_RESOURCES（与测试一致）。
//
// 度量与 build/oracle/oracle.txt 的 Java 侧一致：
//   seed 0 / 1 / 42 / 9206294873968313284 × chunk 0,0 / 1,-1 / 5,7 / 16,-16
// 每个组合先跑一遍冷启动（首次装载模板 NBT + 计算元数据），
// 再跑一遍热启动，输出每次预测耗时。
// ============================================================

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "jigsaw/JigsawPlacement.hpp"
#include "pool/Pool.hpp"
#include "predictor/HandTrialChamberPredictor.hpp"
#include "template/StructureTemplateManager.hpp"

#include "test/TestResources.hpp"

using namespace std::chrono;

namespace {

std::string readFileText(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        throw std::runtime_error("无法读取文件: " + path);
    }
    std::string out;
    char buf[65536];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        out.append(buf, n);
    }
    std::fclose(f);
    return out;
}

} // namespace

int main(int argc, char** argv) {
    using trialfinder::HandTrialChamberPredictor;
    using trialfinder::JigsawPlacement;
    using trialfinder::PoolAliasBinding;
    using trialfinder::PoolRegistry;
    using trialfinder::SharedMetadata;
    using trialfinder::StructureTemplateManager;

    std::string root = argc > 1 ? argv[1] : trialfinder::testResourceRoot();
    std::string probe = root + "/data/minecraft/worldgen/structure/trial_chambers.json";
    if (root.empty() || std::fopen(probe.c_str(), "rb") == nullptr) {
        std::fprintf(stderr, "缺少资源根目录: %s\n", root.c_str());
        return 2;
    }

    StructureTemplateManager templates;
    templates.setRoot(root);
    auto sharedMetadata = std::make_shared<SharedMetadata>();
    PoolRegistry pools(templates);
    pools.setSharedMetadata(sharedMetadata);
    pools.loadAllFromDirectory(root);

    std::vector<PoolAliasBinding> aliases =
        trialfinder::parseTrialChambersAliases(readFileText(probe));
    JigsawPlacement::Config config = trialfinder::trialChambersConfig(aliases);

    struct Combo {
        int64_t seed;
        int chunkX, chunkZ;
        const char* label;
    };
    const Combo combos[] = {
        {0LL, 0, 0, "seed 0   chunk 0,0   "},
        {0LL, 1, -1, "seed 0   chunk 1,-1  "},
        {0LL, 5, 7, "seed 0   chunk 5,7   "},
        {0LL, 16, -16, "seed 0   chunk 16,-16"},
        {1LL, 0, 0, "seed 1   chunk 0,0   "},
        {1LL, 1, -1, "seed 1   chunk 1,-1  "},
        {1LL, 5, 7, "seed 1   chunk 5,7   "},
        {1LL, 16, -16, "seed 1   chunk 16,-16"},
        {42LL, 0, 0, "seed 42  chunk 0,0   "},
        {42LL, 1, -1, "seed 42  chunk 1,-1  "},
        {42LL, 5, 7, "seed 42  chunk 5,7   "},
        {42LL, 16, -16, "seed 42  chunk 16,-16"},
        {9206294873968313284LL, 0, 0, "seed B   chunk 0,0   "},
        {9206294873968313284LL, 1, -1, "seed B   chunk 1,-1  "},
        {9206294873968313284LL, 5, 7, "seed B   chunk 5,7   "},
        {9206294873968313284LL, 16, -16, "seed B   chunk 16,-16"},
    };
    constexpr int kCombos = static_cast<int>(sizeof(combos) / sizeof(combos[0]));

    std::printf("=== TrialSpawnerFinder C++ B 流预测基准 (26.2) ===\n");
    std::printf("资源根: %s\n", root.c_str());

    // 第一轮：冷启动（含模板 NBT 装载 + 元数据计算）
    std::vector<double> cold(kCombos), warm(kCombos);
    double coldSum = 0, warmSum = 0;
    for (int pass = 0; pass < 2; pass++) {
        std::printf("\n--- 第 %d 轮（%s） ---\n", pass + 1,
                    pass == 0 ? "冷启动" : "热启动");
        for (int i = 0; i < kCombos; i++) {
            HandTrialChamberPredictor predictor(combos[i].seed, config, pools, templates);
            auto t0 = steady_clock::now();
            JigsawPlacement::Prediction pred =
                predictor.predict(combos[i].chunkX * 16, combos[i].chunkZ * 16);
            auto t1 = steady_clock::now();
            double ms = duration<double, std::milli>(t1 - t0).count();
            if (pass == 0) {
                cold[i] = ms;
                coldSum += ms;
            } else {
                warm[i] = ms;
                warmSum += ms;
            }
            std::printf("  %s %8.3f ms  spawner=%zu\n",
                        combos[i].label, ms, pred.spawners.size());
        }
    }

    std::printf("\n--- 汇总 ---\n");
    std::printf("  冷启动首轮总耗时        : %8.3f ms（均值 %.3f ms/次）\n",
                coldSum, coldSum / kCombos);
    std::printf("  热启动稳态总耗时        : %8.3f ms（均值 %.3f ms/次）\n",
                warmSum, warmSum / kCombos);
    double coldMin = *std::min_element(cold.begin(), cold.end());
    double coldMax = *std::max_element(cold.begin(), cold.end());
    double warmMin = *std::min_element(warm.begin(), warm.end());
    double warmMax = *std::max_element(warm.begin(), warm.end());
    std::printf("  冷启动范围              : %.3f – %.3f ms\n", coldMin, coldMax);
    std::printf("  热启动范围              : %.3f – %.3f ms\n", warmMin, warmMax);
    return 0;
}
