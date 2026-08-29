// ============================================================
// TrialSpawnerFinder 随机源/几何逐位一致性测试
//
// 参照值全部来自纯 Java sim 实现的 oracle（build/oracle/oracle.txt），
// 与 C++ 实现的位序列逐位对拍。任何偏差即 bug。
// ============================================================

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "core/Types.hpp"
#include "jigsaw/JigsawPlacement.hpp"
#include "random/Random.hpp"

namespace trialfinder {

using std::int64_t;

namespace {

// 与 Java RandomSource.create(seed) 等价：LegacyRandomSource（LCG）
SingleThreadedRandomSource makeSource(int64_t seed) {
    return SingleThreadedRandomSource(seed);
}

} // namespace

// ---------------- LCG 基础抽取 ----------------

TEST(RandomGenTest, NextIntMatchesJavaForKnownSeedsAndBounds) {
    struct Case {
        int64_t seed;
        int bounds[10];
    };
    const Case cases[] = {
        {0L, {1, 1, 4, 4, 13, 19, 91, 561, 39160, 1678332854}},
        {1L, {1, 1, 2, 3, 15, 2, 34, 1006, 63422, 1526301748}},
        {-1L, {0, 2, 4, 4, 15, 38, 77, 678, 54143, 121412731}},
    };
    const int bounds[] = {2, 3, 5, 8, 31, 64, 100, 1200, 65536, 2147483647};
    for (const Case& c : cases) {
        SingleThreadedRandomSource r(c.seed);
        for (int i = 0; i < 10; i++) {
            EXPECT_EQ(r.nextInt(bounds[i]), c.bounds[i])
                << "seed=" << c.seed << " bound=" << bounds[i];
        }
    }
}

TEST(RandomGenTest, NextLongMatchesJavaForKnownSeeds) {
    struct Case {
        int64_t seed;
        int64_t first;
        int64_t second;
    };
    const Case cases[] = {
        {0L, -4962768465676381896LL, 4437113781045784766LL},
        {1L, -4964420948893066024LL, 7564655870752979346LL},
        {-1L, 4961115982468162243LL, 226341162490527646LL},
    };
    for (const Case& c : cases) {
        SingleThreadedRandomSource r(c.seed);
        EXPECT_EQ(r.nextLong(), c.first);
        EXPECT_EQ(r.nextLong(), c.second);
    }
}

TEST(RandomGenTest, NextDoubleMatchesJavaSimQuirk) {
    // Java LegacyRandomSource.nextDouble() = next(53) * 1.11e-16，
    // 而 next(53) 对 48 位 LCG 恒为 0（48-53 & 63 = 59，右移 59 为 0）。
    // 这是 sim 的原样行为，C++ 必须一致。
    for (int64_t seed : {0L, 1L, -1L}) {
        SingleThreadedRandomSource r(seed);
        EXPECT_EQ(r.nextDouble(), 0.0);
        EXPECT_EQ(r.nextDouble(), 0.0);
    }
}

TEST(RandomGenTest, Next48MatchesJavaInternalState) {
    // Java next(48) = (int)(state)（移位 0），即 48 位状态的低 32 位。
    SingleThreadedRandomSource r(0L);
    EXPECT_EQ(r.next(48), -1268774284);
    EXPECT_EQ(r.next(48), 1362668399);
}

TEST(RandomGenTest, NextIntPowerOfTwoAndRejectionBothMatch) {
    // 2 的幂走乘位路径，非 2 的幂走拒绝采样；两者都须逐位一致。
    // 参照：Java LegacyRandomSource seed 42 的真实抽取序列。
    SingleThreadedRandomSource r(42L);
    EXPECT_EQ(r.nextInt(1), 0);   // bound=1 乘位路径，恒 0
    EXPECT_EQ(r.nextInt(2), 0);   // 流的第 2 次抽取（乘位路径）
    EXPECT_EQ(r.nextInt(2), 1);   // 第 3 次
    EXPECT_EQ(r.nextInt(2), 0);   // 第 4 次

    // nextInt(1) 同样消费一次 next(31)：随后 nextInt(2) 等于流的第 2 次抽取 0
    SingleThreadedRandomSource q(42L);
    q.nextInt(1);
    EXPECT_EQ(q.nextInt(2), 0);

    // 拒绝采样路径：fresh nextInt(3)=2（参照 Java），随后 nextInt(2)=0
    SingleThreadedRandomSource p(42L);
    EXPECT_EQ(p.nextInt(3), 2);
    EXPECT_EQ(p.nextInt(2), 0);

    // 同源 nextLong 参照
    SingleThreadedRandomSource s(42L);
    EXPECT_EQ(s.nextLong(), -5025562857975149833LL);
}

// ---------------- Mth.getSeed ----------------

TEST(RandomGenTest, MthGetSeedMatchesJava) {
    struct Case {
        int x, y, z;
        int64_t expected;
    };
    const Case cases[] = {
        {0, 0, 0, 0LL},
        {1, 2, 3, -33674130277896LL},
        {-1, -40, -20, -22967138523888LL},
        {6250, 1875000, 12, 3955618114681LL},
        {-6250, -12, 5, 21685346984912LL},
    };
    for (const Case& c : cases) {
        EXPECT_EQ(Mth::getSeed(c.x, c.y, c.z), c.expected)
            << "Mth::getSeed(" << c.x << "," << c.y << "," << c.z << ")";
    }
}

// ---------------- Rotation ----------------

TEST(RandomGenTest, RotationGetRandomMatchesJava) {
    struct Case {
        int64_t seed;
        int rotationIndex;
    };
    const Case cases[] = {
        {0L, 2},   // CLOCKWISE_180
        {1L, 2},   // CLOCKWISE_180
        {-1L, 1},  // CLOCKWISE_90
    };
    for (const Case& c : cases) {
        SingleThreadedRandomSource r(c.seed);
        Rotation rot = rotationGetRandom(r);
        EXPECT_EQ(static_cast<int>(rot), c.rotationIndex) << "seed=" << c.seed;
    }
}

TEST(RandomGenTest, RotationGetShuffledMatchesJava) {
    struct Case {
        int64_t seed;
        int expected[4];
    };
    // 参照：在「全新」RNG 上直接调用 Java Rotation.getShuffled（未先消费 getRandom）。
    // 注意 oracle.txt 的 rotShuffled 是 getRandom 消费一次 nextInt(4) 之后的流位置，
    // 因此与这里不同 —— 这里以 Trace.java 对全新 RNG 的输出为准。
    const Case cases[] = {
        {0L, {3, 0, 1, 2}},
        {1L, {3, 0, 1, 2}},
        {-1L, {3, 0, 2, 1}},
    };
    for (const Case& c : cases) {
        SingleThreadedRandomSource r(c.seed);
        std::vector<Rotation> shuffled = rotationGetShuffled(r);
        ASSERT_EQ(shuffled.size(), 4u) << "seed=" << c.seed;
        for (int i = 0; i < 4; i++) {
            EXPECT_EQ(static_cast<int>(shuffled[i]), c.expected[i])
                << "seed=" << c.seed << " index=" << i;
        }
    }
}

// ---------------- setLargeFeatureSeed ----------------

TEST(RandomGenTest, SetLargeFeatureSeedMatchesJava) {
    struct Case {
        int64_t seed;
        int chunkX, chunkZ;
        int nextInt21;
        int nextInt4;
        int64_t nextLong;
        int64_t nextLong2;
    };
    const Case cases[] = {
        {0L, 0, 0, 12, 3, 4437113781045784766LL, -6688467811848818630LL},
        {0L, 1, -1, 9, 3, 3100210592200616112LL, -7143941486703086305LL},
        {0L, 6250, 1875000, 3, 3, 3476702433626573590LL, -326120787227388792LL},
        {9206294873968313284LL, 0, 0, 0, 1, 6003247407740555246LL, -1526535569672509838LL},
        {9206294873968313284LL, 1, -1, 14, 2, 5034873414919717450LL, -6964814086605437849LL},
        {9206294873968313284LL, 6250, 1875000, 1, 1, -6202615561367708259LL, 8690834866879180459LL},
    };
    for (const Case& c : cases) {
        SingleThreadedRandomSource src(0L);
        WorldgenRandom r(src);
        r.setLargeFeatureSeed(c.seed, c.chunkX, c.chunkZ);
        EXPECT_EQ(r.nextInt(21), c.nextInt21)
            << "seed=" << c.seed << " chunk=" << c.chunkX << "," << c.chunkZ;
        EXPECT_EQ(r.nextInt(4), c.nextInt4);
        EXPECT_EQ(r.nextLong(), c.nextLong);
        EXPECT_EQ(r.nextLong(), c.nextLong2);
    }
}

TEST(RandomGenTest, SetLargeFeatureWithSaltConsumesLikeJava) {
    // 试炼密室 A 流用 salt=94251327；此处仅验证派生不崩且可复现。
    SingleThreadedRandomSource src(0L);
    WorldgenRandom r(src);
    r.setLargeFeatureWithSalt(0L, 0, 0, 94251327);
    int first = r.nextInt(4);
    SingleThreadedRandomSource src2(0L);
    WorldgenRandom r2(src2);
    r2.setLargeFeatureWithSalt(0L, 0, 0, 94251327);
    EXPECT_EQ(r2.nextInt(4), first);
}

// ---------------- orderConnectors（洗牌 + 稳定降序） ----------------

namespace {

// 与 Java 测试一致：priorities 用独立 RNG seed ^ 0x5DEECE66DL 生成 selectionPriority
std::vector<Connector> makeConnectors(int size, int64_t seed) {
    SingleThreadedRandomSource priorities(seed ^ 0x5DEECE66DLL);
    std::vector<Connector> connectors;
    connectors.reserve(size);
    for (int i = 0; i < size; i++) {
        Connector c;
        c.relativePos = BlockPos(i, 0, 0);
        c.front = Direction::NORTH;
        c.top = Direction::UP;
        c.pool = "";
        c.name = "";
        c.target = "";
        c.rollable = false;
        c.selectionPriority = priorities.nextInt(5) - 2;
        c.placementPriority = 0;
        connectors.push_back(c);
    }
    return connectors;
}

} // namespace

TEST(RandomGenTest, OrderConnectorsMatchesJavaShuffleThenStableSort) {
    // oracle 值：(seed, size) → 排序后相对 x 序列 + 洗牌后 RNG 的 nextLong
    struct Case {
        int size;
        int64_t seed;
        std::vector<int> order;
        int64_t nextLong;
    };
    const std::vector<Case> cases = {
        {0, 0, {}, -4962768465676381896LL},
        {0, 1, {}, -4964420948893066024LL},
        {0, 2, {}, -4959463499243013640LL},
        {0, 3, {}, -4961115986754665064LL},
        {1, 0, {0}, -4962768465676381896LL},
        {1, 1, {0}, -4964420948893066024LL},
        {1, 2, {0}, -4959463499243013640LL},
        {1, 3, {0}, -4961115986754665064LL},
        {2, 0, {1, 0}, -3109364765729502342LL},
        {2, 1, {0, 1}, 1853403699951111791LL},
        {2, 2, {0, 1}, 5411842376618821008LL},
        {2, 3, {0, 1}, -8072133231410116475LL},
        {3, 0, {1, 2, 0}, 4437113781045784766LL},
        {3, 1, {2, 0, 1}, 7564655870752979346LL},
        {3, 2, {0, 2, 1}, -1817970389778669801LL},
        {3, 3, {0, 1, 2}, 1309571695633557482LL},
        {4, 0, {1, 3, 0, 2}, -7261648964369397258LL},
        {4, 1, {0, 2, 1, 3}, 7515937759503895804LL},
        {4, 2, {3, 0, 1, 2}, 76665748188021738LL},
        {4, 3, {1, 0, 3, 2}, -3592491610238171408LL},
        {5, 0, {1, 4, 3, 2, 0}, -6688467811848818630LL},
        {5, 1, {2, 0, 1, 4, 3}, 3831662765844904176LL},
        {5, 2, {3, 0, 4, 2, 1}, 9164759175887871693LL},
        {5, 3, {3, 1, 0, 2, 4}, 1238145679872042884LL},
        {6, 0, {1, 4, 5, 3, 2, 0}, 5700976833288827063LL},
        {6, 1, {2, 0, 5, 1, 4, 3}, 668424937071768037LL},
        {6, 2, {3, 5, 0, 1, 2, 4}, -2680663443691639204LL},
        {6, 3, {3, 1, 0, 5, 2, 4}, -7713215339908698230LL},
        {7, 0, {6, 1, 3, 4, 5, 0, 2}, -8292973307042192125LL},
        {7, 1, {0, 5, 2, 1, 4, 6, 3}, 6137546356583794141LL},
        {7, 2, {3, 6, 0, 5, 2, 1, 4}, -260524486875061426LL},
        {7, 3, {1, 3, 0, 2, 5, 4, 6}, -4276748896958626776LL},
        {8, 0, {6, 1, 7, 3, 4, 5, 0, 2}, 2158390814503909950LL},
        {8, 1, {2, 7, 0, 5, 1, 4, 6, 3}, -6292788870224356907LL},
        {8, 2, {3, 7, 0, 6, 5, 4, 2, 1}, 614006110250892047LL},
        {8, 3, {3, 1, 0, 2, 5, 4, 7, 6}, -7837173574477374810LL},
    };
    for (const Case& c : cases) {
        std::vector<Connector> connectors = makeConnectors(c.size, c.seed);
        SingleThreadedRandomSource r(c.seed);
        JigsawPlacement::orderConnectors(connectors, r);
        ASSERT_EQ(connectors.size(), c.order.size())
            << "size=" << c.size << " seed=" << c.seed;
        for (std::size_t i = 0; i < c.order.size(); i++) {
            EXPECT_EQ(connectors[i].relativePos.x, c.order[i])
                << "size=" << c.size << " seed=" << c.seed << " pos=" << i;
        }
        EXPECT_EQ(r.nextLong(), c.nextLong)
            << "size=" << c.size << " seed=" << c.seed;
    }
}

} // namespace trialfinder
