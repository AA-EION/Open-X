#pragma once

#include <cmath>
#include <array>
#include <concepts>
#include <algorithm>
#include <numbers>

namespace openx::dsp {

template <std::floating_point T>
class SymplecticHamiltonianPerturbator {
public:
    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        q = T{0.1};
        p = T{0.0};
        dt = T{1} / sr;
    }

    void reset() noexcept {
        q = T{0.1};
        p = T{0.0};
    }

    [[nodiscard]] T step(T omega = T{3.5}, T lambda = T{1.2}) noexcept {
        // Störmer-Verlet Symplectic Integrator for Duffing Hamiltonian: H = 1/2 p^2 + 1/2 omega^2 q^2 + 1/4 lambda q^4
        // dV/dq = omega^2 q + lambda q^3
        const T halfDt = dt * T{0.5};
        const T dV1 = omega * omega * q + lambda * q * q * q;
        const T p_half = p - halfDt * dV1;
        
        q += dt * p_half;
        
        const T dV2 = omega * omega * q + lambda * q * q * q;
        p = p_half - halfDt * dV2;

        return q;
    }

private:
    T sr{44100};
    T q{0.1}, p{0.0}, dt{1e-4};
};

template <std::floating_point T, size_t NumLines = 16>
class FdnReverb {
public:
    static_assert(NumLines == 16, "Orthogonal Householder FDN configured for 16 delay channels");

    struct Parameters {
        T decayTimeSec{2.5};
        T dampingHz{5000.0};
        T chaosModulation{0.2}; // Symplectic Hamiltonian chaotic injection
        T dryWet{0.3};
    };

    FdnReverb() {
        prepare(T{44100});
    }

    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        hamiltonian.prepare(sr);

        // Mutually coprime prime delay lengths (approximately 20ms to 75ms)
        static constexpr std::array<size_t, NumLines> BasePrimes{{
            1087, 1153, 1229, 1297, 1381, 1453, 1523, 1607,
            1693, 1777, 1867, 1973, 2069, 2161, 2267, 2377
        }};

        const T scale = sr / T{44100};
        for (size_t i = 0; i < NumLines; ++i) {
            lengths[i] = static_cast<size_t>(std::round(static_cast<T>(BasePrimes[i]) * scale));
            buffers[i].resize(lengths[i] + 64);
            std::fill(buffers[i].begin(), buffers[i].end(), T{0});
            writeIndices[i] = 0;
            dampingStates[i] = 0;
        }

