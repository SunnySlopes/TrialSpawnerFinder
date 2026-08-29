#pragma once
// ============================================================
// 复刻 cn.trialfinder.sim.util.Util 子集（26.2 语义）。
// Fisher–Yates 洗牌与 getRandom 与原版逐位一致。
// ============================================================

#include <cstddef>
#include <cstdint>
#include <vector>

#include "random/Random.hpp"

namespace trialfinder {

namespace Util {

/** 从数组取随机元素（Java {@code Util.getRandom(T[], RandomSource)}）。 */
template <typename T, size_t N>
T getRandom(T (&array)[N], RandomSource& random) {
    return array[random.nextInt(static_cast<int>(N))];
}

/** 从 vector 取随机元素。 */
template <typename T>
const T& getRandom(const std::vector<T>& list, RandomSource& random) {
    return list[static_cast<std::size_t>(random.nextInt(static_cast<int>(list.size())))];
}

/** 原版从后往前的 Fisher–Yates 洗牌（Java {@code Util.shuffle}）。 */
template <typename T>
void shuffle(std::vector<T>& list, RandomSource& random) {
    for (std::size_t i = list.size(); i > 1; i--) {
        std::swap(list[i - 1], list[static_cast<std::size_t>(random.nextInt(static_cast<int>(i)))]);
    }
}

/** 复制 + 洗牌（Java {@code Util.shuffledCopy}）。 */
template <typename T>
std::vector<T> shuffledCopy(const std::vector<T>& list, RandomSource& random) {
    std::vector<T> copy = list;
    shuffle(copy, random);
    return copy;
}

} // namespace Util

} // namespace trialfinder
