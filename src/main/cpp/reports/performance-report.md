# TrialSpawnerFinder C++ 核心性能报告

> 对应 Minecraft 26.2 重构线，B 流 Jigsaw 拼接 / 试炼密室预测器 C++ 移植。
> 逐位一致性与基准方法见《bit-consistency-report.md》。
> 生成时间：2026-08-29。构建：MSVC 14.44 / Release (/O2)，Windows 11 Pro。

## 1. 结论

C++ 移植版与纯 Java sim oracle（`build/oracle/oracle.txt`）**同机**对比：

| 指标 | Java（oracle） | C++（本次） | 比值 C++/Java |
|---|---|---|---|
| 冷启动首预测 | 304 ms | ~0.90 s | ≈ 3.0× |
| 热启动单次预测均值 | ~13.5 ms | 42.4 ms | ≈ 3.1× |
| 热启动单次预测范围 | 2 – 27 ms | 5.2 – 69.7 ms | — |

- 冷启动首预测即 seed 0 / chunk (0,0)，含 **模板 gzip NBT 磁盘装载 + 首次元数据计算**。
- 热启动为 16 组 (seed, chunk) 组合第二轮（全部模板已驻留内存）。
- 本次已应用一项保位优化（`orderConnectorsCopy`，见 §4），把热启动均值从 ~78 ms 降到 42.4 ms。

> Java 侧数值取自 oracle.txt PREDICT 段（16 组合依次首轮冷启动、次轮起为 JIT 热态）。
> Java 冷启动为 oracle 进程内的首个 predict（模板首次装载 + JIT 冷编译，不含 JVM 启动）；
> C++ 冷启动为进程首个 predict（模板 gzip NBT 磁盘装载 + 首次元数据计算）。
> 两者均为各自进程的首次预测，口径可比。

## 2. 逐组合数据

基准命令：

```bash
cmake -S src/main/cpp -B build/vs -DBUILD_BENCH=ON -DBUILD_TESTING=ON
cmake --build build/vs --config Release
build/vs/bin/Release/trialfinder_bench.exe
```

资源根：`minecraft-26.2-runtime/build/resources/main`（真实 26.2 资源）。

| seed | chunk | Java 热 (ms) | C++ 冷 (ms) | C++ 热 (ms) |
|---|---|---|---|---|
| 0    | 0,0    | 304（冷） | 897.3 | 69.7 |
| 0    | 1,-1   | 27 | 86.2 | 56.8 |
| 0    | 5,7    | 18 | 56.3 | 51.1 |
| 0    | 16,-16 | 26 | 55.4 | 35.2 |
| 1    | 0,0    | 17 | 37.7 | 22.3 |
| 1    | 1,-1   | 14 | 49.3 | 47.1 |
| 1    | 5,7    | 14 | 59.9 | 32.7 |
| 1    | 16,-16 | 12 | 40.3 | 57.8 |
| 42   | 0,0    | 2 | 5.2 | 5.2 |
| 42   | 1,-1   | 8 | 32.3 | 38.0 |
| 42   | 5,7    | 12 | 40.6 | 46.2 |
| 42   | 16,-16 | 9 | 33.9 | 32.7 |
| B    | 0,0    | 9 | 37.1 | 41.3 |
| B    | 1,-1   | 11 | 43.0 | 51.7 |
| B    | 5,7    | 9 | 38.1 | 39.1 |
| B    | 16,-16 | 14 | 52.4 | 51.5 |

> B = 9206294873968313284。冷启动除首个组合外，其余组合的首轮已受益于进程内共享元数据缓存
> （Java `METADATA_CACHE` 的忠实复刻），故与热轮差异很小。

## 3. 耗时分解（profile 插桩测得）

单次预测内部：

```
总耗时 ≈ 启动(createStart/setLargeFeatureSeed) + 拼接(generateChildren BFS) + 收集+排序
启动 / 收集+排序   < 0.2 ms（可忽略）
拼接               > 99% 总耗时
```

拼接内部按每 piece 统计：

| 环节 | 单 piece 耗时 | 说明 |
|---|---|---|
| `fillConnectors`（源 piece） | ~0.005 ms | 命中元数据缓存，只做一次洗牌 |
| `getShuffledTemplates`（权重池洗牌） | 0 – 0.05 ms | **非瓶颈**；整个预测累计 < 0.2 ms |
| **内层 target 循环 `fillConnectors`** | **最高 342 ms** | **瓶颈**：单 piece 最多触发 4044 次旋转 × 每次物化 ~25 个 connector |

**瓶颈根因**：hallway 池按权重展开后含 **1231** 个候选模板。B 流算法对每个源 connector
都要遍历全部候选模板 × 4 旋转，逐个调用 `fillConnectors`（洗牌 + 排序 + 拷贝 connector 列表），
即使绝大多数候选根本接不上。这是原版/Java 算法的固有搜索空间，**RNG 消耗顺序决定它无法剪枝**；
C++ 相对 Java 的额外开销在于 `Connector` 内三个 `std::string`（pool/name/target）的深层拷贝。

冷启动首预测额外成本：模板 gzip NBT 磁盘装载 + 首次 `(模板, 旋转)` 元数据计算
（`getJigsaws`/`getSpawnerBlocks`/包围盒提取），约占首预测的 90%。

## 4. 已应用优化：orderConnectorsCopy（保位）

`JigsawPlacement::orderConnectors` 原实现在位洗牌 + **插入排序期间逐元素深拷贝 Connector
（三个 std::string）**，复杂度 O(n²) 次字符串拷贝。新实现：

- 先建 `const Connector*` 指针置换数组；
- Fisher-Yates 洗牌指针（`nextInt(remaining)` **消耗序列与 Java 完全一致**）；
- 稳定插入排序指针（比较逻辑不变，等值优先级保持稳定）；
- 最后按序**一次性**物化结果向量。

效果：

| | 热启动均值 | 热启动范围 |
|---|---|---|
| 优化前（profile 实测） | ~78 ms | 10 – 133 ms |
| 优化后（bench 实测） | **42.4 ms** | **5.2 – 69.7 ms** |

优化后 37/37 GoogleTest 全部通过（含 orderConnectors 逐位对拍、16 组 count/exists、
4 组完整 spawner 列表、spawnerInfo / vault 完整列表），RNG 位序列与最终顺序零偏差。

## 5. 已知热点与后续优化方向（未实施，避免影响位一致性）

1. **per-rotation connector 列表深拷贝**：热路径每调用 `fillConnectors` 物化 ~25 个 Connector
   （~75 次字符串拷贝）。可改为直接遍历洗牌后的指针置换、只对 `canAttach` 命中的 connector
   物化，RNG 顺序不变即可保位。
2. **字符串视图**：connector 的 pool/name/target 可改用指向共享元数据存储的 `std::string_view`，
   彻底消除热路径堆分配（当前为忠实复刻，`Connector` 自带 string）。
3. **冷启动模板装载**：~0.9 s 中约九成为磁盘 gzip NBT + 解析。可在 Java 侧并行预加载
   （JNI `initEngine` 已支持传字节数组），或按 `collectTemplateIds` 预取全部模板。

这些优化均为纯常数级改进，不影响算法与 RNG 序列；如需实施可在独立分支验证 37/37 后合入。
