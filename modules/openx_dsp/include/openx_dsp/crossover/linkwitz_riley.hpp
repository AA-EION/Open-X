#pragma once

#include <cmath>
#include <array>
#include <concepts>
#include <numbers>
#include <span>

namespace openx::dsp {

template <std::floating_point T>
class Biquad {
public:
    struct Coefficients {
        T b0{1}, b1{0}, b2{0}, a1{0}, a2{0};
    };

    constexpr void setCoefficients(const Coefficients& c) noexcept { coeff = c; }

    constexpr void reset() noexcept {
        s1 = 0;
        s2 = 0;
    }

    [[nodiscard]] constexpr T processSample(T x) noexcept {
        const T y = coeff.b0 * x + s1;
        s1 = coeff.b1 * x - coeff.a1 * y + s2;
        s2 = coeff.b2 * x - coeff.a2 * y;
        return y;
    }

private:
    Coefficients coeff{};
    T s1{0}, s2{0};
};

template <std::floating_point T>
class LinkwitzRiley4 {
public:
    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        reset();
    }

    void reset() noexcept {
        lp1.reset(); lp2.reset();
        hp1.reset(); hp2.reset();
        ap.reset();
    }

    void setCutoff(T cutoffHz) noexcept {
        const T omega = std::numbers::pi_v<T> * cutoffHz / sr;
        const T theta = std::tan(omega);
        const T thetaSq = theta * theta;
        constexpr T sqrt2 = std::numbers::sqrt2_v<T>;

        const T d = thetaSq + sqrt2 * theta + 1;
        const T b0_lp = thetaSq / d;
        const T b1_lp = 2 * b0_lp;
        const T b2_lp = b0_lp;

        const T b0_hp = 1 / d;
        const T b1_hp = -2 * b0_hp;
        const T b2_hp = b0_hp;

        const T a1 = 2 * (thetaSq - 1) / d;
        const T a2 = (thetaSq - sqrt2 * theta + 1) / d;

        const typename Biquad<T>::Coefficients lpCoeff{ b0_lp, b1_lp, b2_lp, a1, a2 };
        const typename Biquad<T>::Coefficients hpCoeff{ b0_hp, b1_hp, b2_hp, a1, a2 };
        const typename Biquad<T>::Coefficients apCoeff{ a2, a1, 1, a1, a2 };

        lp1.setCoefficients(lpCoeff);
        lp2.setCoefficients(lpCoeff);
        hp1.setCoefficients(hpCoeff);
        hp2.setCoefficients(hpCoeff);
        ap.setCoefficients(apCoeff);
    }

    struct Output {
        T low;
        T high;
    };

    [[nodiscard]] Output processSample(T x) noexcept {
        const T low = lp2.processSample(lp1.processSample(x));
        const T high = hp2.processSample(hp1.processSample(x));
        return { low, high };
    }

    [[nodiscard]] T processAllpassSample(T x) noexcept {
        return ap.processSample(x);
    }

private:
    T sr{44100};
    Biquad<T> lp1, lp2;
    Biquad<T> hp1, hp2;
    Biquad<T> ap;
};

template <std::floating_point T, size_t NumBands = 3>
    requires (NumBands >= 2 && NumBands <= 8)
class PhaseAlignedMultibandSplitter {
public:
    static constexpr size_t NumCrossovers = NumBands - 1;

    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        for (auto& xo : crossovers) xo.prepare(sr);
        for (auto& apChain : compensators) {
            for (auto& ap : apChain) ap.prepare(sr);
        }
    }

    void reset() noexcept {
        for (auto& xo : crossovers) xo.reset();
        for (auto& apChain : compensators) {
            for (auto& ap : apChain) ap.reset();
        }
    }

    void setFrequencies(std::span<const T, NumCrossovers> cutoffs) noexcept {
        for (size_t i = 0; i < NumCrossovers; ++i) {
            crossovers[i].setCutoff(cutoffs[i]);
            for (size_t b = 0; b < NumBands; ++b) {
                compensators[b][i].setCutoff(cutoffs[i]);
            }
        }
    }

    void process(T input, std::array<T, NumBands>& outputs) noexcept {
        T current = input;
        for (size_t i = 0; i < NumCrossovers; ++i) {
            auto [low, high] = crossovers[i].processSample(current);
            outputs[i] = low;
            current = high;
            // Phase compensation: all lower bands (0 .. i-1) must pass through allpass of crossover i
            for (size_t b = 0; b < i; ++b) {
                outputs[b] = compensators[b][i].processAllpassSample(outputs[b]);
            }
        }
        outputs[NumBands - 1] = current;
    }

private:
    T sr{44100};
    std::array<LinkwitzRiley4<T>, NumCrossovers> crossovers;
    std::array<std::array<LinkwitzRiley4<T>, NumCrossovers>, NumBands> compensators;
};

} // namespace openx::dsp
