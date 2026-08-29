package cn.trialfinder.io;

import cn.minecraftfinder.core.AreaShape;
import cn.minecraftfinder.core.BlockPoint;
import cn.minecraftfinder.core.ResultFiles;
import cn.minecraftfinder.core.ResultTableWriter;
import cn.trialfinder.config.FinderConfig;
import cn.trialfinder.model.SearchResult;

import java.io.IOException;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.stream.Collectors;

public final class ResultWriter {
    private static final List<String> HEADERS = List.of(
            "排名", "中心X", "中心Z", "密室数量", "试炼刷怪笼数量", "密室位置");

    /** check-top 启用时追加的列。 */
    private static final List<String> CHECK_HEADERS = List.of("快速刷怪笼", "慢速刷怪笼", "宝库数量");

    /** 单个结果的快/慢刷怪笼与宝库统计（对应旧项目 {@code --check-top}）。 */
    public record CheckResult(int fastSpawners, int slowSpawners, int vaults) {
        public int totalSpawners() {
            return this.fastSpawners + this.slowSpawners;
        }
    }

    private ResultWriter() {
    }

    public static void write(Path path, List<SearchResult> results) throws IOException {
        write(path, results, null, List.of());
    }

    /**
     * 写结果 CSV/TXT；{@code checks} 非空时追加快/慢刷怪笼与宝库列（对应旧项目 {@code --check-top}）。
     *
     * @param checks 每个结果一条统计；为 null 或长度不足时该结果补空列
     */
    public static void write(Path path, List<SearchResult> results,
                             List<CheckResult> checks) throws IOException {
        write(path, results, checks, List.of());
    }

    /**
     * 同 {@link #write(Path, List, List)}，但在 CSV 与对齐文本头部写入 {@code # } 参数注释行
     * （结果文件自文档化，可用 {@code query --file} 直接回读坐标）。
     *
     * @param parameterLines 参数说明行；空时文件与旧版格式完全一致
     */
    public static void write(Path path, List<SearchResult> results,
                             List<CheckResult> checks,
                             List<String> parameterLines) throws IOException {
        boolean withCheck = checks != null && !checks.isEmpty();
        List<String> headers = withCheck
                ? new ArrayList<>(HEADERS) { {
                        addAll(CHECK_HEADERS);
                    } }
                : HEADERS;
        List<List<String>> csvRows = new ArrayList<>(results.size());
        List<List<String>> textRows = new ArrayList<>(results.size());
        for (int index = 0; index < results.size(); index++) {
            SearchResult result = results.get(index);
            List<String> row = new ArrayList<>(List.of(
                    Integer.toString(index + 1),
                    Long.toString(result.centerX()),
                    Long.toString(result.centerZ()),
                    Integer.toString(result.structureCount()),
                    Integer.toString(result.spawnerCount())));
            row.add(result.structures().stream().map(ResultWriter::formatPoint)
                    .collect(Collectors.joining("|")));
            if (withCheck) {
                if (index < checks.size()) {
                    CheckResult check = checks.get(index);
                    row.add(Integer.toString(check.fastSpawners()));
                    row.add(Integer.toString(check.slowSpawners()));
                    row.add(Integer.toString(check.vaults()));
                } else {
                    row.add("0");
                    row.add("0");
                    row.add("0");
                }
            }
            List<String> rowFixed = List.copyOf(row);
            csvRows.add(rowFixed);
            textRows.add(rowWithTextSeparator(rowFixed, result));
        }
        ResultTableWriter.write(path, headers, csvRows, textRows, parameterLines);
    }

    // ------------------------------------------------------------ 参数说明

    /** 控制台单行参数摘要（搜索开始时打印）。 */
    public static String parameterLine(FinderConfig config) {
        return "参数：seed=%d；搜索范围=%s；统计=半径%d方块、形状%s；达标=密室≥%d、刷怪笼≥%d；快速扫描线程=%d"
                .formatted(
                        config.seed(), searchAreaText(config),
                        config.clusterRadiusBlocks(), shapeName(config.areaShape()),
                        config.minStructures(), config.minSpawners(),
                        config.scanThreads());
    }

    /** 结果文件头部的 {@code # } 参数注释行。 */
    public static List<String> parameterLines(FinderConfig config, int checkTop) {
        List<String> lines = new ArrayList<>();
        lines.add("TrialSpawnerFinder 搜索结果");
        lines.add("seed=" + config.seed());
        lines.add("搜索范围=" + searchAreaText(config));
        lines.add("统计范围=半径" + config.clusterRadiusBlocks() + "方块、形状"
                + shapeName(config.areaShape()));
        lines.add("达标条件=密室≥" + config.minStructures() + "、刷怪笼≥" + config.minSpawners());
        lines.add("快速扫描线程=" + config.scanThreads());
        if (checkTop > 0) {
            lines.add("check-top=" + checkTop);
        }
        return List.copyOf(lines);
    }

    private static String searchAreaText(FinderConfig config) {
        return config.fullWorld()
                ? "全图"
                : "中心(%d,%d)、半径%,d、形状%s".formatted(
                        config.searchCenterX(), config.searchCenterZ(),
                        config.searchRadiusBlocks(), shapeName(config.searchAreaShape()));
    }

    private static String shapeName(AreaShape shape) {
        return shape.name().toLowerCase(Locale.ROOT);
    }

    /** 对齐文本版：密室位置用 " | " 分隔（阅读友好），其余列与 CSV 一致。 */
    private static List<String> rowWithTextSeparator(List<String> csvRow, SearchResult result) {
        List<String> text = new ArrayList<>(csvRow);
        String structures = result.structures().stream().map(ResultWriter::formatPoint)
                .collect(Collectors.joining(" | "));
        text.set(5, structures);
        return text;
    }

    public static Path textPath(Path csvPath) {
        return ResultFiles.textPath(csvPath);
    }

    private static String formatPoint(BlockPoint point) {
        return point.x() + "," + point.z();
    }
}
