#pragma once

#include <cmath>
#include <array>
#include <concepts>
#include <algorithm>
#include <openx_dsp/eq/dynamic_biquad_engine.hpp>

namespace openx::dsp {

enum class GateMode : int {
    Gate = 0,
    Duck = 1,
    Expander = 2
};

enum class GateStyle : int {
    Clean = 0,
    Classic = 1,
    Vocal = 2
};

enum class GateState : int {
    Closed = 0,
    Open = 1,
    Hold = 2,
    Ducking = 3
};

template <std::floating_point T, size_t LookaheadSamples = 8192>
    requires ((LookaheadSamples & (LookaheadSamples - 1)) == 0)
class PredictiveGate {
public:
    static constexpr size_t BufferMask = LookaheadSamples - 1;

    struct Parameters {
        int mode{0};                     // 0: Gate, 1: Duck, 2: Expander
        int style{0};                    // 0: Clean, 1: Classic, 2: Vocal
        T openThresholdDb{-30.0};        // Open threshold in dB (-60 to 0)
        T closeThresholdDb{-36.0};       // Close threshold / hysteresis in dB (-60 to 0)
        T ratio{4.0};                    // Expansion ratio (1.0 to 20.0+)
        T rangeDb{-60.0};                // Attenuation range floor in dB (-80 to 0, <= -80 is mute)
        T kneeDb{3.0};                   // Knee smoothness in dB (0 to 24)
        T attackMs{2.0};                 // Attack time in ms (0.01 to 100)
        T holdMs{20.0};                  // Hold time in ms (0 to 1000)
        T releaseMs{80.0};               // Release time in ms (5 to 2000)
        T lookaheadMs{5.0};              // Lookahead time in ms (0 to 20)
        T scLowCutHz{20.0};              // Sidechain HPF frequency in Hz (10 to 10000)
        T scHighCutHz{20000.0};          // Sidechain LPF frequency in Hz (100 to 22000)
        T dryWet{1.0};                   // 0.0 (dry) to 1.0 (wet)
    };

    void prepare(T sampleRate) noexcept {
        sr = sampleRate > T{0} ? sampleRate : T{48000};
        scHpf.prepare(sr);
        scLpf.prepare(sr);
        reset();
    }

    void reset() noexcept {
        delayBuffer.fill(T{0});
        writeIndex = 0;
        gateState = (params.mode == static_cast<int>(GateMode::Duck)) ? GateState::Open : GateState::Closed;
        holdCounter = 0;
        currentGain = (params.mode == static_cast<int>(GateMode::Duck)) ? T{1} : T{0};
        e0 = T{0}; e1 = T{0}; e2 = T{0};
        detectorEnvelope = T{0};
        detHoldCounter = 0;
        currentDelaySamples = targetDelaySamples;
        scHpf.reset();
        scLpf.reset();
    }

