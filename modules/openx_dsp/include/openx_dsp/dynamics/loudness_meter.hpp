#pragma once

#include <cmath>
#include <numbers>
#include <array>
#include <vector>
#include <atomic>
#include <concepts>
#include <algorithm>
#include <span>

namespace openx::dsp {

template <std::floating_point T>
class BiquadFilter {
public:
    void setCoefficients(T inB0, T inB1, T inB2, T inA1, T inA2) noexcept {
        b0 = inB0; b1 = inB1; b2 = inB2;
        a1 = inA1; a2 = inA2;
    }

    void reset() noexcept {
        s1 = T{0};
        s2 = T{0};
    }

    [[nodiscard]] inline T processSample(T x) noexcept {
        const T y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }

private:
    T b0{1}, b1{0}, b2{0}, a1{0}, a2{0};
    T s1{0}, s2{0};
};

template <std::floating_point T>
class KWeightingFilter {
public:
    void prepare(T sampleRate) noexcept {
        sr = std::max(sampleRate, T{8000.0});
        updateCoefficients();
        reset();
    }

    void reset() noexcept {
        stage1.reset();
        stage2.reset();
    }

    [[nodiscard]] inline T processSample(T x) noexcept {
        const T y1 = stage1.processSample(x);
        return stage2.processSample(y1);
    }

private:
    void updateCoefficients() noexcept {
        if (std::abs(sr - T{48000.0}) < T{1.0}) {
            // ITU-R BS.1770-4 Table 1 exact normalized coefficients at 48 kHz
            stage1.setCoefficients(
                T{1.53512485958697}, T{-2.69169618940638}, T{1.19839281085285},
                T{-1.69065929318241}, T{0.73248077421585}
            );
            stage2.setCoefficients(
                T{1.0}, T{-2.0}, T{1.0},
                T{-1.99004745483398}, T{0.99007225036621}
            );
        } else {
            // Generalized bilinear transform matching BS.1770-4 specifications for arbitrary sample rate
            // Stage 1: High shelf filter (f0 = 1681.97445 Hz, Q = 0.7071752, Gain = +3.99984 dB)
            constexpr T f0_s1 = T{1681.974450955533};
            constexpr T q_s1  = T{0.7071752369554196};
            constexpr T gain_db = T{3.999843853973347};
            const T v_s1 = std::pow(T{10.0}, gain_db / T{20.0});
            const T sqrt_v = std::sqrt(v_s1);
            const T k_s1 = std::tan(std::numbers::pi_v<T> * f0_s1 / sr);
            const T k2_s1 = k_s1 * k_s1;

            const T a0_s1 = T{1.0} + (k_s1 / (q_s1 * sqrt_v)) + k2_s1;
            const T b0_s1 = (v_s1 + (sqrt_v * k_s1 / q_s1) + k2_s1) / a0_s1;
            const T b1_s1 = (T{2.0} * (k2_s1 - v_s1)) / a0_s1;
            const T b2_s1 = (v_s1 - (sqrt_v * k_s1 / q_s1) + k2_s1) / a0_s1;
            const T a1_s1 = (T{2.0} * (k2_s1 - T{1.0})) / a0_s1;
            const T a2_s1 = (T{1.0} - (k_s1 / (q_s1 * sqrt_v)) + k2_s1) / a0_s1;
            stage1.setCoefficients(b0_s1, b1_s1, b2_s1, a1_s1, a2_s1);

            // Stage 2: RLB High-pass filter (f0 = 38.13547 Hz, Q = 0.500327)
            constexpr T f0_s2 = T{38.13547087602444};
            constexpr T q_s2  = T{0.5003270373238773};
            const T k_s2 = std::tan(std::numbers::pi_v<T> * f0_s2 / sr);
            const T k2_s2 = k_s2 * k_s2;

            const T a0_s2 = T{1.0} + (k_s2 / q_s2) + k2_s2;
            const T b0_s2 = T{1.0} / a0_s2;
            const T b1_s2 = T{-2.0} / a0_s2;
            const T b2_s2 = T{1.0} / a0_s2;
            const T a1_s2 = (T{2.0} * (k2_s2 - T{1.0})) / a0_s2;
            const T a2_s2 = (T{1.0} - (k_s2 / q_s2) + k2_s2) / a0_s2;
            stage2.setCoefficients(b0_s2, b1_s2, b2_s2, a1_s2, a2_s2);
        }
    }

    T sr{48000};
    BiquadFilter<T> stage1;
    BiquadFilter<T> stage2;
};

template <std::floating_point T>
class LoudnessMeter {
public:
    static constexpr size_t SubBlockMs = 10;
    static constexpr size_t MomentaryBlocks = 40;  // 400 ms
    static constexpr size_t ShortTermBlocks = 300; // 3000 ms
    static constexpr size_t HistogramBins = 1000;       // -70.0 to +30.0 LUFS in 0.1 LU steps
    static constexpr float HistogramMinLufs = -70.0f;

