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
        sr = sampleRate;
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
        
        g = std::tan(omega);
        r = T{1} / (T{2} * std::max(q, T{0.05}));
        two_r = T{2} * r;
        twoR_plus_g = two_r + g;
        h = T{1} / (T{1} + two_r * g + g * g);
        gain = gainLinear;
    }

    void setGain(T gainLinear) noexcept {
        gain = gainLinear;
    }

    [[nodiscard]] T processSample(T x) noexcept {
        const T hp = (x - twoR_plus_g * s1 - s2) * h;
        const T bp = g * hp + s1;
        s1 = g * hp + bp;
        const T lp = g * bp + s2;
        s2 = g * bp + lp;

        switch (type) {
            case Type::Lowpass:   return lp;
            case Type::Highpass:  return hp;
            case Type::Bandpass:  return bp;
            case Type::Bell:      return x + (gain - T{1}) * (two_r * bp);
            case Type::LowShelf:  return x + (gain - T{1}) * lp;
            case Type::HighShelf: return x + (gain - T{1}) * hp;
            case Type::Notch:     return lp + hp;
            default:              return x;
        }
    }

private:
    T sr{44100};
    Type type{Type::Bell};
    T g{0}, r{0}, two_r{0}, twoR_plus_g{0}, h{0}, gain{1};
    T s1{0}, s2{0};
};

template <std::floating_point T>
class DynamicBiquadEngine {
public:
    struct Parameters {
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
    };

    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        filter.prepare(sr);
        sidechainFilter.prepare(sr);
        reset();
    }

    void reset() noexcept {
        filter.reset();
        sidechainFilter.reset();
        envelope = 0;
        currentGainLinear = 1;
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

        sidechainFilter.setParameters(
            TptStateVariableFilter<T>::Type::Bandpass,
            params.frequency,
            params.q,
            T{1}
        );
        filter.setParameters(
            TptStateVariableFilter<T>::Type::Bell,
            params.frequency,
            params.q,
            currentGainLinear
        );
    }

    [[nodiscard]] T processSample(T input) noexcept {
        // 1. Extract sidechain signal through bandpass filter centered at dynamic band
        const T scSample = sidechainFilter.processSample(input);
        const T absSc = std::abs(scSample);

        // 2. Ballistics envelope detection (branch-free leaky integrator)
        const T coeff = (absSc > envelope) ? attCoeff : relCoeff;
        envelope = coeff * envelope + (T{1} - coeff) * absSc;

        // 3. Compute dynamic modulation in decibels (cached / bypassed below threshold)
        T targetLinear = staticGainLinear;
        if (absDynGainMax > T{1e-6} && envelope > lowerKneeLinear) {
            constexpr T minLinear = T{1e-5};
            const T envDb = T{20} * std::log10(std::max(envelope, minLinear));
            const T deltaOvershoot = envDb - params.thresholdDb;

            T deltaDb{0};
            if (params.kneeDb > T{0} && deltaOvershoot >= -halfKnee && deltaOvershoot <= halfKnee) {
                const T kneeFactor = deltaOvershoot + halfKnee;
                deltaDb = slope * (kneeFactor * kneeFactor) * invTwoKnee;
            } else if (deltaOvershoot > halfKnee) {
                deltaDb = slope * deltaOvershoot;
            }

            deltaDb = std::min(deltaDb, absDynGainMax);
            if (params.downward) deltaDb = -deltaDb;

            const T totalGainDb = params.staticGainDb + deltaDb;
            targetLinear = std::exp(totalGainDb * (std::numbers::ln10_v<T> / T{20}));
        }

        // 4. Smooth parameter coefficient update (one-pole smoother)
        constexpr T gainSmoothCoeff = T{0.995};
        currentGainLinear = gainSmoothCoeff * currentGainLinear + (T{1} - gainSmoothCoeff) * targetLinear;

        // 5. Apply dynamic filter (cached SVF parameters, only updating gain)
        filter.setGain(currentGainLinear);

        return filter.processSample(input);
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
    TptStateVariableFilter<T> filter;
    TptStateVariableFilter<T> sidechainFilter;
};

} // namespace openx::dsp
