#pragma once

#include <cmath>
#include <array>
#include <concepts>
#include <algorithm>
#include <numbers>
#include <utility>
#include <cstddef>

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
            if (std::abs(s) < T{1e-25}) s = T{0};
            return y;
        }
    };

    T sr{44100};
    FirstOrderAllpass ap1_1, ap1_2;
    FirstOrderAllpass ap2_1, ap2_2;
};

// 2nd-order Topology-Preserving Transform (TPT) SVF for Sidechain HPF and LPF
template <std::floating_point T>
class SidechainFilter {
public:
    void prepare(T sampleRate) noexcept {
        sr = sampleRate > T{0} ? sampleRate : T{44100};
        reset();
        setCutoffs(T{20}, T{20000});
    }

    void reset() noexcept {
        s1_hp = 0; s2_hp = 0;
        s1_lp = 0; s2_lp = 0;
    }

    void setCutoffs(T hpfHz, T lpfHz) noexcept {
        // 2nd-order Butterworth HPF (Q = 1/sqrt(2))
        const T hpFreq = std::clamp(hpfHz, T{10}, sr * T{0.49});
        const T wHp = std::numbers::pi_v<T> * hpFreq / sr;
        g_hp = std::tan(wHp);
        twoR_hp = T{1.4142135623730951};
        h_hp = T{1} / (T{1} + twoR_hp * g_hp + g_hp * g_hp);

        // 2nd-order Butterworth LPF (Q = 1/sqrt(2))
        const T lpFreq = std::clamp(lpfHz, T{50}, sr * T{0.49});
        const T wLp = std::numbers::pi_v<T> * lpFreq / sr;
        g_lp = std::tan(wLp);
        twoR_lp = T{1.4142135623730951};
        h_lp = T{1} / (T{1} + twoR_lp * g_lp + g_lp * g_lp);
    }

    [[nodiscard]] T processSample(T x) noexcept {
        // High-pass filter stage
        const T hp = (x - (twoR_hp + g_hp) * s1_hp - s2_hp) * h_hp;
        const T bp_hp = g_hp * hp + s1_hp;
        s1_hp = g_hp * hp + bp_hp;
        const T lp_hp = g_hp * bp_hp + s2_hp;
        s2_hp = g_hp * bp_hp + lp_hp;
        const T afterHp = hp;

        // Low-pass filter stage
        const T hp_lp = (afterHp - (twoR_lp + g_lp) * s1_lp - s2_lp) * h_lp;
        const T bp_lp = g_lp * hp_lp + s1_lp;
        s1_lp = g_lp * hp_lp + bp_lp;
        const T lp_lp = g_lp * bp_lp + s2_lp;
        s2_lp = g_lp * bp_lp + lp_lp;

        // Denormal flushing
        if (std::abs(s1_hp) < T{1e-25}) s1_hp = 0;
        if (std::abs(s2_hp) < T{1e-25}) s2_hp = 0;
        if (std::abs(s1_lp) < T{1e-25}) s1_lp = 0;
        if (std::abs(s2_lp) < T{1e-25}) s2_lp = 0;

        return lp_lp;
    }

private:
    T sr{44100};
    T g_hp{0}, twoR_hp{1.41421356f}, h_hp{1};
    T s1_hp{0}, s2_hp{0};
    T g_lp{100}, twoR_lp{1.41421356f}, h_lp{1};
    T s1_lp{0}, s2_lp{0};
};

enum class CompressionStyle : int {
    Clean = 0,
    Classic = 1,
    Opto = 2,
    Vocal = 3,
    Mastering = 4,
    Punch = 5,
    Bus = 6,
    Pumping = 7
};

template <std::floating_point T>
class CompressorEngine {
public:
    static constexpr size_t LookaheadBufferCapacity = 8192;
    static constexpr size_t LookaheadMask = LookaheadBufferCapacity - 1;

    struct Parameters {
        T thresholdDb{-20};
        T ratio{4};
        T kneeDb{6};
        T attackMs{15};
        T releaseMs{120};
        T makeupGainDb{0};
        T transientPunch{0.5}; // TS-WD dynamic cross-modulation [0, 1]
        CompressionStyle style{CompressionStyle::Clean};
        T holdMs{0};
        T lookaheadMs{0};
        bool autoRelease{false};
        bool autoGain{false};
        T scHpfHz{20};
        T scLpfHz{20000};
        bool scAudition{false};
        T mix{1.0}; // 0.0 to 1.0 (wet/dry)
    };

