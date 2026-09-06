#pragma once

#include <cmath>
#include <array>
#include <vector>
#include <concepts>
#include <algorithm>
#include <numbers>
#include <utility>

namespace openx::dsp {

// Symplectic Hamiltonian Chaotic Perturbator
// Employs a Störmer-Verlet symplectic integrator on a Duffing oscillator to continuously
// disrupt modal clustering without destroying phase space volume or energy conservation.
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
        const auto [qn, _] = stepState(omega, lambda);
        return q;
    }

    [[nodiscard]] std::pair<T, T> stepState(T omega = T{3.5}, T lambda = T{1.2}) noexcept {
        // Störmer-Verlet Symplectic Integrator for Duffing Hamiltonian: H = 1/2 p^2 + 1/2 omega^2 q^2 + 1/4 lambda q^4
        // dV/dq = omega^2 q + lambda q^3
        const T halfDt = dt * T{0.5};
        const T dV1 = omega * omega * q + lambda * q * q * q;
        const T p_half = p - halfDt * dV1;
        
        q += dt * p_half;
        
        const T dV2 = omega * omega * q + lambda * q * q * q;
        p = p_half - halfDt * dV2;

        // Normalized coordinates in [-1.0, 1.0] for canonical phase projection
        const T q_norm = std::clamp(q * T{10.0}, T{-1.0}, T{1.0});
        const T p_norm = std::clamp(p * (T{1} / T{0.35}), T{-1.0}, T{1.0});
        return { q_norm, p_norm };
    }

private:
    T sr{44100};
    T q{0.1}, p{0.0}, dt{1e-4};
};

// High-precision fractional pre-delay buffer supporting up to 1.5 seconds of delay
template <std::floating_point T>
class PreDelayEngine {
public:
    void prepare(T sampleRate, T maxSeconds = T{1.5}) {
        sr = sampleRate;
        const size_t needed = static_cast<size_t>(std::ceil(sr * maxSeconds)) + 128;
        bufferL.assign(needed, T{0});
        bufferR.assign(needed, T{0});
        writePos = 0;
    }

    void reset() noexcept {
        std::fill(bufferL.begin(), bufferL.end(), T{0});
        std::fill(bufferR.begin(), bufferR.end(), T{0});
        writePos = 0;
    }

    [[nodiscard]] std::pair<T, T> process(T inL, T inR, T delaySamples) noexcept {
        const size_t sz = bufferL.size();
        if (sz == 0) return { inL, inR };

        bufferL[writePos] = inL;
        bufferR[writePos] = inR;

        const T d = std::clamp(delaySamples, T{0}, static_cast<T>(sz - 2));
        T rPos = static_cast<T>(writePos) - d;
        while (rPos < T{0}) rPos += static_cast<T>(sz);
        while (rPos >= static_cast<T>(sz)) rPos -= static_cast<T>(sz);

        const size_t i0 = static_cast<size_t>(rPos);
        const size_t i1 = (i0 + 1) % sz;
        const T frac = rPos - static_cast<T>(i0);

        const T outL = bufferL[i0] + frac * (bufferL[i1] - bufferL[i0]);
        const T outR = bufferR[i0] + frac * (bufferR[i1] - bufferR[i0]);

        writePos = (writePos + 1) % sz;
        return { outL, outR };
    }

private:
    T sr{44100};
    std::vector<T> bufferL;
    std::vector<T> bufferR;
    size_t writePos{0};
};

// Schroeder/Gerzon allpass diffusion filter
template <std::floating_point T>
class AllpassDiffuser {
public:
    void prepare(size_t maxDelay) {
        buffer.assign(maxDelay + 64, T{0});
        delayLength = maxDelay;
        writeIdx = 0;
    }

    void reset() noexcept {
        std::fill(buffer.begin(), buffer.end(), T{0});
        writeIdx = 0;
    }

    void setDelay(size_t len) noexcept {
        delayLength = std::clamp(len, size_t{1}, buffer.size() - 1);
    }

    [[nodiscard]] T process(T x, T g) noexcept {
        if (g <= T{0.0001} || delayLength == 0 || buffer.empty()) return x;
        const size_t sz = buffer.size();
        const size_t readIdx = (writeIdx + sz - delayLength) % sz;
        const T bufOut = buffer[readIdx];
        const T v = x - g * bufOut;
        const T y = bufOut + g * v;
        buffer[writeIdx] = v;
        writeIdx = (writeIdx + 1) % sz;
        return y;
    }

private:
    std::vector<T> buffer;
    size_t delayLength{100};
    size_t writeIdx{0};
};

