#pragma once
// ============================================================
// 复刻 cn.trialfinder.sim.util.WeightedList（26.2 语义）。
// getRandom 与原版一致：一次 nextInt(totalWeight) 映射到累计权重区间。
// ============================================================

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "random/Random.hpp"

namespace trialfinder {

template <typename T>
class WeightedList {
public:
    struct Item {
        T value;
        int weight;
    };

    explicit WeightedList(std::vector<Item> items)
        : items_(std::move(items)) {
        int total = 0;
        for (const Item& item : items_) {
            total += item.weight;
        }
        totalWeight_ = total;
    }

    bool isEmpty() const { return items_.empty(); }

    /** 复刻 {@code Optional<T> getRandom}。totalWeight==0 或未命中时返回 nullopt。 */
    std::optional<T> getRandom(RandomSource& random) const {
        if (totalWeight_ == 0) {
            return std::nullopt;
        }
        int index = random.nextInt(totalWeight_);
        for (const Item& item : items_) {
            index -= item.weight;
            if (index < 0) {
                return item.value;
            }
        }
        return std::nullopt;
    }

    T getRandomOrThrow(RandomSource& random) const {
        if (totalWeight_ == 0) {
            throw std::runtime_error("带权列表为空");
        }
        std::optional<T> value = getRandom(random);
        if (!value) {
            throw std::runtime_error("带权列表 getRandom 未命中");
        }
        return *value;
    }

    const std::vector<Item>& unwrap() const { return items_; }

    int totalWeight() const { return totalWeight_; }

    template <typename U>
    class Builder {
    public:
        Builder& add(U value, int weight) {
            entries_.push_back(Item{std::move(value), weight});
            return *this;
        }
        Builder& add(U value) { return add(std::move(value), 1); }
        WeightedList<U> build() { return WeightedList<U>(std::move(entries_)); }

    private:
        std::vector<Item> entries_;
    };

private:
    std::vector<Item> items_;
    int totalWeight_ = 0;
};

} // namespace trialfinder
