#pragma once

#include <array>
#include <atomic>
#include <concepts>
#include <span>

namespace openx::ui {

template <size_t HistoryLength = 512>
    requires ((HistoryLength & (HistoryLength - 1)) == 0)
class ScrollingHistory {
public:
    static constexpr size_t Mask = HistoryLength - 1;

    struct Frame {
        float inputDb{-100.0f};
        float outputDb{-100.0f};
        float gainReductionDb{0.0f};
    };

    ScrollingHistory() {
        buffer.fill(Frame{});
    }

    void push(float inDb, float outDb, float grDb) noexcept {
        const uint32_t s = seq.load(std::memory_order_relaxed);
        seq.store(s + 1, std::memory_order_release); // Mark odd (write in progress)

        const size_t idx = writeIndex.load(std::memory_order_relaxed);
        buffer[idx] = Frame{ inDb, outDb, grDb };
        writeIndex.store((idx + 1) & Mask, std::memory_order_relaxed);

        seq.store(s + 2, std::memory_order_release); // Mark even (write completed)
    }

    void readOrdered(std::array<Frame, HistoryLength>& dest) const noexcept {
        constexpr int maxRetries = 10;
        for (int retry = 0; retry < maxRetries; ++retry) {
            const uint32_t s1 = seq.load(std::memory_order_acquire);
            if ((s1 & 1) != 0) {
                continue; // Concurrent write in progress, retry
            }

            const size_t currentWrite = writeIndex.load(std::memory_order_relaxed);
            for (size_t i = 0; i < HistoryLength; ++i) {
                const size_t readIdx = (currentWrite + i) & Mask;
                dest[i] = buffer[readIdx];
            }

            std::atomic_thread_fence(std::memory_order_acquire);
            const uint32_t s2 = seq.load(std::memory_order_relaxed);
            if (s1 == s2) {
                return; // Valid untorn snapshot
            }
        }
    }

private:
    std::array<Frame, HistoryLength> buffer{};
    std::atomic<size_t> writeIndex{0};
    alignas(64) std::atomic<uint32_t> seq{0};
};

} // namespace openx::ui
