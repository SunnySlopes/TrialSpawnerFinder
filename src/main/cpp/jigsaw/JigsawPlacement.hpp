#pragma once
// ============================================================
// TrialSpawnerFinder C++ JigsawPlacement（B 流递归拼接）
//
// 逐位复刻 cn.trialfinder.sim.world.TrialChamberPredictor 的
// B 流（26.2 语义）：createStartContext → createStart → generateChildren
// → generatePiece。RNG 消费顺序与原版一致：
//  - setLargeFeatureSeed / sampleStartHeight / Rotation.getRandom；
//  - orderConnectors（洗牌 + 稳定降序）、getShuffledTemplates、
//    Rotation.getShuffled、fillConnectors(target) 的洗牌；
//  - SequencedPriorityIterator 决定 BFS 顺序。
// 生物群系过滤（A 流/气候系统）不在移植范围，此处恒为通过。
// ============================================================

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/Types.hpp"
#include "jigsaw/FreeSpace.hpp"
#include "jigsaw/PoolAlias.hpp"
#include "jigsaw/SequencedPriorityIterator.hpp"
#include "pool/Pool.hpp"
#include "random/Random.hpp"
#include "template/StructureTemplateManager.hpp"

namespace trialfinder {

// ---------------- 配置类型 ----------------

/** 复刻 {@code JigsawStructure.MaxDistance}（26.2 语义）。 */
struct MaxDistance {
    int horizontal;
    int vertical;
    MaxDistance(int value) : horizontal(value), vertical(value) {}
    MaxDistance(int horizontal_, int vertical_)
        : horizontal(horizontal_), vertical(vertical_) {}
};

/** 复刻 {@code DimensionPadding}（26.2 语义）。trial_chambers 用 bottom=10, top=10。 */
struct DimensionPadding {
    int bottom;
    int top;
    DimensionPadding(int bottom_ = 0, int top_ = 0) : bottom(bottom_), top(top_) {}
    bool isZero() const { return bottom == 0 && top == 0; }
};

// ---------------- 输出类型 ----------------

/** 复刻 {@code cn.trialfinder.model.SpawnerPoint}。 */
struct SpawnerPoint {
    int x, y, z;
};

/** 复刻 {@code TrialChamberPredictor.SpawnerInfo}。 */
struct SpawnerInfo {
    int x, y, z;
    std::string normalConfig;
    std::string ominousConfig;
};

/** 复刻 {@code TrialChamberPredictor.VaultInfo}。 */
struct VaultInfo {
    int x, y, z;
    bool ominous;
};

// ---------------- Connector / ElementMetadata ----------------

/** 复刻 {@code TrialChamberPredictor.Connector}。pool/name/target 为完整 "namespace:path"。 */
struct Connector {
    BlockPos relativePos;
    Direction front;
    Direction top;
    std::string pool;
    std::string name;
    std::string target;
    bool rollable;
    int selectionPriority;
    int placementPriority;
};

/** 复刻 {@code TrialChamberPredictor.ElementMetadata}（缓存未洗牌的 connectors/bbox/spawners）。 */
struct ElementMetadata {
    std::vector<Connector> connectors;
    BoundingBox boundingBox;
    std::vector<RelativeSpawner> spawners;
    bool supported = false;

    static ElementMetadata unsupported() {
        ElementMetadata m;
        m.supported = false;
        return m;
    }
};

// ---------------- 共享元数据缓存 ----------------

/**
 * 进程内共享的元数据缓存（复刻 Java {@code METADATA_CACHE}）。
 * 键为池元素原始指针；元素由 PoolRegistry 持有，缓存与池同寿命
 * （由资源持有者创建/销毁，先于池释放），因此无悬挂/地址复用风险。
 * 每个元素按 4 种旋转缓存未洗牌的 connectors / bbox / spawners ——
 * 这些值与 seed/位置无关，故跨预测器实例共享是确定的。
 */
struct SharedMetadata {
    std::recursive_mutex mutex;
    std::unordered_map<const StructurePoolElement*,
                       std::array<std::optional<ElementMetadata>, 4>> byElement;
};

// ---------------- JigsawPlacement ----------------

/**
 * B 流 Jigsaw 拼接引擎（复刻 TrialChamberPredictor 的 B 流，26.2 语义）。
 * 每个实例持有可复用的 WorldgenRandom（非原子 LCG），单线程使用。
 */
class JigsawPlacement {
public:
    /** 复刻 {@code SimStructureConfig} 的字段面。 */
    struct Config {
        std::string startPool;          // 完整 "namespace:path"
        std::string startJigsawName;    // 空 = 无 start_jigsaw_name
        int size = 20;
        int startHeightMin = -40;
        int startHeightMax = -20;
        bool useExpansionHack = false;
        bool projectStartToHeightmap = false;
        MaxDistance maxDistance = MaxDistance(116);
        DimensionPadding dimensionPadding = DimensionPadding(10, 10);
        std::vector<PoolAliasBinding> poolAliases;
        int minY = -64;
        int maxY = 320;

