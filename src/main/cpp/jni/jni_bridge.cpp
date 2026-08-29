// ============================================================
// TrialSpawnerFinder JNI 桥接（B 流 C++ 核心 <-> Java 调用方）
//
// 对应 cn.trialfinder.jni.BFlowNative：
//   initEngine(byte[][] templateData, String[] templateIds, int maxDepth) -> long
//   setDataRoot(String root)
//   generateChamber(long engine, long seed, int x, int z) -> long
//   getSpawnerCount(long engine, long result) -> int
//   getSpawnerPositions(long engine, long result) -> int[][]
//   freeResult(long engine, long result)
//   freeEngine(long engine)
//
// 职责：Java 侧保留资源加载（模板 NBT 已解压字节）+ 并发调度；
// C++ 侧执行 B 流 Jigsaw 拼接。池 JSON 与 trial_chambers.json 由
// C++ 从 dataRoot 磁盘目录加载（Java 只传根路径）。
// ============================================================

#include <jni.h>

#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "pool/Pool.hpp"
#include "predictor/HandTrialChamberPredictor.hpp"
#include "template/StructureTemplateManager.hpp"

// 引擎上下文：池 + 模板 + 别名 + 按 seed 缓存的预测器。
// 定义在匿名命名空间，避免跨 TU 泄漏。
namespace {
struct EngineContext {
    trialfinder::StructureTemplateManager templates;
    trialfinder::PoolRegistry pools;
    std::shared_ptr<trialfinder::SharedMetadata> sharedMetadata;
    std::vector<trialfinder::PoolAliasBinding> aliases;
    std::unordered_map<int64_t, std::unique_ptr<trialfinder::HandTrialChamberPredictor>> predictors;
    int maxDepth = 0;

    EngineContext() : pools(templates),
                      sharedMetadata(std::make_shared<trialfinder::SharedMetadata>()) {
        pools.setSharedMetadata(sharedMetadata);
    }
};

using trialfinder::JigsawPlacement;

} // namespace

// ---------------- 工具 ----------------

namespace {

inline EngineContext* engineOf(jlong handle) {
    return reinterpret_cast<EngineContext*>(handle);
}

inline JigsawPlacement::Prediction* resultOf(jlong handle) {
    return reinterpret_cast<JigsawPlacement::Prediction*>(handle);
}

std::string readFileText(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("无法读取文件: " + path);
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

jlong makeEngineHandle(EngineContext* ctx) {
    return reinterpret_cast<jlong>(ctx);
}

} // namespace

// ---------------- BFlowNative 实现 ----------------

extern "C" {

JNIEXPORT jlong JNICALL
Java_cn_trialfinder_jni_BFlowNative_initEngine(
    JNIEnv* env, jclass, jobjectArray templateData, jobjectArray templateIds, jint maxDepth) {
    try {
        EngineContext* ctx = new EngineContext();
        ctx->maxDepth = maxDepth;
        jsize count = templateData == nullptr ? 0 : env->GetArrayLength(templateData);
        jsize idCount = templateIds == nullptr ? 0 : env->GetArrayLength(templateIds);
        for (jsize i = 0; i < count; i++) {
            jbyteArray arr = static_cast<jbyteArray>(env->GetObjectArrayElement(templateData, i));
            if (arr == nullptr) {
                continue;
            }
            jsize len = env->GetArrayLength(arr);
            jbyte* bytes = env->GetByteArrayElements(arr, nullptr);
            if (bytes != nullptr) {
                if (i < idCount) {
                    jstring idStr = static_cast<jstring>(env->GetObjectArrayElement(templateIds, i));
                    const char* idChars = idStr != nullptr
                        ? env->GetStringUTFChars(idStr, nullptr) : nullptr;
                    if (idChars != nullptr) {
                        ctx->templates.registerTemplate(
                            idChars, reinterpret_cast<const uint8_t*>(bytes),
                            static_cast<std::size_t>(len));
                        env->ReleaseStringUTFChars(idStr, idChars);
                    }
                    if (idStr != nullptr) {
                        env->DeleteLocalRef(idStr);
                    }
                }
                env->ReleaseByteArrayElements(arr, bytes, JNI_ABORT);
            }
            env->DeleteLocalRef(arr);
        }
        return makeEngineHandle(ctx);
    } catch (const std::exception& e) {
        env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), e.what());
        return 0;
    }
}