    void setParameters(const Parameters& p) noexcept {
        params = p;

        // Enforce Schmitt trigger constraint: close threshold <= open threshold
        effectiveCloseThreshDb = std::min(params.closeThresholdDb, params.openThresholdDb);
        openThreshLinear = std::pow(T{10}, params.openThresholdDb / T{20});
        closeThreshLinear = std::pow(T{10}, effectiveCloseThreshDb / T{20});

        floorLinear = (params.rangeDb <= T{-79.9}) ? T{0} : std::pow(T{10}, params.rangeDb / T{20});

        // Ballistics time constants
        const T attMs = std::max(params.attackMs, T{0.01});
        const T relMs = std::max(params.releaseMs, T{1.0});

        attCoeff = std::exp(-T{1} / (attMs * T{0.001} * sr));
        relCoeff = std::exp(-T{1} / (relMs * T{0.001} * sr));

        // Style-specific ballistics adaptations
        attCoeffVocal = std::exp(-T{1} / ((attMs * T{1.3} + T{0.3}) * T{0.001} * sr));
        relCoeffSlow  = std::exp(-T{1} / ((relMs * T{2.0}) * T{0.001} * sr));

        attCoeffClassic = std::exp(-T{1} / (std::max(attMs * T{0.8}, T{0.01}) * T{0.001} * sr));
        relCoeffClassicTail = std::exp(-T{1} / ((relMs * T{2.5}) * T{0.001} * sr));

        detHoldSamples = static_cast<size_t>(T{0.004} * sr);
        detectorRelCoeff = std::exp(-T{1} / (T{0.008} * sr));

        holdSamples = static_cast<size_t>(std::max(params.holdMs, T{0}) * T{0.001} * sr);

        // Lookahead delay in samples (clamped to BufferMask)
        const T lookSamples = params.lookaheadMs * T{0.001} * sr;
        targetDelaySamples = std::clamp(static_cast<size_t>(std::round(lookSamples)), size_t{0}, BufferMask);
        currentDelaySamples = targetDelaySamples;

        // Sidechain filtering
        scHpf.setParameters(TptStateVariableFilter<T>::Type::Highpass, params.scLowCutHz, T{0.7071}, T{1});
        scLpf.setParameters(TptStateVariableFilter<T>::Type::Lowpass, params.scHighCutHz, T{0.7071}, T{1});
    }

    [[nodiscard]] T filterSidechain(T scInput) noexcept {
        T scSignal = scInput;
        if (params.scLowCutHz > T{15.0}) {
            scSignal = scHpf.processSample(scSignal);
        }
        if (params.scHighCutHz < sr * T{0.49}) {
            scSignal = scLpf.processSample(scSignal);
        }
        return scSignal;
    }

    [[nodiscard]] T detectLevel(T filteredSc) noexcept {
        const T absSc = std::abs(filteredSc);
        e2 = e1;
        e1 = e0;
        e0 = absSc;

        const T velocity = e0 - e1;
        const T accel = e0 - T{2} * e1 + e2;

        T predictedPeak = absSc;
        if (velocity > T{0}) {
            constexpr T h = T{1.0};
            predictedPeak = std::max(absSc, e0 + velocity * h + T{0.5} * std::max(T{0}, accel) * h * h);
        }

        if (predictedPeak >= detectorEnvelope) {
            detectorEnvelope = predictedPeak;
            detHoldCounter = detHoldSamples;
        } else {
            if (detHoldCounter > 0) {
                --detHoldCounter;
            } else {
                detectorEnvelope = detectorRelCoeff * detectorEnvelope + (T{1} - detectorRelCoeff) * predictedPeak;
            }
        }

        return detectorEnvelope;
    }