    void prepare(T sampleRate) noexcept {
        sr = std::max(sampleRate, T{8000.0});
        subBlockSamples = std::max(size_t{1}, static_cast<size_t>(sr * T{0.001} * static_cast<T>(SubBlockMs)));
        kFilterL.prepare(sr);
        kFilterR.prepare(sr);
        reset();
    }

    void reset() noexcept {
        kFilterL.reset();
        kFilterR.reset();
        subBlockEnergyBuffer.fill(T{0});
        subBlockIndex = 0;
        sampleInBlockCounter = 0;
        currentSubBlockEnergy = T{0};

        momentarySum = T{0};
        shortTermSum = T{0};

        histCounts.fill(0);
        histEnergies.fill(0.0);
        totalHistCount = 0;
        totalHistEnergy = 0.0;
        integratedHopCounter = 0;
        resetRequested.store(false, std::memory_order_relaxed);

        momentaryLufs.store(-100.0f, std::memory_order_relaxed);
        shortTermLufs.store(-100.0f, std::memory_order_relaxed);
        integratedLufs.store(-100.0f, std::memory_order_relaxed);
        maxMomentaryLufs.store(-100.0f, std::memory_order_relaxed);
        maxShortTermLufs.store(-100.0f, std::memory_order_relaxed);
    }

    void resetIntegrated() noexcept {
        resetRequested.store(true, std::memory_order_release);
        integratedLufs.store(-100.0f, std::memory_order_relaxed);
        maxMomentaryLufs.store(-100.0f, std::memory_order_relaxed);
        maxShortTermLufs.store(-100.0f, std::memory_order_relaxed);
    }

    inline void processSample(T left, T right) noexcept {
        const T yL = kFilterL.processSample(left);
        const T yR = kFilterR.processSample(right);

        // BS.1770-4 stereo weighting: wL = 1.0, wR = 1.0
        const T energy = yL * yL + yR * yR;
        currentSubBlockEnergy += energy;
        ++sampleInBlockCounter;

        if (sampleInBlockCounter >= subBlockSamples) {
            finishSubBlock();
        }
    }

    [[nodiscard]] float getMomentaryLufs() const noexcept {
        return momentaryLufs.load(std::memory_order_relaxed);
    }

    [[nodiscard]] float getShortTermLufs() const noexcept {
        return shortTermLufs.load(std::memory_order_relaxed);
    }

    [[nodiscard]] float getIntegratedLufs() const noexcept {
        return integratedLufs.load(std::memory_order_relaxed);
    }

    [[nodiscard]] float getMaxMomentaryLufs() const noexcept {
        return maxMomentaryLufs.load(std::memory_order_relaxed);
    }

    [[nodiscard]] float getMaxShortTermLufs() const noexcept {
        return maxShortTermLufs.load(std::memory_order_relaxed);
    }

private:
    void finishSubBlock() noexcept {
        // Average energy in sub-block
        const T subEnergy = currentSubBlockEnergy / static_cast<T>(subBlockSamples);
        currentSubBlockEnergy = T{0};
        sampleInBlockCounter = 0;

        // Circular ring buffer eviction & accumulation
        const size_t oldestShortTermIdx = (subBlockIndex + ShortTermBlocks) % ShortTermBlocks;
        const size_t oldestMomentaryIdx = (subBlockIndex + ShortTermBlocks - MomentaryBlocks) % ShortTermBlocks;

        const T evictedShortTerm = subBlockEnergyBuffer[oldestShortTermIdx];
        const T evictedMomentary = subBlockEnergyBuffer[oldestMomentaryIdx];

        subBlockEnergyBuffer[subBlockIndex] = subEnergy;

        shortTermSum += subEnergy - evictedShortTerm;
        if (shortTermSum < T{0}) shortTermSum = T{0};

        momentarySum += subEnergy - evictedMomentary;
        if (momentarySum < T{0}) momentarySum = T{0};

        subBlockIndex = (subBlockIndex + 1) % ShortTermBlocks;

        // Calculate Momentary LUFS (400 ms)
        const T meanMomentaryEnergy = momentarySum / static_cast<T>(MomentaryBlocks);
        float mVal = -100.0f;
        if (meanMomentaryEnergy > T{1e-10}) {
            mVal = static_cast<float>(T{-0.691} + T{10.0} * std::log10(meanMomentaryEnergy));
            mVal = std::clamp(mVal, -100.0f, 10.0f);
        }
        momentaryLufs.store(mVal, std::memory_order_relaxed);

        // Update Max M
        float curMaxM = maxMomentaryLufs.load(std::memory_order_relaxed);
        if (mVal > curMaxM) {
            maxMomentaryLufs.store(mVal, std::memory_order_relaxed);
        }

        // Calculate Short-Term LUFS (3.0 s)
        const T meanShortTermEnergy = shortTermSum / static_cast<T>(ShortTermBlocks);
        float sVal = -100.0f;
        if (meanShortTermEnergy > T{1e-10}) {
            sVal = static_cast<float>(T{-0.691} + T{10.0} * std::log10(meanShortTermEnergy));
            sVal = std::clamp(sVal, -100.0f, 10.0f);
        }
        shortTermLufs.store(sVal, std::memory_order_relaxed);

        // Update Max S
        float curMaxS = maxShortTermLufs.load(std::memory_order_relaxed);
        if (sVal > curMaxS) {
            maxShortTermLufs.store(sVal, std::memory_order_relaxed);
        }

        // Evaluate Integrated 400ms block every 100ms (every 10 sub-blocks = 75% overlap)
        ++integratedHopCounter;
        if (integratedHopCounter >= 10) {
            integratedHopCounter = 0;
            evaluateIntegratedBlock(meanMomentaryEnergy);
        }
    }