// 4-stage cascade allpass diffuser per channel for transient smearing and echo density
template <std::floating_point T, size_t Stages = 4>
class DiffusionCascade {
public:
    void prepare(T sampleRate) {
        sr = sampleRate;
        static constexpr std::array<size_t, Stages> BaseDelaysL{ 149, 379, 563, 829 };
        static constexpr std::array<size_t, Stages> BaseDelaysR{ 173, 347, 601, 797 };
        const T scale = sr / T{44100};

        for (size_t i = 0; i < Stages; ++i) {
            const size_t maxLenL = static_cast<size_t>(std::ceil(BaseDelaysL[i] * scale * T{2.5})) + 64;
            const size_t maxLenR = static_cast<size_t>(std::ceil(BaseDelaysR[i] * scale * T{2.5})) + 64;
            diffusersL[i].prepare(maxLenL);
            diffusersR[i].prepare(maxLenR);
        }
        setParameters(T{0.0}, T{1.0});
    }

    void reset() noexcept {
        for (size_t i = 0; i < Stages; ++i) {
            diffusersL[i].reset();
            diffusersR[i].reset();
        }
    }

    void setParameters(T diffusion, T space) noexcept {
        coeff = std::clamp(diffusion * T{0.68}, T{0.0}, T{0.75});

        static constexpr std::array<size_t, Stages> BaseDelaysL{ 149, 379, 563, 829 };
        static constexpr std::array<size_t, Stages> BaseDelaysR{ 173, 347, 601, 797 };
        const T scale = (sr / T{44100}) * std::clamp(space, T{0.4}, T{1.8});

        for (size_t i = 0; i < Stages; ++i) {
            diffusersL[i].setDelay(static_cast<size_t>(std::round(BaseDelaysL[i] * scale)));
            diffusersR[i].setDelay(static_cast<size_t>(std::round(BaseDelaysR[i] * scale)));
        }
    }

    [[nodiscard]] std::pair<T, T> process(T inL, T inR) noexcept {
        if (coeff <= T{0.0001}) return { inL, inR };

        T sigL = inL;
        T sigR = inR;
        for (size_t i = 0; i < Stages; ++i) {
            sigL = diffusersL[i].process(sigL, coeff);
            sigR = diffusersR[i].process(sigR, coeff);
        }
        return { sigL, sigR };
    }

private:
    T sr{44100};
    T coeff{0.0};
    std::array<AllpassDiffuser<T>, Stages> diffusersL;
    std::array<AllpassDiffuser<T>, Stages> diffusersR;
};

// Spatial Early Reflections cluster generator (8 decorrelated reflections per channel)
template <std::floating_point T>
class EarlyReflectionsGenerator {
public:
    struct TapConfig {
        T delayMs;
        T gain;
    };

    void prepare(T sampleRate) {
        sr = sampleRate;
        const size_t needed = static_cast<size_t>(std::ceil(sr * T{0.25})) + 64; // up to 250ms
        bufferL.assign(needed, T{0});
        bufferR.assign(needed, T{0});
        writePos = 0;
        lpStateL = 0;
        lpStateR = 0;

        const T omega = T{2} * std::numbers::pi_v<T> * std::clamp(T{6500}, T{500}, sr * T{0.45}) / sr;
        lpCoeff = std::clamp(std::exp(-omega), T{0.05}, T{0.95});
    }

    void reset() noexcept {
        std::fill(bufferL.begin(), bufferL.end(), T{0});
        std::fill(bufferR.begin(), bufferR.end(), T{0});
        writePos = 0;
        lpStateL = 0;
        lpStateR = 0;
    }

