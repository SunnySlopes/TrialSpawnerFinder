# TrialSpawnerFinder C++ 核心模块
# 逐位复刻 minecraft-26.2-runtime 的 B 流 Jigsaw 拼接 / 预测器 / 模板缓存。
#
# 用法：
#   cmake -S src/main/cpp -B build/cpp
#   cmake --build build/cpp --config Release
#
# 可选开关：
#   -DBUILD_JNI=ON    构建 JNI 桥接动态库 bflow（需要 JDK 25）
#   -DBUILD_TESTING=ON 构建 GoogleTest 单元测试（需要 GTest）

cmake_minimum_required(VERSION 3.20)
project(TrialSpawnerFinderNative VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

option(BUILD_JNI "Build the JNI bridge shared library (bflow)" OFF)
option(BUILD_TESTING "Build GoogleTest unit tests" OFF)
option(BUILD_BENCH "Build the standalone performance benchmark (trialfinder_bench)" OFF)

# 可选 zlib：支持从磁盘读取 gzip 压缩的 .nbt 模板文件。
# JNI 桥接由 Java 侧解压，不需要 zlib。
find_package(ZLIB QUIET)
if(NOT ZLIB_FOUND)
  # 系统无 zlib：FetchContent 下载 zlib 源码构建（Jigsaw/Predictor 集成测试读磁盘 gzip NBT 需要）。
  include(FetchContent)
  set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(zlib
    URL https://codeload.github.com/madler/zlib/tar.gz/refs/tags/v1.3.1
    URL_HASH SHA256=17e88863f3600672ab49182f217281b6fc4d3c762bde361935e436a95214d05c
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  FetchContent_MakeAvailable(zlib)
  set(ZLIB_FOUND TRUE)
  set(ZLIB_TARGET zlibstatic)
else()
  set(ZLIB_TARGET ZLIB::ZLIB)
endif()

# ---- 核心静态库 ----
add_library(trialfinder_core STATIC
    core/Types.cpp
    random/Random.cpp
    nbt/NbtParser.cpp
    template/StructureTemplate.cpp
    template/StructureTemplateManager.cpp
    pool/Pool.cpp
    pool/JsonParser.cpp
    jigsaw/FreeSpace.cpp
    jigsaw/JigsawPlacement.cpp
    predictor/HandTrialChamberPredictor.cpp
)

target_include_directories(trialfinder_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(trialfinder_core PUBLIC
    TRIALFINDER_VERSION="${PROJECT_VERSION}")

if(ZLIB_FOUND)
  target_link_libraries(trialfinder_core PUBLIC ${ZLIB_TARGET})
  target_compile_definitions(trialfinder_core PUBLIC TRIALFINDER_HAVE_ZLIB=1)
endif()

if(MSVC)
  # /utf-8：源文件为 UTF-8（含中文注释），避免 GBK 代码页误读导致 C2001/C4819
  target_compile_options(trialfinder_core PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(trialfinder_core PRIVATE -Wall -Wextra)
endif()

# ---- JNI 桥接动态库 ----
if(BUILD_JNI)
  find_package(JNI REQUIRED)
  add_library(bflow SHARED jni/jni_bridge.cpp)
  target_include_directories(bflow PRIVATE ${JNI_INCLUDE_DIRS})
  target_link_libraries(bflow PRIVATE trialfinder_core ${JNI_LIBRARIES})
  if(MSVC)
    target_compile_options(bflow PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(bflow PRIVATE -Wall -Wextra)
  endif()
endif()

# ---- 性能基准 ----
if(BUILD_BENCH)
  add_executable(trialfinder_bench
      bench/bench_main.cpp
  )
  target_link_libraries(trialfinder_bench PRIVATE trialfinder_core)
  target_compile_definitions(trialfinder_bench PRIVATE
      _CRT_SECURE_NO_WARNINGS
      TRIALFINDER_TEST_RESOURCES="${CMAKE_CURRENT_SOURCE_DIR}/../../../minecraft-26.2-runtime/build/resources/main")
  if(MSVC)
    target_compile_options(trialfinder_bench PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(trialfinder_bench PRIVATE -Wall -Wextra)
  endif()
endif()

# ---- 单元测试 ----
if(BUILD_TESTING)
  enable_testing()
  # 优先 find_package；未找到时用 FetchContent 下载 googletest（需网络）。
  find_package(GTest QUIET)
  if(NOT GTest_FOUND)
    include(FetchContent)
    set(FETCHCONTENT_QUIET OFF)
    # 用 codeload 压缩包而非 git 克隆（git 协议在此网络下易超时）
    FetchContent_Declare(
      googletest
      URL https://codeload.github.com/google/googletest/tar.gz/refs/tags/v1.14.0
      URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
      DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    # MSVC 发行版默认 /MD，需与 gtest 的 CRT 匹配
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
  endif()
  add_executable(trialfinder_tests
      test/RandomGenTest.cpp
      test/TemplateTest.cpp
      test/JigsawTest.cpp
      test/PredictorTest.cpp
  )
  target_link_libraries(trialfinder_tests PRIVATE trialfinder_core GTest::gtest_main)
  # 集成测试读取仓库内 minecraft-26.2-runtime 的真实资源（缺资源则跳过）。
  # 可用环境变量 TRIALFINDER_TEST_RESOURCES 覆盖。
  target_compile_definitions(trialfinder_tests PRIVATE
      _CRT_SECURE_NO_WARNINGS
      TRIALFINDER_TEST_RESOURCES="${CMAKE_CURRENT_SOURCE_DIR}/../../../minecraft-26.2-runtime/build/resources/main")
  if(MSVC)
    target_compile_options(trialfinder_tests PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(trialfinder_tests PRIVATE -Wall -Wextra)
  endif()
  include(GoogleTest)
  gtest_discover_tests(trialfinder_tests)
endif()
