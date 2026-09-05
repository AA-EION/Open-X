#pragma once

#include <cmath>
#include <array>
#include <concepts>
#include <algorithm>
#include <numbers>
#include <span>
#include "../crossover/linkwitz_riley.hpp"
#include "compressor_engine.hpp"

namespace openx::dsp {

/**
 * @brief High-precision dynamics engine supporting upward and downward compression and expansion.
 *
 * Implements the 4 core dynamics quadrants:
 *  - Downward Compression (Compress mode, Range <= 0): Attenuates signal peaks above threshold
 *  - Upward Compression   (Compress mode, Range > 0):  Boosts low-level signals below threshold
 *  - Downward Expansion   (Expand mode,   Range <= 0): Attenuates low-level signals below threshold (gating)
 *  - Upward Expansion     (Expand mode,   Range > 0):  Boosts signal peaks above threshold (punch enhancement)
 */
template <std::floating_point T>
class MultibandDynamicsEngine {
public:
    enum class DynamicsMode : int {
        Compress = 0,
        Expand   = 1
    };

    struct Parameters {
        DynamicsMode mode{DynamicsMode::Compress};
        T thresholdDb{-18};
        T rangeDb{-12};      // Signed range limit: negative = max attenuation, positive = max boost
        T ratio{2.5};        // 1.0 to 20.0
        T attackMs{20};      // 0.1 to 250 ms
        T releaseMs{100};    // 10 to 1500 ms
        T kneeDb{4};         // 0 to 20 dB (soft knee width)
        T makeupGainDb{0};   // -24 to +24 dB
        bool solo{false};
        bool mute{false};
        bool bypass{false};
    };

    void prepare(T sampleRate) noexcept {
        sr = sampleRate > 0 ? sampleRate : T{48000};
        analyticFollower.prepare(sr);
        setParameters(params);
        reset();
    }

    void reset() noexcept {
        analyticFollower.reset();
        envelope = 0;
        gainReductionLinear = 1;
        currentGainChangeDb = 0;
    }

    void setParameters(const Parameters& p) noexcept {
        params = p;
        attCoeff = std::exp(-T{1} / (std::max(params.attackMs, T{0.05}) * T{0.001} * sr));
        relCoeff = std::exp(-T{1} / (std::max(params.releaseMs, T{0.1}) * T{0.001} * sr));
        makeupLinear = std::pow(T{10}, params.makeupGainDb / T{20});

        const T safeRatio = std::max(params.ratio, T{1.0});
        compSlope = T{1} - (T{1} / safeRatio);
        expSlope = safeRatio - T{1};
    }

