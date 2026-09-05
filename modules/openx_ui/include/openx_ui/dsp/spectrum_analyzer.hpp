#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <cmath>
#include <concepts>
#include <span>

namespace openx::ui {

template <typename DataType>
class TriBuffer {
public:
    TriBuffer() = default;

    bool pull() noexcept {
        if (newFrameReady.exchange(false, std::memory_order_acquire)) {
            readIdx = cleanIdx.exchange(readIdx, std::memory_order_acq_rel);
            return true;
        }
        return false;
    }

    [[nodiscard]] const DataType& getReader() const noexcept {
        return buffers[static_cast<size_t>(readIdx)];
    }

    [[nodiscard]] DataType& getWriter() noexcept {
        return buffers[static_cast<size_t>(writeIdx)];
    }

    void publish() noexcept {
        writeIdx = cleanIdx.exchange(writeIdx, std::memory_order_acq_rel);
        newFrameReady.store(true, std::memory_order_release);
    }

    std::array<DataType, 3>& getBuffers() noexcept {
        return buffers;
    }

private:
    std::array<DataType, 3> buffers{};
    alignas(64) std::atomic<int> cleanIdx{2};
    std::atomic<bool> newFrameReady{false};
    alignas(64) int readIdx{0};
    alignas(64) int writeIdx{1};
};

template <size_t FftOrder = 11> // 2048 points
class SpectrumAnalyzer : private juce::Thread {
public:
    static constexpr size_t FftSize = 1 << FftOrder;
    static constexpr size_t ScopeSize = FftSize / 2;
    static constexpr size_t FifoSize = FftSize * 4;
    static constexpr size_t FifoMask = FifoSize - 1;

    SpectrumAnalyzer()
        : juce::Thread("OpenX_SpectrumAnalyzerWorker"),
          forwardFft(FftOrder),
          window(FftSize, juce::dsp::WindowingMethod::hann)
    {
        ringBuffer.fill(0.0f);
        fftData.fill(0.0f);
        scopeData.fill(-100.0f);
        for (auto& b : triBuffer.getBuffers()) {
            b.fill(-100.0f);
        }
        startThread(juce::Thread::Priority::belowNormal);
    }

    ~SpectrumAnalyzer() override {
        stopThread(2000);
    }

    // Audio thread: Strictly wait-free, lock-free, zero allocation, zero syscall push
    void pushSample(float sample) noexcept {
        const size_t writeIdx = ringWriteIndex.load(std::memory_order_relaxed);
        ringBuffer[writeIdx] = sample;
        ringWriteIndex.store((writeIdx + 1) & FifoMask, std::memory_order_release);
    }

    // GUI thread: Render update (never stalls message thread with FFT)
    void update(float sampleRate, float tiltDbPerOctave = 4.5f) noexcept {
        sampleRate_.store(sampleRate, std::memory_order_relaxed);
        tiltDbPerOctave_.store(tiltDbPerOctave, std::memory_order_relaxed);

        if (triBuffer.pull()) {
            scopeData = triBuffer.getReader();
        }
    }

    [[nodiscard]] std::span<const float, ScopeSize> getScopeData() const noexcept {
        return scopeData;
    }

private:
    void run() override {
        std::array<float, ScopeSize> localScope{};
        localScope.fill(-100.0f);

        while (!threadShouldExit()) {
            wait(16);
            if (threadShouldExit()) break;

            const size_t currentW = ringWriteIndex.load(std::memory_order_acquire);
            for (size_t i = 0; i < FftSize; ++i) {
                const size_t readIdx = (currentW + FifoSize - FftSize + i) & FifoMask;
                fftData[i] = ringBuffer[readIdx];
            }

            window.multiplyWithWindowingTable(fftData.data(), FftSize);
            forwardFft.performFrequencyOnlyForwardTransform(fftData.data());

            const float sr = sampleRate_.load(std::memory_order_relaxed);
            const float effectiveSr = (sr > 0.0f) ? sr : 48000.0f;
            const float binWidth = effectiveSr / static_cast<float>(FftSize);
            constexpr float minDb = -100.0f;
            constexpr float maxDb = 12.0f;
            const float tilt = tiltDbPerOctave_.load(std::memory_order_relaxed);

            for (size_t i = 0; i < ScopeSize; ++i) {
                const float freq = static_cast<float>(i) * binWidth;
                const float rawMagnitude = fftData[i] / static_cast<float>(FftSize);
                float db = juce::Decibels::gainToDecibels(rawMagnitude, minDb);

                if (freq > 20.0f) {
                    const float octavesAbove20 = std::log2(freq / 20.0f);
                    db += octavesAbove20 * (tilt / 3.0f);
                }

                db = std::clamp(db, minDb, maxDb);

                if (db > localScope[i]) {
                    localScope[i] = db;
                } else {
                    localScope[i] = localScope[i] * 0.88f + db * 0.12f;
                }
            }

            triBuffer.getWriter() = localScope;
            triBuffer.publish();
        }
    }

    juce::dsp::FFT forwardFft;
    juce::dsp::WindowingFunction<float> window;
    alignas(64) std::array<float, FifoSize> ringBuffer{};
    alignas(64) std::atomic<size_t> ringWriteIndex{0};
    alignas(64) std::atomic<float> sampleRate_{48000.0f};
    alignas(64) std::atomic<float> tiltDbPerOctave_{4.5f};
    std::array<float, FftSize * 2> fftData{};
    std::array<float, ScopeSize> scopeData{};
    TriBuffer<std::array<float, ScopeSize>> triBuffer{};
};

} // namespace openx::ui
