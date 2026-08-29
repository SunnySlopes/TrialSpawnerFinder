#include "jigsaw/JigsawPlacement.hpp"

#include <algorithm>
#include <stdexcept>
#include <typeinfo>

#include "core/Util.hpp"

namespace trialfinder {

namespace {

inline BlockPos relative(const BlockPos& pos, Direction d) {
    return pos.offset(directionStepX(d), directionStepY(d), directionStepZ(d));
}

} // namespace

// ---------------- 对外入口 ----------------

JigsawPlacement::Prediction JigsawPlacement::predict(int x, int z) {
    StartContext context;
    context.random = &layoutRandom_;
    {
        int chunkX = Mth::floorDiv(x, 16);
        int chunkZ = Mth::floorDiv(z, 16);
        WorldgenRandom& random = *context.random;
        random.setLargeFeatureSeed(seed_, chunkX, chunkZ);
        // Java 的 WorldgenRandom 即 RandomSource；C++ 是包装类，位序列取自底层 RandomSource。
        int startY = config_.sampleStartHeight(random.randomSource());
        BlockPos start(chunkX << 4, startY, chunkZ << 4);
        context.aliases = PoolAliasLookup::create(config_.poolAliases, start, seed_);
        context.layout = createStart(start, random, context.aliases);
    }

    Prediction prediction;
    prediction.x = x;
    prediction.z = z;
    if (!context.layout) {
        return prediction;
    }
    StartLayout layout = *context.layout;

    std::vector<LightPiece> pieces;
    pieces.push_back(layout.piece);
    if (maxDepth_ > 0) {
        generateChildren(layout, pieces, context.random->randomSource(), context.aliases);
    }

    std::vector<SpawnerPoint> spawners;
    std::vector<SpawnerInfo> infos;
    std::vector<VaultInfo> vaults;
    for (LightPiece& piece : pieces) {
        collectSpawners(piece, spawners, infos);
        collectVaults(piece, vaults);
    }

    // 去重（对应 Java HashSet）+ 排序（对应 Comparator）
    auto xyzSpawnerCmp = [](const SpawnerPoint& a, const SpawnerPoint& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    };
    auto spawnerEq = [](const SpawnerPoint& a, const SpawnerPoint& b) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    };
    std::sort(spawners.begin(), spawners.end(), xyzSpawnerCmp);
    spawners.erase(std::unique(spawners.begin(), spawners.end(), spawnerEq), spawners.end());

    auto infoFullCmp = [](const SpawnerInfo& a, const SpawnerInfo& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        if (a.z != b.z) return a.z < b.z;
        if (a.normalConfig != b.normalConfig) return a.normalConfig < b.normalConfig;
        return a.ominousConfig < b.ominousConfig;
    };
    auto infoFullEq = [](const SpawnerInfo& a, const SpawnerInfo& b) {
        return a.x == b.x && a.y == b.y && a.z == b.z
            && a.normalConfig == b.normalConfig && a.ominousConfig == b.ominousConfig;
    };
    auto infoXyzCmp = [](const SpawnerInfo& a, const SpawnerInfo& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    };
    std::sort(infos.begin(), infos.end(), infoFullCmp);
    infos.erase(std::unique(infos.begin(), infos.end(), infoFullEq), infos.end());
    std::stable_sort(infos.begin(), infos.end(), infoXyzCmp);

    auto vaultFullCmp = [](const VaultInfo& a, const VaultInfo& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        if (a.z != b.z) return a.z < b.z;
        return a.ominous < b.ominous;
    };
    auto vaultFullEq = [](const VaultInfo& a, const VaultInfo& b) {
        return a.x == b.x && a.y == b.y && a.z == b.z && a.ominous == b.ominous;
    };
    auto vaultXyzCmp = [](const VaultInfo& a, const VaultInfo& b) {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    };
    std::sort(vaults.begin(), vaults.end(), vaultFullCmp);
    vaults.erase(std::unique(vaults.begin(), vaults.end(), vaultFullEq), vaults.end());
    std::stable_sort(vaults.begin(), vaults.end(), vaultXyzCmp);

