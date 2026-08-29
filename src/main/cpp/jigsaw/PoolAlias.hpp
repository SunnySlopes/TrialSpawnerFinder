#pragma once
// ============================================================
// TrialSpawnerFinder C++ 池别名查找（C 流）
//
// 复刻 cn.trialfinder.sim.data 的 PoolAliasBinding / PoolAliasLookup
// （26.2 语义）。算法与官方一致：
//   RandomSource.create(worldSeed).forkPositional().at(startPos)
//   = new LegacyRandomSource(Mth.getSeed(x,y,z) ^ forkSeed)
// 其中 create(worldSeed) 是 LegacyRandomSource，forkPositional 消费一次
// nextLong 得到 forkSeed。
// ============================================================

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/Types.hpp"
#include "core/WeightedList.hpp"
#include "pool/Pool.hpp"
#include "random/Random.hpp"

namespace trialfinder {

/** 复刻 {@code PoolAliasBinding}（26.2 语义）。 */
struct PoolAliasBinding {
    enum class Kind : uint8_t { DIRECT = 0, RANDOM = 1, RANDOM_GROUP = 2 };

    Kind kind = Kind::DIRECT;
    std::string alias;   // DIRECT / RANDOM 的别名
    std::string target;  // DIRECT 的目标
    std::vector<std::pair<std::string, int>> targets;  // RANDOM 的加权目标
    std::vector<std::vector<PoolAliasBinding>> groups; // RANDOM_GROUP 的组
    std::vector<int> groupWeights;                     // RANDOM_GROUP 的组权重

    static PoolAliasBinding direct(std::string aliasId, std::string targetId) {
        PoolAliasBinding b;
        b.kind = Kind::DIRECT;
        b.alias = withDefaultNamespace(aliasId);
        b.target = withDefaultNamespace(targetId);
        return b;
    }

    static PoolAliasBinding random(std::string aliasId,
                                   std::vector<std::pair<std::string, int>> targets) {
        PoolAliasBinding b;
        b.kind = Kind::RANDOM;
        b.alias = withDefaultNamespace(aliasId);
        b.targets = std::move(targets);
        return b;
    }

    static PoolAliasBinding randomGroup(std::vector<std::vector<PoolAliasBinding>> groups,
                                        std::vector<int> groupWeights) {
        PoolAliasBinding b;
        b.kind = Kind::RANDOM_GROUP;
        b.groups = std::move(groups);
        b.groupWeights = std::move(groupWeights);
        return b;
    }

    /** 复刻 {@code forEachResolved}——消费随机流并写入 alias→target 映射。 */
    void forEachResolved(RandomSource& random,
                         std::unordered_map<std::string, std::string>& map) const {
        switch (kind) {
            case Kind::DIRECT:
                map[alias] = target;
                break;
            case Kind::RANDOM: {
                WeightedList<std::string> weighted(buildWeightedTargets());
                std::optional<std::string> resolved = weighted.getRandom(random);
                if (resolved) {
                    map[alias] = std::move(*resolved);
                }
                break;
            }
            case Kind::RANDOM_GROUP: {
                std::vector<WeightedList<int>::Item> items;
                items.reserve(groupWeights.size());
                for (std::size_t i = 0; i < groupWeights.size(); i++) {
                    items.push_back({static_cast<int>(i), groupWeights[i]});
                }
                WeightedList<int> weighted(std::move(items));
                std::optional<int> groupIndex = weighted.getRandom(random);
                if (groupIndex) {
                    const std::vector<PoolAliasBinding>& group = groups[*groupIndex];
                    for (const PoolAliasBinding& binding : group) {
                        binding.forEachResolved(random, map);
                    }
                }
                break;
            }
        }
    }

private:
    std::vector<WeightedList<std::string>::Item> buildWeightedTargets() const {
        std::vector<WeightedList<std::string>::Item> items;
        items.reserve(targets.size());
        for (const auto& pair : targets) {
            items.push_back({withDefaultNamespace(pair.first), pair.second});
        }
        return items;
    }
};

/** 复刻 {@code PoolAliasLookup}（26.2 语义）。 */
class PoolAliasLookup {
public:
    /** 等价 {@code EMPTY}——查找返回原 key。 */
    PoolAliasLookup() = default;

    explicit PoolAliasLookup(std::unordered_map<std::string, std::string> map)
        : map_(std::move(map)) {}

    static PoolAliasLookup create(const std::vector<PoolAliasBinding>& bindings,
                                  const BlockPos& startPos, int64_t worldSeed) {
        if (bindings.empty()) {
            return PoolAliasLookup();
        }
        // RandomSource.create(worldSeed) = LegacyRandomSource；forkPositional 消费 nextLong
        SingleThreadedRandomSource base(worldSeed);
        int64_t forkSeed = base.nextLong();
        PositionalRandomFactory factory(forkSeed);
        SingleThreadedRandomSource random = factory.at(startPos.x, startPos.y, startPos.z);
        std::unordered_map<std::string, std::string> map;
        for (const PoolAliasBinding& binding : bindings) {
            binding.forEachResolved(random, map);
        }
        return PoolAliasLookup(std::move(map));
    }

    /** 复刻 {@code lookup}——未命中返回 key 自身。 */
    std::string lookup(const std::string& key) const {
        auto it = map_.find(key);
        return it == map_.end() ? key : it->second;
    }

    bool isEmpty() const { return map_.empty(); }

private:
    std::unordered_map<std::string, std::string> map_;
};

} // namespace trialfinder