    [[nodiscard]] T processSample(T input) noexcept {
        // 1. Instantaneous envelope extraction using quadrature analytic network
        const T instantEnv = analyticFollower.computeInstantaneousEnvelope(input);

        // 2. Ballistics integration
        const T coeff = (instantEnv > envelope) ? attCoeff : relCoeff;
        envelope = coeff * envelope + (T{1} - coeff) * instantEnv;

        if (params.bypass) {
            currentGainChangeDb = 0;
            return input;
        }

        // 3. Convert envelope to dB
        constexpr T minLinear = T{1e-5}; // -100 dBFS floor
        const T envDb = T{20} * std::log10(std::max(envelope, minLinear));

        // 4. Smooth C2 continuous soft knee calculation
        const T halfKnee = params.kneeDb * T{0.5};
        T smoothOvershoot = 0;
        T smoothUndershoot = 0;

        // Overshoot = envDb - threshold
        const T deltaOver = envDb - params.thresholdDb;
        if (params.kneeDb > T{0.01}) {
            if (deltaOver > halfKnee) {
                smoothOvershoot = deltaOver;
            } else if (deltaOver > -halfKnee) {
                const T k = deltaOver + halfKnee;
                smoothOvershoot = (k * k) / (T{2} * params.kneeDb);
            }
        } else {
            smoothOvershoot = std::max(T{0}, deltaOver);
        }

        // Undershoot = threshold - envDb
        const T deltaUnder = params.thresholdDb - envDb;
        if (params.kneeDb > T{0.01}) {
            if (deltaUnder > halfKnee) {
                smoothUndershoot = deltaUnder;
            } else if (deltaUnder > -halfKnee) {
                const T k = deltaUnder + halfKnee;
                smoothUndershoot = (k * k) / (T{2} * params.kneeDb);
            }
        } else {
            smoothUndershoot = std::max(T{0}, deltaUnder);
        }

        // 5. Dynamic Gain Computer based on Mode and Range
        T targetGainChangeDb = 0;
        if (params.mode == DynamicsMode::Compress) {
            if (params.rangeDb <= T{0}) {
                // Downward Compression (peaks above threshold attenuated)
                const T rawDb = -compSlope * smoothOvershoot;
                targetGainChangeDb = std::max(rawDb, params.rangeDb);
            } else {
                // Upward Compression (quiet signals below threshold boosted)
                const T rawDb = compSlope * smoothUndershoot;
                targetGainChangeDb = std::min(rawDb, params.rangeDb);
            }
        } else { // DynamicsMode::Expand
            if (params.rangeDb <= T{0}) {
                // Downward Expansion (quiet signals below threshold attenuated)
                const T rawDb = -expSlope * smoothUndershoot;
                targetGainChangeDb = std::max(rawDb, params.rangeDb);
            } else {
                // Upward Expansion (peaks above threshold boosted)
                const T rawDb = expSlope * smoothOvershoot;
                targetGainChangeDb = std::min(rawDb, params.rangeDb);
            }
        }

        // 6. Direct gain calculation strictly obeying attack/release ballistics
        gainReductionLinear = std::pow(T{10}, targetGainChangeDb / T{20});
        currentGainChangeDb = targetGainChangeDb;

        // 7. Output with makeup gain
        return input * gainReductionLinear * makeupLinear;
    }

    [[nodiscard]] T getGainChangeDb() const noexcept {
        return currentGainChangeDb;
    }

private:
    T sr{48000};
    Parameters params{};
    T attCoeff{0}, relCoeff{0};
    T compSlope{0.6}, expSlope{1.5};
    T envelope{0};
    T gainReductionLinear{1};
    T makeupLinear{1};
    T currentGainChangeDb{0};
    AnalyticEnvelopeFollower<T> analyticFollower;
};

/**
 * @brief Phase-aligned multiband dynamics processor supporting N configurable bands.
 */
template <std::floating_point T, size_t NumBands = 4>
    requires (NumBands >= 2 && NumBands <= 8)
class MultibandProcessor {
public:
    static constexpr size_t NumCrossovers = NumBands - 1;

    struct Parameters {
        std::array<T, NumCrossovers> crossoverFrequenciesHz{};
        std::array<typename MultibandDynamicsEngine<T>::Parameters, NumBands> bands;
    };

    void prepare(T sampleRate) noexcept {
        sr = sampleRate > 0 ? sampleRate : T{48000};
        splitter.prepare(sr);
        for (auto& engine : bandEngines) {
            engine.prepare(sr);
        }
        setParameters(params);
        reset();
    }

    void reset() noexcept {
        splitter.reset();
        for (auto& engine : bandEngines) {
            engine.reset();
        }
    }

    void setParameters(const Parameters& p) noexcept {
        params = p;

        // Ensure crossover frequencies are strictly ascending and separated
        std::array<T, NumCrossovers> safeCrossovers = params.crossoverFrequenciesHz;
        T minCutoff = T{20};
        const T maxCutoff = sr * T{0.48};

        for (size_t i = 0; i < NumCrossovers; ++i) {
            safeCrossovers[i] = std::clamp(safeCrossovers[i], minCutoff, maxCutoff);
            minCutoff = safeCrossovers[i] * T{1.08} + T{5};
        }

        T upperBound = maxCutoff;
        for (int i = static_cast<int>(NumCrossovers) - 1; i >= 0; --i) {
            const size_t idx = static_cast<size_t>(i);
            if (safeCrossovers[idx] > upperBound) {
                safeCrossovers[idx] = upperBound;
            }
            upperBound = safeCrossovers[idx] * T{0.92} - T{5};
            if (upperBound < T{20}) upperBound = T{20};
        }

        splitter.setFrequencies(safeCrossovers);
        for (size_t b = 0; b < NumBands; ++b) {
            bandEngines[b].setParameters(params.bands[b]);
        }
    }