    prediction.spawners = std::move(spawners);
    prediction.spawnerInfos = std::move(infos);
    prediction.vaults = std::move(vaults);
    prediction.exists = !prediction.spawners.empty();
    return prediction;
}

// ---------------- createStart ----------------

std::optional<JigsawPlacement::StartLayout> JigsawPlacement::createStart(
    const BlockPos& start, WorldgenRandom& random, PoolAliasLookup& aliases) {
    Rotation rotation = rotationGetRandom(random.randomSource());
    std::string resolvedKey = aliases.lookup(config_.startPool);
    StructureTemplatePool& startPool = pools_.getOrThrow(resolvedKey);
    std::shared_ptr<const StructurePoolElement> element =
        startPool.getRandomTemplate(random.randomSource());
    if (element.get() == &EmptyPoolElement::INSTANCE) {
        return std::nullopt;
    }

    BlockPos anchoredPosition = start;
    if (!config_.startJigsawName.empty()) {
        std::optional<BlockPos> found = findStartingJigsaw(
            *element, config_.startJigsawName, start, rotation, random.randomSource());
        if (!found) {
            return std::nullopt;
        }
        anchoredPosition = *found;
    }

    BlockPos localAnchor = anchoredPosition.subtract(start);
    BlockPos piecePosition = start.subtract(localAnchor);
    BoundingBox box = boundingBox(*element, piecePosition, rotation);
    int centerX = (box.maxX + box.minX) / 2;
    int centerZ = (box.maxZ + box.minZ) / 2;
    int groundY;
    if (config_.projectStartToHeightmap) {
        throwUnsupportedTerrainHeight("projectStartToHeightmap");
    }
    groundY = piecePosition.y;
    int verticalOffset = groundY - (box.minY + element->getGroundLevelDelta());
    piecePosition = piecePosition.offset(0, verticalOffset, 0);
    box = box.moved(0, verticalOffset, 0);

    DimensionPadding padding = config_.dimensionPadding;
    if (!padding.isZero()
        && (box.minY < config_.minY + padding.bottom
            || box.maxY > config_.maxY - padding.top)) {
        return std::nullopt;
    }

    LightPiece piece{element.get(), piecePosition,
                     element->getGroundLevelDelta(), rotation, box};
    return StartLayout{piece, BlockPos(centerX, groundY + localAnchor.y, centerZ)};
}

std::optional<BlockPos> JigsawPlacement::findStartingJigsaw(
    const StructurePoolElement& element, const std::string& name,
    const BlockPos& start, Rotation rotation, RandomSource& random) {
    std::vector<Connector> connectors = fillConnectors(element, start, rotation, random);
    for (const Connector& connector : connectors) {
        if (name == connector.name) {
            return start.offset(connector.relativePos);
        }
    }
    return std::nullopt;
}

// ---------------- generateChildren / generatePiece ----------------

void JigsawPlacement::generateChildren(
    StartLayout& start, std::vector<LightPiece>& pieces,
    RandomSource& random, PoolAliasLookup& aliases) {
    LightPiece& first = start.piece;
    BlockPos center = start.biomePosition;
    MaxDistance distance = config_.maxDistance;
    DimensionPadding padding = config_.dimensionPadding;
    int minY = config_.minY;
    int maxY = config_.maxY;
    int allowedMinX = center.x - distance.horizontal;
    int allowedMinY = std::max(center.y - distance.vertical, minY + padding.bottom);
    int allowedMinZ = center.z - distance.horizontal;
    int allowedMaxXExclusive = center.x + distance.horizontal + 1;
    int allowedMaxYExclusive = std::min(center.y + distance.vertical + 1,
                                        maxY + 1 - padding.top);
    int allowedMaxZExclusive = center.z + distance.horizontal + 1;
    std::shared_ptr<FreeSpace> shape = std::make_shared<FreeSpace>(
        allowedMinX, allowedMinY, allowedMinZ,
        allowedMaxXExclusive, allowedMaxYExclusive, allowedMaxZExclusive,
        &first.box);
    SequencedPriorityIterator<QueuedPiece> queue;
    generatePiece(first, shape, 0, pieces, queue, random, aliases);
    while (queue.hasNext()) {
        QueuedPiece next = queue.next();
        generatePiece(next.piece, next.shape, next.depth,
                      pieces, queue, random, aliases);
    }
}

