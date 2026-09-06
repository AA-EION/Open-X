#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_dsp/dynamics/brickwall_limiter.hpp>
#include <openx_dsp/dynamics/loudness_meter.hpp>
#include <openx_dsp/dynamics/true_peak_detector.hpp>
#include <openx_dsp/state/parameter_manager.hpp>
#include <openx_ui/dsp/scrolling_history.hpp>
#include <array>
#include <atomic>

namespace openx::limit {

enum class ParamId : size_t {
    Ceiling,
    Threshold,
    Release,
    DispersionEnable,
    DispersionFreq,
    Style,
    Attack,
    Lookahead,
    TransientLink,
    ReleaseLink,
    TruePeakEnable,
    DcFilterEnable,
    AuditionMode,
    Count
};

constexpr std::array<openx::state::ParameterDescriptor, static_cast<size_t>(ParamId::Count)> Descriptors{{
    { static_cast<size_t>(ParamId::Ceiling),          "ceiling",        "Ceiling",            -24.0f,   0.0f,  -0.1f, 1.0f,  "dB" },
    { static_cast<size_t>(ParamId::Threshold),        "threshold",      "Gain / Thresh",      -24.0f,  30.0f,   0.0f, 1.0f,  "dB" },
    { static_cast<size_t>(ParamId::Release),          "release",        "Release",             10.0f, 1000.0f,  50.0f, 0.35f, "ms" },
    { static_cast<size_t>(ParamId::DispersionEnable), "disp_enable",    "Phase Dispersion",     0.0f,   1.0f,   1.0f, 1.0f,  "" },
    { static_cast<size_t>(ParamId::DispersionFreq),   "disp_freq",      "Dispersion Freq",     40.0f, 500.0f, 120.0f, 0.5f,  "Hz" },
    { static_cast<size_t>(ParamId::Style),            "style",          "Style",                0.0f,   7.0f,   0.0f, 1.0f,  "" },
    { static_cast<size_t>(ParamId::Attack),           "attack",         "Attack",               0.0f,  30.0f,   2.0f, 0.4f,  "ms" },
    { static_cast<size_t>(ParamId::Lookahead),        "lookahead",      "Lookahead",            0.1f,   5.0f,   1.5f, 0.5f,  "ms" },
    { static_cast<size_t>(ParamId::TransientLink),    "trans_link",     "Transients Link",      0.0f, 100.0f, 100.0f, 1.0f,  "%" },
    { static_cast<size_t>(ParamId::ReleaseLink),      "rel_link",       "Release Link",         0.0f, 100.0f, 100.0f, 1.0f,  "%" },
    { static_cast<size_t>(ParamId::TruePeakEnable),   "true_peak",      "True Peak Limiting",   0.0f,   1.0f,   1.0f, 1.0f,  "" },
    { static_cast<size_t>(ParamId::DcFilterEnable),   "dc_filter",      "DC Filter",            0.0f,   1.0f,   1.0f, 1.0f,  "" },
    { static_cast<size_t>(ParamId::AuditionMode),     "audition",       "Audition Mode",        0.0f,   2.0f,   0.0f, 1.0f,  "" },
}};

class PluginProcessor final : public juce::AudioProcessor {
public:
    PluginProcessor();
    ~PluginProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Limit-X"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getApvts() noexcept { return apvts; }
    openx::ui::ScrollingHistory<512>& getHistory() noexcept { return history; }
    openx::dsp::LoudnessMeter<float>& getLoudnessMeter() noexcept { return loudnessMeter; }

    [[nodiscard]] float getInputTruePeakDb() const noexcept {
        return inputTruePeak.load(std::memory_order_relaxed);
    }

    [[nodiscard]] float getOutputTruePeakDb() const noexcept {
        return outputTruePeak.load(std::memory_order_relaxed);
    }

    [[nodiscard]] float getMaxGainReductionDb() const noexcept {
        return maxGainReduction.load(std::memory_order_relaxed);
    }

    void resetIntegratedLoudness() noexcept {
        loudnessMeter.resetIntegrated();
    }

private:
    juce::AudioProcessorValueTreeState apvts;
    openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)> params;

    std::array<openx::dsp::BrickwallLimiter<float, 256>, 2> limiters;
    std::array<openx::dsp::TruePeakDetector<float, 1024>, 2> inTpDetectors;
    std::array<openx::dsp::TruePeakDetector<float, 1024>, 2> outTpDetectors;
    openx::dsp::LoudnessMeter<float> loudnessMeter;
    openx::ui::ScrollingHistory<512> history;

    std::atomic<float> inputTruePeak{-100.0f};
    std::atomic<float> outputTruePeak{-100.0f};
    std::atomic<float> maxGainReduction{0.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace openx::limit
