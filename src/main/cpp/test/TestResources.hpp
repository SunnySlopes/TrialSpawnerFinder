#pragma once
// ============================================================
// 测试资源定位
//
// 优先读环境变量 TRIALFINDER_TEST_RESOURCES（ctest 可注入）；
// 否则回退到 CMake 定义的编译期宏 TRIALFINDER_TEST_RESOURCES
// （默认为仓库内 minecraft-26.2-runtime/build/resources/main）。
// 两者都缺失时返回空串，调用方应跳过集成用例。
// ============================================================

#include <cstdlib>
#include <string>

namespace trialfinder {

inline std::string testResourceRoot() {
    const char* env = std::getenv("TRIALFINDER_TEST_RESOURCES");
    if (env != nullptr && env[0] != '\0') {
        return env;
    }
#ifdef TRIALFINDER_TEST_RESOURCES
    return TRIALFINDER_TEST_RESOURCES;
#else
    return "";
#endif
}

inline std::string testResourcePath(const std::string& relative) {
    std::string root = testResourceRoot();
    if (root.empty()) {
        return "";
    }
    return root + "/" + relative;
}

} // namespace trialfinder