    void prepare(T sampleRate) noexcept {
        sr = sampleRate > T{0} ? sampleRate : T{44100};
        analyticFollowerL.prepare(sr);
        analyticFollowerR.prepare(sr);
        scFilterL.prepare(sr);
        scFilterR.prepare(sr);
        reset();
        setParameters(params);
    }

    void reset() noexcept {
        analyticFollowerL.reset();
        analyticFollowerR.reset();
        scFilterL.reset();
        scFilterR.reset();
        delayBufferL.fill(0);
        delayBufferR.fill(0);
        delayWritePos = 0;
        envelope = 0;
        prevEnvelope = 0;
        rmsEnvelope = 0;
        gainReductionLinear = 1;
        holdCounter = 0;
    }

    void setParameters(const Parameters& p) noexcept {
        params = p;

        scFilterL.setCutoffs(params.scHpfHz, params.scLpfHz);
        scFilterR.setCutoffs(params.scHpfHz, params.scLpfHz);

        // Effective Knee adjustment by style
        effectiveKneeDb = params.kneeDb;
        if (params.style == CompressionStyle::Opto) {
            effectiveKneeDb = std::min(params.kneeDb + T{6}, T{36});
        } else if (params.style == CompressionStyle::Mastering) {
            effectiveKneeDb = std::min(params.kneeDb + T{4}, T{36});
        }

        invRatio = T{1} / std::max(params.ratio, T{1});
        slope = T{1} - invRatio;
        halfKnee = effectiveKneeDb * T{0.5};
        invTwoKnee = (effectiveKneeDb > T{0}) ? (T{1} / (T{2} * effectiveKneeDb)) : T{0};
        lowerKneeDb = params.thresholdDb - halfKnee;
        lowerKneeLinear = std::pow(T{10}, lowerKneeDb / T{20});

        // Nominal ballistics coefficients
        const T nominalAttMs = std::max(params.attackMs, T{0.01});
        const T nominalRelMs = std::max(params.releaseMs, T{1.0});
        attCoeff = std::exp(-T{1} / (nominalAttMs * T{0.001} * sr));
        relCoeff = std::exp(-T{1} / (nominalRelMs * T{0.001} * sr));

        // Lookahead delay in samples
        const T targetLookaheadSamples = std::clamp(params.lookaheadMs * T{0.001} * sr, T{0}, T{LookaheadBufferCapacity - 1});
        lookaheadSamples = static_cast<size_t>(std::round(targetLookaheadSamples));

        // Hold samples
        holdSamples = static_cast<size_t>(std::max(params.holdMs, T{0}) * T{0.001} * sr);

        // Auto Gain Calculation
        T totalMakeupDb = params.makeupGainDb;
        if (params.autoGain) {
            totalMakeupDb += computeAutoGainDb(params.thresholdDb, params.ratio, effectiveKneeDb, params.style);
        }
        makeupLinear = std::pow(T{10}, totalMakeupDb / T{20});
    }

    [[nodiscard]] static T computeAutoGainDb(T thresholdDb, T ratio, T /*kneeDb*/, CompressionStyle style) noexcept {
        const T invR = T{1} / std::max(ratio, T{1});
        const T sl = T{1} - invR;
        // Dynamic makeup compensates approximate gain reduction at nominal program level
        T autoDb = -T{0.5} * thresholdDb * sl;
        if (style == CompressionStyle::Vocal) {
            autoDb += T{1.5};
        } else if (style == CompressionStyle::Bus) {
            autoDb += T{1.0};
        } else if (style == CompressionStyle::Punch) {
            autoDb += T{0.8};
        }
        return std::clamp(autoDb, T{0}, T{30});
    }

