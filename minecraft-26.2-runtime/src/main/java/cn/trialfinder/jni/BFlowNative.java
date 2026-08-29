package cn.trialfinder.jni;

import cn.trialfinder.sim.resource.ClasspathResourceLoader;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.GZIPInputStream;

/**
 * B 流 C++ 核心（{@code libbflow}）的 JNI 入口。
 *
 * <p>职责划分：Java 保留资源加载（模板 NBT 解压字节、池 JSON 根路径）与并发调度；
 * C++ 执行 Jigsaw 拼接（{@code JigsawPlacement} / {@code HandTrialChamberPredictor}）。
 * 模板按 "namespace:path" 注册；池 JSON 与 trial_chambers.json 由 C++ 从
 * {@link #setDataRoot} 指定的资源根目录磁盘加载。
 *
 * <p>Native 方法对应 {@code src/main/cpp/jni/jni_bridge.cpp}。
 */
public final class BFlowNative {
    static {
        System.loadLibrary("bflow");
    }

    private BFlowNative() {
    }

    /** 创建引擎：注册模板（已解压 NBT 字节）并记录拼接深度上限。返回引擎句柄。 */
    public static native long initEngine(byte[][] templateData, String[] templateIds, int maxDepth);

    /** 指定数据根目录：C++ 从 {@code <root>/data/minecraft/...} 加载池 JSON 与 trial_chambers.json。 */
    public static native void setDataRoot(long engine, String root);

    /** 预测试炼密室布局（方块坐标），返回结果句柄。seed 与 initEngine 时传入的种子一致。 */
    public static native long generateChamber(long engine, long seed, int x, int z);

    /** 结果中的刷怪笼数量。 */
    public static native int getSpawnerCount(long engine, long result);

    /** 结果中的刷怪笼坐标（{@code int[N][3]}，每行 x/y/z）。 */
    public static native int[][] getSpawnerPositions(long engine, long result);

    /** 释放结果句柄。 */
    public static native void freeResult(long engine, long result);

    /** 释放引擎句柄（连同按 seed 缓存的预测器）。 */
    public static native void freeEngine(long engine);

    // ---- 任务书 API 兼容（无模板 id 表，由 C++ 从 dataRoot 懒加载模板） ----

    public static long initEngine(byte[][] templateData, int maxDepth) {
        return initEngine(templateData, (String[]) null, maxDepth);
    }

    // ---- 资源加载（Java 保留职责） ----

    /** 从 classpath 收集全部结构模板：id 为 "namespace:path"，字节为 gzip 解压后的 NBT。 */
    public static EngineData loadTemplatesFromClasspath() throws IOException {
        List<String> resources = ClasspathResourceLoader.listResourcePaths(
                "data/minecraft/structure", ".nbt");
        List<String> ids = new ArrayList<>();
        List<byte[]> data = new ArrayList<>();
        for (String resource : resources) {
            String path = resource.substring("data/".length());
            int dot = path.lastIndexOf(".nbt");
            ids.add("minecraft:" + path.substring(0, dot));
            data.add(gunzip(ClasspathResourceLoader.open(resource)));
        }
        return new EngineData(ids.toArray(new String[0]), data.toArray(new byte[0][]));
    }

    private static byte[] gunzip(InputStream stream) throws IOException {
        try (GZIPInputStream gz = new GZIPInputStream(stream);
             ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            byte[] buf = new byte[1 << 16];
            int n;
            while ((n = gz.read(buf)) > 0) {
                out.write(buf, 0, n);
            }
            return out.toByteArray();
        }
    }

    /** 模板集合：并行 id 表与 NBT 字节表。 */
    public record EngineData(String[] ids, byte[][] data) {
    }
}
