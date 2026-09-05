#pragma once

#include <cmath>
#include <array>
#include <concepts>
#include <algorithm>
#include <span>

namespace openx::dsp {

template <std::floating_point T, size_t BufferSize = 1024>
    requires ((BufferSize & (BufferSize - 1)) == 0)
class TruePeakDetector {
public:
    static constexpr size_t BufferMask = BufferSize - 1;
    static constexpr size_t Oversampling = 4;
    static constexpr size_t PolyphaseTaps = 12; // 48 total taps / 4 phases

    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        reset();
    }

    void reset() noexcept {
        buffer.fill(0);
        writeIndex = 0;
        currentPeak = 0;
    }

    void setReleaseTime(T releaseMs) noexcept {
        decayCoeff = std::exp(-T{1} / (std::max(releaseMs, T{1.0}) * T{0.001} * sr));
    }

    [[nodiscard]] T processSample(T input) noexcept {
        // Push sample into power-of-two circular ring buffer
        buffer[writeIndex] = input;

        // ITU-R BS.1770-4 Table 1 Polyphase filter bank (Phase 0 = direct delay, phases 1-3 interpolated)
        // Static normalized coefficients for 4x interpolation
        static constexpr std::array<std::array<T, PolyphaseTaps>, Oversampling> PolyphaseMatrix{{
            // Phase 0: Identity impulse at tap 5
            { 0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 0, 0 },
            // Phase 1 (+0.25 phase delay)
            { -0.0017,  0.0076, -0.0234,  0.0638, -0.1652,  0.8967,  0.2743, -0.0827,  0.0357, -0.0152,  0.0055, -0.0013 },
            // Phase 2 (+0.50 phase delay - Nyquist symmetric)
            { -0.0029,  0.0125, -0.0384,  0.1065, -0.3168,  0.6402,  0.6402, -0.3168,  0.1065, -0.0384,  0.0125, -0.0029 },
            // Phase 3 (+0.75 phase delay)
            { -0.0013,  0.0055, -0.0152,  0.0357, -0.0827,  0.2743,  0.8967, -0.1652,  0.0638, -0.0234,  0.0076, -0.0017 }
        }};

        std::array<T, Oversampling> oversampledSamples{};
        T maxSampleInSubgrid = 0;
        size_t maxIndex = 0;

        for (size_t phase = 0; phase < Oversampling; ++phase) {
            T acc = 0;
            for (size_t tap = 0; tap < PolyphaseTaps; ++tap) {
                const size_t readIdx = (writeIndex - tap + BufferSize) & BufferMask;
                acc += buffer[readIdx] * PolyphaseMatrix[phase][tap];
            }
            oversampledSamples[phase] = acc;
            const T absVal = std::abs(acc);
            if (absVal > maxSampleInSubgrid) {
                maxSampleInSubgrid = absVal;
                maxIndex = phase;
            }
        }

        // Sub-sample parabolic interpolation around highest point in oversampled grid
        T truePeakEstimate = maxSampleInSubgrid;
        if (maxIndex > 0 && maxIndex < Oversampling - 1) {
            const T ym1 = std::abs(oversampledSamples[maxIndex - 1]);
            const T y0  = std::abs(oversampledSamples[maxIndex]);
            const T yp1 = std::abs(oversampledSamples[maxIndex + 1]);
            const T denom = T{2} * (T{2} * y0 - ym1 - yp1);
            if (std::abs(denom) > T{1e-8}) {
                const T delta = (yp1 - ym1) / denom;
                truePeakEstimate = y0 + T{0.125} * (yp1 - ym1) * delta;
            }
        }

        // Peak hold and release tracking
        if (truePeakEstimate > currentPeak) {
            currentPeak = truePeakEstimate;
        } else {
            currentPeak = currentPeak * decayCoeff;
        }

        writeIndex = (writeIndex + 1) & BufferMask;
        return currentPeak;
    }

    [[nodiscard]] T getCurrentPeakDb() const noexcept {
        constexpr T minDbLinear = T{1e-6};
        return T{20} * std::log10(std::max(currentPeak, minDbLinear));
    }

private:
    T sr{44100};
    T decayCoeff{0.999};
    T currentPeak{0};
    size_t writeIndex{0};
    alignas(64) std::array<T, BufferSize> buffer{};
};

} // namespace openx::dsp
