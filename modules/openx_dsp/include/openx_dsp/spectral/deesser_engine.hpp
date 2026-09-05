#pragma once

#include <cmath>
#include <numbers>
#include <array>
#include <concepts>
#include <algorithm>
#include "../eq/dynamic_biquad_engine.hpp"

namespace openx::dsp {

template <std::floating_point T, size_t LpcOrder = 8>
class LpcFormantTracker {
public:
    void prepare(T sampleRate) noexcept {
        sr = (sampleRate > T{0}) ? sampleRate : T{48000};
        lambda = std::exp(-T{1} / (T{0.004} * sr));
        alpha = std::exp(-T{1} / (T{0.003} * sr));
        zcrAlpha = T{1} - std::exp(-T{1} / (T{0.002} * sr));
        reset();
    }

    void reset() noexcept {
        reflectionCoeffs.fill(0);
        backwardStates.fill(0);
        energy.fill(T{1e-4});
        inputEnergy = T{1e-4};
        residualEnergy = T{1e-4};
        prevSample = 0;
        zcrSmoothed = 0;
    }

    // Burg lattice algorithm step: online normalized gradient adaptation of reflection coefficients
    [[nodiscard]] T filterAndExtractResidual(T x) noexcept {
        T f = x;
        T b = x;

        for (size_t m = 0; m < LpcOrder; ++m) {
            const T prevB = backwardStates[m];
            const T newF = f + reflectionCoeffs[m] * prevB;
            const T newB = prevB + reflectionCoeffs[m] * f;

            // Normalized LMS energy tracking per stage (prevents hard clamping and instability on loud speech)
            energy[m] = lambda * energy[m] + (T{1} - lambda) * (f * f + prevB * prevB);
            energy[m] = std::max(energy[m], T{1e-4});

            constexpr T mu = T{0.005};
            constexpr T eps = T{1e-6};
            const T normFactor = mu / (energy[m] + eps);
            reflectionCoeffs[m] = std::clamp(reflectionCoeffs[m] - normFactor * (f * prevB), T{-0.98}, T{0.98});

            backwardStates[m] = b;

            f = newF;
            b = newB;
        }

        // Short-term energy of input and residual
        inputEnergy = alpha * inputEnergy + (T{1} - alpha) * (x * x);
        residualEnergy = alpha * residualEnergy + (T{1} - alpha) * (f * f);
        inputEnergy = std::max(inputEnergy, T{1e-5});
        residualEnergy = std::max(residualEnergy, T{1e-5});

        // Zero-crossing rate detection
        const T isCross = ((x > 0 && prevSample <= 0) || (x < 0 && prevSample >= 0)) ? T{1} : T{0};
        prevSample = x;
        zcrSmoothed = (T{1} - zcrAlpha) * zcrSmoothed + zcrAlpha * isCross;

        return f; // Prediction error residual e[n] (contains turbulent noise)
    }

    // Returns vocal sibilance likelihood in [0, 1]
    // Voiced vowels have strong formants: predictor cancels them, residual is low (< 0.2) and ZCR is low.
    // Sibilance (s, z, sh, ch) has high unvoiced turbulence: residual ratio is high (> 0.5) and ZCR is high.
    [[nodiscard]] T getSibilanceLikelihood() const noexcept {
        constexpr T eps = T{1e-6};
        const T residualRatio = residualEnergy / (inputEnergy + eps);
        // Map residual ratio [0.2, 0.75] -> [0.0, 1.0]
        const T lpcTurbulence = std::clamp((residualRatio - T{0.2}) / T{0.55}, T{0}, T{1});
        // Map ZCR [0.06, 0.26] -> [0.0, 1.0]
        const T zcrScore = std::clamp((zcrSmoothed - T{0.06}) / T{0.20}, T{0}, T{1});

        return std::clamp(T{0.55} * lpcTurbulence + T{0.45} * zcrScore, T{0}, T{1});
    }

    [[nodiscard]] T getResidualEnergy() const noexcept { return residualEnergy; }
    [[nodiscard]] T getInputEnergy() const noexcept { return inputEnergy; }
    [[nodiscard]] T getZcr() const noexcept { return zcrSmoothed; }

private:
    T sr{44100};
    T lambda{0.995};
    T alpha{0.992};
    T zcrAlpha{0.010};
    std::array<T, LpcOrder> reflectionCoeffs{};
    std::array<T, LpcOrder> backwardStates{};
    std::array<T, LpcOrder> energy{};
    T inputEnergy{1e-4};
    T residualEnergy{1e-4};
    T prevSample{0};
    T zcrSmoothed{0};
};

template <std::floating_point T>
class DeEsserEngine {
public:
    enum class DetectionMode : int {
        SingleVocal = 0,
        Allround = 1
    };

    enum class ProcessingBandMode : int {
        WideBand = 0,
        SplitBand = 1
    };