    [[nodiscard]] std::pair<T, T> process(T inL, T inR, T space) noexcept {
        const size_t sz = bufferL.size();
        if (sz == 0) return { inL, inR };

        bufferL[writePos] = inL;
        bufferR[writePos] = inR;

        // Coprime prime delay spacings for realistic room boundaries
        static constexpr std::array<TapConfig, 8> TapsL{{
            {  7.2f,  0.62f }, { 13.8f, -0.52f }, { 21.4f,  0.46f }, { 29.1f, -0.39f },
            { 37.5f,  0.33f }, { 44.2f, -0.28f }, { 53.0f,  0.24f }, { 63.5f, -0.20f }
        }};

        static constexpr std::array<TapConfig, 8> TapsR{{
            {  8.9f,  0.60f }, { 15.2f, -0.50f }, { 19.8f,  0.45f }, { 31.4f, -0.37f },
            { 36.1f,  0.32f }, { 47.7f, -0.27f }, { 51.5f,  0.23f }, { 67.2f, -0.19f }
        }};

        const T safeSpace = std::clamp(space, T{0.1}, T{2.5});
        T sumL = 0, sumR = 0;

        for (size_t k = 0; k < 8; ++k) {
            // Left tap read
            const T dSamplesL = std::clamp(TapsL[k].delayMs * T{0.001} * sr * safeSpace, T{1}, static_cast<T>(sz - 2));
            T rPosL = static_cast<T>(writePos) - dSamplesL;
            while (rPosL < T{0}) rPosL += static_cast<T>(sz);
            while (rPosL >= static_cast<T>(sz)) rPosL -= static_cast<T>(sz);
            const size_t idx0L = static_cast<size_t>(rPosL);
            const size_t idx1L = (idx0L + 1) % sz;
            const T fracL = rPosL - static_cast<T>(idx0L);
            sumL += (bufferL[idx0L] + fracL * (bufferL[idx1L] - bufferL[idx0L])) * TapsL[k].gain;

            // Right tap read
            const T dSamplesR = std::clamp(TapsR[k].delayMs * T{0.001} * sr * safeSpace, T{1}, static_cast<T>(sz - 2));
            T rPosR = static_cast<T>(writePos) - dSamplesR;
            while (rPosR < T{0}) rPosR += static_cast<T>(sz);
            while (rPosR >= static_cast<T>(sz)) rPosR -= static_cast<T>(sz);
            const size_t idx0R = static_cast<size_t>(rPosR);
            const size_t idx1R = (idx0R + 1) % sz;
            const T fracR = rPosR - static_cast<T>(idx0R);
            sumR += (bufferR[idx0R] + fracR * (bufferR[idx1R] - bufferR[idx0R])) * TapsR[k].gain;
        }

        writePos = (writePos + 1) % sz;

        // One-pole boundary absorption filter (6.5 kHz target)
        lpStateL = (T{1} - lpCoeff) * sumL + lpCoeff * lpStateL;
        lpStateR = (T{1} - lpCoeff) * sumR + lpCoeff * lpStateR;

        constexpr T norm = T{0.35355339}; // 1 / sqrt(8)
        return { lpStateL * norm, lpStateR * norm };
    }

private:
    T sr{44100};
    T lpCoeff{0.45};
    std::vector<T> bufferL;
    std::vector<T> bufferR;
    size_t writePos{0};
    T lpStateL{0};
    T lpStateR{0};
};

// Single-channel loop filter with frequency-dependent decay shaping and damping
template <std::floating_point T>
class ReverbLoopFilter {
public:
    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        reset();
    }

    void reset() noexcept {
        dampState = 0;
        lowCutState = 0;
        lowCutPrevIn = 0;
        lsState = 0;
        midState1 = 0;
        midState2 = 0;
        hsState = 0;
    }

    // Configure loop filter coefficients based on line delay length tauSec and decay EQ
    void update(T tauSec, T baseRt60, T dampingHz, T lowCutHz,
                T decayRateLow, T decayRateLowFreq,
                T decayRateMid, T decayRateMidFreq, T decayRateMidQ,
                T decayRateHigh, T decayRateHighFreq) noexcept
    {
        const T rt60 = std::max(baseRt60, T{0.1});
        // Base Sabine gain for line: -60 dB attenuation over baseRt60
        baseGain = std::pow(T{10}, (-3 * tauSec) / rt60);

        // 1. High frequency damping (one-pole lowpass)
        const T omegaDamp = std::numbers::pi_v<T> * std::clamp(dampingHz, T{100}, sr * T{0.45}) / sr;
        dampCoeff = std::clamp(T{1} - std::tan(omegaDamp), T{0.01}, T{0.99});

        // 2. Low cut damping (one-pole highpass)
        if (lowCutHz > T{20}) {
            const T omegaLc = std::numbers::pi_v<T> * std::clamp(lowCutHz, T{10}, T{1000}) / sr;
            const T tanLc = std::tan(omegaLc);
            lowCutAlpha = (T{1} - tanLc) / (T{1} + tanLc);
            hasLowCut = true;
        } else {
            hasLowCut = false;
        }

        // Calculate line-specific dB loop gain adjustments:
        // DeltaG_dB = -60 * (tauSec / rt60) * (1 / M - 1)
        const T tauOverRt = tauSec / rt60;

        T deltaDbLow = 0;
        hasLowShelf = std::abs(decayRateLow - T{1}) > T{0.01};
        if (hasLowShelf) {
            const T mLow = std::clamp(decayRateLow, T{0.2}, T{3.5});
            deltaDbLow = -T{60} * tauOverRt * (T{1} / mLow - T{1});
        }

        T deltaDbMid = 0;
        hasMidPeak = std::abs(decayRateMid - T{1}) > T{0.01};
        if (hasMidPeak) {
            const T mMid = std::clamp(decayRateMid, T{0.2}, T{3.5});
            deltaDbMid = -T{60} * tauOverRt * (T{1} / mMid - T{1});
        }

        T deltaDbHigh = 0;
        hasHighShelf = std::abs(decayRateHigh - T{1}) > T{0.01};
        if (hasHighShelf) {
            const T mHigh = std::clamp(decayRateHigh, T{0.2}, T{3.5});
            deltaDbHigh = -T{60} * tauOverRt * (T{1} / mHigh - T{1});
        }

        // Passivity enforcement:
        // Base loop attenuation is -60 * tauOverRt dB.
        // To guarantee |H_loop(e^jw)| < 1.0 everywhere even under worst-case frequency overlap,
        // the total positive boost across the cascade cannot exceed 0.94 * 60 * tauOverRt dB.
        const T maxSafeBoost = T{60} * tauOverRt * T{0.94};
        const T totalPositiveBoost = std::max(T{0}, deltaDbLow) + std::max(T{0}, deltaDbMid) + std::max(T{0}, deltaDbHigh);
        if (totalPositiveBoost > maxSafeBoost && totalPositiveBoost > T{1e-4}) {
            const T scale = maxSafeBoost / totalPositiveBoost;
            if (deltaDbLow > T{0}) deltaDbLow *= scale;
            if (deltaDbMid > T{0}) deltaDbMid *= scale;
            if (deltaDbHigh > T{0}) deltaDbHigh *= scale;
        }

        if (hasLowShelf)
            setupLowShelf(deltaDbLow, decayRateLowFreq);
        if (hasMidPeak)
            setupMidPeak(deltaDbMid, decayRateMidFreq, decayRateMidQ);
        if (hasHighShelf)
            setupHighShelf(deltaDbHigh, decayRateHighFreq);
    }

    [[nodiscard]] inline T process(T x) noexcept {
        // Subnormal prevention
        if (std::abs(x) < T{1e-25}) x = T{0};

        // 1. High damping one-pole lowpass
        dampState = (T{1} - dampCoeff) * x + dampCoeff * dampState;
        T sig = dampState;

        // 2. Low cut damping one-pole highpass
        if (hasLowCut) {
            const T hp = (T{1} + lowCutAlpha) * T{0.5} * (sig - lowCutPrevIn) + lowCutAlpha * lowCutState;
            lowCutPrevIn = sig;
            lowCutState = hp;
            sig = hp;
        }

        // 3. Low shelf decay EQ (Direct Form II Transposed)
        if (hasLowShelf) {
            const T yLs = lsB0 * sig + lsState;
            lsState = lsB1 * sig - lsA1 * yLs;
            sig = yLs;
        }

        // 4. Mid peak decay EQ (Direct Form II Transposed biquad)
        if (hasMidPeak) {
            const T yMid = midB0 * sig + midState1;
            midState1 = midB1 * sig - midA1 * yMid + midState2;
            midState2 = midB2 * sig - midA2 * yMid;
            sig = yMid;
        }

        // 5. High shelf decay EQ (Direct Form II Transposed)
        if (hasHighShelf) {
            const T yHs = hsB0 * sig + hsState;
            hsState = hsB1 * sig - hsA1 * yHs;
            sig = yHs;
        }

        // Apply base Sabine feedback gain
        T out = sig * baseGain;

        // Soft saturation safety rail at +-4.0 (preserves natural delay line crest factor without clipping)
        if (out > T{4.0}) out = T{4.0};
        else if (out < T{-4.0}) out = T{-4.0};

        return out;
    }

