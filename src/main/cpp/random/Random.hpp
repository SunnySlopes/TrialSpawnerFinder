#pragma once
// ============================================================
// TrialSpawnerFinder C++ 随机源
//
// 逐位复刻 cn.trialfinder.sim.random 的
// LegacyRandomSource / SingleThreadedRandomSource / WorldgenRandom /
// MarsagliaPolarGaussian / PositionalRandomFactory（26.2 语义）。
//
// 关键位一致细节：
//  - LCG 状态 48 位，MULTIPLIER=0x5DEECE66D, INCREMENT=0xB；
//  - next(bits) 的移位用 (48-bits) & 63，复刻 Java long 移位取模（含负值怪癖）；
//  - nextInt(bound) 的拒绝采样条件按 int32 回绕比较；
//  - 结构种子派生用 uint64 回绕乘法，位模式与 Java 有符号 long 一致。
// ============================================================

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "core/Types.hpp"

namespace trialfinder {

using std::int32_t;
using std::int64_t;
using std::uint32_t;
using std::uint64_t;

// ---------------- MarsagliaPolarGaussian ----------------

/** 复刻 {@code MarsagliaPolarGaussian}（26.2 语义）。 */
class MarsagliaPolarGaussian {
public:
    explicit MarsagliaPolarGaussian(class RandomSource& randomSource)
        : randomSource_(randomSource) {}

    void reset() { hasNextGaussian_ = false; }

    double nextGaussian();

private:
    class RandomSource& randomSource_;
    double nextGaussian_ = 0.0;
    bool hasNextGaussian_ = false;
};

// ---------------- RandomSource ----------------

/** 复刻 {@code RandomSource} 接口子集。 */
class RandomSource {
public:
    virtual ~RandomSource() = default;

    virtual void setSeed(int64_t seed) = 0;
    virtual int next(int bits) = 0;

    virtual int nextInt() { return next(32); }
    virtual int nextInt(int bound);
    virtual int64_t nextLong();
    virtual bool nextBoolean() { return next(1) != 0; }
    virtual float nextFloat() { return static_cast<float>(next(24)) * 5.9604645E-8f; }
    virtual double nextDouble() { return static_cast<double>(next(53)) * 1.1102230246251565E-16; }
    virtual double nextGaussian() { return gaussianSource().nextGaussian(); }

    /** 幂等分支（Java {@code RandomSource.nextInt(bound)}）。 */
    int nextIntIdempotent(int bound);

    void consumeCount(int count) {
        for (int i = 0; i < count; i++) {
            nextInt();
        }
    }

protected:
    MarsagliaPolarGaussian& gaussianSource() { return gaussian_; }

private:
    MarsagliaPolarGaussian gaussian_{*this};
};

// ---------------- SingleThreadedRandomSource ----------------

/**
 * 复刻 {@code SingleThreadedRandomSource}（非原子 LCG）。
 * 对应 Java 中 {@code TrialChamberPredictor.NonAtomicLegacyRandomSource} 的底层，
 * C++ 预测器每个实例单线程使用。
 */
class SingleThreadedRandomSource : public RandomSource {
public:
    static constexpr uint64_t MODULUS_MASK = (1ULL << 48) - 1;
    static constexpr uint64_t MULTIPLIER = 0x5DEECE66DULL;
    static constexpr uint64_t INCREMENT = 0xBULL;

    explicit SingleThreadedRandomSource(int64_t seedValue) {
        setSeed(seedValue);
    }

    void setSeed(int64_t seedValue) override {
        seed_ = (static_cast<uint64_t>(seedValue) ^ MULTIPLIER) & MODULUS_MASK;
        gaussianSource().reset();
    }

    int next(int bits) override {
        seed_ = (seed_ * MULTIPLIER + INCREMENT) & MODULUS_MASK;
        int shift = (48 - bits) & 63;
        return static_cast<int32_t>(seed_ >> shift);
    }

    int64_t getSeedState() const { return static_cast<int64_t>(seed_); }

private:
    uint64_t seed_ = 0;
};

// ---------------- WorldgenRandom ----------------

/**
 * 复刻 {@code WorldgenRandom}（26.2 语义）。
 * 包装底层 {@link RandomSource}，提供结构种子派生方法。
 */
class WorldgenRandom {
public:
    explicit WorldgenRandom(RandomSource& randomSource) : randomSource_(randomSource) {}

    RandomSource& randomSource() { return randomSource_; }

    int getCount() const { return count_; }

    int next(int bits) {
        count_++;
        return randomSource_.next(bits);
    }

    void setSeed(int64_t seedValue) {
        randomSource_.setSeed(seedValue);
    }

    int nextInt() { return next(32); }

    int nextInt(int bound) {
        if (bound <= 0) {
            throw std::invalid_argument("Bound 必须为正: " + std::to_string(bound));
        }
        return randomSource_.nextInt(bound);
    }

    int64_t nextLong() { return randomSource_.nextLong(); }

    bool nextBoolean() { return randomSource_.nextBoolean(); }

    float nextFloat() { return randomSource_.nextFloat(); }

    double nextDouble() { return randomSource_.nextDouble(); }

    double nextGaussian() { return randomSource_.nextGaussian(); }

    /** 原版 {@code setLargeFeatureWithSalt}（试炼密室：salt = 94251327）。 */
    void setLargeFeatureWithSalt(int64_t worldSeed, int32_t regionX, int32_t regionZ, int32_t salt) {
        uint64_t m = (uint64_t)(int64_t)regionX * 341873128712ULL
                   + (uint64_t)(int64_t)regionZ * 132897987541ULL
                   + (uint64_t)worldSeed + (uint64_t)(int64_t)salt;
        setSeed(static_cast<int64_t>(m));
    }

