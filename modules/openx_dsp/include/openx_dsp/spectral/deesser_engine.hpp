#pragma once

#include <cmath>
#include <array>
#include <concepts>
#include <algorithm>
#include "../eq/dynamic_biquad_engine.hpp"

namespace openx::dsp {

template <std::floating_point T, size_t LpcOrder = 8>
class LpcFormantTracker {
public:
    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        reset();
    }

    void reset() noexcept {
        buffer.fill(0);
        reflectionCoeffs.fill(0);
        forwardStates.fill(0);
        backwardStates.fill(0);
        energy.fill(T{1e-4});
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
            constexpr T lambda = T{0.995};
            energy[m] = lambda * energy[m] + (T{1} - lambda) * (f * f + prevB * prevB);

            constexpr T mu = T{0.005};
            constexpr T eps = T{1e-6};
            const T normFactor = mu / (energy[m] + eps);
            reflectionCoeffs[m] = std::clamp(reflectionCoeffs[m] - normFactor * (f * prevB), T{-0.98}, T{0.98});

            forwardStates[m] = newF;
            backwardStates[m] = b;

            f = newF;
            b = newB;
        }

        return f; // Prediction error residual e[n] (contains turbulent noise)
    }

private:
    T sr{44100};
    std::array<T, LpcOrder> buffer{};
    std::array<T, LpcOrder> reflectionCoeffs{};
    std::array<T, LpcOrder> forwardStates{};
    std::array<T, LpcOrder> backwardStates{};
    std::array<T, LpcOrder> energy{};
};

template <std::floating_point T>
class DeEsserEngine {
public:
    struct Parameters {
        T frequencyHz{6000.0};
        T thresholdDb{-24.0};
        T reductionDb{-12.0};
        T bandwidthQ{2.0};
        bool useLpcResidualSubtraction{true};
    };

    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        lpcTracker.prepare(sr);
        sidechainFilter.prepare(sr);
        notchFilter.prepare(sr);
        reset();
    }

    void reset() noexcept {
        lpcTracker.reset();
        sidechainFilter.reset();
        notchFilter.reset();
        envelope = 0;
        currentGain = 1;
    }

    void setParameters(const Parameters& p) noexcept {
        params = p;
        attCoeff = std::exp(-T{1} / (T{0.001} * sr));
        relCoeff = std::exp(-T{1} / (T{0.050} * sr));
        maxAttenLinear = std::pow(T{10}, params.reductionDb / T{20});
        thresholdLinear = std::pow(T{10}, params.thresholdDb / T{20});

        sidechainFilter.setParameters(TptStateVariableFilter<T>::Type::Bandpass, params.frequencyHz, params.bandwidthQ, T{1});
        notchFilter.setParameters(TptStateVariableFilter<T>::Type::Bell, params.frequencyHz, params.bandwidthQ, T{1});
    }

    [[nodiscard]] T processSample(T input) noexcept {
        // 1. Sibilance band isolation
        const T scSample = sidechainFilter.processSample(input);
        const T absSc = std::abs(scSample);

        // 2. High-speed envelope detection with pre-calculated ballistics coefficients
        const T coeff = (absSc > envelope) ? attCoeff : relCoeff;
        envelope = coeff * envelope + (T{1} - coeff) * absSc;

        T targetGainLinear = 1;
        if (envelope > thresholdLinear) {
            constexpr T minLinear = T{1e-5};
            const T envDb = T{20} * std::log10(std::max(envelope, minLinear));
            const T excess = envDb - params.thresholdDb;
            if (excess > 0) {
                const T deltaLinear = std::exp(-excess * (std::numbers::ln10_v<T> / T{20}));
                targetGainLinear = std::max(maxAttenLinear, deltaLinear);
            }
        }

        currentGain = T{0.95} * currentGain + T{0.05} * targetGainLinear;

        // 3. Continuous processing for notch filter and LPC tracker to eliminate transition clicks
        notchFilter.setGain(currentGain);
        const T notchOut = notchFilter.processSample(input);
        const T residual = lpcTracker.filterAndExtractResidual(input);

        // 4. Smooth continuous blend into LPC residual subtraction during sibilance bursts
        const T sibilanceSuppression = (T{1} - currentGain);
        const T lpcOut = input - residual * sibilanceSuppression;

        const T blend = params.useLpcResidualSubtraction
            ? std::clamp(sibilanceSuppression / T{0.2}, T{0}, T{1})
            : T{0};

        return (T{1} - blend) * notchOut + blend * lpcOut;
    }

private:
    T sr{44100};
    Parameters params{};
    T attCoeff{0}, relCoeff{0};
    T maxAttenLinear{0.25}, thresholdLinear{0.063};
    T envelope{0};
    T currentGain{1};
    LpcFormantTracker<T, 8> lpcTracker;
    TptStateVariableFilter<T> sidechainFilter;
    TptStateVariableFilter<T> notchFilter;
};

} // namespace openx::dsp
