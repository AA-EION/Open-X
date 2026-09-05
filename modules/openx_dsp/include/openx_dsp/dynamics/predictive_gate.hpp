#pragma once

#include <cmath>
#include <array>
#include <concepts>
#include <algorithm>

namespace openx::dsp {

template <std::floating_point T, size_t LookaheadSamples = 256>
    requires ((LookaheadSamples & (LookaheadSamples - 1)) == 0)
class PredictiveGate {
public:
    static constexpr size_t BufferMask = LookaheadSamples - 1;

    struct Parameters {
        T openThresholdDb{-30.0};
        T closeThresholdDb{-36.0}; // Schmitt trigger hysteresis
        T rangeDb{-60.0};
        T attackMs{5.0};
        T holdMs{20.0};
        T releaseMs{80.0};
    };

    void prepare(T sampleRate) noexcept {
        sr = sampleRate;
        reset();
    }

    void reset() noexcept {
        delayBuffer.fill(0);
        writeIndex = 0;
        gateState = false;
        holdCounter = 0;
        currentGain = 0;
        e0 = 0; e1 = 0; e2 = 0;
    }

    void setParameters(const Parameters& p) noexcept {
        params = p;
        openThreshLinear = std::pow(T{10}, params.openThresholdDb / T{20});
        closeThreshLinear = std::pow(T{10}, params.closeThresholdDb / T{20});
        floorLinear = std::pow(T{10}, params.rangeDb / T{20});
        attCoeff = std::exp(-T{1} / (std::max(params.attackMs, T{0.1}) * T{0.001} * sr));
        relCoeff = std::exp(-T{1} / (std::max(params.releaseMs, T{1.0}) * T{0.001} * sr));
        holdSamples = static_cast<size_t>(params.holdMs * T{0.001} * sr);
    }

    [[nodiscard]] T processSample(T input) noexcept {
        delayBuffer[writeIndex] = input;

        // 1. Envelope extraction on lookahead signal
        const T absIn = std::abs(input);
        e2 = e1;
        e1 = e0;
        e0 = absIn;

        // 2. 2nd-order Taylor Series Kinematic Transient Predictor:
        // Normalized 1-sample lookahead step prevents 32,768x acceleration noise explosion
        const T velocity = e0 - e1;
        const T accel = e0 - T{2} * e1 + e2;

        constexpr T h = T{1.0};
        const T predictedEnvelope = std::max(T{0}, e0 + velocity * h + T{0.5} * accel * h * h);

        // 3. Schmitt Trigger State Machine with Predictive Triggering
        if (predictedEnvelope >= openThreshLinear || absIn >= openThreshLinear) {
            gateState = true;
            holdCounter = holdSamples;
        } else if (absIn < closeThreshLinear) {
            if (holdCounter > 0) {
                --holdCounter;
            } else {
                gateState = false;
            }
        }

        // 4. Target Gain Synthesis
        const T targetGain = gateState ? T{1} : floorLinear;

        // 5. Asymmetric Anti-Exponential Smoothing
        if (targetGain > currentGain) {
            currentGain = attCoeff * currentGain + (T{1} - attCoeff) * targetGain;
        } else {
            currentGain = relCoeff * currentGain + (T{1} - relCoeff) * targetGain;
        }

        // 6. Read delayed sample from lookahead ring buffer
        const size_t readIndex = (writeIndex + 1) & BufferMask;
        const T delayedSample = delayBuffer[readIndex];

        writeIndex = (writeIndex + 1) & BufferMask;
        return delayedSample * currentGain;
    }

    [[nodiscard]] T getGainReductionDb() const noexcept {
        return T{20} * std::log10(std::max(currentGain, T{1e-5}));
    }

    [[nodiscard]] static constexpr size_t getLatencySamples() noexcept {
        return LookaheadSamples - 1;
    }

private:
    T sr{44100};
    Parameters params{};
    T openThreshLinear{0.03};
    T closeThreshLinear{0.015};
    T floorLinear{0.001};
    T attCoeff{0.99}, relCoeff{0.999};
    size_t holdSamples{882};
    size_t holdCounter{0};
    bool gateState{false};
    T currentGain{0};
    T e0{0}, e1{0}, e2{0};
    size_t writeIndex{0};
    std::array<T, LookaheadSamples> delayBuffer{};
};

} // namespace openx::dsp
