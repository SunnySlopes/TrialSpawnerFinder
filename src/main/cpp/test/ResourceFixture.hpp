#pragma once
// ============================================================
// 资源集成测试共享 fixture
//
// 所有读磁盘真实资源的用例共享一份模板管理器/池/配置
// （SetUpTestSuite 只跑一次）。缺资源时跳过用例。
// 静态成员用 inline（C++17）保证多翻译单元单一实例。
// ============================================================

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "jigsaw/JigsawPlacement.hpp"
#include "pool/Pool.hpp"
#include "predictor/HandTrialChamberPredictor.hpp"
#include "template/StructureTemplateManager.hpp"
#include "test/TestResources.hpp"

namespace trialfinder {

inline std::string readFileText(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("无法读取文件: " + path);
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

class ResourceTestBase : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        root_ = testResourceRoot();
        std::string probe =
            testResourcePath("data/minecraft/worldgen/structure/trial_chambers.json");
        if (root_.empty() || probe.empty() || !std::filesystem::exists(probe)) {
            root_.clear();
            return;
        }
        templates_ = std::make_unique<StructureTemplateManager>();
        templates_->setRoot(root_);
        sharedMetadata_ = std::make_shared<SharedMetadata>();
        pools_ = std::make_unique<PoolRegistry>(*templates_);
        pools_->setSharedMetadata(sharedMetadata_);
        pools_->loadAllFromDirectory(root_);

        std::string jsonText = readFileText(probe);
        std::vector<PoolAliasBinding> aliases = parseTrialChambersAliases(jsonText);
        config_ = trialChambersConfig(aliases);
    }

    static void TearDownTestSuite() {
        pools_.reset();          // 先释放池（元素随之销毁）
        templates_.reset();
        sharedMetadata_.reset(); // 再释放共享缓存 —— 缓存键与池同寿命，防止悬垂
        config_ = JigsawPlacement::Config{};
        root_.clear();
    }

    void SetUp() override {
        if (root_.empty()) {
            GTEST_SKIP() << "缺少测试资源根目录（设置 TRIALFINDER_TEST_RESOURCES）";
        }
    }

    static inline std::string root_;
    static inline std::unique_ptr<StructureTemplateManager> templates_;
    static inline std::unique_ptr<PoolRegistry> pools_;
    static inline std::shared_ptr<SharedMetadata> sharedMetadata_;
    static inline JigsawPlacement::Config config_;
};

} // namespace trialfinder
