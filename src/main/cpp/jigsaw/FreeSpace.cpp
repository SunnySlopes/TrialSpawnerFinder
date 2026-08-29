#include "jigsaw/FreeSpace.hpp"

#include <algorithm>

namespace trialfinder {

FreeSpace::FreeSpace(int minX, int minY, int minZ,
                     int maxXExclusive, int maxYExclusive, int maxZExclusive,
                     const BoundingBox* initiallyBlocked)
    : minX_(minX), minY_(minY), minZ_(minZ),
      maxXExclusive_(maxXExclusive), maxYExclusive_(maxYExclusive), maxZExclusive_(maxZExclusive),
      baseX_(minX), baseY_(minY), baseZ_(minZ),
      cellsX_(std::max(1, Mth::ceilDiv(maxXExclusive - minX, CELL_SIZE))),
      cellsY_(std::max(1, Mth::ceilDiv(maxYExclusive - minY, CELL_SIZE))),
      cellsZ_(std::max(1, Mth::ceilDiv(maxZExclusive - minZ, CELL_SIZE))) {
    if (initiallyBlocked != nullptr) {
        cells_.assign(static_cast<std::size_t>(cellsX_) * cellsY_ * cellsZ_, {});
        add(*initiallyBlocked);
    }
}

bool FreeSpace::contains(const BoundingBox& candidate) {
    return containsFast(candidate);
}

void FreeSpace::occupy(const BoundingBox& box) {
    add(box);
}

bool FreeSpace::containsFast(const BoundingBox& candidate) {
    if (static_cast<double>(candidate.minX) + 0.25 < static_cast<double>(minX_)
        || static_cast<double>(candidate.maxX) + 0.75 > static_cast<double>(maxXExclusive_)
        || static_cast<double>(candidate.minY) + 0.25 < static_cast<double>(minY_)
        || static_cast<double>(candidate.maxY) + 0.75 > static_cast<double>(maxYExclusive_)
        || static_cast<double>(candidate.minZ) + 0.25 < static_cast<double>(minZ_)
        || static_cast<double>(candidate.maxZ) + 0.75 > static_cast<double>(maxZExclusive_)) {
        return false;
    }
    if (cells_.empty()) {
        for (const BlockedBox& entry : blocked_) {
            if (intersects(candidate, entry.box)) {
                return false;
            }
        }
        return true;
    }

    int currentQuery = ++queryId_;
    for (int x = cellX(candidate.minX); x <= cellX(candidate.maxX); x++) {
        for (int y = cellY(candidate.minY); y <= cellY(candidate.maxY); y++) {
            for (int z = cellZ(candidate.minZ); z <= cellZ(candidate.maxZ); z++) {
                const std::vector<std::size_t>& entries = cells_[index(x, y, z)];
                if (entries.empty()) {
                    continue;
                }
                for (std::size_t entryIndex : entries) {
                    BlockedBox& entry = blocked_[entryIndex];
                    if (entry.lastQuery == currentQuery) {
                        continue;
                    }
                    entry.lastQuery = currentQuery;
                    if (intersects(candidate, entry.box)) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

void FreeSpace::add(const BoundingBox& box) {
    BlockedBox entry;
    entry.box = box;
    blocked_.push_back(entry);
    if (cells_.empty()) {
        return;
    }
    std::size_t entryIndex = blocked_.size() - 1;
    for (int x = cellX(box.minX); x <= cellX(box.maxX); x++) {
        for (int y = cellY(box.minY); y <= cellY(box.maxY); y++) {
            for (int z = cellZ(box.minZ); z <= cellZ(box.maxZ); z++) {
                cells_[index(x, y, z)].push_back(entryIndex);
            }
        }
    }
}

int FreeSpace::cellX(int x) const {
    return std::max(0, std::min(cellsX_ - 1, Mth::floorDiv(x - baseX_, CELL_SIZE)));
}

int FreeSpace::cellY(int y) const {
    return std::max(0, std::min(cellsY_ - 1, Mth::floorDiv(y - baseY_, CELL_SIZE)));
}

int FreeSpace::cellZ(int z) const {
    return std::max(0, std::min(cellsZ_ - 1, Mth::floorDiv(z - baseZ_, CELL_SIZE)));
}

} // namespace trialfinder
