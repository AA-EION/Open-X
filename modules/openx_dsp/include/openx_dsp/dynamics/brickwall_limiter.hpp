#pragma once

#include <cmath>
#include <numbers>
#include <array>
#include <concepts>
#include <algorithm>
#include <span>
#include "true_peak_detector.hpp"

namespace openx::dsp {

template <std::floating_point T>
class PhaseDispersionNetwork {
public:
    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        reset();
    }

    void reset() noexcept {
        s1 = 0; s2 = 0;
    }

    void setDispersion(T centerFreqHz, T q) noexcept {
        const T omega = std::numbers::pi_v<T> * std::clamp(centerFreqHz, T{20}, sr * T{0.45}) / sr;
        const T alpha = std::sin(omega) / (T{2} * std::max(q, T{0.1}));
        const T cosw = std::cos(omega);

        const T a0 = T{1} + alpha;
        b0 = (T{1} - alpha) / a0;
        b1 = (T{-2} * cosw) / a0;
        b2 = T{1};
        a1 = (T{-2} * cosw) / a0;
        a2 = (T{1} - alpha) / a0;
    }

    [[nodiscard]] T processSample(T x) noexcept {
        const T y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }

private:
    T sr{44100};
    T b0{1}, b1{0}, b2{0}, a1{0}, a2{0};
    T s1{0}, s2{0};
};

template <typename ValType, size_t Cap>
class CircularMonotonicQueue {
public:
    void clear() noexcept {
        head = 0;
        tail = 0;
        count = 0;
    }

    void push(ValType val, size_t index, size_t windowSize) noexcept {
        // 1. Evict expired entries older than the window
        while (count > 0) {
            if (index > windowSize && data[head].index < index - windowSize) {
                head = (head + 1) % Cap;
                --count;
            } else {
                break;
            }
        }

        // 2. Maintain monotonicity (pop elements greater than or equal to incoming val)
        while (count > 0) {
            const size_t lastIdx = (tail + Cap - 1) % Cap;
            if (data[lastIdx].val >= val) {
                tail = lastIdx;
                --count;
            } else {
                break;
            }
        }

        // 3. Push new entry
        data[tail] = Entry{ val, index };
        tail = (tail + 1) % Cap;
        ++count;
    }

    [[nodiscard]] ValType getMin() const noexcept {
        return (count > 0) ? data[head].val : ValType{1};
    }

private:
    struct Entry {
        ValType val{1};
        size_t index{0};
    };
    std::array<Entry, Cap> data{};
    size_t head{0};
    size_t tail{0};
    size_t count{0};
};

template <std::floating_point T, size_t LookaheadSamples = 256>
    requires ((LookaheadSamples & (LookaheadSamples - 1)) == 0)
class BrickwallLimiter {
public:
    static constexpr size_t BufferMask = LookaheadSamples - 1;
    static constexpr size_t LatencySamples = LookaheadSamples - 1;

    struct Parameters {
        T ceilingDb{-0.1};
        T thresholdDb{0.0};
        T releaseMs{50.0};
        bool enablePhaseDispersion{true};
        T dispersionFreqHz{120.0};
        T dispersionQ{0.7071};
    };

    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        tpDetector.prepare(sr);
        phaseDispersion.prepare(sr);
        reset();
    }

    void reset() noexcept {
        delayBuffer.fill(0);
        minGainTracker.clear();
        tpDetector.reset();
        phaseDispersion.reset();
        writeIndex = 0;
        sampleCounter = 0;
        currentGain = 1;
    }

    void setParameters(const Parameters& p) noexcept {
        params = p;
        ceilingLinear = std::pow(T{10}, params.ceilingDb / T{20});
        thresholdLinear = std::pow(T{10}, params.thresholdDb / T{20});
        relCoeff = std::exp(-T{1} / (std::max(params.releaseMs, T{1.0}) * T{0.001} * sr));
        phaseDispersion.setDispersion(params.dispersionFreqHz, params.dispersionQ);
    }

    [[nodiscard]] T processSample(T input) noexcept {
        // 1. CO-PDN: Asymmetric Waveform Re-centering via All-pass Phase Rotation
        const T dispersedInput = params.enablePhaseDispersion ? phaseDispersion.processSample(input) : input;

        // 2. Inter-Sample Peak Estimation via 4x Polyphase Detector on non-delayed signal
        const T peak = tpDetector.processSample(dispersedInput);

        // 3. Compute Required Gain Attenuation for Peak to meet Ceiling in advance
        T desiredGain = 1;
        if (peak > ceilingLinear) {
            desiredGain = ceilingLinear / peak;
        }

        // 4. Lookahead sliding minimum gain over the delay horizon
        minGainTracker.push(desiredGain, sampleCounter++, LatencySamples);
        const T targetGain = minGainTracker.getMin();

        // 5. Gain ballistics: instant lookahead attack, smooth release
        if (targetGain < currentGain) {
            currentGain = targetGain;
        } else {
            currentGain = relCoeff * currentGain + (T{1} - relCoeff) * targetGain;
        }

        // 6. Write audio into circular delay buffer (delayed by LookaheadSamples - 1)
        delayBuffer[writeIndex] = dispersedInput;
        const size_t readIndex = (writeIndex + 1) & BufferMask;
        const T delayedSample = delayBuffer[readIndex];
        writeIndex = (writeIndex + 1) & BufferMask;

        // 7. Strict ceiling enforcement guarantee (0.00 dB overshoot tolerance)
        const T absSample = std::abs(delayedSample);
        if (absSample * currentGain > ceilingLinear && absSample > T{0}) {
            currentGain = ceilingLinear / absSample;
        }

        const T out = std::clamp(delayedSample * currentGain, -ceilingLinear, ceilingLinear);
        return out;
    }

    [[nodiscard]] T getGainReductionDb() const noexcept {
        return T{-20} * std::log10(std::max(currentGain, T{1e-5}));
    }

    [[nodiscard]] static constexpr size_t getLatencySamples() noexcept {
        return LatencySamples;
    }

private:
    T sr{44100};
    Parameters params{};
    T ceilingLinear{1};
    T thresholdLinear{1};
    T relCoeff{0.999};
    T currentGain{1};
    size_t writeIndex{0};
    size_t sampleCounter{0};

    std::array<T, LookaheadSamples> delayBuffer{};
    CircularMonotonicQueue<T, LookaheadSamples * 2> minGainTracker{};
    TruePeakDetector<T, 1024> tpDetector;
    PhaseDispersionNetwork<T> phaseDispersion;
};

} // namespace openx::dsp