    // Process single channel (mono)
    [[nodiscard]] T processSample(T input) noexcept {
        // 1. Sidechain filtering
        const T scFiltered = scFilterL.processSample(input);
        if (params.scAudition) {
            return scFiltered;
        }

        // 2. Lookahead delay line
        delayBufferL[delayWritePos] = input;
        const size_t readPos = (delayWritePos + LookaheadBufferCapacity - lookaheadSamples) & LookaheadMask;
        const T delayedIn = delayBufferL[readPos];
        delayWritePos = (delayWritePos + 1) & LookaheadMask;

        // 3. Detector signal conditioning based on style
        T detInput = scFiltered;
        if (params.style == CompressionStyle::Classic) {
            // Vintage feedback-style detector coupling
            detInput *= (T{0.6} + T{0.4} * gainReductionLinear);
        }

        // Instantaneous envelope
        const T instantEnv = analyticFollowerL.computeInstantaneousEnvelope(detInput);

        // Update RMS / running energy for auto-release and vocal mode
        constexpr T rmsAlpha = T{0.002};
        rmsEnvelope = (T{1} - rmsAlpha) * rmsEnvelope + rmsAlpha * instantEnv;
        if (rmsEnvelope < T{1e-25}) rmsEnvelope = T{0};

        // 4. Style-dependent ballistics & auto-release
        T activeAttCoeff = attCoeff;
        T activeRelCoeff = relCoeff;
        computeEffectiveCoefficients(instantEnv, activeAttCoeff, activeRelCoeff);

        // Envelope ballistics integration
        const T envCoeff = (instantEnv > envelope) ? activeAttCoeff : activeRelCoeff;
        envelope = envCoeff * envelope + (T{1} - envCoeff) * instantEnv;
        if (envelope < T{1e-25}) envelope = T{0};

        // Effective detector envelope for vocal / mastering
        T detEnv = envelope;
        if (params.style == CompressionStyle::Vocal) {
            // Dual-detector hybrid (peak + RMS leveler)
            detEnv = T{0.6} * envelope + T{0.4} * rmsEnvelope;
        }

        // 5. Gain Computer (C2 continuous logarithmic gain reduction)
        const T targetGrLinear = computeGainReductionLinear(detEnv);

        // 6. Hold time and ballistics smoothing (incorporating lookahead delay horizon)
        const size_t totalHoldSamples = holdSamples + lookaheadSamples;
        if (targetGrLinear <= gainReductionLinear) {
            // Attacking into deeper gain reduction
            gainReductionLinear = activeAttCoeff * gainReductionLinear + (T{1} - activeAttCoeff) * targetGrLinear;
            holdCounter = totalHoldSamples;
        } else {
            // Releasing
            if (holdCounter > 0) {
                --holdCounter;
                // Hold gain constant
            } else {
                gainReductionLinear = activeRelCoeff * gainReductionLinear + (T{1} - activeRelCoeff) * targetGrLinear;
            }
        }

        // 7. Dynamic TS-WD Punch Modulation & Saturation
        const T envDelta = std::max(T{0}, instantEnv - prevEnvelope);
        prevEnvelope = instantEnv;
        if (prevEnvelope < T{1e-25}) prevEnvelope = T{0};

        T punchFactor = T{1};
        if (params.style == CompressionStyle::Punch) {
            punchFactor += (params.transientPunch * T{1.5}) * std::tanh(T{14} * envDelta);
        } else if (params.transientPunch > T{0}) {
            punchFactor += params.transientPunch * std::tanh(T{10} * envDelta);
        }

        T compressedSustain = delayedIn * gainReductionLinear * punchFactor;

        // Warm harmonic saturation for Classic, Opto, and Bus modes
        if (params.style == CompressionStyle::Classic || params.style == CompressionStyle::Opto || params.style == CompressionStyle::Bus) {
            const T grMagnitudeDb = -getGainReductionDb();
            if (grMagnitudeDb > T{0.5}) {
                const T drive = T{1} + T{0.03} * std::min(grMagnitudeDb, T{12});
                compressedSustain = std::tanh(compressedSustain * drive) / drive;
            }
        }

        const T wetOutput = compressedSustain * makeupLinear;

        // 8. Phase-aligned Dry/Wet Mix
        return (T{1} - params.mix) * delayedIn + params.mix * wetOutput;
    }