    void evaluateIntegratedBlock(T blockEnergy) noexcept {
        if (resetRequested.exchange(false, std::memory_order_acq_rel)) {
            histCounts.fill(0);
            histEnergies.fill(0.0);
            totalHistCount = 0;
            totalHistEnergy = 0.0;
            integratedLufs.store(-100.0f, std::memory_order_relaxed);
        }

        // BS.1770-4 Absolute gate: -70.0 LKFS
        // z_abs = 10^((-70.0 + 0.691) / 10) ~ 1.172433e-7
        constexpr T zAbsThreshold = T{1.172433e-7};

        if (blockEnergy >= zAbsThreshold) {
            const float blockLufs = static_cast<float>(T{-0.691} + T{10.0} * std::log10(blockEnergy));
            int bin = static_cast<int>((blockLufs - HistogramMinLufs) * 10.0f);
            bin = std::clamp(bin, 0, static_cast<int>(HistogramBins - 1));

            histCounts[static_cast<size_t>(bin)]++;
            histEnergies[static_cast<size_t>(bin)] += static_cast<double>(blockEnergy);
            totalHistCount++;
            totalHistEnergy += static_cast<double>(blockEnergy);
        }

        if (totalHistCount == 0) {
            integratedLufs.store(-100.0f, std::memory_order_relaxed);
            return;
        }

        // Step 1: Average energy of all blocks above absolute threshold (-70 LKFS)
        const double meanPass1 = totalHistEnergy / static_cast<double>(totalHistCount);

        // Step 2: Relative threshold: -10 LU relative to meanPass1
        const double zRelThreshold = meanPass1 * 0.1;
        const float relLufs = static_cast<float>(-0.691 + 10.0 * std::log10(std::max(zRelThreshold, 1e-12)));
        int relBin = static_cast<int>((relLufs - HistogramMinLufs) * 10.0f);
        relBin = std::clamp(relBin, 0, static_cast<int>(HistogramBins - 1));

        // Step 3: Average blocks above relative threshold
        double sumPass2 = 0.0;
        uint64_t countPass2 = 0;
        for (size_t b = static_cast<size_t>(relBin); b < HistogramBins; ++b) {
            sumPass2 += histEnergies[b];
            countPass2 += histCounts[b];
        }

        if (countPass2 == 0) {
            integratedLufs.store(-100.0f, std::memory_order_relaxed);
            return;
        }

        const double finalMeanEnergy = sumPass2 / static_cast<double>(countPass2);
        float iVal = -100.0f;
        if (finalMeanEnergy > 1e-10) {
            iVal = static_cast<float>(-0.691 + 10.0 * std::log10(finalMeanEnergy));
            iVal = std::clamp(iVal, -100.0f, 10.0f);
        }
        integratedLufs.store(iVal, std::memory_order_relaxed);
    }

    T sr{48000};
    size_t subBlockSamples{480};
    size_t sampleInBlockCounter{0};
    T currentSubBlockEnergy{0};

    KWeightingFilter<T> kFilterL;
    KWeightingFilter<T> kFilterR;

    std::array<T, ShortTermBlocks> subBlockEnergyBuffer{};
    size_t subBlockIndex{0};

    T momentarySum{0};
    T shortTermSum{0};

    std::array<uint32_t, HistogramBins> histCounts{};
    std::array<double, HistogramBins> histEnergies{};
    uint64_t totalHistCount{0};
    double totalHistEnergy{0.0};
    size_t integratedHopCounter{0};
    std::atomic<bool> resetRequested{false};

    std::atomic<float> momentaryLufs{-100.0f};
    std::atomic<float> shortTermLufs{-100.0f};
    std::atomic<float> integratedLufs{-100.0f};
    std::atomic<float> maxMomentaryLufs{-100.0f};
    std::atomic<float> maxShortTermLufs{-100.0f};
};

} // namespace openx::dsp
