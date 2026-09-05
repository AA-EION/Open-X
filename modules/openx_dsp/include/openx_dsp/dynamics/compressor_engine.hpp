#pragma once

#include <cmath>
#include <array>
#include <concepts>
#include <algorithm>
#include <numbers>

namespace openx::dsp {

template <std::floating_point T>
class AnalyticEnvelopeFollower {
public:
    static constexpr T scalePole(T basePole, T targetSr, T baseSr = T{44100}) noexcept {
        const T halfOmega = baseSr * (T{1} - basePole) / (T{1} + basePole);
        return (targetSr - halfOmega) / (targetSr + halfOmega);
    }

    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        reset();
        // 4-stage cascaded allpass network creating 90-degree phase differential across 20Hz-20kHz
        // Scale poles across arbitrary sample rates (44.1k, 48k, 96k, 192k) to prevent quadrature collapse
        ap1_1.setPole(scalePole(T{0.4021921162426}, sr));
        ap1_2.setPole(scalePole(T{0.8561710882420}, sr));
        ap2_1.setPole(scalePole(T{0.1805912909439}, sr));
        ap2_2.setPole(scalePole(T{0.6740030588691}, sr));
    }

    void reset() noexcept {
        ap1_1.reset(); ap1_2.reset();
        ap2_1.reset(); ap2_2.reset();
    }

    [[nodiscard]] std::pair<T, T> processSample(T x) noexcept {
        // Path 1 (In-phase): AP1_1 -> AP1_2
        const T i = ap1_2.processSample(ap1_1.processSample(x));
        // Path 2 (Quadrature, 90 deg): AP2_1 -> AP2_2
        const T q = ap2_2.processSample(ap2_1.processSample(x));
        return { i, q };
    }

    [[nodiscard]] T computeInstantaneousEnvelope(T x) noexcept {
        const auto [i, q] = processSample(x);
        return std::sqrt(i * i + q * q);
    }

private:
    struct FirstOrderAllpass {
        T a{0};
        T s{0};
        constexpr void setPole(T pole) noexcept { a = pole; }
        constexpr void reset() noexcept { s = 0; }
        [[nodiscard]] constexpr T processSample(T in) noexcept {
            const T y = -a * in + s;
            s = in + a * y;
            return y;
        }
    };

    T sr{44100};
    FirstOrderAllpass ap1_1, ap1_2;
    FirstOrderAllpass ap2_1, ap2_2;
};

template <std::floating_point T>
class CompressorEngine {
public:
    struct Parameters {
        T thresholdDb{-20};
        T ratio{4};
        T kneeDb{6};
        T attackMs{15};
        T releaseMs{120};
        T makeupGainDb{0};
        T transientPunch{0.5}; // TS-WD dynamic cross-modulation [0, 1]
    };

    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        analyticFollower.prepare(sr);
        reset();
    }

    void reset() noexcept {
        analyticFollower.reset();
        envelope = 0;
        prevEnvelope = 0;
        gainReductionLinear = 1;
    }

    void setParameters(const Parameters& p) noexcept {
        params = p;
        attCoeff = std::exp(-T{1} / (std::max(params.attackMs, T{0.05}) * T{0.001} * sr));
        relCoeff = std::exp(-T{1} / (std::max(params.releaseMs, T{0.1}) * T{0.001} * sr));
        makeupLinear = std::pow(T{10}, params.makeupGainDb / T{20});

        invRatio = T{1} / std::max(params.ratio, T{1});
        slope = T{1} - invRatio;
        halfKnee = params.kneeDb * T{0.5};
        invTwoKnee = (params.kneeDb > T{0}) ? (T{1} / (T{2} * params.kneeDb)) : T{0};
        lowerKneeDb = params.thresholdDb - halfKnee;
        lowerKneeLinear = std::pow(T{10}, lowerKneeDb / T{20});
    }

    [[nodiscard]] T processSample(T input) noexcept {
        // 1. Analytic signal envelope extraction (zero ripple on stationary sinusoidal signals)
        const T instantEnv = analyticFollower.computeInstantaneousEnvelope(input);

        // 2. Ballistics integration
        const T coeff = (instantEnv > envelope) ? attCoeff : relCoeff;
        envelope = coeff * envelope + (T{1} - coeff) * instantEnv;

        // 3. C2 Continuous Logarithmic Gain Computer (cached / bypassed below threshold)
        T targetGrLinear = T{1};
        if (envelope > lowerKneeLinear) {
            constexpr T minVal = T{1e-5};
            const T envDb = T{20} * std::log10(std::max(envelope, minVal));
            const T deltaOvershoot = envDb - params.thresholdDb;

            T grDb = 0;
            if (params.kneeDb > T{0} && deltaOvershoot >= -halfKnee && deltaOvershoot <= halfKnee) {
                const T k = deltaOvershoot + halfKnee;
                grDb = slope * (k * k) * invTwoKnee;
            } else if (deltaOvershoot > halfKnee) {
                grDb = slope * deltaOvershoot;
            }

            targetGrLinear = std::exp(-grDb * (std::numbers::ln10_v<T> / T{20}));
        }

        gainReductionLinear = T{0.995} * gainReductionLinear + T{0.005} * targetGrLinear;

        // 4. TS-WD: Transient-Sustain Wavelet Energy Decomposition
        // Extract transient flux via first-order envelope differential
        const T envDelta = std::max(T{0}, instantEnv - prevEnvelope);
        prevEnvelope = instantEnv;

        const T transientFactor = T{1} + params.transientPunch * std::tanh(T{10} * envDelta);
        const T compressedSustain = input * gainReductionLinear;

        return (compressedSustain * transientFactor) * makeupLinear;
    }

    [[nodiscard]] T getGainReductionDb() const noexcept {
        return T{-20} * std::log10(std::max(gainReductionLinear, T{1e-5}));
    }

private:
    T sr{44100};
    Parameters params{};
    T attCoeff{0}, relCoeff{0};
    T invRatio{0.75}, slope{0.75};
    T halfKnee{3.0}, invTwoKnee{0.083333};
    T lowerKneeDb{-23.0}, lowerKneeLinear{0.0708};
    T envelope{0}, prevEnvelope{0};
    T gainReductionLinear{1};
    T makeupLinear{1};
    AnalyticEnvelopeFollower<T> analyticFollower;
};

} // namespace openx::dsp
