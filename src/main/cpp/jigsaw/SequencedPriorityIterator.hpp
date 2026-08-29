#pragma once
// ============================================================
// 复刻 cn.trialfinder.sim.util.SequencedPriorityIterator（26.2 语义）。
// 按优先级降序处理；同一优先级内 FIFO。它决定 Jigsaw 拼接的 BFS 顺序，
// 进而决定 RNG 消费顺序。
// ============================================================

#include <deque>
#include <functional>
#include <map>
#include <stdexcept>
#include <utility>

namespace trialfinder {

template <typename T>
class SequencedPriorityIterator {
public:
    void add(T item, int priority) {
        queues_[priority].push_back(std::move(item));
    }

    bool hasNext() const {
        for (const auto& entry : queues_) {
            if (!entry.second.empty()) {
                return true;
            }
        }
        return false;
    }

    T next() {
        for (auto& entry : queues_) {
            std::deque<T>& queue = entry.second;
            if (!queue.empty()) {
                T item = std::move(queue.front());
                queue.pop_front();
                return item;
            }
        }
        throw std::runtime_error("SequencedPriorityIterator 无元素");
    }

private:
    // 优先级降序（Java TreeMap Comparator.reverseOrder()）
    std::map<int, std::deque<T>, std::greater<int>> queues_;
};

} // namespace trialfinder