private:
    void setupLowShelf(T deltaDb, T freqHz) noexcept {
        const T a = std::pow(T{10}, deltaDb / T{20});
        const T omega = std::numbers::pi_v<T> * std::clamp(freqHz, T{30}, T{2000}) / sr;
        const T k = std::tan(omega);

        if (a >= T{1}) {
            const T norm = T{1} + k;
            lsB0 = (T{1} + a * k) / norm;
            lsB1 = (a * k - T{1}) / norm;
            lsA1 = (k - T{1}) / norm;
        } else {
            const T norm = T{1} + k / a;
            lsB0 = (T{1} + k) / norm;
            lsB1 = (k - T{1}) / norm;
            lsA1 = (k / a - T{1}) / norm;
        }
    }

    void setupMidPeak(T deltaDb, T freqHz, T q) noexcept {
        const T a = std::pow(T{10}, deltaDb / T{40}); // amplitude ratio sqrt(linear)
        const T safeQ = std::clamp(q, T{0.2}, T{5.0});
        const T omega = T{2} * std::numbers::pi_v<T> * std::clamp(freqHz, T{100}, sr * T{0.45}) / sr;
        const T alpha = std::sin(omega) / (T{2} * safeQ);
        const T cosW = std::cos(omega);

        const T b0_raw = T{1} + alpha * a;
        const T b1_raw = -T{2} * cosW;
        const T b2_raw = T{1} - alpha * a;
        const T a0_raw = T{1} + alpha / a;
        const T a1_raw = -T{2} * cosW;
        const T a2_raw = T{1} - alpha / a;

        const T invA0 = T{1} / a0_raw;
        midB0 = b0_raw * invA0;
        midB1 = b1_raw * invA0;
        midB2 = b2_raw * invA0;
        midA1 = a1_raw * invA0;
        midA2 = a2_raw * invA0;
    }

    void setupHighShelf(T deltaDb, T freqHz) noexcept {
        const T a = std::pow(T{10}, deltaDb / T{20});
        const T omega = std::numbers::pi_v<T> * std::clamp(freqHz, T{500}, sr * T{0.45}) / sr;
        const T k = std::tan(omega);

        if (a >= T{1}) {
            const T norm = T{1} + k;
            hsB0 = (a + k) / norm;
            hsB1 = (k - a) / norm;
            hsA1 = (k - T{1}) / norm;
        } else {
            const T norm = T{1} / a + k;
            hsB0 = (T{1} + k) / norm;
            hsB1 = (k - T{1}) / norm;
            hsA1 = (k - T{1} / a) / norm;
        }
    }

    T sr{44100};
    T baseGain{0.8};
    T dampCoeff{0.5};
    T dampState{0};

    bool hasLowCut{false};
    T lowCutAlpha{0.99};
    T lowCutState{0};
    T lowCutPrevIn{0};

    // Low shelf states & coeffs
    bool hasLowShelf{false};
    T lsB0{1}, lsB1{0}, lsA1{0};
    T lsState{0};

    // Mid peak states & coeffs
    bool hasMidPeak{false};
    T midB0{1}, midB1{0}, midB2{0}, midA1{0}, midA2{0};
    T midState1{0}, midState2{0};

    // High shelf states & coeffs
    bool hasHighShelf{false};
    T hsB0{1}, hsB1{0}, hsA1{0};
    T hsState{0};
};

