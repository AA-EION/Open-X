#pragma once

#include <cmath>
#include <concepts>
#include <numbers>
#include <algorithm>

namespace openx::dsp {

template <std::floating_point T>
class TptStateVariableFilter {
public:
    enum class Type { Lowpass, Highpass, Bandpass, Bell, LowShelf, HighShelf, Notch };

    void prepare(T sampleRate) noexcept {
        sr = sampleRate > T{0} ? sampleRate : T{48000};
        reset();
    }

    void reset() noexcept {
        s1 = 0;
        s2 = 0;
    }

    void setParameters(Type filterType, T freqHz, T q, T gainLinear) noexcept {
        type = filterType;
        const T clampedFreq = std::clamp(freqHz, T{10}, sr * T{0.499});
        const T omega = std::numbers::pi_v<T> * clampedFreq / sr;
        const T clampedQ = std::max(q, T{0.05});
        k = T{1} / clampedQ;

        const T gLinear = std::max(gainLinear, T{1e-6});
        const T A = std::sqrt(gLinear);
        const T g0 = std::tan(omega);

        switch (type) {
            case Type::Lowpass: { // High Cut (12 dB/oct)
                g = g0;
                updateInternalCoefficients();
                m0 = T{0};
                m1 = T{0};
                m2 = T{1};
                break;
            }
            case Type::Highpass: { // Low Cut (12 dB/oct)
                g = g0;
                updateInternalCoefficients();
                m0 = T{1};
                m1 = -k;
                m2 = -T{1};
                break;
            }
            case Type::Bandpass: { // Normalized Bandpass (0 dB peak)
                g = g0;
                updateInternalCoefficients();
                m0 = T{0};
                m1 = k; // k * v1 normalizes bandpass peak to 1.0 (0 dB)
                m2 = T{0};
                break;
            }
            case Type::Bell: { // Peaking Bell
                g = g0;
                updateInternalCoefficients();
                m0 = T{1};
                m1 = k * (gLinear - T{1});
                m2 = T{0};
                break;
            }
            case Type::LowShelf: { // Low Shelf
                g = g0 / std::max(std::sqrt(A), T{1e-3});
                updateInternalCoefficients();
                m0 = T{1};
                m1 = k * (A - T{1});
                m2 = gLinear - T{1};
                break;
            }
            case Type::HighShelf: { // High Shelf
                g = g0 * std::max(std::sqrt(A), T{1e-3});
                updateInternalCoefficients();
                m0 = gLinear;
                m1 = k * (T{1} - A) * A;
                m2 = T{1} - gLinear;
                break;
            }
            case Type::Notch: { // Notch
                g = g0;
                updateInternalCoefficients();
                m0 = T{1};
                m1 = -k;
                m2 = T{0};
                break;
            }
        }
    }

    void setGain(T gainLinear) noexcept {
        const T gLinear = std::max(gainLinear, T{1e-6});
        switch (type) {
            case Type::Bell: {
                m1 = k * (gLinear - T{1});
                break;
            }
            case Type::LowShelf: {
                const T A = std::sqrt(gLinear);
                m1 = k * (A - T{1});
                m2 = gLinear - T{1};
                break;
            }
            case Type::HighShelf: {
                const T A = std::sqrt(gLinear);
                m0 = gLinear;
                m1 = k * (T{1} - A) * A;
                m2 = T{1} - gLinear;
                break;
            }
            default:
                break;
        }
    }

