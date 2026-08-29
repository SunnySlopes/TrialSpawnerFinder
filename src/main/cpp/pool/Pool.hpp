#pragma once
// ============================================================
// TrialSpawnerFinder C++ 模板池
//
// 逐位复刻 cn.trialfinder.sim.pool（26.2 语义）：
//  StructurePoolElement / SinglePoolElement / ListPoolElement /
//  EmptyPoolElement / StructureTemplatePool / PoolRegistry
// 及 Projection。所有权用 shared_ptr（模板池元素、池均共享持有）。
// ============================================================

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "core/Util.hpp"
#include <utility>
#include <vector>

#include "core/Types.hpp"
#include "random/Random.hpp"
#include "template/StructureTemplate.hpp"

namespace trialfinder {

class StructureTemplateManager;
class Json;

// 前向声明：共享元数据缓存体（mutex + 元素键表）定义于 JigsawPlacement.hpp。
// 由资源持有者（JNI 引擎 / 测试 fixture）创建，生命周期与池对象一致，
// 供所有预测器实例共享 —— 复刻 Java TrialChamberPredictor.METADATA_CACHE。
struct SharedMetadata;

/** 复刻 {@code Projection}（26.2 语义）。试炼密室只用 RIGID。 */
enum class Projection : uint8_t { RIGID = 0, TERRAIN_MATCHING = 1 };

inline Projection projectionByName(const std::string& name) {
    if (name == "rigid") return Projection::RIGID;
    if (name == "terrain_matching") return Projection::TERRAIN_MATCHING;
    throw std::invalid_argument("未知 projection: " + name);
}

/** 补默认命名空间（对应 Identifier.withDefaultNamespace / ResourceKey.create）。 */
inline std::string withDefaultNamespace(const std::string& id) {
    if (id.find(':') != std::string::npos) {
        return id;
    }
    return "minecraft:" + id;
}

// ---------------- StructurePoolElement ----------------

/** 复刻 {@code StructurePoolElement}（26.2 语义）。模拟从不写入世界。 */
class StructurePoolElement {
public:
    virtual ~StructurePoolElement() = default;

    virtual BlockPos getSize(StructureTemplateManager& manager, Rotation rotation) const = 0;

    virtual std::vector<JigsawBlockInfo> getShuffledJigsawBlocks(
        StructureTemplateManager& manager, const BlockPos& pos,
        Rotation rotation, RandomSource& random) const = 0;

    virtual BoundingBox getBoundingBox(
        StructureTemplateManager& manager, const BlockPos& pos, Rotation rotation) const = 0;

    virtual int getGroundLevelDelta() const { return 1; }

    /** 递归收集可达的模板 id（用于预加载所有模板）。 */
    virtual void collectTemplateIds(std::function<void(const std::string&)> out) const {}

    Projection getProjection() const { return projection_; }

    StructurePoolElement& setProjection(Projection projection) {
        projection_ = projection;
        return *this;
    }

protected:
    explicit StructurePoolElement(Projection projection) : projection_(projection) {}

private:
    Projection projection_ = Projection::RIGID;
};

// ---------------- SinglePoolElement ----------------

/** 复刻 {@code SinglePoolElement}（26.2 语义）。 */
class SinglePoolElement : public StructurePoolElement {
public:
    SinglePoolElement(const std::string& templateId, Projection projection)
        : StructurePoolElement(projection), templateId_(templateId) {}

    const std::string& getTemplateLocation() const { return templateId_; }

    StructureTemplate& getTemplate(StructureTemplateManager& manager) const;

    BlockPos getSize(StructureTemplateManager& manager, Rotation rotation) const override;

    std::vector<JigsawBlockInfo> getShuffledJigsawBlocks(
        StructureTemplateManager& manager, const BlockPos& pos,
        Rotation rotation, RandomSource& random) const override;

    BoundingBox getBoundingBox(
        StructureTemplateManager& manager, const BlockPos& pos, Rotation rotation) const override;

    void collectTemplateIds(std::function<void(const std::string&)> out) const override;

private:
    std::string templateId_;
};

// ---------------- ListPoolElement ----------------

/** 复刻 {@code ListPoolElement}（26.2 语义）。 */
class ListPoolElement : public StructurePoolElement {
public:
    ListPoolElement(std::vector<std::shared_ptr<const StructurePoolElement>> elements,
                    Projection projection)
        : StructurePoolElement(projection), elements_(std::move(elements)) {
        if (elements_.empty()) {
            throw std::invalid_argument("elements 为空");
        }
        for (const auto& element : elements_) {
            const_cast<StructurePoolElement*>(element.get())->setProjection(projection);
        }
    }

    BlockPos getSize(StructureTemplateManager& manager, Rotation rotation) const override;

    std::vector<JigsawBlockInfo> getShuffledJigsawBlocks(
        StructureTemplateManager& manager, const BlockPos& pos,
        Rotation rotation, RandomSource& random) const override;

    BoundingBox getBoundingBox(
        StructureTemplateManager& manager, const BlockPos& pos, Rotation rotation) const override;

    void collectTemplateIds(std::function<void(const std::string&)> out) const override;

    const std::vector<std::shared_ptr<const StructurePoolElement>>& getElements() const {
        return elements_;
    }

private:
    std::vector<std::shared_ptr<const StructurePoolElement>> elements_;
};

// ---------------- EmptyPoolElement ----------------

/** 复刻 {@code EmptyPoolElement} 单例（26.2 语义）。 */
class EmptyPoolElement : public StructurePoolElement {
public:
    static const EmptyPoolElement& INSTANCE;