// Post-EQ 3-Band Equalizer with Auto-Gain Compensation for final reverb tonal sculpting
template <std::floating_point T>
class PostEqEngine {
public:
    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        reset();
    }

    void reset() noexcept {
        s1L = s2L = s1R = s2R = 0;
        s3L = s4L = s3R = s4R = 0;
        s5L = s6L = s5R = s6R = 0;
    }

    void setParameters(T lowGainDb, T lowFreqHz,
                       T midGainDb, T midFreqHz, T midQ,
                       T highGainDb, T highFreqHz) noexcept
    {
        active = (std::abs(lowGainDb) > T{0.05} || std::abs(midGainDb) > T{0.05} || std::abs(highGainDb) > T{0.05});
        if (!active) return;

        setupBiquad(b0_low, b1_low, b2_low, a1_low, a2_low, lowGainDb, lowFreqHz, T{0.7071}, true, false);
        setupBiquad(b0_mid, b1_mid, b2_mid, a1_mid, a2_mid, midGainDb, midFreqHz, midQ, false, false);
        setupBiquad(b0_hi, b1_hi, b2_hi, a1_hi, a2_hi, highGainDb, highFreqHz, T{0.7071}, false, true);

        // Auto-gain compensation: offset average boost/cut to prevent level jumps
        const T totalBoost = (lowGainDb + midGainDb + highGainDb) / T{3};
        autoGain = std::pow(T{10}, -totalBoost * T{0.4} / T{20});
    }

    [[nodiscard]] std::pair<T, T> process(T inL, T inR) noexcept {
        if (!active) return { inL, inR };

        // Band 1 (Low)
        const T y1L = b0_low * inL + s1L; s1L = b1_low * inL - a1_low * y1L + s2L; s2L = b2_low * inL - a2_low * y1L;
        const T y1R = b0_low * inR + s1R; s1R = b1_low * inR - a1_low * y1R + s2R; s2R = b2_low * inR - a2_low * y1R;

        // Band 2 (Mid)
        const T y2L = b0_mid * y1L + s3L; s3L = b1_mid * y1L - a1_mid * y2L + s4L; s4L = b2_mid * y1L - a2_mid * y2L;
        const T y2R = b0_mid * y1R + s3R; s3R = b1_mid * y1R - a1_mid * y2R + s4R; s4R = b2_mid * y1R - a2_mid * y2R;

        // Band 3 (High)
        const T y3L = b0_hi * y2L + s5L; s5L = b1_hi * y2L - a1_hi * y3L + s6L; s6L = b2_hi * y2L - a2_hi * y3L;
        const T y3R = b0_hi * y2R + s5R; s5R = b1_hi * y2R - a1_hi * y3R + s6R; s6R = b2_hi * y2R - a2_hi * y3R;

        return { y3L * autoGain, y3R * autoGain };
    }