    enum class SidechainFilterType : int {
        Bandpass = 0,
        Highpass = 1
    };

    enum class AuditionMode : int {
        Normal = 0,
        Sidechain = 1,
        Delta = 2
    };

    enum class StereoProcessingMode : int {
        Stereo = 0,
        Mid = 1,
        Side = 2
    };

    struct Parameters {
        T frequencyHz{6000.0};
        T thresholdDb{-24.0};
        T reductionDb{-12.0};
        T bandwidthQ{2.0};
        bool useLpcResidualSubtraction{true};
        DetectionMode detectionMode{DetectionMode::SingleVocal};
        ProcessingBandMode bandMode{ProcessingBandMode::SplitBand};
        SidechainFilterType filterType{SidechainFilterType::Bandpass};
        AuditionMode auditionMode{AuditionMode::Normal};
        T lookaheadMs{5.0};
        T stereoLink{1.0}; // 0.0 = dual-mono, 1.0 = 100% linked
        StereoProcessingMode stereoMode{StereoProcessingMode::Stereo};
    };

    static constexpr size_t MaxDelay = 8192;
    static constexpr size_t DelayMask = MaxDelay - 1;

    void prepare(T sampleRate) noexcept {
        sr = (sampleRate > T{0}) ? sampleRate : T{48000};
        lpcTracker.prepare(sr);
        delayedLpcTracker.prepare(sr);
        sidechainFilter.prepare(sr);
        splitFilter.prepare(sr);
        reset();
    }

    void reset() noexcept {
        lpcTracker.reset();
        delayedLpcTracker.reset();
        sidechainFilter.reset();
        splitFilter.reset();
        delayBuffer.fill(0);
        delayWriteIdx = 0;
        delaySamples = 0;
        envelope = 0;
        currentGain = 1;
        currentGainReductionDb = 0;
        sibilanceActivity = 0;
        sidechainLevelDb = -100;
    }

    void setParameters(const Parameters& p) noexcept {
        params = p;
        attCoeff = std::exp(-T{1} / (T{0.001} * sr));
        relCoeff = std::exp(-T{1} / (T{0.035} * sr));
        gainSmoothCoeff = std::exp(-T{1} / (T{0.001} * sr));
        maxAttenLinear = std::pow(T{10}, params.reductionDb / T{20});
        thresholdLinear = std::pow(T{10}, params.thresholdDb / T{20});

        // Configure sidechain filter
        if (params.filterType == SidechainFilterType::Highpass) {
            sidechainFilter.setParameters(TptStateVariableFilter<T>::Type::Highpass, params.frequencyHz, params.bandwidthQ, T{1});
        } else {
            sidechainFilter.setParameters(TptStateVariableFilter<T>::Type::Bandpass, params.frequencyHz, params.bandwidthQ, T{1});
        }

        // Configure split filter type
        if (params.filterType == SidechainFilterType::Highpass) {
            splitFilter.setParameters(TptStateVariableFilter<T>::Type::HighShelf, params.frequencyHz, params.bandwidthQ, currentGain);
        } else {
            splitFilter.setParameters(TptStateVariableFilter<T>::Type::Bell, params.frequencyHz, params.bandwidthQ, currentGain);
        }

        // Lookahead delay sizing
        const size_t targetDelay = static_cast<size_t>(std::clamp(
            std::round(params.lookaheadMs * T{0.001} * sr),
            T{0},
            static_cast<T>(MaxDelay - 1)
        ));
        delaySamples = targetDelay;
    }

    [[nodiscard]] const Parameters& getParameters() const noexcept { return params; }
    [[nodiscard]] size_t getLatencySamples() const noexcept { return delaySamples; }
    [[nodiscard]] T getCurrentGainReductionDb() const noexcept { return currentGainReductionDb; }
    [[nodiscard]] T getSibilanceActivity() const noexcept { return sibilanceActivity; }
    [[nodiscard]] T getSidechainLevelDb() const noexcept { return sidechainLevelDb; }

    // Step 1 of decoupled processing (used for stereo linking & channel coupling)
    void processSidechain(T input, T& outScSample, T& outEffectiveEnv) noexcept {
        // 1. Extract sidechain signal through HP or BP filter
        const T scSample = sidechainFilter.processSample(input);
        const T absSc = std::abs(scSample);
        outScSample = scSample;

        // 2. High-speed envelope detection with ballistics
        const T coeff = (absSc > envelope) ? attCoeff : relCoeff;
        envelope = coeff * envelope + (T{1} - coeff) * absSc;

        // Level reporting in dB
        constexpr T minLinear = T{1e-5};
        sidechainLevelDb = T{20} * std::log10(std::max(absSc, minLinear));

        // 3. Formant & Sibilance tracking
        if (params.detectionMode == DetectionMode::SingleVocal && params.useLpcResidualSubtraction) {
            lpcTracker.filterAndExtractResidual(input);
            const T likelihood = lpcTracker.getSibilanceLikelihood();
            // In Single Vocal mode, suppress detection if voiced vocal formants are strongly present
            outEffectiveEnv = envelope * (T{0.12} + T{0.88} * likelihood);
            sibilanceActivity = likelihood * std::clamp(envelope / (thresholdLinear + T{1e-4}), T{0}, T{1});
        } else {
            // Allround mode or LPC disabled: purely level & frequency based
            outEffectiveEnv = envelope;
            sibilanceActivity = std::clamp((envelope - thresholdLinear) / (thresholdLinear + T{1e-4}), T{0}, T{1});
        }
    }