        reset();
    }

    void reset() noexcept {
        hamiltonian.reset();
        for (size_t i = 0; i < NumLines; ++i) {
            std::fill(buffers[i].begin(), buffers[i].end(), T{0});
            writeIndices[i] = 0;
            dampingStates[i] = 0;
        }
    }

    void setParameters(const Parameters& p) noexcept {
        params = p;
        const T rt60 = std::max(params.decayTimeSec, T{0.1});
        for (size_t i = 0; i < NumLines; ++i) {
            const T delaySec = static_cast<T>(lengths[i]) / sr;
            // Sabine attenuation per line: -60 dB at rt60
            feedbackGains[i] = std::pow(T{10}, (-3 * delaySec) / rt60);
        }

        const T omegaDamp = std::numbers::pi_v<T> * std::clamp(params.dampingHz, T{100}, sr * T{0.45}) / sr;
        dampCoeff = std::clamp(T{1} - std::tan(omegaDamp), T{0.01}, T{0.99});
    }

    [[nodiscard]] std::pair<T, T> processSample(T inL, T inR) noexcept {
        const T monoIn = (inL + inR) * T{0.5};
        const T chaos = hamiltonian.step() * params.chaosModulation * T{4.0};

        std::array<T, NumLines> delayOutputs{};
        T sumDelayOut = 0;

        // 1. Read from fractional delay lines with Hermite interpolation
        for (size_t i = 0; i < NumLines; ++i) {
            const T modOffset = (i % 2 == 0) ? chaos : -chaos;
            const size_t bufSize = buffers[i].size();
            const T targetDelay = std::clamp(static_cast<T>(lengths[i]) + modOffset, T{4}, static_cast<T>(bufSize - 4));

            T readPtr = static_cast<T>(writeIndices[i]) - targetDelay;
            const T bufSizeT = static_cast<T>(bufSize);
            while (readPtr < 0) readPtr += bufSizeT;
            while (readPtr >= bufSizeT) readPtr -= bufSizeT;

            const size_t idx0 = static_cast<size_t>(readPtr);
            const T frac = readPtr - static_cast<T>(idx0);

            const size_t idxM1 = (idx0 + bufSize - 1) % bufSize;
            const size_t idx1 = (idx0 + 1) % bufSize;
            const size_t idx2 = (idx0 + 2) % bufSize;

            const T ym1 = buffers[i][idxM1];
            const T y0  = buffers[i][idx0];
            const T y1  = buffers[i][idx1];
            const T y2  = buffers[i][idx2];

            const T c0 = y0;
            const T c1 = T{0.5} * (y1 - ym1);
            const T c2 = ym1 - T{2.5} * y0 + T{2.0} * y1 - T{0.5} * y2;
            const T c3 = T{0.5} * (y2 - ym1) + T{1.5} * (y0 - y1);

            delayOutputs[i] = ((c3 * frac + c2) * frac + c1) * frac + c0;
            sumDelayOut += delayOutputs[i];
        }

        // 2. Householder Unitary Reflection Matrix (O(N) operations: A = I - 2/N * 1 * 1^T)
        constexpr T householderFactor = T{2} / static_cast<T>(NumLines);
        const T householderTerm = sumDelayOut * householderFactor;

        std::array<T, NumLines> feedbackSignals{};
        for (size_t i = 0; i < NumLines; ++i) {
            const T mixed = delayOutputs[i] - householderTerm;
            
            // 3. Frequency-dependent damping per line
            dampingStates[i] = (T{1} - dampCoeff) * mixed + dampCoeff * dampingStates[i];
            feedbackSignals[i] = dampingStates[i] * feedbackGains[i];
        }

        // 4. Write back into delay lines with input injection
        for (size_t i = 0; i < NumLines; ++i) {
            const T inputInject = (i % 2 == 0) ? monoIn : -monoIn;
            buffers[i][writeIndices[i]] = inputInject + feedbackSignals[i];
            writeIndices[i] = (writeIndices[i] + 1) % buffers[i].size();
        }

        // 5. Decorrelated Stereo Output summation (interleaved prime distribution with orthogonal polarities)
        T outL = 0, outR = 0;
        for (size_t i = 0; i < NumLines; ++i) {
            const T sign = (i % 4 == 0 || i % 4 == 3) ? T{1} : T{-1};
            if (i % 2 == 0) {
                outL += sign * delayOutputs[i];
            } else {
                outR += sign * delayOutputs[i];
            }
        }

        constexpr T norm = T{1} / std::numbers::sqrt2_v<T>;
        constexpr T scale8 = T{0.35355339059327373}; // 1.0 / sqrt(8)
        outL *= norm * scale8;
        outR *= norm * scale8;

        const T wet = params.dryWet;
        const T dry = T{1} - wet;
        return { inL * dry + outL * wet, inR * dry + outR * wet };
    }

private:
    T sr{44100};
    Parameters params{};
    T dampCoeff{0.5};
    std::array<size_t, NumLines> lengths{};
    std::array<std::vector<T>, NumLines> buffers;
    std::array<size_t, NumLines> writeIndices{};
    std::array<T, NumLines> feedbackGains{};
    std::array<T, NumLines> dampingStates{};
    SymplecticHamiltonianPerturbator<T> hamiltonian;
};

} // namespace openx::dsp