JNIEXPORT void JNICALL
Java_cn_trialfinder_jni_BFlowNative_setDataRoot(JNIEnv* env, jclass, jlong engine, jstring root) {
    try {
        const char* rootChars = root != nullptr ? env->GetStringUTFChars(root, nullptr) : nullptr;
        if (rootChars == nullptr) {
            throw std::runtime_error("setDataRoot 缺少 root 路径");
        }
        std::string rootStr(rootChars);
        env->ReleaseStringUTFChars(root, rootChars);

        EngineContext* ctx = engineOf(engine);
        ctx->templates.setRoot(rootStr);
        ctx->pools.loadAllFromDirectory(rootStr);
        std::string aliasesPath =
            rootStr + "/data/minecraft/worldgen/structure/trial_chambers.json";
        ctx->aliases = trialfinder::parseTrialChambersAliases(readFileText(aliasesPath));
    } catch (const std::exception& e) {
        env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), e.what());
    }
}

JNIEXPORT jlong JNICALL
Java_cn_trialfinder_jni_BFlowNative_generateChamber(
    JNIEnv* env, jclass, jlong engine, jlong seed, jint x, jint z) {
    try {
        EngineContext* ctx = engineOf(engine);
        std::unique_ptr<trialfinder::HandTrialChamberPredictor>& predictor =
            ctx->predictors[static_cast<int64_t>(seed)];
        if (!predictor) {
            trialfinder::JigsawPlacement::Config config =
                trialfinder::trialChambersConfig(ctx->aliases);
            if (ctx->maxDepth > 0) {
                config.size = ctx->maxDepth;
            }
            predictor = std::make_unique<trialfinder::HandTrialChamberPredictor>(
                static_cast<int64_t>(seed), std::move(config), ctx->pools, ctx->templates);
        }
        JigsawPlacement::Prediction* result =
            new JigsawPlacement::Prediction(predictor->predict(x, z));
        return reinterpret_cast<jlong>(result);
    } catch (const std::exception& e) {
        env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), e.what());
        return 0;
    }
}

JNIEXPORT jint JNICALL
Java_cn_trialfinder_jni_BFlowNative_getSpawnerCount(
    JNIEnv* env, jclass, jlong engine, jlong result) {
    try {
        (void)engine;
        return static_cast<jint>(resultOf(result)->spawners.size());
    } catch (const std::exception& e) {
        env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), e.what());
        return 0;
    }
}

JNIEXPORT jobjectArray JNICALL
Java_cn_trialfinder_jni_BFlowNative_getSpawnerPositions(
    JNIEnv* env, jclass, jlong engine, jlong result) {
    try {
        (void)engine;
        JigsawPlacement::Prediction* prediction = resultOf(result);
        jclass intArrayClass = env->FindClass("[I");
        if (intArrayClass == nullptr) {
            return nullptr;
        }
        jobjectArray out = env->NewObjectArray(
            static_cast<jsize>(prediction->spawners.size()), intArrayClass, nullptr);
        for (std::size_t i = 0; i < prediction->spawners.size(); i++) {
            jintArray row = env->NewIntArray(3);
            jint values[3] = {
                prediction->spawners[i].x,
                prediction->spawners[i].y,
                prediction->spawners[i].z,
            };
            env->SetIntArrayRegion(row, 0, 3, values);
            env->SetObjectArrayElement(out, static_cast<jsize>(i), row);
            env->DeleteLocalRef(row);
        }
        return out;
    } catch (const std::exception& e) {
        env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), e.what());
        return nullptr;
    }
}

JNIEXPORT void JNICALL
Java_cn_trialfinder_jni_BFlowNative_freeResult(JNIEnv*, jclass, jlong engine, jlong result) {
    (void)engine;
    delete resultOf(result);
}

JNIEXPORT void JNICALL
Java_cn_trialfinder_jni_BFlowNative_freeEngine(JNIEnv* env, jclass, jlong engine) {
    try {
        delete engineOf(engine);
    } catch (const std::exception& e) {
        env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), e.what());
    }
}

} // extern "C"