void JigsawPlacement::generatePiece(
    const LightPiece& source, const std::shared_ptr<FreeSpace>& contextFree, int depth,
    std::vector<LightPiece>& pieces, SequencedPriorityIterator<QueuedPiece>& queue,
    RandomSource& random, PoolAliasLookup& aliases) {
    bool sourceRigid = source.element->getProjection() == Projection::RIGID;
    std::shared_ptr<FreeSpace> sourceFree;
    int sourceBoxY = source.box.minY;

    std::vector<Connector> sourceConnectors =
        fillConnectors(*source.element, source.position, source.rotation, random);
    for (std::size_t sourceIndex = 0; sourceIndex < sourceConnectors.size(); sourceIndex++) {
        const Connector& sourceConnector = sourceConnectors[sourceIndex];
        Direction sourceDirection = sourceConnector.front;
        BlockPos sourceJigsawPos = source.position.offset(sourceConnector.relativePos);
        BlockPos targetJigsawPos = relative(sourceJigsawPos, sourceDirection);
        int sourceJigsawLocalY = sourceJigsawPos.y - sourceBoxY;

        std::string poolKey = aliases.lookup(sourceConnector.pool);
        const StructureTemplatePool* selectedEntry = pools_.get(poolKey);
        if (selectedEntry == nullptr) {
            continue;
        }
        const StructureTemplatePool& selectedPool = *selectedEntry;
        const StructureTemplatePool* fallbackPool = selectedPool.getFallback();
        if (fallbackPool == nullptr) {
            fallbackPool = pools_.emptyPool();
        }

        bool attachInsideSource = source.box.isInside(targetJigsawPos);
        std::shared_ptr<FreeSpace> childrenFree;
        if (attachInsideSource) {
            if (!sourceFree) {
                const BoundingBox& box = source.box;
                sourceFree = std::make_shared<FreeSpace>(
                    box.minX, box.minY, box.minZ,
                    box.maxX + 1, box.maxY + 1, box.maxZ + 1, nullptr);
            }
            childrenFree = sourceFree;
        } else {
            childrenFree = contextFree;
        }

        std::vector<std::shared_ptr<const StructurePoolElement>> targetElements;
        if (depth != maxDepth_) {
            std::vector<std::shared_ptr<const StructurePoolElement>> selected =
                selectedPool.getShuffledTemplates(random);
            targetElements.insert(targetElements.end(), selected.begin(), selected.end());
        }
        std::vector<std::shared_ptr<const StructurePoolElement>> fallback =
            fallbackPool->getShuffledTemplates(random);
        targetElements.insert(targetElements.end(), fallback.begin(), fallback.end());
        int placementPriority = sourceConnector.placementPriority;

        for (const auto& targetElement : targetElements) {
            if (targetElement.get() == &EmptyPoolElement::INSTANCE) {
                break;
            }
            for (Rotation targetRotation : rotationGetShuffled(random)) {
                std::vector<Connector> targetConnectors = fillConnectors(
                    *targetElement, BlockPos::ZERO, targetRotation, random);
                BoundingBox hackBox = boundingBox(
                    *targetElement, BlockPos::ZERO, targetRotation);
                int expandTo = useExpansionHack_ && hackBox.getYSpan() <= 16
                    ? expansionSize(targetConnectors, hackBox, aliases)
                    : 0;

                for (std::size_t targetIndex = 0;
                     targetIndex < targetConnectors.size(); targetIndex++) {
                    const Connector& targetConnector = targetConnectors[targetIndex];
                    if (!canAttach(sourceConnector, targetConnector)) {
                        continue;
                    }
                    BlockPos targetJigsawLocalPos = targetConnector.relativePos;
                    BlockPos rawTargetPosition = targetJigsawPos.subtract(targetJigsawLocalPos);
                    BoundingBox rawTargetBox = boundingBox(
                        *targetElement, rawTargetPosition, targetRotation);
                    int rawTargetY = rawTargetBox.minY;
                    bool targetRigid = targetElement->getProjection() == Projection::RIGID;
                    int targetJigsawLocalY = targetJigsawLocalPos.y;
                    int deltaY = sourceJigsawLocalY - targetJigsawLocalY
                        + directionStepY(sourceDirection);
                    int targetBoxY;
                    if (sourceRigid && targetRigid) {
                        targetBoxY = sourceBoxY + deltaY;
                    } else {
                        throwUnsupportedTerrainHeight("非 RIGID piece 拼接");
                    }

                    int verticalOffset = targetBoxY - rawTargetY;
                    BoundingBox targetBox = rawTargetBox.moved(0, verticalOffset, 0);
                    BlockPos targetPosition = rawTargetPosition.offset(0, verticalOffset, 0);
                    if (expandTo > 0) {
                        int newSize = std::max(
                            expandTo + 1, targetBox.maxY - targetBox.minY);
                        targetBox = targetBox.encapsulate(BlockPos(
                            targetBox.minX, targetBox.minY + newSize, targetBox.minZ));
                    }
                    if (!childrenFree->contains(targetBox)) {
                        continue;
                    }

                    childrenFree->occupy(targetBox);
                    int targetGroundDelta = targetRigid
                        ? source.groundLevelDelta - deltaY
                        : targetElement->getGroundLevelDelta();
                    LightPiece target{targetElement.get(), targetPosition,
                                      targetGroundDelta, targetRotation, targetBox};
                    pieces.push_back(target);
                    if (depth + 1 <= maxDepth_) {
                        queue.add(QueuedPiece{target, childrenFree, depth + 1},
                                  placementPriority);
                    }
                    goto nextSourceConnector;
                }
            }
        }
    nextSourceConnector:;
    }
}