    // Process stereo channels with phase-aligned linking
    void processStereo(T inL, T inR, T& outL, T& outR) noexcept {
        // 1. Sidechain filtering
        const T scL = scFilterL.processSample(inL);
        const T scR = scFilterR.processSample(inR);

        if (params.scAudition) {
            outL = scL;
            outR = scR;
            return;
        }

        // 2. Lookahead delay line
        delayBufferL[delayWritePos] = inL;
        delayBufferR[delayWritePos] = inR;
        const size_t readPos = (delayWritePos + LookaheadBufferCapacity - lookaheadSamples) & LookaheadMask;
        const T delayedInL = delayBufferL[readPos];
        const T delayedInR = delayBufferR[readPos];
        delayWritePos = (delayWritePos + 1) & LookaheadMask;

        // 3. Style-dependent detector conditioning
        T detInL = scL;
        T detInR = scR;
        if (params.style == CompressionStyle::Classic) {
            const T fb = T{0.6} + T{0.4} * gainReductionLinear;
            detInL *= fb;
            detInR *= fb;
        }

        // Instantaneous envelopes
        const T envL = analyticFollowerL.computeInstantaneousEnvelope(detInL);
        const T envR = analyticFollowerR.computeInstantaneousEnvelope(detInR);

        // Linked stereo detection (preserves stereo image stability)
        const T instantEnv = std::max(envL, envR);

        constexpr T rmsAlpha = T{0.002};
        rmsEnvelope = (T{1} - rmsAlpha) * rmsEnvelope + rmsAlpha * instantEnv;
        if (rmsEnvelope < T{1e-25}) rmsEnvelope = T{0};

        // 4. Style-dependent ballistics & auto-release
        T activeAttCoeff = attCoeff;
        T activeRelCoeff = relCoeff;
        computeEffectiveCoefficients(instantEnv, activeAttCoeff, activeRelCoeff);

        const T envCoeff = (instantEnv > envelope) ? activeAttCoeff : activeRelCoeff;
        envelope = envCoeff * envelope + (T{1} - envCoeff) * instantEnv;
        if (envelope < T{1e-25}) envelope = T{0};

        T detEnv = envelope;
        if (params.style == CompressionStyle::Vocal) {
            detEnv = T{0.6} * envelope + T{0.4} * rmsEnvelope;
        }

        // 5. Gain Computer
        const T targetGrLinear = computeGainReductionLinear(detEnv);

        // 6. Hold time and ballistics smoothing (incorporating lookahead delay horizon)
        const size_t totalHoldSamples = holdSamples + lookaheadSamples;
        if (targetGrLinear <= gainReductionLinear) {
            gainReductionLinear = activeAttCoeff * gainReductionLinear + (T{1} - activeAttCoeff) * targetGrLinear;
            holdCounter = totalHoldSamples;
        } else {
            if (holdCounter > 0) {
                --holdCounter;
            } else {
                gainReductionLinear = activeRelCoeff * gainReductionLinear + (T{1} - activeRelCoeff) * targetGrLinear;
            }
        }

        // 7. Dynamic TS-WD Punch Modulation & Saturation
        const T envDelta = std::max(T{0}, instantEnv - prevEnvelope);
        prevEnvelope = instantEnv;
        if (prevEnvelope < T{1e-25}) prevEnvelope = T{0};

        T punchFactor = T{1};
        if (params.style == CompressionStyle::Punch) {
            punchFactor += (params.transientPunch * T{1.5}) * std::tanh(T{14} * envDelta);
        } else if (params.transientPunch > T{0}) {
            punchFactor += params.transientPunch * std::tanh(T{10} * envDelta);
        }

        T compL = delayedInL * gainReductionLinear * punchFactor;
        T compR = delayedInR * gainReductionLinear * punchFactor;

        if (params.style == CompressionStyle::Classic || params.style == CompressionStyle::Opto || params.style == CompressionStyle::Bus) {
            const T grMagnitudeDb = -getGainReductionDb();
            if (grMagnitudeDb > T{0.5}) {
                const T drive = T{1} + T{0.03} * std::min(grMagnitudeDb, T{12});
                compL = std::tanh(compL * drive) / drive;
                compR = std::tanh(compR * drive) / drive;
            }
        }

        const T wetL = compL * makeupLinear;
        const T wetR = compR * makeupLinear;

        // 8. Phase-aligned Dry/Wet Mix
        outL = (T{1} - params.mix) * delayedInL + params.mix * wetL;
        outR = (T{1} - params.mix) * delayedInR + params.mix * wetR;
    }

    [[nodiscard]] T getGainReductionDb() const noexcept {
        return T{20} * std::log10(std::max(gainReductionLinear, T{1e-5}));
    }