private:
    void setupBiquad(T& b0, T& b1, T& b2, T& a1, T& a2,
                     T gainDb, T freqHz, T q, bool isLowShelf, bool isHighShelf) noexcept
    {
        const T a = std::pow(T{10}, gainDb / T{40});
        const T safeQ = std::clamp(q, T{0.2}, T{5.0});
        const T omega = T{2} * std::numbers::pi_v<T> * std::clamp(freqHz, T{20}, sr * T{0.45}) / sr;
        const T sinW = std::sin(omega);
        const T cosW = std::cos(omega);

        if (isLowShelf) {
            const T alpha = sinW / (T{2} * safeQ);
            const T beta = T{2} * std::sqrt(a) * alpha;
            const T a0 = (a + T{1}) + (a - T{1}) * cosW + beta;
            const T invA0 = T{1} / a0;
            b0 = (a * ((a + T{1}) - (a - T{1}) * cosW + beta)) * invA0;
            b1 = (T{2} * a * ((a - T{1}) - (a + T{1}) * cosW)) * invA0;
            b2 = (a * ((a + T{1}) - (a - T{1}) * cosW - beta)) * invA0;
            a1 = (-T{2} * ((a - T{1}) + (a + T{1}) * cosW)) * invA0;
            a2 = ((a + T{1}) + (a - T{1}) * cosW - beta) * invA0;
        } else if (isHighShelf) {
            const T alpha = sinW / (T{2} * safeQ);
            const T beta = T{2} * std::sqrt(a) * alpha;
            const T a0 = (a + T{1}) - (a - T{1}) * cosW + beta;
            const T invA0 = T{1} / a0;
            b0 = (a * ((a + T{1}) + (a - T{1}) * cosW + beta)) * invA0;
            b1 = (-T{2} * a * ((a - T{1}) + (a + T{1}) * cosW)) * invA0;
            b2 = (a * ((a + T{1}) - (a - T{1}) * cosW - beta)) * invA0;
            a1 = (T{2} * ((a - T{1}) - (a + T{1}) * cosW)) * invA0;
            a2 = ((a + T{1}) - (a - T{1}) * cosW - beta) * invA0;
        } else {
            // Peaking
            const T alpha = sinW / (T{2} * safeQ);
            const T a0 = T{1} + alpha / a;
            const T invA0 = T{1} / a0;
            b0 = (T{1} + alpha * a) * invA0;
            b1 = (-T{2} * cosW) * invA0;
            b2 = (T{1} - alpha * a) * invA0;
            a1 = (-T{2} * cosW) * invA0;
            a2 = (T{1} - alpha / a) * invA0;
        }
    }

    T sr{44100};
    bool active{false};
    T autoGain{1.0};
    T b0_low{1}, b1_low{0}, b2_low{0}, a1_low{0}, a2_low{0};
    T b0_mid{1}, b1_mid{0}, b2_mid{0}, a1_mid{0}, a2_mid{0};
    T b0_hi{1}, b1_hi{0}, b2_hi{0}, a1_hi{0}, a2_hi{0};

    T s1L{0}, s2L{0}, s1R{0}, s2R{0};
    T s3L{0}, s4L{0}, s3R{0}, s4R{0};
    T s5L{0}, s6L{0}, s5R{0}, s6R{0};
};

// Automatic Reverb Ducking Envelope Follower
// Triggers on dry incoming audio to suppress wet reverberant masking during loud signals
template <std::floating_point T>
class ReverbDucker {
public:
    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        env = 0;
        setAttackRelease(T{10.0}, T{150.0});
    }

    void reset() noexcept {
        env = 0;
    }

    void setAttackRelease(T attackMs, T releaseMs) noexcept {
        attCoeff = std::exp(-T{1} / (attackMs * T{0.001} * sr));
        relCoeff = std::exp(-T{1} / (releaseMs * T{0.001} * sr));
    }

    [[nodiscard]] inline T computeGain(T dryL, T dryR, T duckingAmount) noexcept {
        if (duckingAmount <= T{0.001}) return T{1.0};

        const T inputAbs = std::max(std::abs(dryL), std::abs(dryR));
        if (inputAbs > env) env = attCoeff * env + (T{1} - attCoeff) * inputAbs;
        else                env = relCoeff * env + (T{1} - relCoeff) * inputAbs;

        // Compression curve: above -30 dBFS, duck wet reverb up to -18 dB
        constexpr T threshold = T{0.03162277}; // -30 dB
        if (env <= threshold) return T{1.0};

        const T overRatio = (env - threshold) / (T{1.0} + env);
        const T duckDb = -T{18.0} * duckingAmount * std::clamp(overRatio * T{3.0}, T{0.0}, T{1.0});
        return std::pow(T{10}, duckDb / T{20});
    }

private:
    T sr{44100};
    T env{0};
    T attCoeff{0.99};
    T relCoeff{0.999};
};

// 16-Channel Orthogonal Householder Feedback Delay Network with Pro-R 2 Feature Set
template <std::floating_point T, size_t NumLines = 16>
class FdnReverb {
public:
    static_assert(NumLines == 16, "Orthogonal Householder FDN configured for 16 delay channels");