int JigsawPlacement::expansionSize(
    const std::vector<Connector>& connectors, const BoundingBox& box,
    PoolAliasLookup& aliases) {
    int largest = 0;
    for (const Connector& connector : connectors) {
        if (!box.isInside(relative(connector.relativePos, connector.front))) {
            continue;
        }
        std::string key = aliases.lookup(connector.pool);
        const StructureTemplatePool* pool = pools_.get(key);
        if (pool == nullptr) {
            continue;
        }
        largest = std::max(largest, pool->getMaxSize(templateManager_));
        largest = std::max(largest, pool->getFallback()->getMaxSize(templateManager_));
    }
    return largest;
}

// ---------------- 收集 ----------------

void JigsawPlacement::collectSpawners(
    LightPiece& piece, std::vector<SpawnerPoint>& spawners,
    std::vector<SpawnerInfo>& infos) {
    ElementMetadata& md = metadata(*piece.element, piece.rotation);
    if (md.supported) {
        for (const RelativeSpawner& relative_ : md.spawners) {
            SpawnerPoint point{piece.position.x + relative_.pos.x,
                               piece.position.y + relative_.pos.y,
                               piece.position.z + relative_.pos.z};
            spawners.push_back(point);
            infos.push_back(SpawnerInfo{point.x, point.y, point.z,
                                        relative_.normalConfig, relative_.ominousConfig});
        }
        return;
    }
    collectSpawnersSlow(*piece.element, piece.position, piece.rotation, spawners, infos);
}

void JigsawPlacement::collectSpawnersSlow(
    const StructurePoolElement& element, const BlockPos& position, Rotation rotation,
    std::vector<SpawnerPoint>& spawners, std::vector<SpawnerInfo>& infos) {
    if (const ListPoolElement* list = dynamic_cast<const ListPoolElement*>(&element)) {
        for (const auto& child : list->getElements()) {
            collectSpawnersSlow(*child, position, rotation, spawners, infos);
        }
        return;
    }
    const SinglePoolElement* single = dynamic_cast<const SinglePoolElement*>(&element);
    if (single == nullptr) {
        return;
    }
    StructureTemplate& template_ = templateManager_.getOrCreate(single->getTemplateLocation());
    StructurePlaceSettings placement;
    placement.setRotation(rotation);
    std::vector<StructureBlockInfo> blocks =
        template_.filterBlocks(position, placement, "minecraft:trial_spawner");
    for (const StructureBlockInfo& block : blocks) {
        SpawnerPoint point{block.pos.x, block.pos.y, block.pos.z};
        spawners.push_back(point);
        infos.push_back(SpawnerInfo{point.x, point.y, point.z,
                                    block.normalConfig, block.ominousConfig});
    }
}