    [[nodiscard]] size_t getLookaheadSamples() const noexcept {
        return lookaheadSamples;
    }

private:
    [[nodiscard]] T computeGainReductionLinear(T detEnv) const noexcept {
        if (detEnv <= lowerKneeLinear) {
            return T{1};
        }

        constexpr T minVal = T{1e-5};
        const T envDb = T{20} * std::log10(std::max(detEnv, minVal));
        const T deltaOvershoot = envDb - params.thresholdDb;

        T grDb = 0;
        if (effectiveKneeDb > T{0} && deltaOvershoot >= -halfKnee && deltaOvershoot <= halfKnee) {
            const T k = deltaOvershoot + halfKnee;
            grDb = slope * (k * k) * invTwoKnee;
        } else if (deltaOvershoot > halfKnee) {
            // In vocal mode, gently tighten ratio on high peaks
            T activeSlope = slope;
            if (params.style == CompressionStyle::Vocal && deltaOvershoot > T{6}) {
                activeSlope = std::min(slope * (T{1} + T{0.02} * (deltaOvershoot - T{6})), T{0.95});
            }
            grDb = activeSlope * deltaOvershoot;
        }

        // Pumping mode: enhance gain reduction depth slightly
        if (params.style == CompressionStyle::Pumping) {
            grDb *= T{1.2};
        }

        return std::exp(-grDb * (std::numbers::ln10_v<T> / T{20}));
    }

    void computeEffectiveCoefficients(T instantEnv, T& activeAttCoeff, T& activeRelCoeff) const noexcept {
        T effAttMs = std::max(params.attackMs, T{0.01});
        T effRelMs = std::max(params.releaseMs, T{1.0});

        // 1. Opto style non-linear photocell ballistics
        if (params.style == CompressionStyle::Opto) {
            // Slower initial onset on small overshoots, faster on strong peaks
            const T currentGr = std::clamp(T{1} - gainReductionLinear, T{0}, T{1});
            effAttMs *= (T{1.3} - T{0.5} * currentGr);
            // Two-stage release: fast initial recovery, extended memory tail
            effRelMs *= (T{0.4} + T{1.6} * currentGr);
        }

        // 2. Bus style snappy VCA curves
        if (params.style == CompressionStyle::Bus) {
            effAttMs *= T{0.85};
        }

        // 3. Auto-Release / Program Dependency
        if (params.autoRelease) {
            const T crest = (instantEnv + T{1e-5}) / (rmsEnvelope + T{1e-5});
            if (crest > T{2.2}) {
                // Short transient burst: release faster to prevent audible pumping
                const T speedup = std::clamp(crest / T{2.2}, T{1.0}, T{3.5});
                effRelMs /= speedup;
            } else if (crest < T{1.4}) {
                // Sustained low-frequency or pad material: release slower to avoid distortion
                const T slowdown = std::clamp(T{1.4} / std::max(crest, T{0.5}), T{1.0}, T{2.5});
                effRelMs *= slowdown;
            }
        }

        // 4. Pumping mode accelerated ballistics
        if (params.style == CompressionStyle::Pumping) {
            effAttMs = std::max(effAttMs * T{0.7}, T{0.01});
            effRelMs = std::max(effRelMs * T{0.6}, T{5.0});
        }

        activeAttCoeff = std::exp(-T{1} / (effAttMs * T{0.001} * sr));
        activeRelCoeff = std::exp(-T{1} / (effRelMs * T{0.001} * sr));
    }

    T sr{44100};
    Parameters params{};
    T attCoeff{0}, relCoeff{0};
    T invRatio{0.75}, slope{0.75};
    T effectiveKneeDb{6.0}, halfKnee{3.0}, invTwoKnee{0.083333};
    T lowerKneeDb{-23.0}, lowerKneeLinear{0.0708};
    T envelope{0}, prevEnvelope{0}, rmsEnvelope{0};
    T gainReductionLinear{1};
    T makeupLinear{1};

    size_t lookaheadSamples{0};
    size_t holdSamples{0};
    size_t holdCounter{0};

    AnalyticEnvelopeFollower<T> analyticFollowerL, analyticFollowerR;
    SidechainFilter<T> scFilterL, scFilterR;

    std::array<T, LookaheadBufferCapacity> delayBufferL{};
    std::array<T, LookaheadBufferCapacity> delayBufferR{};
    size_t delayWritePos{0};
};

} // namespace openx::dsp
