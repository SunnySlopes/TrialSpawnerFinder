#pragma once
// ============================================================
// TrialSpawnerFinder C++ 结构模板管理器
//
// 复刻 cn.trialfinder.sim.template.StructureTemplateManager
// （26.2 语义）。JNI 模式由 Java 侧传入解压后的 NBT 字节注册；
// 独立部署可 setRoot 后按 data/<ns>/structure/<path>.nbt 懒加载。
// ============================================================

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "template/StructureTemplate.hpp"

namespace trialfinder {

class StructureTemplateManager {
public:
    /** 返回缓存模板；未注册时若设置了 root 则从磁盘懒加载，否则抛异常。 */
    StructureTemplate& getOrCreate(const std::string& id);

    /** 注册一个模板（已解压 NBT 字节）。id 为完整 "namespace:path"。 */
    void registerTemplate(const std::string& id, const uint8_t* data, std::size_t len);

    /** 注册 gzip 压缩 NBT 字节（需 zlib）。 */
    void registerTemplateCompressed(const std::string& id, const uint8_t* data, std::size_t len);

    /** 独立部署：模板文件根目录（其下有 data/<ns>/structure/<path>.nbt）。 */
    void setRoot(const std::string& root) { root_ = root; }

    bool has(const std::string& id) const {
        return cache_.find(id) != cache_.end();
    }

    std::size_t cacheSize() const { return cache_.size(); }

private:
    StructureTemplate& load(const std::string& id);

    std::unordered_map<std::string, std::unique_ptr<StructureTemplate>> cache_;
    std::string root_;
};

} // namespace trialfinder