void JigsawPlacement::collectVaults(LightPiece& piece, std::vector<VaultInfo>& vaults) {
    const StructurePoolElement& element = *piece.element;
    if (const ListPoolElement* list = dynamic_cast<const ListPoolElement*>(&element)) {
        for (const auto& child : list->getElements()) {
            collectVaultsElement(*child, piece, vaults);
        }
        return;
    }
    collectVaultsElement(element, piece, vaults);
}

void JigsawPlacement::collectVaultsElement(
    const StructurePoolElement& element, const LightPiece& piece,
    std::vector<VaultInfo>& vaults) {
    const SinglePoolElement* single = dynamic_cast<const SinglePoolElement*>(&element);
    if (single == nullptr) {
        return;
    }
    StructureTemplate& template_ = templateManager_.getOrCreate(single->getTemplateLocation());
    StructurePlaceSettings placement;
    placement.setRotation(piece.rotation);
    bool ominous = single->getTemplateLocation().find("ominous_vault") != std::string::npos;
    std::vector<StructureBlockInfo> blocks =
        template_.filterBlocks(piece.position, placement, "minecraft:vault");
    for (const StructureBlockInfo& block : blocks) {
        vaults.push_back(VaultInfo{block.pos.x, block.pos.y, block.pos.z, ominous});
    }
}

// ---------------- 元数据 ----------------

ElementMetadata& JigsawPlacement::metadata(
    const StructurePoolElement& element, Rotation rotation) {
    if (&element == &EmptyPoolElement::INSTANCE) {
        static ElementMetadata unsupported = ElementMetadata::unsupported();
        return unsupported;
    }
    std::array<std::optional<ElementMetadata>, 4>& variants = localMetadata_[&element];
    int index = static_cast<int>(rotation);
    if (variants[index]) {
        return *variants[index];
    }
    // 本地未命中：查共享缓存（复刻 Java localMetadata → METADATA_CACHE 两级缓存）。
    // createMetadata 递归调用 metadata()，故用 recursive_mutex。
    if (const std::shared_ptr<SharedMetadata>& shared = pools_.sharedMetadata()) {
        std::lock_guard<std::recursive_mutex> lock(shared->mutex);
        std::array<std::optional<ElementMetadata>, 4>& global = shared->byElement[&element];
        if (global[index]) {
            variants[index] = global[index];  // 拷贝进本地，后续无锁命中
        } else {
            global[index] = createMetadata(element, rotation);
            variants[index] = global[index];
        }
        return *variants[index];
    }
    if (!variants[index]) {
        variants[index] = createMetadata(element, rotation);
    }
    return *variants[index];
}

ElementMetadata JigsawPlacement::createMetadata(
    const StructurePoolElement& element, Rotation rotation) {
    if (const ListPoolElement* list = dynamic_cast<const ListPoolElement*>(&element)) {
        const auto& children = list->getElements();
        if (children.empty()) {
            return ElementMetadata::unsupported();
        }
        ElementMetadata first = metadata(*children[0], rotation);
        if (!first.supported) {
            return ElementMetadata::unsupported();
        }
        std::vector<RelativeSpawner> spawners;
        for (const auto& child : children) {
            ElementMetadata childMetadata = metadata(*child, rotation);
            if (!childMetadata.supported) {
                return ElementMetadata::unsupported();
            }
            spawners.insert(spawners.end(),
                            childMetadata.spawners.begin(), childMetadata.spawners.end());
        }
        ElementMetadata result;
        result.connectors = std::move(first.connectors);
        result.boundingBox = element.getBoundingBox(templateManager_, BlockPos::ZERO, rotation);
        result.spawners = std::move(spawners);
        result.supported = true;
        return result;
    }
    if (const SinglePoolElement* single = dynamic_cast<const SinglePoolElement*>(&element)) {
        StructureTemplate& template_ =
            templateManager_.getOrCreate(single->getTemplateLocation());
        std::vector<RelativeSpawner> spawners = template_.getSpawnerBlocks(rotation);
        ElementMetadata result;
        result.connectors = toConnectors(
            template_.getJigsaws(BlockPos::ZERO, rotation), BlockPos::ZERO);
        result.boundingBox = element.getBoundingBox(templateManager_, BlockPos::ZERO, rotation);
        result.spawners = std::move(spawners);
        result.supported = true;
        return result;
    }
    return ElementMetadata::unsupported();
}