        int sampleStartHeight(RandomSource& random) const {
            return random.nextInt(startHeightMax - startHeightMin + 1) + startHeightMin;
        }
    };

    /** 复刻 {@code Prediction} 输出面。 */
    struct Prediction {
        int x = 0;
        int z = 0;
        bool exists = false;
        std::vector<SpawnerPoint> spawners;
        std::vector<SpawnerInfo> spawnerInfos;
        std::vector<VaultInfo> vaults;
    };

    JigsawPlacement(int64_t seed, Config config,
                    PoolRegistry& pools, StructureTemplateManager& templateManager)
        : seed_(seed), config_(std::move(config)), pools_(pools),
          templateManager_(templateManager), layoutRandom_(randomSource_),
          maxDepth_(config_.size), useExpansionHack_(config_.useExpansionHack) {}

    /** 复刻 {@code predict(BlockPoint)}。 */
    Prediction predict(int x, int z);

    /** 复刻 Java {@code TrialChamberPredictor.orderConnectors}（洗牌 + 稳定降序）。
     *  公开以便单元测试逐位对拍。 */
    static void orderConnectors(std::vector<Connector>& connectors, RandomSource& random);

private:
    struct LightPiece {
        const StructurePoolElement* element;
        BlockPos position;
        int groundLevelDelta;
        Rotation rotation;
        BoundingBox box;
    };

    struct QueuedPiece {
        LightPiece piece;
        std::shared_ptr<FreeSpace> shape;
        int depth;
    };

    struct StartLayout {
        LightPiece piece;
        BlockPos biomePosition;
    };

    struct StartContext {
        std::optional<StartLayout> layout;
        WorldgenRandom* random;
        PoolAliasLookup aliases;
    };

    // ---- createStart ----
    std::optional<StartLayout> createStart(
        const BlockPos& start, WorldgenRandom& random, PoolAliasLookup& aliases);
    std::optional<BlockPos> findStartingJigsaw(
        const StructurePoolElement& element, const std::string& name,
        const BlockPos& start, Rotation rotation, RandomSource& random);

    // ---- generateChildren / generatePiece ----
    void generateChildren(StartLayout& start, std::vector<LightPiece>& pieces,
                          RandomSource& random, PoolAliasLookup& aliases);
    void generatePiece(const LightPiece& source,
                       const std::shared_ptr<FreeSpace>& contextFree, int depth,
                       std::vector<LightPiece>& pieces,
                       SequencedPriorityIterator<QueuedPiece>& queue,
                       RandomSource& random, PoolAliasLookup& aliases);
    int expansionSize(const std::vector<Connector>& connectors,
                      const BoundingBox& box, PoolAliasLookup& aliases);

    // ---- 收集 ----
    void collectSpawners(LightPiece& piece,
                         std::vector<SpawnerPoint>& spawners,
                         std::vector<SpawnerInfo>& infos);
    void collectSpawnersSlow(const StructurePoolElement& element,
                             const BlockPos& position, Rotation rotation,
                             std::vector<SpawnerPoint>& spawners,
                             std::vector<SpawnerInfo>& infos);
    void collectVaults(LightPiece& piece, std::vector<VaultInfo>& vaults);
    void collectVaultsElement(const StructurePoolElement& element,
                              const LightPiece& piece, std::vector<VaultInfo>& vaults);

    // ---- 元数据 ----
    ElementMetadata& metadata(const StructurePoolElement& element, Rotation rotation);
    ElementMetadata createMetadata(const StructurePoolElement& element, Rotation rotation);
    static std::vector<Connector> toConnectors(
        const std::vector<JigsawBlockInfo>& infos, const BlockPos& origin);

    std::vector<Connector> fillConnectors(
        const StructurePoolElement& element, const BlockPos& position,
        Rotation rotation, RandomSource& random);

    /** 洗牌 + 稳定降序（不修改源），返回新向量。
     *  RNG 消耗顺序与最终顺序与 Java {@code orderConnectors} 逐位一致；
     *  用指针置换避免插入排序期间对 std::string 成员的深层拷贝。 */
    static std::vector<Connector> orderConnectorsCopy(
        const std::vector<Connector>& src, RandomSource& random);

    BoundingBox boundingBox(const StructurePoolElement& element,
                            const BlockPos& position, Rotation rotation);

    static bool canAttach(const Connector& source, const Connector& target) {
        return source.front == directionOpposite(target.front)
            && (source.rollable || source.top == target.top)
            && source.target == target.name;
    }

    [[noreturn]] static void throwUnsupportedTerrainHeight(const char* reason) {
        throw std::runtime_error(std::string("试炼密室只使用 RIGID 投影，不应触发地形高度查询: ") + reason);
    }

    int64_t seed_;
    Config config_;
    PoolRegistry& pools_;
    StructureTemplateManager& templateManager_;

    SingleThreadedRandomSource randomSource_{0};
    WorldgenRandom layoutRandom_;

    int maxDepth_;
    bool useExpansionHack_;

    std::unordered_map<const StructurePoolElement*,
                       std::array<std::optional<ElementMetadata>, 4>> localMetadata_;
};

} // namespace trialfinder