    /** 带 no-op 删除器的 shared_ptr（不转移所有权）。 */
    static std::shared_ptr<const EmptyPoolElement> sharedInstance();

    BlockPos getSize(StructureTemplateManager& manager, Rotation rotation) const override;
    std::vector<JigsawBlockInfo> getShuffledJigsawBlocks(
        StructureTemplateManager& manager, const BlockPos& pos,
        Rotation rotation, RandomSource& random) const override;
    BoundingBox getBoundingBox(
        StructureTemplateManager& manager, const BlockPos& pos, Rotation rotation) const override;

private:
    EmptyPoolElement();
};

// ---------------- StructureTemplatePool ----------------

/**
 * 复刻 {@code StructureTemplatePool}（26.2 语义）。
 * 模板在构造时按权重展开，与 vanilla 一致。
 */
class StructureTemplatePool {
public:
    static const std::string EMPTY_KEY;

    using WeightedElement = std::pair<std::shared_ptr<const StructurePoolElement>, int>;

    StructureTemplatePool(std::string fallbackId,
                          std::vector<WeightedElement> templates)
        : fallbackId_(std::move(fallbackId)), rawTemplates_(std::move(templates)) {
        for (const WeightedElement& pair : rawTemplates_) {
            const auto& element = pair.first;
            for (int i = 0; i < pair.second; i++) {
                templates_.push_back(element);
            }
        }
    }

    int getMaxSize(StructureTemplateManager& manager) const;

    /** 未解析的 fallback 池 id（完整 "namespace:path"）。 */
    const std::string& getFallbackId() const { return fallbackId_; }

    /** 解析后的 fallback 池；resolveFallbacks 之前为 nullptr。 */
    const StructureTemplatePool* getFallback() const { return fallback_; }
    void resolveFallback(const StructureTemplatePool* pool) { fallback_ = pool; }

    std::shared_ptr<const StructurePoolElement> getRandomTemplate(RandomSource& random) const {
        return templates_.empty()
            ? EmptyPoolElement::sharedInstance()
            : templates_[random.nextInt(static_cast<int>(templates_.size()))];
    }

    std::vector<std::shared_ptr<const StructurePoolElement>> getShuffledTemplates(
        RandomSource& random) const {
        return Util::shuffledCopy(templates_, random);
    }

    int size() const { return static_cast<int>(templates_.size()); }

    const std::vector<WeightedElement>& getTemplates() const { return rawTemplates_; }

private:
    std::string fallbackId_;
    const StructureTemplatePool* fallback_ = nullptr;
    std::vector<WeightedElement> rawTemplates_;
    std::vector<std::shared_ptr<const StructurePoolElement>> templates_;
    mutable int maxSize_ = INT32_MIN;
};

// ---------------- PoolRegistry ----------------

/**
 * 从 JSON 注册/加载结构模板池（复刻 {@code PoolRegistry}，26.2 语义）。
 * JNI 模式由 Java 侧传 pool JSON 文本；独立部署可 loadFromDirectory。
 */
class PoolRegistry {
public:
    explicit PoolRegistry(StructureTemplateManager& templateManager)
        : templateManager_(templateManager) {}

    /** 注册 {@code minecraft:empty} 哨兵池。 */
    void registerEmpty();

    /** 从 JSON 文本注册一个池。id 为完整 "namespace:path"。 */
    void registerPool(const std::string& id, const std::string& jsonText);

    /**
     * 独立部署：从资源目录加载全部池 JSON。
     * 等价 Java {@code PoolRegistry.loadAll()}，目录为 {@code <root>/data/<ns>/worldgen/template_pool/**\/.json}，
     * 相对路径（不含前缀）作为池 id。随后自动 resolveFallbacks。
     */
    void loadAllFromDirectory(const std::string& root);

    StructureTemplatePool& getOrThrow(const std::string& key) {
        auto it = pools_.find(withDefaultNamespace(key));
        if (it == pools_.end()) {
            throw std::runtime_error("池未注册: " + withDefaultNamespace(key));
        }
        return *it->second;
    }

    const StructureTemplatePool* get(const std::string& key) const {
        auto it = pools_.find(withDefaultNamespace(key));
        return it == pools_.end() ? nullptr : it->second.get();
    }

    const StructureTemplatePool* emptyPool() const { return emptyPool_; }

    bool isEmptyPool(const StructurePoolElement* element) const {
        return element == &EmptyPoolElement::INSTANCE;
    }

    bool isEmptyPool(const StructureTemplatePool& pool) const { return &pool == emptyPool_; }

    /** 把各池的 fallback id 解析为池指针，缺失时回退空池。 */
    void resolveFallbacks();

    StructureTemplateManager& templateManager() { return templateManager_; }

    /** 挂接共享元数据缓存（跨预测器共享；可空，空则退化为每实例本地缓存）。 */
    void setSharedMetadata(std::shared_ptr<SharedMetadata> cache) {
        sharedMetadata_ = std::move(cache);
    }

    const std::shared_ptr<SharedMetadata>& sharedMetadata() const { return sharedMetadata_; }

private:
    std::shared_ptr<const StructurePoolElement> parseElement(const Json& element);

    StructureTemplateManager& templateManager_;
    std::shared_ptr<SharedMetadata> sharedMetadata_;
    std::unordered_map<std::string, std::unique_ptr<StructureTemplatePool>> pools_;
    const StructureTemplatePool* emptyPool_ = nullptr;
};

} // namespace trialfinder