std::vector<Connector> JigsawPlacement::toConnectors(
    const std::vector<JigsawBlockInfo>& infos, const BlockPos& origin) {
    std::vector<Connector> connectors;
    connectors.reserve(infos.size());
    for (const JigsawBlockInfo& jigsaw : infos) {
        const StructureBlockInfo& info = jigsaw.info;
        connectors.push_back(Connector{
            info.pos.subtract(origin),
            JigsawBlock::getFrontFacing(info.state),
            JigsawBlock::getTopFacing(info.state),
            jigsaw.pool, jigsaw.name, jigsaw.target,
            jigsaw.jointType == JointType::ROLLABLE,
            jigsaw.selectionPriority, jigsaw.placementPriority});
    }
    return connectors;
}

std::vector<Connector> JigsawPlacement::fillConnectors(
    const StructurePoolElement& element, const BlockPos& position,
    Rotation rotation, RandomSource& random) {
    ElementMetadata& md = metadata(element, rotation);
    if (!md.supported) {
        std::vector<JigsawBlockInfo> blocks = element.getShuffledJigsawBlocks(
            templateManager_, position, rotation, random);
        return toConnectors(blocks, position);
    }
    // 直接从缓存的未洗牌 connectors 生成洗牌+排序结果（只拷贝一次）。
    return orderConnectorsCopy(md.connectors, random);
}

void JigsawPlacement::orderConnectors(
    std::vector<Connector>& connectors, RandomSource& random) {
    connectors = orderConnectorsCopy(connectors, random);
}

std::vector<Connector> JigsawPlacement::orderConnectorsCopy(
    const std::vector<Connector>& src, RandomSource& random) {
    // 指针置换版：Fisher-Yates 洗牌 + 稳定降序。RNG 消耗（nextInt(remaining)）
    // 与 Java 逐位一致；插入排序只移动指针，最终再按序拷贝一次，
    // 避免原地移动 Connector 时对 pool/name/target 三个 std::string 的深层拷贝。
    const std::size_t n = src.size();
    std::vector<const Connector*> order(n);
    for (std::size_t i = 0; i < n; i++) {
        order[i] = &src[i];
    }
    for (int remaining = static_cast<int>(n); remaining > 1; remaining--) {
        int selected = random.nextInt(remaining);
        std::swap(order[remaining - 1], order[selected]);
    }
    for (int index = 1; index < static_cast<int>(n); index++) {
        const Connector* current = order[index];
        int insertion = index;
        while (insertion > 0
               && order[insertion - 1]->selectionPriority < current->selectionPriority) {
            order[insertion] = order[insertion - 1];
            insertion--;
        }
        order[insertion] = current;
    }
    std::vector<Connector> out;
    out.reserve(n);
    for (const Connector* c : order) {
        out.push_back(*c);
    }
    return out;
}

BoundingBox JigsawPlacement::boundingBox(
    const StructurePoolElement& element, const BlockPos& position, Rotation rotation) {
    ElementMetadata& md = metadata(element, rotation);
    if (!md.supported) {
        return element.getBoundingBox(templateManager_, position, rotation);
    }
    return md.boundingBox.moved(position.x, position.y, position.z);
}

} // namespace trialfinder
