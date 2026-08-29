#include "predictor/HandTrialChamberPredictor.hpp"

#include <stdexcept>
#include <utility>

#include "pool/JsonParser.hpp"

namespace trialfinder {

namespace {

PoolAliasBinding parseAlias(const Json& alias) {
    std::string type = alias.getString("type");
    if (type == "minecraft:direct") {
        return PoolAliasBinding::direct(
            alias.getString("alias"), alias.getString("target"));
    }
    if (type == "minecraft:random") {
        std::string aliasId = alias.getString("alias");
        const Json* targets = alias.getArray("targets");
        std::vector<std::pair<std::string, int>> items;
        if (targets != nullptr) {
            items.reserve(targets->size());
            for (const Json& target : targets->array) {
                items.emplace_back(target.getString("data"), target.getInt("weight"));
            }
        }
        return PoolAliasBinding::random(aliasId, std::move(items));
    }
    if (type == "minecraft:random_group") {
        const Json* groupsJson = alias.getArray("groups");
        std::vector<std::vector<PoolAliasBinding>> groups;
        std::vector<int> weights;
        if (groupsJson != nullptr) {
            groups.reserve(groupsJson->size());
            weights.reserve(groupsJson->size());
            for (const Json& groupJson : groupsJson->array) {
                const Json* data = groupJson.getArray("data");
                std::vector<PoolAliasBinding> group;
                if (data != nullptr) {
                    group.reserve(data->size());
                    for (const Json& bindingJson : data->array) {
                        group.push_back(parseAlias(bindingJson));
                    }
                }
                groups.push_back(std::move(group));
                weights.push_back(groupJson.getInt("weight"));
            }
        }
        return PoolAliasBinding::randomGroup(std::move(groups), std::move(weights));
    }
    throw std::runtime_error("未知别名类型: " + type);
}

} // namespace

std::vector<PoolAliasBinding> parseTrialChambersAliases(const std::string& jsonText) {
    Json root = Json::parse(jsonText);
    const Json* poolAliases = root.getArray("pool_aliases");
    std::vector<PoolAliasBinding> bindings;
    if (poolAliases != nullptr) {
        bindings.reserve(poolAliases->size());
        for (const Json& aliasJson : poolAliases->array) {
            bindings.push_back(parseAlias(aliasJson));
        }
    }
    return bindings;
}

JigsawPlacement::Config trialChambersConfig(const std::vector<PoolAliasBinding>& aliases) {
    // 26.2 trial_chambers：start_pool 无前缀，由 withDefaultNamespace 补 minecraft:
    JigsawPlacement::Config config;
    config.startPool = "trial_chambers/chamber/end";
    config.startJigsawName.clear();      // Optional.empty()
    config.size = 20;
    config.startHeightMin = -40;
    config.startHeightMax = -20;
    config.useExpansionHack = false;
    config.projectStartToHeightmap = false;
    config.maxDistance = MaxDistance(116);
    config.dimensionPadding = DimensionPadding(10, 10);
    config.poolAliases = aliases;
    config.minY = -64;
    config.maxY = 320;
    return config;
}

} // namespace trialfinder