    [[nodiscard]] T processWithLevel(T mainInput, T detLevelLinear, T auditionSignal, bool auditionSidechain = false) noexcept {
        // 1. Write main input into delay ring buffer
        delayBuffer[writeIndex] = mainInput;

        // 2. Read delayed main sample
        const size_t readIndex = (writeIndex + LookaheadSamples - currentDelaySamples) & BufferMask;
        const T delayedMain = (currentDelaySamples == 0) ? mainInput : delayBuffer[readIndex];
        writeIndex = (writeIndex + 1) & BufferMask;

        if (auditionSidechain) {
            return auditionSignal;
        }

        constexpr T minLinear = T{1e-5};
        const T detLevel = std::max(detLevelLinear, minLinear);
        const T detDb = T{20} * std::log10(detLevel);

        // 3. State Machine & Target Gain Evaluation
        T targetGainLinear = T{1};
        const auto mode = static_cast<GateMode>(params.mode);
        const auto style = static_cast<GateStyle>(params.style);

        if (mode == GateMode::Gate) {
            // Schmitt Trigger Dual-Threshold Hysteresis with Soft Knee
            if (gateState == GateState::Closed) {
                if (detDb >= params.openThresholdDb || detLevel >= openThreshLinear) {
                    gateState = GateState::Open;
                    holdCounter = holdSamples;
                    targetGainLinear = T{1};
                } else if (params.kneeDb > T{0.1} && detDb > params.openThresholdDb - params.kneeDb) {
                    const T delta = (detDb - (params.openThresholdDb - params.kneeDb)) / params.kneeDb;
                    const T t = std::clamp(delta, T{0}, T{1});
                    const T interpDb = params.rangeDb * (T{1} - t);
                    targetGainLinear = (params.rangeDb <= T{-79.9} && interpDb <= T{-79.9}) ? T{0} : std::pow(T{10}, interpDb / T{20});
                } else {
                    targetGainLinear = floorLinear;
                }
            } else {
                // Currently Open or in Hold
                if (detDb >= effectiveCloseThreshDb) {
                    gateState = GateState::Open;
                    holdCounter = holdSamples;
                    targetGainLinear = T{1};
                } else if (holdCounter > 0) {
                    --holdCounter;
                    gateState = GateState::Hold;
                    targetGainLinear = T{1};
                } else {
                    gateState = GateState::Closed;
                    if (params.kneeDb > T{0.1} && detDb > effectiveCloseThreshDb - params.kneeDb) {
                        const T delta = (detDb - (effectiveCloseThreshDb - params.kneeDb)) / params.kneeDb;
                        const T t = std::clamp(delta, T{0}, T{1});
                        const T interpDb = params.rangeDb * (T{1} - t);
                        targetGainLinear = (params.rangeDb <= T{-79.9} && interpDb <= T{-79.9}) ? T{0} : std::pow(T{10}, interpDb / T{20});
                    } else {
                        targetGainLinear = floorLinear;
                    }
                }
            }
        } else if (mode == GateMode::Duck) {
            // Inverted Gate (Ducker) with Hysteresis
            if (gateState == GateState::Open) {
                if (detDb >= params.openThresholdDb || detLevel >= openThreshLinear) {
                    gateState = GateState::Ducking;
                    holdCounter = holdSamples;
                }
            } else {
                // Currently Ducking or in Hold
                if (detDb >= effectiveCloseThreshDb) {
                    gateState = GateState::Ducking;
                    holdCounter = holdSamples;
                } else if (holdCounter > 0) {
                    --holdCounter;
                    gateState = GateState::Hold;
                } else {
                    gateState = GateState::Open;
                }
            }

            if (gateState == GateState::Ducking || gateState == GateState::Hold) {
                if (params.kneeDb > T{0.1} && detDb < params.openThresholdDb + params.kneeDb) {
                    const T delta = (detDb - params.openThresholdDb) / params.kneeDb;
                    const T t = std::clamp(delta, T{0}, T{1});
                    const T interpDb = params.rangeDb * t;
                    targetGainLinear = (params.rangeDb <= T{-79.9} && interpDb <= T{-79.9}) ? T{0} : std::pow(T{10}, interpDb / T{20});
                } else {
                    targetGainLinear = floorLinear;
                }
            } else {
                targetGainLinear = T{1};
            }
        } else { // GateMode::Expander
            // Downward Expander with Soft Knee, Ratio, and Hold
            const T thresh = params.openThresholdDb;
            const T ratio = std::max(params.ratio, T{1.0});
            const T knee = params.kneeDb;
            const T halfKnee = knee * T{0.5};

            if (detDb >= thresh) {
                gateState = GateState::Open;
                holdCounter = holdSamples;
                targetGainLinear = T{1};
            } else if (holdCounter > 0) {
                --holdCounter;
                gateState = GateState::Hold;
                targetGainLinear = T{1};
            } else {
                gateState = GateState::Closed;
                const T delta = thresh - detDb; // delta > 0
                T targetGainDb = T{0};

                if (knee > T{0.05} && delta < halfKnee) {
                    const T y = delta + halfKnee;
                    targetGainDb = -(ratio - T{1}) * (y * y) / (T{4} * std::max(halfKnee, T{0.01}));
                } else {
                    targetGainDb = -(ratio - T{1}) * delta;
                }

                targetGainDb = std::max(targetGainDb, params.rangeDb);

                if (params.rangeDb <= T{-79.9} && targetGainDb <= T{-79.9}) {
                    targetGainLinear = T{0};
                } else {
                    targetGainLinear = std::pow(T{10}, targetGainDb / T{20});
                }
            }
        }

        // 4. Ballistics Smoothing with Style Modifiers
        T effectiveAtt = attCoeff;
        T effectiveRel = relCoeff;

        if (style == GateStyle::Vocal) {
            effectiveAtt = attCoeffVocal;
            if (currentGain < T{0.5}) {
                effectiveRel = relCoeffSlow;
            }
        } else if (style == GateStyle::Classic) {
            effectiveAtt = attCoeffClassic;
            if (currentGain < T{0.1}) {
                effectiveRel = relCoeffClassicTail;
            }
        }

        if (mode == GateMode::Duck) {
            if (targetGainLinear < currentGain) {
                currentGain = effectiveAtt * currentGain + (T{1} - effectiveAtt) * targetGainLinear;
            } else {
                currentGain = effectiveRel * currentGain + (T{1} - effectiveRel) * targetGainLinear;
            }
        } else {
            if (targetGainLinear > currentGain) {
                currentGain = effectiveAtt * currentGain + (T{1} - effectiveAtt) * targetGainLinear;
            } else {
                currentGain = effectiveRel * currentGain + (T{1} - effectiveRel) * targetGainLinear;
            }
        }

        // 5. Apply gain reduction to delayed main signal
        const T wetSample = delayedMain * currentGain;

        // 6. Dry / Wet mix crossfade
        const T dryWet = std::clamp(params.dryWet, T{0}, T{1});
        return dryWet * wetSample + (T{1} - dryWet) * delayedMain;
    }

