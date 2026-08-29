@echo off
setlocal enabledelayedexpansion
rem ============================================================
rem  TrialSpawnerFinder C++ 核心模块构建脚本（Windows / MSVC）
rem
rem  用法:
rem    build.bat              仅核心库（Release）
rem    build.bat --test       构建并运行 GoogleTest 单测
rem    build.bat --bench      构建并运行性能基准（trialfinder_bench）
rem    build.bat --jni        构建 JNI 桥接动态库（需 JDK 25）
rem    build.bat --debug      Debug 构建
rem ============================================================

set "ROOT=%~dp0.."
set "BUILD_DIR=%ROOT%\build"
set "CONFIG=Release"

if "%1"=="--debug" set "CONFIG=Debug"
set "CMAKE_OPTS=-DCMAKE_BUILD_TYPE=%CONFIG%"
if "%1"=="--test"  set "CMAKE_OPTS=%CMAKE_OPTS% -DBUILD_TESTING=ON"
if "%1"=="--bench" set "CMAKE_OPTS=%CMAKE_OPTS% -DBUILD_BENCH=ON"
if "%1"=="--jni"   set "CMAKE_OPTS=%CMAKE_OPTS% -DBUILD_JNI=ON"

where cmake >nul 2>nul
if errorlevel 1 (
    echo [错误] 未找到 cmake，请安装 CMake 3.20+ 并加入 PATH。
    exit /b 1
)

echo [1/3] 配置 CMake ...  %CMAKE_OPTS%
cmake -S "%ROOT%" -B "%BUILD_DIR%" %CMAKE_OPTS%
if errorlevel 1 goto :fail

echo [2/3] 构建 ...
cmake --build "%BUILD_DIR%" --config %CONFIG%
if errorlevel 1 goto :fail

echo [3/3] 构建完成。产物目录: %BUILD_DIR%\bin
if "%1"=="--test" (
    echo.
    echo ====== 运行单元测试 ======
    "%BUILD_DIR%\bin\%CONFIG%\trialfinder_tests.exe"
    if errorlevel 1 goto :fail
)
if "%1"=="--bench" (
    echo.
    echo ====== 运行性能基准 ======
    "%BUILD_DIR%\bin\%CONFIG%\trialfinder_bench.exe"
    if errorlevel 1 goto :fail
)
exit /b 0

:fail
echo [错误] 构建失败。
exit /b 1