    [[nodiscard]] T processSample(T x) noexcept {
        const T v3 = x - s2;
        const T v1 = a1 * s1 + a2 * v3;
        const T v2 = s2 + a2 * s1 + a3 * v3;
        s1 = T{2} * v1 - s1;
        s2 = T{2} * v2 - s2;
        return m0 * x + m1 * v1 + m2 * v2;
    }

private:
    void updateInternalCoefficients() noexcept {
        a1 = T{1} / (T{1} + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    T sr{44100};
    Type type{Type::Bell};
    T g{0}, k{1};
    T a1{0}, a2{0}, a3{0};
    T m0{1}, m1{0}, m2{0};
    T s1{0}, s2{0};
};

template <std::floating_point T>
class DynamicBiquadEngine {
public:
    enum class FilterType : int {
        Bell = 0,
        LowShelf = 1,
        HighShelf = 2,
        Notch = 3,
        LowCut = 4,
        HighCut = 5
    };

    struct Parameters {
        FilterType filterType{FilterType::Bell};
        T frequency{1000};
        T q{0.7071};
        T staticGainDb{0};
        T dynamicGainMaxDb{0};
        T thresholdDb{-20};
        T ratio{2.0};
        T kneeDb{3.0};
        T attackMs{10};
        T releaseMs{100};
        bool downward{true};
        bool bypassed{false};
    };

    void prepare(T sampleRate) noexcept {
        sr = sampleRate > T{0} ? sampleRate : T{48000};
        filter.prepare(sr);
        sidechainFilter.prepare(sr);
        reset();
    }

    void reset() noexcept {
        filter.reset();
        sidechainFilter.reset();
        envelope = 0;
        currentGainLinear = staticGainLinear;
        currentDeltaDb = 0;
    }

    void setParameters(const Parameters& p) noexcept {
        params = p;
        attCoeff = std::exp(-T{1} / (std::max(params.attackMs, T{0.1}) * T{0.001} * sr));
        relCoeff = std::exp(-T{1} / (std::max(params.releaseMs, T{0.1}) * T{0.001} * sr));

        invRatio = T{1} / std::max(params.ratio, T{1});
        slope = T{1} - invRatio;
        halfKnee = params.kneeDb * T{0.5};
        invTwoKnee = (params.kneeDb > T{0}) ? (T{1} / (T{2} * params.kneeDb)) : T{0};
        staticGainLinear = std::pow(T{10}, params.staticGainDb / T{20});
        lowerKneeDb = params.thresholdDb - halfKnee;
        lowerKneeLinear = std::pow(T{10}, lowerKneeDb / T{20});
        absDynGainMax = std::abs(params.dynamicGainMaxDb);

        if (envelope == T{0}) {
            currentGainLinear = staticGainLinear;
        }

        // Sidechain filter: 0 dB normalized Bandpass centered at band frequency & Q
        sidechainFilter.setParameters(
            TptStateVariableFilter<T>::Type::Bandpass,
            params.frequency,
            params.q,
            T{1}
        );

        typename TptStateVariableFilter<T>::Type svfType = TptStateVariableFilter<T>::Type::Bell;
        switch (params.filterType) {
            case FilterType::Bell:      svfType = TptStateVariableFilter<T>::Type::Bell; break;
            case FilterType::LowShelf:  svfType = TptStateVariableFilter<T>::Type::LowShelf; break;
            case FilterType::HighShelf: svfType = TptStateVariableFilter<T>::Type::HighShelf; break;
            case FilterType::Notch:     svfType = TptStateVariableFilter<T>::Type::Notch; break;
            case FilterType::LowCut:    svfType = TptStateVariableFilter<T>::Type::Highpass; break;
            case FilterType::HighCut:   svfType = TptStateVariableFilter<T>::Type::Lowpass; break;
            default:                    svfType = TptStateVariableFilter<T>::Type::Bell; break;
        }

        filter.setParameters(
            svfType,
            params.frequency,
            params.q,
            currentGainLinear
        );
    }

    [[nodiscard]] T processSample(T input) noexcept {
        if (params.bypassed) {
            currentDeltaDb = 0;
            return input;
        }

        // 1. Extract sidechain signal through normalized bandpass filter centered at dynamic band
        const T scSample = sidechainFilter.processSample(input);
        const T absSc = std::abs(scSample);

        // 2. Ballistics envelope detection (branch-free leaky integrator)
        const T coeff = (absSc > envelope) ? attCoeff : relCoeff;
        envelope = coeff * envelope + (T{1} - coeff) * absSc;

        // 3. Compute dynamic modulation in decibels
        T targetLinear = staticGainLinear;
        T deltaDb{0};
        if (absDynGainMax > T{1e-6} && envelope > lowerKneeLinear) {
            constexpr T minLinear = T{1e-5};
            const T envDb = T{20} * std::log10(std::max(envelope, minLinear));
            const T deltaOvershoot = envDb - params.thresholdDb;

            if (params.kneeDb > T{0} && deltaOvershoot >= -halfKnee && deltaOvershoot <= halfKnee) {
                const T kneeFactor = deltaOvershoot + halfKnee;
                deltaDb = slope * (kneeFactor * kneeFactor) * invTwoKnee;
            } else if (deltaOvershoot > halfKnee) {
                deltaDb = slope * deltaOvershoot;
            }

            deltaDb = std::min(deltaDb, absDynGainMax);
            if (params.downward) deltaDb = -deltaDb;

            const T totalGainDb = params.staticGainDb + deltaDb;
            targetLinear = std::pow(T{10}, totalGainDb / T{20});
        }

        currentDeltaDb = deltaDb;
        currentGainLinear = targetLinear;

        // 4. Apply dynamic filter
        filter.setGain(currentGainLinear);

        return filter.processSample(input);
    }

    [[nodiscard]] T processSoloSample(T input) noexcept {
        // Isolated 0 dB normalized resonance auditioning
        return sidechainFilter.processSample(input);
    }

    [[nodiscard]] T getDynamicDeltaDb() const noexcept {
        return currentDeltaDb;
    }

    [[nodiscard]] T getCurrentGainLinear() const noexcept {
        return currentGainLinear;
    }

    [[nodiscard]] static T computeMagnitudeDb(FilterType type, T f, T f0, T q, T gainDb) noexcept {
        const T clampedF0 = std::max(f0, T{1});
        const T w = f / clampedF0;
        const T w2 = w * w;
        const T oneMinusW2Sq = (T{1} - w2) * (T{1} - w2);
        const T clampedQ = std::max(q, T{0.05});
        const T qTerm = (w / clampedQ) * (w / clampedQ);
        const T denom = std::max(oneMinusW2Sq + qTerm, T{1e-12});
        const T linGain = std::pow(T{10}, gainDb / T{20});
        const T g2 = linGain * linGain;

        T magSq = T{1};
        switch (type) {
            case FilterType::Bell:
                magSq = (oneMinusW2Sq + qTerm * g2) / denom;
                break;
            case FilterType::LowShelf: {
                const T A = std::pow(T{10}, gainDb / T{40});
                const T A2 = A * A;
                const T termQ = (A / (clampedQ * clampedQ)) * w2;
                const T num = (A - w2) * (A - w2) + termQ;
                const T den = (T{1} - A * w2) * (T{1} - A * w2) + termQ;
                magSq = A2 * (num / std::max(den, T{1e-12}));
                break;
            }
            case FilterType::HighShelf: {
                const T A = std::pow(T{10}, gainDb / T{40});
                const T A2 = A * A;
                const T termQ = (A / (clampedQ * clampedQ)) * w2;
                const T num = (T{1} - A * w2) * (T{1} - A * w2) + termQ;
                const T den = (A - w2) * (A - w2) + termQ;
                magSq = A2 * (num / std::max(den, T{1e-12}));
                break;
            }
            case FilterType::Notch:
                magSq = oneMinusW2Sq / denom;
                break;
            case FilterType::LowCut:
                magSq = (w2 * w2) / denom;
                break;
            case FilterType::HighCut:
                magSq = T{1} / denom;
                break;
        }

        return T{10} * std::log10(std::max(magSq, T{1e-12}));
    }

private:
    T sr{44100};
    Parameters params{};
    T attCoeff{0}, relCoeff{0};
    T invRatio{0.5}, slope{0.5};
    T halfKnee{1.5}, invTwoKnee{0.166667};
    T staticGainLinear{1}, lowerKneeDb{-21.5}, lowerKneeLinear{0.084};
    T absDynGainMax{0};
    T envelope{0};
    T currentGainLinear{1};
    T currentDeltaDb{0};
    TptStateVariableFilter<T> filter;
    TptStateVariableFilter<T> sidechainFilter;
};

} // namespace openx::dsp