    // Step 2 of decoupled processing: applies gain reduction to delayed audio
    [[nodiscard]] T applyGainWithEffectiveEnvelope(T input, T scSample, T effectiveEnv) noexcept {
        // 1. Buffer audio into circular lookahead delay
        delayBuffer[delayWriteIdx] = input;
        const size_t readIdx = (delayWriteIdx + MaxDelay - delaySamples) & DelayMask;
        const T xDelayed = delayBuffer[readIdx];
        delayWriteIdx = (delayWriteIdx + 1) & DelayMask;

        // 2. Compute target gain reduction from effective envelope
        T targetGainLinear = T{1};
        if (effectiveEnv > thresholdLinear) {
            constexpr T minLinear = T{1e-5};
            const T envDb = T{20} * std::log10(std::max(effectiveEnv, minLinear));
            const T excess = envDb - params.thresholdDb;
            if (excess > 0) {
                const T deltaLinear = std::exp(-excess * (std::numbers::ln10_v<T> / T{20}));
                targetGainLinear = std::max(maxAttenLinear, deltaLinear);
            }
        }

        // Smooth gain trajectory
        currentGain = gainSmoothCoeff * currentGain + (T{1} - gainSmoothCoeff) * targetGainLinear;
        currentGainReductionDb = T{20} * std::log10(std::max(currentGain, T{1e-4}));

        // 3. Process main audio according to Band Mode
        T processed = xDelayed;
        if (params.bandMode == ProcessingBandMode::WideBand) {
            // Wide-band gain ducking
            processed = xDelayed * currentGain;

            // Optional VT-LPSE residual subtraction in wideband mode
            if (params.useLpcResidualSubtraction && currentGain < T{0.999}) {
                const T delayedResidual = delayedLpcTracker.filterAndExtractResidual(xDelayed);
                const T sibilanceSuppression = (T{1} - currentGain);
                const T lpcOut = xDelayed - delayedResidual * sibilanceSuppression;
                const T blend = std::clamp(sibilanceSuppression / T{0.25}, T{0}, T{1});
                processed = (T{1} - blend) * processed + blend * lpcOut;
            }
        } else {
            // Split-band frequency selective ducking
            splitFilter.setGain(currentGain);
            processed = splitFilter.processSample(xDelayed);

            // Optional VT-LPSE residual subtraction strictly band-limited to sibilance band
            if (params.useLpcResidualSubtraction && currentGain < T{0.999}) {
                const T delayedResidual = delayedLpcTracker.filterAndExtractResidual(xDelayed);
                // Extract only the residual within the targeted band
                const T bandResidual = delayedResidual - splitFilter.processSample(delayedResidual);
                const T sibilanceSuppression = (T{1} - currentGain);
                const T blend = std::clamp(sibilanceSuppression / T{0.25}, T{0}, T{0.5});
                processed -= bandResidual * blend;
            }
        }

        // 4. Audition mode routing
        switch (params.auditionMode) {
            case AuditionMode::Sidechain:
                return scSample;
            case AuditionMode::Delta:
                return xDelayed - processed;
            case AuditionMode::Normal:
            default:
                return processed;
        }
    }

    // Unified single-channel process
    [[nodiscard]] T processSample(T input) noexcept {
        T scSample{0};
        T effectiveEnv{0};
        processSidechain(input, scSample, effectiveEnv);
        return applyGainWithEffectiveEnvelope(input, scSample, effectiveEnv);
    }

private:
    T sr{44100};
    Parameters params{};
    T attCoeff{0}, relCoeff{0}, gainSmoothCoeff{0.95};
    T maxAttenLinear{0.25}, thresholdLinear{0.063};
    T envelope{0};
    T currentGain{1};
    T currentGainReductionDb{0};
    T sibilanceActivity{0};
    T sidechainLevelDb{-100};

    std::array<T, MaxDelay> delayBuffer{};
    size_t delayWriteIdx{0};
    size_t delaySamples{0};

    LpcFormantTracker<T, 8> lpcTracker;
    LpcFormantTracker<T, 8> delayedLpcTracker;
    TptStateVariableFilter<T> sidechainFilter;
    TptStateVariableFilter<T> splitFilter;
};

} // namespace openx::dsp
