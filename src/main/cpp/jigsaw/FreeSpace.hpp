#pragma once
// ============================================================
// TrialSpawnerFinder C++ FreeSpace
//
// 复刻 cn.trialfinder.sim.jigsaw.FreeSpace（26.2 语义）——Jigsaw 拼接的
// 占用空间碰撞检测。边界是半开区间 [min, maxExclusive)；候选方块是闭区间
// 整数盒 [min, max]（占据 [min, max+1)）。contains 的 0.25/0.75 偏移判定
// 与原版逐位一致。
// ============================================================

#include <cstdint>
#include <cstddef>
#include <vector>

#include "core/Types.hpp"

namespace trialfinder {

class FreeSpace {
public:
    static constexpr int CELL_SIZE = 16;

    FreeSpace(int minX, int minY, int minZ,
              int maxXExclusive, int maxYExclusive, int maxZExclusive,
              const BoundingBox* initiallyBlocked);

    /** 复刻 {@code contains(BoundingBox)}。 */
    bool contains(const BoundingBox& candidate);

    /** 复刻 {@code occupy(BoundingBox)}。 */
    void occupy(const BoundingBox& box);

private:
    struct BlockedBox {
        BoundingBox box;
        int lastQuery = 0;
    };

    bool containsFast(const BoundingBox& candidate);
    void add(const BoundingBox& box);

    int cellX(int x) const;
    int cellY(int y) const;
    int cellZ(int z) const;
    std::size_t index(int x, int y, int z) const {
        return (static_cast<std::size_t>(x) * static_cast<std::size_t>(cellsY_)
                + static_cast<std::size_t>(y)) * static_cast<std::size_t>(cellsZ_)
            + static_cast<std::size_t>(z);
    }

    static bool intersects(const BoundingBox& first, const BoundingBox& second) {
        return first.maxX >= second.minX && first.minX <= second.maxX
            && first.maxY >= second.minY && first.minY <= second.maxY
            && first.maxZ >= second.minZ && first.minZ <= second.maxZ;
    }

    int minX_, minY_, minZ_;
    int maxXExclusive_, maxYExclusive_, maxZExclusive_;
    int baseX_, baseY_, baseZ_;
    int cellsX_, cellsY_, cellsZ_;
    std::vector<BlockedBox> blocked_;
    // 空 = 快速路径（线性扫 blocked_）；非空 = 16x16x16 单元格空间哈希（存 blocked_ 索引）
    std::vector<std::vector<std::size_t>> cells_;
    int queryId_ = 0;
};

} // namespace trialfinder