    [[nodiscard]] T processSample(T input) noexcept {
        // 1. Check if any band is soloed
        bool anySoloed = false;
        for (size_t b = 0; b < NumBands; ++b) {
            if (params.bands[b].solo) {
                anySoloed = true;
                break;
            }
        }

        // 2. Split input into phase-aligned bands
        std::array<T, NumBands> splitBands{};
        splitter.process(input, splitBands);

        // 3. Process each band through its dynamics engine
        T output = 0;
        for (size_t b = 0; b < NumBands; ++b) {
            const auto& bp = params.bands[b];

            if (bp.mute || (anySoloed && !bp.solo)) {
                // Keep engine ballistics updated smoothly even when band is silent
                bandEngines[b].processSample(splitBands[b]);
                continue;
            }

            if (bp.bypass) {
                bandEngines[b].processSample(splitBands[b]);
                output += splitBands[b];
                continue;
            }

            output += bandEngines[b].processSample(splitBands[b]);
        }

        return output;
    }

    [[nodiscard]] T getBandGainChangeDb(size_t bandIdx) const noexcept {
        if (bandIdx < NumBands) {
            return bandEngines[bandIdx].getGainChangeDb();
        }
        return T{0};
    }

private:
    T sr{48000};
    Parameters params{};
    PhaseAlignedMultibandSplitter<T, NumBands> splitter;
    std::array<MultibandDynamicsEngine<T>, NumBands> bandEngines;
};

/**
 * @brief Legacy 3-band processor kept for full backwards compatibility with unit tests.
 */
template <std::floating_point T>
class MultibandProcessor3Band {
public:
    struct BandParameters {
        T thresholdDb{-20};
        T ratio{3};
        T attackMs{20};
        T releaseMs{100};
        T gainDb{0};
    };

    struct Parameters {
        T lowMidCrossoverHz{250.0};
        T midHighCrossoverHz{3500.0};
        std::array<BandParameters, 3> bands;
    };

    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        splitter.prepare(sr);
        for (auto& comp : bandCompressors) comp.prepare(sr);
        reset();
    }

    void reset() noexcept {
        splitter.reset();
        for (auto& comp : bandCompressors) comp.reset();
    }

    void setParameters(const Parameters& p) noexcept {
        params = p;
        const std::array<T, 2> cutoffs{ params.lowMidCrossoverHz, params.midHighCrossoverHz };
        splitter.setFrequencies(cutoffs);

        for (size_t b = 0; b < 3; ++b) {
            typename CompressorEngine<T>::Parameters cp;
            cp.thresholdDb = params.bands[b].thresholdDb;
            cp.ratio = params.bands[b].ratio;
            cp.attackMs = params.bands[b].attackMs;
            cp.releaseMs = params.bands[b].releaseMs;
            cp.makeupGainDb = params.bands[b].gainDb;
            cp.kneeDb = T{4};
            cp.transientPunch = T{0.2};
            bandCompressors[b].setParameters(cp);
        }
    }

    [[nodiscard]] T processSample(T input) noexcept {
        std::array<T, 3> splitBands{};
        splitter.process(input, splitBands);

        T output = 0;
        for (size_t b = 0; b < 3; ++b) {
            output += bandCompressors[b].processSample(splitBands[b]);
        }

        return output;
    }

private:
    T sr{44100};
    Parameters params{};
    PhaseAlignedMultibandSplitter<T, 3> splitter;
    std::array<CompressorEngine<T>, 3> bandCompressors;
};

} // namespace openx::dsp