    /** 原版 {@code setLargeFeatureSeed}——结构内部 jigsaw 随机流。 */
    void setLargeFeatureSeed(int64_t worldSeed, int32_t chunkX, int32_t chunkZ) {
        setSeed(worldSeed);
        int64_t m = nextLong();
        int64_t n = nextLong();
        int64_t o = (int64_t)((uint64_t)(int64_t)chunkX * (uint64_t)m
                    ^ (uint64_t)(int64_t)chunkZ * (uint64_t)n
                    ^ (uint64_t)worldSeed);
        setSeed(o);
    }

    void setFeatureSeed(int64_t worldSeed, int32_t chunkX, int32_t chunkZ) {
        int64_t m = worldSeed + (int64_t)chunkX + 10000LL * (int64_t)chunkZ;
        setSeed(m);
    }

    int64_t setDecorationSeed(int64_t worldSeed, int32_t chunkX, int32_t chunkZ) {
        setSeed(worldSeed);
        int64_t m = nextLong() | 1LL;
        int64_t n = nextLong() | 1LL;
        int64_t o = (int64_t)((uint64_t)(int64_t)chunkX * (uint64_t)m
                    + (uint64_t)(int64_t)chunkZ * (uint64_t)n
                    ^ (uint64_t)worldSeed);
        setSeed(o);
        return o;
    }

private:
    RandomSource& randomSource_;
    int count_ = 0;
};

// ---------------- PositionalRandomFactory ----------------

/**
 * 复刻 {@code LegacyPositionalRandomFactory} 子集——
 * C 流（pool alias）派生：{@code at(x,y,z) = new LegacyRandomSource(Mth.getSeed(x,y,z) ^ seed)}。
 */
class PositionalRandomFactory {
public:
    explicit PositionalRandomFactory(int64_t seed) : seed_(seed) {}

    /** 复刻 {@code LegacyRandomSource.forkPositional().at(x,y,z)}。 */
    SingleThreadedRandomSource at(int x, int y, int z) const {
        return SingleThreadedRandomSource(Mth::getSeed(x, y, z) ^ seed_);
    }

    SingleThreadedRandomSource at(const BlockPos& pos) const {
        return at(pos.x, pos.y, pos.z);
    }

    int64_t seed() const { return seed_; }

private:
    int64_t seed_;
};

// ---------------- RandomSupport ----------------

/** 复刻 {@code RandomSupport} 子集（26.2 语义）。 */
namespace RandomSupport {
constexpr int64_t GOLDEN_RATIO_64 = -7046029254386353131LL;
constexpr int64_t SILVER_RATIO_64 = 7640891576956012809LL;

inline int64_t mixStafford13(int64_t l) {
    l = (l ^ (l >> 30)) * -4658895280553007687LL;
    l = (l ^ (l >> 27)) * -7723592293110705685LL;
    return l ^ (l >> 31);
}

struct Seed128bit {
    int64_t seedLo;
    int64_t seedHi;

    Seed128bit xor_(int64_t lo, int64_t hi) const {
        return Seed128bit{seedLo ^ lo, seedHi ^ hi};
    }

    Seed128bit mixed() const {
        return Seed128bit{mixStafford13(seedLo), mixStafford13(seedHi)};
    }
};

inline Seed128bit upgradeSeedTo128bitUnmixed(int64_t seedValue) {
    int64_t lo = seedValue ^ SILVER_RATIO_64;
    int64_t hi = lo + GOLDEN_RATIO_64;
    return Seed128bit{lo, hi};
}

inline Seed128bit upgradeSeedTo128bit(int64_t seedValue) {
    return upgradeSeedTo128bitUnmixed(seedValue).mixed();
}
} // namespace RandomSupport

// ---------------- 头文件内联实现 ----------------

inline double MarsagliaPolarGaussian::nextGaussian() {
    if (hasNextGaussian_) {
        hasNextGaussian_ = false;
        return nextGaussian_;
    }
    double d0, d1, d2;
    do {
        d0 = 2.0 * randomSource_.nextDouble() - 1.0;
        d1 = 2.0 * randomSource_.nextDouble() - 1.0;
        d2 = d0 * d0 + d1 * d1;
    } while (d2 >= 1.0 || d2 == 0.0);
    double d3 = std::sqrt(-2.0 * std::log(d2) / d2);
    nextGaussian_ = d1 * d3;
    hasNextGaussian_ = true;
    return d0 * d3;
}

inline int RandomSource::nextInt(int bound) {
    if (bound <= 0) {
        throw std::invalid_argument("Bound 必须为正: " + std::to_string(bound));
    }
    if ((bound & -bound) == bound) {
        return static_cast<int32_t>((static_cast<int64_t>(bound) * next(31)) >> 31);
    }
    int bits, value;
    do {
        bits = next(31);
        value = bits % bound;
    } while (static_cast<int32_t>(
                 static_cast<uint32_t>(bits) - static_cast<uint32_t>(value)
               + static_cast<uint32_t>(bound - 1)) < 0);
    return value;
}

inline int64_t RandomSource::nextLong() {
    // Java: ((long)next(32) << 32) + next(32)。低 32 位以 int 符号扩展后相加，
    // 当 bit31 置位时会对高 32 位产生借位 —— 必须用 + 而非按位或。
    int32_t a = static_cast<int32_t>(next(32));
    int32_t b = static_cast<int32_t>(next(32));
    return (static_cast<int64_t>(a) << 32) + static_cast<int64_t>(b);
}

inline int RandomSource::nextIntIdempotent(int bound) {
    return nextInt(bound);
}

} // namespace trialfinder
