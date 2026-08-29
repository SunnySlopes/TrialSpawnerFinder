# TrialSpawnerFinder C++ 核心逐位一致性报告

> 对应 Minecraft 26.2 重构线，B 流 Jigsaw 拼接 / 试炼密室预测器 C++ 移植。
> 原则：**任何偏差都是 bug**。参照值为纯 Java sim oracle（`build/oracle/oracle.txt`，
> 由 Java 版 26.2 sim 生成），C++ 结果与 oracle **逐位对拍**。
> 生成时间：2026-08-29。MSVC 14.44 / Release，37/37 测试通过。

## 1. 结论

C++ 移植版在 **37 项 GoogleTest 全部通过**（`trialfinder_tests.exe`），
覆盖 RNG 原语、模板解析、池装配、B 流拼接与完整预测输出。对 oracle 的对照面：

- **16 组 (seed, chunk)** 的 spawnerCount / exists 全对拍；
- **4 组完整 spawner 坐标列表**逐位对拍（seed 0/42/B、4 个不同 chunk）；
- **1 组完整 spawnerInfo 列表**（normal/ominous 配置串）逐位对拍；
- **1 组完整 vault 列表**（坐标 + ominous 标志）逐位对拍；
- RNG 位序列：nextInt / nextLong / nextDouble / next(48) / setLargeFeatureSeed /
  Rotation.getRandom / getShuffled / orderConnectors 全部对拍。

无一偏差。

## 2. 对拍维度与测试用例

### 2.1 随机数原语（RandomGenTest，11 项）

| 维度 | 测试 | 对拍内容 |
|---|---|---|
| `nextInt(bound)` | `NextIntMatchesJavaForKnownSeedsAndBounds` | 已知 seed/界值下的序列 |
| `nextLong()` | `NextLongMatchesJavaForKnownSeeds` | 高位/低位 32 位 + **借位语义** |
| `nextDouble()` | `NextDoubleMatchesJavaSimQuirk` | Java sim 的 `(next(26)<<27)+next(27)` 复刻 |
| `next(48)` 内部状态 | `Next48MatchesJavaInternalState` | 48 位 LCG 原始状态 |
| 2 的幂 / 拒绝采样 | `NextIntPowerOfTwoAndRejectionBothMatch` | `nextInt` 两条分支各自逐位一致 |
| `Mth.getSeed` | `MthGetSeedMatchesJava` | 坐标→LCG seed 折叠 |
| `Rotation.getRandom` | `RotationGetRandomMatchesJava` | 枚举抽取 |
| `Rotation.getShuffled` | `RotationGetShuffledMatchesJava` | Fisher-Yates 洗牌 + **消耗顺序** |
| `setLargeFeatureSeed` | `SetLargeFeatureSeedMatchesJava` | chunk/坐标→种子折叠 + 首个 nextLong |
| setLargeFeatureWithSalt | `SetLargeFeatureWithSaltConsumesLikeJava` | 盐值变体 |
| `orderConnectors` | `OrderConnectorsMatchesJavaShuffleThenStableSort` | 洗牌 + 稳定降序，30 组 (size,seed) 全对拍 |

**关键位级事实（本移植发现并修正过的坑）**：
- `nextLong()` = `((long)next(32) << 32) + next(32)`，低 32 位以 `int` 符号扩展后**相加**；
  当 bit31 置位时对高 32 位产生**借位**。早期实现误用按位或，偏差恒为 ±2³²，已按 Java 语义修正。
- `Rotation.getShuffled` 的 oracle 取值若取自「getRandom 已消耗一个 nextInt(4) 之后」的流，
  会与 fresh RNG 错位一位；测试以 **fresh RNG** 的流位置对拍（{3,0,1,2} / {3,0,1,2} / {3,0,2,1}）。

### 2.2 模板 / NBT（TemplateTest，10 项）

| 维度 | 测试 |
|---|---|
| NBT 复合标签解析 | `NbtIoParsesSimpleCompound` / `NbtIoReadRejectsTruncated` |
| 尺寸与调色板 | `LoadParsesSizeAndPalette` |
| 旋转尺寸 | `RotatedSizeSwapsXZForQuarterTurns` |
| Jigsaw 块提取 | `JigsawExtractionMatchesNbt` / `JigsawTranslationOffsetsByPos` |
| 90° 旋转 | `QuarterTurnRotatesJigsawPositionAndOrientation` |
| 刷怪笼块 + 配置 | `SpawnerBlocksExtractedWithConfigs` / `FilterBlocksFindsTrialSpawner` |
| 包围盒 | `BoundingBoxMatchesRotatedSize` |

