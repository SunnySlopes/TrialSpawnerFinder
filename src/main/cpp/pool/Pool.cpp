#include "pool/Pool.hpp"

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <stdexcept>

#include "core/Util.hpp"
#include "pool/JsonParser.hpp"
#include "template/StructureTemplateManager.hpp"

namespace trialfinder {

// ---------------- SinglePoolElement ----------------

StructureTemplate& SinglePoolElement::getTemplate(StructureTemplateManager& manager) const {
    return manager.getOrCreate(templateId_);
}

BlockPos SinglePoolElement::getSize(
    StructureTemplateManager& manager, Rotation rotation) const {
    return getTemplate(manager).getSize(rotation);
}

std::vector<JigsawBlockInfo> SinglePoolElement::getShuffledJigsawBlocks(
    StructureTemplateManager& manager, const BlockPos& pos,
    Rotation rotation, RandomSource& random) const {
    std::vector<JigsawBlockInfo> list = getTemplate(manager).getJigsaws(pos, rotation);
    Util::shuffle(list, random);
    // 按选择优先级降序稳定排序（对应 Java HIGHEST_SELECTION_PRIORITY_FIRST）
    std::stable_sort(list.begin(), list.end(), [](const JigsawBlockInfo& a, const JigsawBlockInfo& b) {
        return a.selectionPriority > b.selectionPriority;
    });
    return list;
}

BoundingBox SinglePoolElement::getBoundingBox(
    StructureTemplateManager& manager, const BlockPos& pos, Rotation rotation) const {
    StructureTemplate& template_ = getTemplate(manager);
    StructurePlaceSettings settings;
    settings.setRotation(rotation);
    return template_.getBoundingBox(settings, pos);
}

void SinglePoolElement::collectTemplateIds(
    std::function<void(const std::string&)> out) const {
    out(templateId_);
}

// ---------------- ListPoolElement ----------------

BlockPos ListPoolElement::getSize(
    StructureTemplateManager& manager, Rotation rotation) const {
    int x = 0;
    int y = 0;
    int z = 0;
    for (const auto& element : elements_) {
        BlockPos size = element->getSize(manager, rotation);
        x = std::max(x, size.x);
        y = std::max(y, size.y);
        z = std::max(z, size.z);
    }
    return BlockPos(x, y, z);
}

std::vector<JigsawBlockInfo> ListPoolElement::getShuffledJigsawBlocks(
    StructureTemplateManager& manager, const BlockPos& pos,
    Rotation rotation, RandomSource& random) const {
    return elements_[0]->getShuffledJigsawBlocks(manager, pos, rotation, random);
}

BoundingBox ListPoolElement::getBoundingBox(
    StructureTemplateManager& manager, const BlockPos& pos, Rotation rotation) const {
    bool hasResult = false;
    BoundingBox result;
    for (const auto& element : elements_) {
        if (element.get() == &EmptyPoolElement::INSTANCE) {
            continue;
        }
        BoundingBox box = element->getBoundingBox(manager, pos, rotation);
        if (!hasResult) {
            result = box;
            hasResult = true;
        } else {
            result = result.encapsulate(BlockPos(box.maxX, box.maxY, box.maxZ));
        }
    }
    if (!hasResult) {
        throw std::runtime_error("无法计算 ListPoolElement 的包围盒");
    }
    return result;
}

void ListPoolElement::collectTemplateIds(
    std::function<void(const std::string&)> out) const {
    for (const auto& element : elements_) {
        element->collectTemplateIds(out);
    }
}

// ---------------- EmptyPoolElement ----------------

const EmptyPoolElement& EmptyPoolElement::INSTANCE = EmptyPoolElement();

EmptyPoolElement::EmptyPoolElement()
    : StructurePoolElement(Projection::TERRAIN_MATCHING) {}

std::shared_ptr<const EmptyPoolElement> EmptyPoolElement::sharedInstance() {
    static std::shared_ptr<const EmptyPoolElement> instance(
        &INSTANCE, [](const EmptyPoolElement*) {});
    return instance;
}

BlockPos EmptyPoolElement::getSize(
    StructureTemplateManager& /*manager*/, Rotation /*rotation*/) const {
    return BlockPos::ZERO;
}

std::vector<JigsawBlockInfo> EmptyPoolElement::getShuffledJigsawBlocks(
    StructureTemplateManager& /*manager*/, const BlockPos& /*pos*/,
    Rotation /*rotation*/, RandomSource& /*random*/) const {
    return {};
}

BoundingBox EmptyPoolElement::getBoundingBox(
    StructureTemplateManager& /*manager*/, const BlockPos& /*pos*/,
    Rotation /*rotation*/) const {
    throw std::runtime_error("EmptyPoolElement.getBoundingBox 无效调用，请先过滤");
}

// ---------------- StructureTemplatePool ----------------

const std::string StructureTemplatePool::EMPTY_KEY = "minecraft:empty";

int StructureTemplatePool::getMaxSize(StructureTemplateManager& manager) const {
    if (maxSize_ == INT32_MIN) {
        int maxSize = 0;
        for (const WeightedElement& pair : rawTemplates_) {
            const auto& element = pair.first;
            if (element.get() == &EmptyPoolElement::INSTANCE) {
                continue;
            }
            int span = element->getBoundingBox(manager, BlockPos::ZERO, Rotation::NONE).getYSpan();
            maxSize = std::max(maxSize, span);
        }
        maxSize_ = maxSize;
    }
    return maxSize_;
}

// ---------------- PoolRegistry ----------------

void PoolRegistry::registerEmpty() {
    auto empty = std::make_unique<StructureTemplatePool>(
        StructureTemplatePool::EMPTY_KEY, std::vector<StructureTemplatePool::WeightedElement>{});
    empty->resolveFallback(empty.get());
    emptyPool_ = empty.get();
    pools_[StructureTemplatePool::EMPTY_KEY] = std::move(empty);
}

void PoolRegistry::registerPool(const std::string& id, const std::string& jsonText) {
    std::string key = withDefaultNamespace(id);
    if (pools_.find(key) != pools_.end()) {
        return;
    }
    Json root = Json::parse(jsonText);
    if (root.type != Json::Type::OBJECT) {
        throw std::runtime_error("pool JSON 根必须是对象: " + key);
    }
    const Json* elements = root.getArray("elements");
    std::vector<StructureTemplatePool::WeightedElement> templates;
    if (elements != nullptr) {
        templates.reserve(elements->size());
        for (const Json& elementEntry : elements->array) {
            int weight = elementEntry.getInt("weight");
            const Json* element = elementEntry.get("element");
            if (element == nullptr) {
                throw std::runtime_error("pool 元素缺少 element: " + key);
            }
            templates.emplace_back(parseElement(*element), weight);
        }
    }
    std::string fallbackId = root.getString("fallback");
    auto pool = std::make_unique<StructureTemplatePool>(
        withDefaultNamespace(fallbackId), std::move(templates));
    pools_[key] = std::move(pool);
}

std::shared_ptr<const StructurePoolElement> PoolRegistry::parseElement(const Json& element) {
    std::string type = element.getString("element_type");
    if (type == "minecraft:empty_pool_element") {
        return EmptyPoolElement::sharedInstance();
    }
    if (type == "minecraft:single_pool_element") {
        std::string location = element.getString("location");
        Projection projection = projectionByName(element.getString("projection"));
        return std::make_shared<SinglePoolElement>(
            withDefaultNamespace(location), projection);
    }
    if (type == "minecraft:list_pool_element") {
        const Json* subElements = element.getArray("elements");
        if (subElements == nullptr) {
            throw std::runtime_error("list_pool_element 缺少 elements");
        }
        std::vector<std::shared_ptr<const StructurePoolElement>> children;
        children.reserve(subElements->size());
        for (const Json& sub : subElements->array) {
            children.push_back(parseElement(sub));
        }
        Projection projection = projectionByName(element.getString("projection"));
        return std::make_shared<ListPoolElement>(std::move(children), projection);
    }
    throw std::runtime_error("不支持的 pool 元素类型: " + type);
}

void PoolRegistry::loadAllFromDirectory(const std::string& root) {
    registerEmpty();
    std::string poolDir = root + "/data/minecraft/worldgen/template_pool";
    std::error_code ec;
    if (!std::filesystem::is_directory(poolDir, ec)) {
        throw std::runtime_error("template_pool 目录不存在: " + poolDir);
    }
    std::filesystem::recursive_directory_iterator it(poolDir, ec);
    const std::filesystem::recursive_directory_iterator end;
    std::vector<std::string> files;
    for (; it != end; it.increment(ec)) {
        if (ec) {
            throw std::runtime_error("遍历 template_pool 失败: " + ec.message());
        }
        const std::filesystem::path& path = it->path();
        if (it->is_regular_file() && path.extension() == ".json") {
            files.push_back(path.string());
        }
    }
    // Java 遍历 classpath 顺序不确定，但注册顺序不影响位一致性；
    // 为可复现性按文件名排序。
    std::sort(files.begin(), files.end());

    const std::string prefix = poolDir + "/";
    for (const std::string& file : files) {
        std::string id = file.substr(prefix.size());
        id = id.substr(0, id.size() - 5);  // 去掉 ".json" 后缀
        // Windows 反斜杠统一为正斜杠（Java 资源路径恒为 '/'）
        std::replace(id.begin(), id.end(), '\\', '/');
        std::ifstream stream(file, std::ios::binary);
        if (!stream) {
            continue;
        }
        std::ostringstream buffer;
        buffer << stream.rdbuf();
        try {
            registerPool(id, buffer.str());
        } catch (const std::exception&) {
            // 跳过损坏的 pool（对齐 Java loadFromClasspath 的 catch）
        }
    }
    resolveFallbacks();
}

void PoolRegistry::resolveFallbacks() {
    for (auto& entry : pools_) {
        StructureTemplatePool& pool = *entry.second;
        const std::string& fallbackId = pool.getFallbackId();
        const StructureTemplatePool* target = nullptr;
        if (!fallbackId.empty()) {
            auto it = pools_.find(fallbackId);
            if (it != pools_.end()) {
                target = it->second.get();
            }
        }
        pool.resolveFallback(target != nullptr ? target : emptyPool_);
    }
}

} // namespace trialfinder