    struct Parameters {
        T decayTimeSec{2.5};     // Base decay time RT60 (0.2 to 20.0 seconds)
        T dampingHz{5000.0};     // High damping frequency (100 to 20000 Hz)
        T chaosModulation{0.2};  // Symplectic Hamiltonian chaotic injection (0.0 to 1.0)
        T dryWet{0.3};           // Dry / Wet mix (0.0 to 1.0)
        T space{1.0};            // Physical space/room scaling (0.1x to 2.0x)
        T predelayMs{0.0};       // Pre-delay in ms (0 to 500 ms)
        T distance{1.0};         // ER / Late balance (0.0 = close/ER, 1.0 = late diffuse tail)
        T diffusion{0.0};        // Transient diffusion / echo density (0.0 to 1.0)
        T stereoWidth{1.0};      // Stereo width multiplier (0.0 = mono, 2.0 = extra wide)
        T lowCutHz{20.0};        // Bass damping low cut (20 to 1000 Hz)
        T ducking{0.0};          // Dry-signal ducking depth (0.0 to 1.0)

        // Decay Rate EQ (frequency-dependent decay time multiplier)
        T decayRateLow{1.0};     // Low shelf decay multiplier (0.2x to 3.0x)
        T decayRateLowFreq{200.0};
        T decayRateMid{1.0};     // Mid peak decay multiplier (0.2x to 3.0x)
        T decayRateMidFreq{1200.0};
        T decayRateMidQ{0.7071};
        T decayRateHigh{1.0};    // High shelf decay multiplier (0.2x to 3.0x)
        T decayRateHighFreq{6000.0};

        // Post EQ (tonal output shaping with auto-gain compensation)
        T postEqLowGain{0.0};
        T postEqLowFreq{150.0};
        T postEqMidGain{0.0};
        T postEqMidFreq{1500.0};
        T postEqMidQ{0.7071};
        T postEqHighGain{0.0};
        T postEqHighFreq{8000.0};
    };

    FdnReverb() {
        prepare(T{44100});
    }

    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        hamiltonian.prepare(sr);
        predelay.prepare(sr, T{1.5});
        earlyReflections.prepare(sr);
        diffusers.prepare(sr);
        postEq.prepare(sr);
        ducker.prepare(sr);

        // 16 mutually coprime prime delay lengths (approximately 20ms to 75ms at base 44.1 kHz)
        static constexpr std::array<size_t, NumLines> BasePrimes{{
            1087, 1153, 1229, 1297, 1381, 1453, 1523, 1607,
            1693, 1777, 1867, 1973, 2069, 2161, 2267, 2377
        }};

        const T scale = sr / T{44100};
        for (size_t i = 0; i < NumLines; ++i) {
            baseLengths[i] = static_cast<size_t>(std::round(static_cast<T>(BasePrimes[i]) * scale));
            // Preallocate buffer large enough for 2.8x Space scaling and 192 kHz
            const size_t maxBuf = static_cast<size_t>(std::ceil(baseLengths[i] * T{2.8})) + 256;
            buffers[i].resize(maxBuf);
            std::fill(buffers[i].begin(), buffers[i].end(), T{0});
            writeIndices[i] = 0;
            loopFilters[i].prepare(sr);
        }