模板 id 规范化（`Identifier.fromString("foo")` → `"minecraft:foo"`）已在
`JigsawBlockInfo::of` 中复刻，保证 name/pool/target 与 Java 一致。

### 2.3 资源装配（JigsawResourceTest，9 项）

- 26.2 固定配置面（startPool/startJigsawName/size/height/padding/maxDistance 等）逐字段对拍；
- `trial_chambers.json` pool_aliases 解析对拍；
- 池注册表从磁盘 JSON 装载、fallback 解析；
- **gzip 压缩 .nbt 模板懒加载**（与 Java 资源流一致）；
- `PredictSeed0Chunk00MatchesOracle`：seed 0 / (0,0) 完整 spawner + spawnerInfo + vault 全对拍；
- `PredictSeed42Chunk00CountMatchesOracle`：seed 42 / (0,0) count=29；
- 块坐标→chunk 转换语义（floorDiv）；
- `PredictIsDeterministic`：同 seed 同 chunk 两次预测完全一致；
- `PredictorReusedAcrossCallsKeepsLocalCache`：复用预测器实例不改变结果。

### 2.4 完整预测对拍（PredictorTest，7 项）

- **16 组 count/exists**：seed {0,1,42,B} × chunk {(0,0),(1,-1),(5,7),(16,-16)}，
  每组的 spawner 数量与存在性与 oracle 完全一致；
- **4 组完整 spawner 坐标列表**：seed 0/(0,0)、seed 42/(0,0)、seed B/(5,7)、seed 1/(16,-16)，
  按排序后逐坐标（x/y/z）对拍；
- **1 组完整 spawnerInfo**：seed 0/(0,0) 21 项，normal/ominous 配置串逐项对拍；
- **1 组完整 vault**：seed 0/(0,0) 16 项，坐标 + ominous 标志逐项对拍。

## 3. 元数据缓存的一致性论证

Java 版有两级元数据缓存：静态 `METADATA_CACHE`（按元素键控）+ 每实例 `localMetadata`。
C++ 以 `SharedMetadata`（进程内，按元素指针键控，4 种旋转槽）复刻静态缓存，由资源持有者
（JNI 引擎 / 测试 fixture）创建、先于池释放；每实例仍有本地缓存。缓存仅存**未洗牌**的
connectors / bbox / spawners —— 与 seed、位置无关，因此跨实例共享是确定的，
不改变任何 RNG 消耗或输出。37/37 验证该设计零偏差。

## 4. 复现方法

```bash
# 构建并运行全部测试（自动下载 googletest / zlib，需网络拉取 codeload 归档）
cmake -S src/main/cpp -B build/vs -DBUILD_TESTING=ON
cmake --build build/vs --config Release
build/vs/bin/Release/trialfinder_tests.exe
# 期望输出：[  PASSED  ] 37 tests.
```

资源根缺省为仓库内 `minecraft-26.2-runtime/build/resources/main`；
可用环境变量 `TRIALFINDER_TEST_RESOURCES` 覆盖。oracle 生成器与期望值同处
`build/oracle/oracle.txt`；测试内嵌的期望值均自该 oracle 手工核录。

## 5. 遗留差异（均已归零）

| 曾出现的偏差 | 根因 | 处理 |
|---|---|---|
| spawner 坐标偏移 | `nextLong` 借位缺失 | 修正 `Random.hpp` 用 `+` 与符号扩展 |
| 旋转洗牌测试期望错位 | 测试取了 getRandom 消耗后的流位置 | 修正测试为 fresh RNG |
| 模板提取返回 0 | `getRotatedJigsaws/getSpawnerBlocks` 未先 `ensureRotated` | 修正 `StructureTemplate` |
| name/pool/target 缺默认命名空间 | 未做 `Identifier.withDefaultNamespace` | 修正 `JigsawBlockInfo::of` |
| 长整型 NBT 写入字节数错 | TAG_Long 负载 8 字节 | 修正测试写入 |

以上全部修复后，与 oracle 的对照面零偏差。