    [[nodiscard]] T processSample(T input) noexcept {
        return processSample(input, input, false);
    }

    [[nodiscard]] T processSample(T mainInput, T scInput, bool auditionSidechain = false) noexcept {
        const T filtSc = filterSidechain(scInput);
        const T detLevel = detectLevel(filtSc);
        return processWithLevel(mainInput, detLevel, filtSc, auditionSidechain);
    }

    [[nodiscard]] GateState getGateState() const noexcept {
        return gateState;
    }

    [[nodiscard]] T getGainReductionDb() const noexcept {
        if (currentGain <= T{1e-5}) return params.rangeDb;
        return T{20} * std::log10(currentGain);
    }

    [[nodiscard]] size_t getLatencySamples() const noexcept {
        return currentDelaySamples;
    }

    [[nodiscard]] static constexpr size_t getMaxLatencySamples() noexcept {
        return LookaheadSamples - 1;
    }

private:
    T sr{44100};
    Parameters params{};
    T effectiveCloseThreshDb{-36.0};
    T openThreshLinear{0.03};
    T closeThreshLinear{0.015};
    T floorLinear{0.001};
    T attCoeff{0.99}, relCoeff{0.999};
    T attCoeffVocal{0.99}, relCoeffSlow{0.999};
    T attCoeffClassic{0.99}, relCoeffClassicTail{0.999};
    size_t detHoldSamples{176};
    size_t detHoldCounter{0};
    T detectorRelCoeff{0.99};
    T detectorEnvelope{0};
    size_t holdSamples{882};
    size_t holdCounter{0};
    GateState gateState{GateState::Closed};
    T currentGain{0};
    T e0{0}, e1{0}, e2{0};
    size_t writeIndex{0};
    size_t targetDelaySamples{0};
    size_t currentDelaySamples{0};
    std::array<T, LookaheadSamples> delayBuffer{};

    TptStateVariableFilter<T> scHpf;
    TptStateVariableFilter<T> scLpf;
};

} // namespace openx::dsp