        reset();
    }

    void reset() noexcept {
        hamiltonian.reset();
        predelay.reset();
        earlyReflections.reset();
        diffusers.reset();
        postEq.reset();
        ducker.reset();

        for (size_t i = 0; i < NumLines; ++i) {
            std::fill(buffers[i].begin(), buffers[i].end(), T{0});
            writeIndices[i] = 0;
            loopFilters[i].reset();
        }
    }

    void setParameters(const Parameters& p) noexcept {
        params = p;
        diffusers.setParameters(params.diffusion, params.space);

        const T safeSpace = std::clamp(params.space, T{0.1}, T{2.2});
        for (size_t i = 0; i < NumLines; ++i) {
            currentLengths[i] = static_cast<T>(baseLengths[i]) * safeSpace;
            const T tauSec = currentLengths[i] / sr;

            loopFilters[i].update(tauSec, params.decayTimeSec, params.dampingHz, params.lowCutHz,
                                  params.decayRateLow, params.decayRateLowFreq,
                                  params.decayRateMid, params.decayRateMidFreq, params.decayRateMidQ,
                                  params.decayRateHigh, params.decayRateHighFreq);
        }

        postEq.setParameters(params.postEqLowGain, params.postEqLowFreq,
                             params.postEqMidGain, params.postEqMidFreq, params.postEqMidQ,
                             params.postEqHighGain, params.postEqHighFreq);
    }

    [[nodiscard]] std::pair<T, T> processSample(T inL, T inR) noexcept {
        // 1. Pre-delay stage with fractional interpolation
        const T predelaySamples = std::clamp(params.predelayMs * T{0.001} * sr, T{0}, sr * T{1.2});
        const auto [pL, pR] = predelay.process(inL, inR, predelaySamples);

        // 2. Early Reflections spatial generation
        const auto [erL, erR] = earlyReflections.process(pL, pR, params.space);

        // 3. Schroeder/Gerzon allpass diffusion smoothing (preserves stereo separation)
        const auto [diffL, diffR] = diffusers.process(pL, pR);

        // 4. Symplectic Hamiltonian chaotic injection with 16-channel phase projection
        const auto [qChaos, pChaos] = hamiltonian.stepState();
        const T chaosDepth = params.chaosModulation * T{8.0};

        std::array<T, NumLines> delayOutputs{};
        T sumDelayOut = 0;

        // 5. Read from 16 delay lines using 3rd-order Hermite interpolation
        for (size_t i = 0; i < NumLines; ++i) {
            constexpr T twoPiOver16 = T{2} * std::numbers::pi_v<T> / static_cast<T>(NumLines);
            const T theta = static_cast<T>(i) * twoPiOver16;
            const T modOffset = chaosDepth * (qChaos * std::cos(theta) + pChaos * std::sin(theta));

            const size_t bufSize = buffers[i].size();
            const T targetDelay = std::clamp(currentLengths[i] + modOffset, T{4}, static_cast<T>(bufSize - 4));

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

        // 6. Householder Unitary Reflection Matrix: A = I - (2 / N) * 1 * 1^T
        constexpr T householderFactor = T{2} / static_cast<T>(NumLines);
        const T householderTerm = sumDelayOut * householderFactor;

        std::array<T, NumLines> feedbackSignals{};
        for (size_t i = 0; i < NumLines; ++i) {
            const T mixed = delayOutputs[i] - householderTerm;
            // 7. Frequency-dependent damping & decay rate EQ per line
            feedbackSignals[i] = loopFilters[i].process(mixed);
        }

        // 8. True Stereo Write back into delay lines with input injection
        for (size_t i = 0; i < NumLines; ++i) {
            const T chInput = (i % 2 == 0) ? diffL : diffR;
            const T sign = (i % 4 == 0 || i % 4 == 1) ? T{1} : T{-1};
            const T inputInject = chInput * sign;
            buffers[i][writeIndices[i]] = inputInject + feedbackSignals[i];
            writeIndices[i] = (writeIndices[i] + 1) % buffers[i].size();
        }

        // 9. Decorrelated Stereo Output summation (orthogonal polarity vectors)
        T lateL = 0, lateR = 0;
        for (size_t i = 0; i < NumLines; ++i) {
            const T sign = (i % 4 == 0 || i % 4 == 3) ? T{1} : T{-1};
            if (i % 2 == 0) {
                lateL += sign * delayOutputs[i];
            } else {
                lateR += sign * delayOutputs[i];
            }
        }

        constexpr T norm = T{1} / std::numbers::sqrt2_v<T>;
        constexpr T scale8 = T{0.35355339059327373}; // 1.0 / sqrt(8)
        lateL *= norm * scale8;
        lateR *= norm * scale8;

        // 10. Distance Crossfade: equal-power blend between Early Reflections and Late Reverb
        const T dist = std::clamp(params.distance, T{0.0}, T{1.0});
        const T erGain = std::cos(dist * (std::numbers::pi_v<T> * T{0.5}));
        const T lateGain = std::sin(dist * (std::numbers::pi_v<T> * T{0.5}));

        const T wetSumL = erGain * erL + lateGain * lateL;
        const T wetSumR = erGain * erR + lateGain * lateR;

        // 11. Mid/Side Stereo Width processing
        const T mid = (wetSumL + wetSumR) * T{0.5};
        const T side = (wetSumL - wetSumR) * T{0.5} * std::clamp(params.stereoWidth, T{0.0}, T{2.0});
        const T wideWetL = mid + side;
        const T wideWetR = mid - side;

        // 12. Post-EQ tonal sculpt with auto-gain compensation
        const auto [postL, postR] = postEq.process(wideWetL, wideWetR);

        // 13. Dynamic Reverb Ducking from dry input
        const T duckGain = ducker.computeGain(inL, inR, params.ducking);
        const T finalWetL = postL * duckGain;
        const T finalWetR = postR * duckGain;

        // 14. Dry/Wet Mix
        const T wet = params.dryWet;
        const T dry = T{1} - wet;
        return { inL * dry + finalWetL * wet, inR * dry + finalWetR * wet };
    }

private:
    T sr{44100};
    Parameters params{};

    std::array<size_t, NumLines> baseLengths{};
    std::array<T, NumLines> currentLengths{};
    std::array<std::vector<T>, NumLines> buffers;
    std::array<size_t, NumLines> writeIndices{};

    std::array<ReverbLoopFilter<T>, NumLines> loopFilters;
    SymplecticHamiltonianPerturbator<T> hamiltonian;
    PreDelayEngine<T> predelay;
    EarlyReflectionsGenerator<T> earlyReflections;
    DiffusionCascade<T, 4> diffusers;
    PostEqEngine<T> postEq;
    ReverbDucker<T> ducker;
};

} // namespace openx::dsp
