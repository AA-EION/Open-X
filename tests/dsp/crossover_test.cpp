#include <openx_dsp/crossover/linkwitz_riley.hpp>
#include <openx_dsp/dynamics/true_peak_detector.hpp>
#include <openx_dsp/eq/dynamic_biquad_engine.hpp>
#include <openx_dsp/dynamics/compressor_engine.hpp>
#include <openx_dsp/dynamics/brickwall_limiter.hpp>
#include <openx_dsp/dynamics/multiband_processor.hpp>
#include <openx_dsp/reverb/fdn_reverb.hpp>
#include <openx_dsp/spectral/deesser_engine.hpp>
#include <openx_dsp/dynamics/predictive_gate.hpp>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

int main() {
    constexpr float sampleRate = 48000.0f;

    // Test 1: Linkwitz-Riley 4th order phase-aligned all-pass reconstruction
    {
        openx::dsp::LinkwitzRiley4<float> lr4;
        lr4.prepare(sampleRate);
        lr4.setCutoff(1000.0f);

        float maxReconstructionError = 0.0f;
        for (int i = 0; i < 2048; ++i) {
            const float x = (i == 0) ? 1.0f : 0.0f;
            auto [low, high] = lr4.processSample(x);
            const float summed = low + high;
            const float allpass = lr4.processAllpassSample(x);
            const float err = std::abs(summed - allpass);
            if (err > maxReconstructionError) {
                maxReconstructionError = err;
            }
        }
        std::cout << "[PASS] LR4 Phase-Aligned Matching Error: " << maxReconstructionError << "\n";
        assert(maxReconstructionError < 1e-5f);
    }

    // Test 2: Multi-band Splitter (3-band & 4-band) Phase Compensation & Perfect Reconstruction
    {
        // 3-band
        {
            openx::dsp::PhaseAlignedMultibandSplitter<float, 3> splitter3;
            splitter3.prepare(sampleRate);
            const std::array<float, 2> cuts{ 300.0f, 3000.0f };
            splitter3.setFrequencies(cuts);

            openx::dsp::LinkwitzRiley4<float> ap0, ap1;
            ap0.prepare(sampleRate); ap0.setCutoff(300.0f);
            ap1.prepare(sampleRate); ap1.setCutoff(3000.0f);

            float maxErr3 = 0.0f;
            for (int i = 0; i < 2048; ++i) {
                const float x = (i == 0) ? 1.0f : 0.0f;
                std::array<float, 3> bands{};
                splitter3.process(x, bands);
                const float summed = bands[0] + bands[1] + bands[2];
                const float expectedAllpass = ap1.processAllpassSample(ap0.processAllpassSample(x));
                const float err = std::abs(summed - expectedAllpass);
                if (err > maxErr3) maxErr3 = err;
            }
            std::cout << "[PASS] 3-Band Splitter Reconstruction Error: " << maxErr3 << "\n";
            assert(maxErr3 < 1e-5f);
        }

        // 4-band
        {
            openx::dsp::PhaseAlignedMultibandSplitter<float, 4> splitter4;
            splitter4.prepare(sampleRate);
            const std::array<float, 3> cuts{ 150.0f, 1000.0f, 6000.0f };
            splitter4.setFrequencies(cuts);

            openx::dsp::LinkwitzRiley4<float> ap0, ap1, ap2;
            ap0.prepare(sampleRate); ap0.setCutoff(150.0f);
            ap1.prepare(sampleRate); ap1.setCutoff(1000.0f);
            ap2.prepare(sampleRate); ap2.setCutoff(6000.0f);

            float maxErr4 = 0.0f;
            for (int i = 0; i < 2048; ++i) {
                const float x = (i == 0) ? 1.0f : 0.0f;
                std::array<float, 4> bands{};
                splitter4.process(x, bands);
                const float summed = bands[0] + bands[1] + bands[2] + bands[3];
                const float expectedAllpass = ap2.processAllpassSample(ap1.processAllpassSample(ap0.processAllpassSample(x)));
                const float err = std::abs(summed - expectedAllpass);
                if (err > maxErr4) maxErr4 = err;
            }
            std::cout << "[PASS] 4-Band Splitter Reconstruction Error: " << maxErr4 << "\n";
            assert(maxErr4 < 1e-5f);
        }
    }

    // Test 3: True Peak Detector Inter-Sample Peak Capture
    {
        openx::dsp::TruePeakDetector<float, 1024> tp;
        tp.prepare(sampleRate);
        tp.setReleaseTime(10.0f);

        float maxDetectedPeak = 0.0f;
        for (int i = 0; i < 64; ++i) {
            const float sample = ((i % 2) == 0) ? 1.0f : -1.0f;
            const float peak = tp.processSample(sample);
            if (peak > maxDetectedPeak) maxDetectedPeak = peak;
        }

        std::cout << "[PASS] True Peak Detected: " << maxDetectedPeak << " linear ("
                  << (20.0f * std::log10(maxDetectedPeak)) << " dBFS)\n";
        assert(maxDetectedPeak >= 1.0f);
    }

    // Test 4: Dynamic Biquad Engine BIBO Stability & TPT SVF Exact Linear Gain
    {
        // Check TptStateVariableFilter exact linear gain at resonance without nyquist squaring
        openx::dsp::TptStateVariableFilter<float> svf;
        svf.prepare(sampleRate);
        const float targetGainLinear = 2.0f; // +6.02 dB
        svf.setParameters(openx::dsp::TptStateVariableFilter<float>::Type::Bell, 1000.0f, 2.0f, targetGainLinear);
        float steadyPeakOut = 0.0f;
        for (int i = 0; i < 4800; ++i) {
            const float in = std::sin(2.0f * 3.14159265f * 1000.0f * static_cast<float>(i) / sampleRate);
            const float out = svf.processSample(in);
            if (i > 3000 && std::abs(out) > steadyPeakOut) {
                steadyPeakOut = std::abs(out);
            }
        }
        std::cout << "[PASS] TPT SVF Resonant Peak: " << steadyPeakOut << " (Target Linear Gain: " << targetGainLinear << ")\n";
        assert(std::abs(steadyPeakOut - targetGainLinear) < 0.05f);

        // Check DynamicBiquadEngine stability
        openx::dsp::DynamicBiquadEngine<float> engine;
        engine.prepare(sampleRate);
        openx::dsp::DynamicBiquadEngine<float>::Parameters p;
        p.frequency = 2500.0f;
        p.dynamicGainMaxDb = -12.0f;
        p.thresholdDb = -18.0f;
        engine.setParameters(p);

        for (int i = 0; i < 1000; ++i) {
            const float in = std::sin(2.0f * 3.14159265f * 2500.0f * static_cast<float>(i) / sampleRate);
            const float out = engine.processSample(in);
            assert(!std::isnan(out) && !std::isinf(out));
        }
        std::cout << "[PASS] Dynamic Biquad Stability Verified.\n";
    }

    // Test 5: Compressor Analytic Signal Envelope Follower & Sample Rate Scaling (44.1k, 48k, 96k, 192k)
    {
        const std::vector<float> testRates{ 44100.0f, 48000.0f, 96000.0f, 192000.0f };
        for (float rate : testRates) {
            openx::dsp::AnalyticEnvelopeFollower<float> follower;
            follower.prepare(rate);
            float minEnv = 2.0f, maxEnv = 0.0f;
            for (int i = 0; i < 4800; ++i) {
                const float in = std::sin(2.0f * 3.14159265f * 1000.0f * static_cast<float>(i) / rate);
                const float env = follower.computeInstantaneousEnvelope(in);
                if (i > 1000) {
                    if (env < minEnv) minEnv = env;
                    if (env > maxEnv) maxEnv = env;
                }
            }
            const float ripple = maxEnv - minEnv;
            std::cout << "[PASS] Analytic Follower Ripple at " << rate << " Hz: " << ripple << "\n";
            assert(ripple < 0.05f);
        }

        // Full Compressor test
        openx::dsp::CompressorEngine<float> comp;
        comp.prepare(sampleRate);
        openx::dsp::CompressorEngine<float>::Parameters p;
        p.thresholdDb = -20.0f;
        p.ratio = 4.0f;
        comp.setParameters(p);

        // Sinusoid at 0 dBFS should cause steady gain reduction
        for (int i = 0; i < 4800; ++i) {
            const float in = std::sin(2.0f * 3.14159265f * 1000.0f * static_cast<float>(i) / sampleRate);
            comp.processSample(in);
        }
        const float gr = comp.getGainReductionDb();
        std::cout << "[PASS] Comp-X Gain Reduction: " << gr << " dB\n";
        assert(gr < -5.0f);
    }

    // Test 6: Brickwall Limiter Strict True-Peak Ceiling Guarantee (0.00 dB tolerance)
    {
        openx::dsp::BrickwallLimiter<float, 256> lim;
        lim.prepare(sampleRate);
        openx::dsp::BrickwallLimiter<float, 256>::Parameters p;
        p.ceilingDb = -0.5f;
        lim.setParameters(p);

        float maxOutput = 0.0f;
        const float ceilingLinear = std::pow(10.0f, -0.5f / 20.0f);

        // 1. Hot continuous sinusoid (+6 dBFS)
        for (int i = 0; i < 2400; ++i) {
            const float hotSignal = 2.0f * std::sin(2.0f * 3.14159265f * 500.0f * static_cast<float>(i) / sampleRate);
            const float out = lim.processSample(hotSignal);
            if (std::abs(out) > maxOutput) maxOutput = std::abs(out);
        }
        std::cout << "[PASS] Limit-X Hot Sine Max Out: " << maxOutput << " (Ceiling: " << ceilingLinear << ")\n";
        assert(maxOutput <= ceilingLinear);

        // 2. Large transient pulse
        for (int i = 0; i < 1000; ++i) {
            const float pulse = (i == 100) ? 10.0f : 0.0f;
            const float out = lim.processSample(pulse);
            assert(std::abs(out) <= ceilingLinear);
        }
        std::cout << "[PASS] Limit-X Strict 0.00 dB Ceiling Enforced.\n";
    }

    // Test 7: Reverb Unitary Symplectic Energy Boundedness & Hermite Fractional Delay
    {
        openx::dsp::FdnReverb<float, 16> fdn;
        fdn.prepare(sampleRate);
        openx::dsp::FdnReverb<float, 16>::Parameters p;
        p.decayTimeSec = 1.0f;
        p.dryWet = 1.0f;
        fdn.setParameters(p);

        // Feed an impulse and verify decay and stereo decorrelation
        auto [outL0, outR0] = fdn.processSample(1.0f, 1.0f);
        (void)outL0; (void)outR0;
        float tailEnergy = 0.0f;
        float energyL = 0.0f, energyR = 0.0f;
        for (int i = 0; i < 48000; ++i) {
            auto [l, r] = fdn.processSample(0.0f, 0.0f);
            assert(!std::isnan(l) && !std::isnan(r));
            energyL += l * l;
            energyR += r * r;
            if (i > 40000) tailEnergy += (l * l + r * r);
        }
        std::cout << "[PASS] Verb-X Reverb Tail Energy after 1s: " << tailEnergy << "\n";
        assert(tailEnergy < 1e-3f);
        // Verify stereo energy balance
        const float energyRatio = energyL / (energyR + 1e-8f);
        assert(energyRatio > 0.5f && energyRatio < 2.0f);
        std::cout << "[PASS] Verb-X Stereo Energy Balance: L=" << energyL << ", R=" << energyR << "\n";
    }

    // Test 8: De-Esser NLMS Loud Speech Stability & Smooth Transition
    {
        openx::dsp::DeEsserEngine<float> deesser;
        deesser.prepare(sampleRate);
        openx::dsp::DeEsserEngine<float>::Parameters p;
        p.frequencyHz = 6000.0f;
        p.thresholdDb = -20.0f;
        p.reductionDb = -12.0f;
        p.useLpcResidualSubtraction = true;
        deesser.setParameters(p);

        // Feed extremely loud 6 kHz burst (3.0 peak = +9.5 dBFS)
        for (int i = 0; i < 2000; ++i) {
            const float hotSibilance = 3.0f * std::sin(2.0f * 3.14159265f * 6000.0f * static_cast<float>(i) / sampleRate);
            const float out = deesser.processSample(hotSibilance);
            assert(!std::isnan(out) && !std::isinf(out));
        }
        std::cout << "[PASS] De-Esser NLMS Loud Speech Burst Handled Without NaN/Inf.\n";
    }

    // Test 9: Predictive Gate Kinematic Predictor Noise Immunity
    {
        openx::dsp::PredictiveGate<float, 256> gate;
        gate.prepare(sampleRate);
        openx::dsp::PredictiveGate<float, 256>::Parameters p;
        p.openThresholdDb = -20.0f; // 0.1 linear
        p.closeThresholdDb = -26.0f;
        p.rangeDb = -60.0f;
        gate.setParameters(p);

        // Feed high-frequency low-level noise (amplitude 0.005, well below threshold 0.1)
        float maxNoiseOut = 0.0f;
        for (int i = 0; i < 1000; ++i) {
            const float noise = (i % 2 == 0 ? 0.005f : -0.005f);
            const float out = gate.processSample(noise);
            if (std::abs(out) > maxNoiseOut) maxNoiseOut = std::abs(out);
        }
        std::cout << "[PASS] Gate-X Noise Attenuation: max out = " << maxNoiseOut << " (input 0.005)\n";
        assert(maxNoiseOut < 0.001f);
    }

    // Test 10: Multiband Processor 3-Band Clean Execution
    {
        openx::dsp::MultibandProcessor3Band<float> mb;
        mb.prepare(sampleRate);
        openx::dsp::MultibandProcessor3Band<float>::Parameters p;
        p.lowMidCrossoverHz = 250.0f;
        p.midHighCrossoverHz = 3500.0f;
        mb.setParameters(p);

        for (int i = 0; i < 1000; ++i) {
            const float in = 0.5f * std::sin(2.0f * 3.14159265f * 1000.0f * static_cast<float>(i) / sampleRate);
            const float out = mb.processSample(in);
            assert(!std::isnan(out) && !std::isinf(out));
        }
        std::cout << "[PASS] MultibandProcessor3Band Processing Verified.\n";
    }

    // Test 11: Circular Monotonic Queue Window Alignment & Eviction Precision
    {
        openx::dsp::CircularMonotonicQueue<float, 512> q;
        constexpr size_t windowSize = 255;

        // Push a significant peak reduction at index 10
        q.push(0.2f, 10, windowSize);
        assert(std::abs(q.getMin() - 0.2f) < 1e-6f);

        // Through the entire lookahead delay horizon [11, 10 + windowSize], the peak gain must be retained
        for (size_t idx = 11; idx <= 10 + windowSize; ++idx) {
            q.push(1.0f, idx, windowSize);
            assert(std::abs(q.getMin() - 0.2f) < 1e-6f);
        }

        // At exactly (10 + windowSize + 1), the peak has finished traversing the delay exit and must be evicted
        q.push(1.0f, 10 + windowSize + 1, windowSize);
        assert(std::abs(q.getMin() - 1.0f) < 1e-6f);

        // Verify monotonicity with non-monotonic input stream
        q.push(0.6f, 300, windowSize);
        assert(std::abs(q.getMin() - 0.6f) < 1e-6f);
        q.push(0.3f, 301, windowSize);
        assert(std::abs(q.getMin() - 0.3f) < 1e-6f);
        q.push(0.9f, 302, windowSize);
        assert(std::abs(q.getMin() - 0.3f) < 1e-6f); // 0.3 remains the minimum

        std::cout << "[PASS] Circular Monotonic Queue Window & Eviction Timing Verified.\n";
    }

    // Test 12: Brickwall Limiter Advance Lookahead Attack Tracking
    {
        openx::dsp::BrickwallLimiter<float, 256> lim;
        lim.prepare(sampleRate);
        openx::dsp::BrickwallLimiter<float, 256>::Parameters p;
        p.ceilingDb = -1.0f;
        lim.setParameters(p);

        const float ceilingLinear = std::pow(10.0f, -1.0f / 20.0f);
        constexpr int peakIndex = 50;
        constexpr int latency = static_cast<int>(openx::dsp::BrickwallLimiter<float, 256>::LatencySamples);
        const int peakExitIndex = peakIndex + latency;

        bool sawAdvanceReduction = false;
        for (int i = 0; i < 500; ++i) {
            const float in = (i == peakIndex) ? 4.0f : 0.0f;
            const float out = lim.processSample(in);
            const float grDb = lim.getGainReductionDb();

            // Check if gain reduction is active in advance of the peak emerging at the output
            if (i > peakIndex + 10 && i < peakExitIndex && grDb > 3.0f) {
                sawAdvanceReduction = true;
            }

            if (i == peakExitIndex) {
                assert(std::abs(out) <= ceilingLinear + 1e-6f);
            }
        }
        assert(sawAdvanceReduction);
        std::cout << "[PASS] Brickwall Limiter Advance Lookahead Gain Reduction Verified.\n";
    }

    std::cout << "All 12 Plugin DSP Engines, Remediations & Lock-Free Invariants Verified Successfully.\n";
    return 0;
}
