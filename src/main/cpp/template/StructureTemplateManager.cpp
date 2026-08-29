#include "template/StructureTemplateManager.hpp"

#include <cstdio>
#include <stdexcept>
#include <vector>

namespace trialfinder {

StructureTemplate& StructureTemplateManager::getOrCreate(const std::string& id) {
    auto it = cache_.find(id);
    if (it != cache_.end()) {
        return *it->second;
    }
    return load(id);
}

void StructureTemplateManager::registerTemplate(
    const std::string& id, const uint8_t* data, std::size_t len) {
    auto template_ = std::make_unique<StructureTemplate>();
    template_->loadBytes(data, len);
    cache_[id] = std::move(template_);
}

void StructureTemplateManager::registerTemplateCompressed(
    const std::string& id, const uint8_t* data, std::size_t len) {
    auto template_ = std::make_unique<StructureTemplate>();
    NbtTag tag = NbtIo::readCompressed(data, len);
    template_->load(tag);
    cache_[id] = std::move(template_);
}

StructureTemplate& StructureTemplateManager::load(const std::string& id) {
    if (root_.empty()) {
        throw std::runtime_error("缺少结构模板资源: " + id);
    }
    // 拆分 namespace:path
    std::string ns = id;
    std::string path = id;
    std::size_t colon = id.find(':');
    if (colon != std::string::npos) {
        ns = id.substr(0, colon);
        path = id.substr(colon + 1);
    }
    std::string file = root_ + "/data/" + ns + "/structure/" + path + ".nbt";

#ifdef TRIALFINDER_HAVE_ZLIB
    // 磁盘上的 .nbt 为 gzip 压缩
    FILE* fp = std::fopen(file.c_str(), "rb");
    if (fp == nullptr) {
        throw std::runtime_error("缺少结构模板文件: " + file);
    }
    std::vector<uint8_t> bytes;
    uint8_t buffer[1 << 16];
    std::size_t n;
    while ((n = std::fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        bytes.insert(bytes.end(), buffer, buffer + n);
    }
    std::fclose(fp);
    auto template_ = std::make_unique<StructureTemplate>();
    NbtTag tag = NbtIo::readCompressed(bytes.data(), bytes.size());
    template_->load(tag);
    return *cache_.emplace(id, std::move(template_)).first->second;
#else
    throw std::runtime_error(
        "StructureTemplateManager::load 需要 zlib（TRIALFINDER_HAVE_ZLIB 未定义）: " + file);
#endif
}

} // namespace trialfinder
