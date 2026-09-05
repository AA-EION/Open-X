#pragma once

#include <cmath>
#include <numbers>
#include <array>
#include <concepts>
#include <algorithm>
#include <span>
#include "true_peak_detector.hpp"
#include "dc_filter.hpp"

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
        const T omega = T{2} * std::numbers::pi_v<T> * std::clamp(centerFreqHz, T{20}, sr * T{0.45}) / sr;
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

    enum class Style : int {
        Transparent = 0,
        Punchy,
        Dynamic,
        Allround,
        Aggressive,
        Modern,
        Bus,
        Safe,
        Count
    };

    struct Parameters {
        T ceilingDb{-0.1};
        T thresholdDb{0.0};
        T releaseMs{50.0};
        bool enablePhaseDispersion{true};
        T dispersionFreqHz{120.0};
        T dispersionQ{0.7071};
        // Pro-L 2 expanded capabilities
        Style style{Style::Transparent};
        T attackMs{2.0};
        T lookaheadMs{0.0}; // 0.0 or <= 0 uses full buffer LatencySamples (exact backward compat)
        bool enableTruePeak{true};
        bool enableDcFilter{true};
        T transientLink{1.0}; // 0.0 to 1.0
        T releaseLink{1.0};   // 0.0 to 1.0
    };

    void prepare(T sampleRate) noexcept {
        sr = std::max(sampleRate, T{1000.0});
        tpDetector.prepare(sr);
        phaseDispersion.prepare(sr);
        dcBlocker.prepare(sr);
        reset();
    }

    void reset() noexcept {
        delayBuffer.fill(0);
        gainDelayBuffer.fill(1);
        minGainTracker.clear();
        tpDetector.reset();
        phaseDispersion.reset();
        dcBlocker.reset();
        writeIndex = 0;
        gainWriteIndex = 0;
        sampleCounter = 0;
        currentGain = 1;
        fastGain = 1;
        slowGain = 1;
        rmsEnergy = 0;
        peakEnergy = 0;
        lastDelayedSample = 0;
    }

    void setParameters(const Parameters& p) noexcept {
        params = p;
        ceilingLinear = std::pow(T{10}, params.ceilingDb / T{20});
        thresholdLinear = std::pow(T{10}, params.thresholdDb / T{20});

        // Determine active lookahead delay
        if (params.lookaheadMs <= T{0.0}) {
            activeLookaheadSamples = LatencySamples;
        } else {
            const size_t reqSamples = static_cast<size_t>(params.lookaheadMs * T{0.001} * sr);
            activeLookaheadSamples = std::clamp(reqSamples, size_t{4}, LatencySamples);
        }

        // Release time ballistics based on style
        const T baseRelMs = std::max(params.releaseMs, T{1.0});
        T fastMs = baseRelMs * T{0.2};
        T slowMs = baseRelMs;

        switch (params.style) {
            case Style::Transparent:
                fastMs = std::clamp(baseRelMs * T{0.2}, T{5.0}, T{30.0});
                slowMs = std::max(baseRelMs, T{20.0});
                break;
            case Style::Punchy:
                fastMs = std::clamp(baseRelMs * T{0.15}, T{8.0}, T{40.0});
                slowMs = std::max(baseRelMs * T{0.8}, T{30.0});
                break;
            case Style::Dynamic:
                fastMs = std::clamp(baseRelMs * T{0.2}, T{5.0}, T{25.0});
                slowMs = baseRelMs;
                break;
            case Style::Allround:
                fastMs = std::clamp(baseRelMs * T{0.25}, T{10.0}, T{50.0});
                slowMs = baseRelMs;
                break;
            case Style::Aggressive:
                fastMs = std::clamp(baseRelMs * T{0.1}, T{3.0}, T{15.0});
                slowMs = std::max(baseRelMs * T{0.6}, T{15.0});
                break;
            case Style::Modern:
                fastMs = std::clamp(baseRelMs * T{0.2}, T{5.0}, T{30.0});
                slowMs = baseRelMs;
                break;
            case Style::Bus:
                fastMs = std::max(T{20.0}, baseRelMs * T{0.4});
                slowMs = baseRelMs * T{1.2};
                break;
            case Style::Safe:
                fastMs = std::max(T{15.0}, baseRelMs * T{0.3});
                slowMs = baseRelMs * T{1.5};
                break;
            default:
                break;
        }

        relFastCoeff = std::exp(-T{1} / (fastMs * T{0.001} * sr));
        relSlowCoeff = std::exp(-T{1} / (slowMs * T{0.001} * sr));
        baseRelCoeff = std::exp(-T{1} / (baseRelMs * T{0.001} * sr));

        // Attack shaping coefficient (lookahead transition)
        if (params.attackMs > T{0.0}) {
            attCoeff = std::exp(-T{1} / (std::max(params.attackMs, T{0.05}) * T{0.001} * sr));
        } else {
            attCoeff = T{0.0};
        }

        phaseDispersion.setDispersion(params.dispersionFreqHz, params.dispersionQ);
    }

    [[nodiscard]] inline T preFilter(T input) noexcept {
        T x = input;
        if (params.enableDcFilter) {
            x = dcBlocker.processSample(x);
        }
        if (params.enablePhaseDispersion) {
            x = phaseDispersion.processSample(x);
        }
        return x;
    }

    [[nodiscard]] inline T detectPeakFromDispersed(T dispersedInput) noexcept {
        return params.enableTruePeak ? tpDetector.processInstantaneous(dispersedInput) : std::abs(dispersedInput);
    }

    [[nodiscard]] inline T detectPeak(T inputOrDispersed) noexcept {
        return detectPeakFromDispersed(inputOrDispersed);
    }

    [[nodiscard]] inline T computeDesiredGain(T peak) const noexcept {
        return (peak > ceilingLinear && peak > T{0}) ? (ceilingLinear / peak) : T{1.0};
    }

    [[nodiscard]] inline T processWithTargetGain(T dispersedInput, T externalTargetGain) noexcept {
        // 1. Lookahead lead delay (aligns lookahead gain ramp with fixed LatencySamples audio delay)
        const size_t leadDelay = (LatencySamples >= activeLookaheadSamples)
                               ? (LatencySamples - activeLookaheadSamples)
                               : 0;
        gainDelayBuffer[gainWriteIndex] = externalTargetGain;
        const size_t gainReadIndex = (gainWriteIndex + LookaheadSamples - leadDelay) & BufferMask;
        const T delayedTargetGain = gainDelayBuffer[gainReadIndex];
        gainWriteIndex = (gainWriteIndex + 1) & BufferMask;

        minGainTracker.push(delayedTargetGain, sampleCounter++, activeLookaheadSamples);
        const T targetGain = minGainTracker.getMin();

        // 2. Crest factor tracking for Style::Dynamic
        const T absIn = std::abs(dispersedInput);
        rmsEnergy = T{0.999} * rmsEnergy + T{0.001} * (absIn * absIn);
        if (absIn > peakEnergy) {
            peakEnergy = absIn;
        } else {
            peakEnergy = T{0.9995} * peakEnergy;
        }

        // 3. Gain ballistics: style-dependent attack shaping and dual-stage release
        if (targetGain < currentGain) {
            if (attCoeff > T{0.0}) {
                currentGain = targetGain + (currentGain - targetGain) * attCoeff;
            } else {
                currentGain = targetGain;
            }
            fastGain = currentGain;
            slowGain = currentGain;
        } else {
            fastGain = relFastCoeff * fastGain + (T{1} - relFastCoeff) * targetGain;
            slowGain = relSlowCoeff * slowGain + (T{1} - relSlowCoeff) * targetGain;

            switch (params.style) {
                case Style::Transparent:
                    currentGain = T{0.65} * fastGain + T{0.35} * slowGain;
                    break;
                case Style::Punchy:
                    currentGain = T{0.80} * fastGain + T{0.20} * slowGain;
                    break;
                case Style::Dynamic: {
                    const T rmsVal = std::sqrt(std::max(rmsEnergy, T{1e-8}));
                    const T crest = peakEnergy / rmsVal;
                    const T crestWeight = std::clamp((crest - T{2.0}) / T{4.0}, T{0.0}, T{1.0});
                    const T dynFastWeight = T{0.4} + T{0.45} * crestWeight;
                    currentGain = dynFastWeight * fastGain + (T{1.0} - dynFastWeight) * slowGain;
                    break;
                }
                case Style::Allround:
                    currentGain = baseRelCoeff * currentGain + (T{1} - baseRelCoeff) * targetGain;
                    break;
                case Style::Aggressive:
                    currentGain = fastGain;
                    break;
                case Style::Modern:
                    currentGain = T{0.5} * fastGain + T{0.5} * slowGain;
                    break;
                case Style::Bus:
                case Style::Safe:
                    currentGain = slowGain;
                    break;
                default:
                    currentGain = baseRelCoeff * currentGain + (T{1} - baseRelCoeff) * targetGain;
                    break;
            }
        }

        // 4. Audio circular delay buffer (fixed delay = LatencySamples for perfect DAW PDC sync)
        delayBuffer[writeIndex] = dispersedInput;
        const size_t readIndex = (writeIndex + LookaheadSamples - LatencySamples) & BufferMask;
        T delayedSample = delayBuffer[readIndex];
        writeIndex = (writeIndex + 1) & BufferMask;
        lastDelayedSample = delayedSample;

        // 5. Style-specific soft-knee saturation (Aggressive style)
        if (params.style == Style::Aggressive) {
            const T satThreshold = ceilingLinear * T{0.85};
            const T absDel = std::abs(delayedSample);
            if (absDel > satThreshold && ceilingLinear > satThreshold) {
                const T normExcess = (absDel - satThreshold) / (ceilingLinear - satThreshold);
                const T tanhCurve = std::tanh(normExcess);
                const T compressed = satThreshold + (ceilingLinear - satThreshold) * tanhCurve;
                delayedSample = (delayedSample >= T{0}) ? compressed : -compressed;
            }
        }

        // 6. Strict ceiling enforcement guarantee (0.00 dB overshoot tolerance)
        const T absSample = std::abs(delayedSample);
        if (absSample * currentGain > ceilingLinear && absSample > T{0}) {
            currentGain = ceilingLinear / absSample;
        }

        return std::clamp(delayedSample * currentGain, -ceilingLinear, ceilingLinear);
    }

    [[nodiscard]] inline T processSampleWithTargetGain(T inputOrDispersed, T externalTargetGain) noexcept {
        return processWithTargetGain(inputOrDispersed, externalTargetGain);
    }

    [[nodiscard]] T processSample(T input) noexcept {
        const T x = preFilter(input);
        const T peak = detectPeakFromDispersed(x);
        const T desiredGain = computeDesiredGain(peak);
        return processWithTargetGain(x, desiredGain);
    }

    [[nodiscard]] inline T getLastDelayedSample() const noexcept {
        return lastDelayedSample;
    }

    [[nodiscard]] T getGainReductionDb() const noexcept {
        return T{-20} * std::log10(std::max(currentGain, T{1e-5}));
    }

    [[nodiscard]] static constexpr size_t getLatencySamples() noexcept {
        return LatencySamples;
    }

    [[nodiscard]] size_t getActiveLookaheadSamples() const noexcept {
        return activeLookaheadSamples;
    }

private:
    T sr{44100};
    Parameters params{};
    T ceilingLinear{1};
    T thresholdLinear{1};
    T relFastCoeff{0.999};
    T relSlowCoeff{0.999};
    T baseRelCoeff{0.999};
    T attCoeff{0.0};
    T currentGain{1};
    T fastGain{1};
    T slowGain{1};
    T rmsEnergy{0};
    T peakEnergy{0};

    size_t activeLookaheadSamples{LatencySamples};
    size_t writeIndex{0};
    size_t gainWriteIndex{0};
    size_t sampleCounter{0};
    T lastDelayedSample{0};

    std::array<T, LookaheadSamples> delayBuffer{};
    std::array<T, LookaheadSamples> gainDelayBuffer{};
    CircularMonotonicQueue<T, LookaheadSamples * 2> minGainTracker{};
    TruePeakDetector<T, 1024> tpDetector;
    PhaseDispersionNetwork<T> phaseDispersion;
    DcBlocker<T> dcBlocker;
};

} // namespace openx::dsp
