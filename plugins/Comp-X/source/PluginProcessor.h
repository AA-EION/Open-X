#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_dsp/dynamics/compressor_engine.hpp>
#include <openx_dsp/state/parameter_manager.hpp>
#include <openx_ui/dsp/scrolling_history.hpp>
#include <array>

namespace openx::comp {

enum class ParamId : size_t {
    Threshold,
    Ratio,
    Knee,
    Attack,
    Release,
    AutoRelease,
    Hold,
    Style,
    Lookahead,
    ScHpf,
    ScLpf,
    ScAudition,
    Makeup,
    AutoGain,
    Mix,
    Punch,
    Count
};

constexpr std::array<openx::state::ParameterDescriptor, static_cast<size_t>(ParamId::Count)> Descriptors{{
    { static_cast<size_t>(ParamId::Threshold),   "threshold",    "Threshold",    -60.0f,   0.0f,   -20.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Ratio),       "ratio",        "Ratio",          1.0f,  30.0f,     4.0f, 0.4f, ":1" },
    { static_cast<size_t>(ParamId::Knee),        "knee",         "Knee",           0.0f,  30.0f,     6.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Attack),      "attack",       "Attack",         0.01f, 250.0f,   15.0f, 0.3f, "ms" },
    { static_cast<size_t>(ParamId::Release),     "release",      "Release",       10.0f, 2500.0f,  120.0f, 0.3f, "ms" },
    { static_cast<size_t>(ParamId::AutoRelease), "auto_release", "Auto Release",   0.0f,   1.0f,     0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Hold),        "hold",         "Hold",           0.0f, 500.0f,     0.0f, 0.4f, "ms" },
    { static_cast<size_t>(ParamId::Style),       "style",        "Style",          0.0f,   7.0f,     0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Lookahead),   "lookahead",    "Lookahead",      0.0f,  20.0f,     0.0f, 0.5f, "ms" },
    { static_cast<size_t>(ParamId::ScHpf),       "sc_hpf",       "SC High-Pass",  10.0f, 1000.0f,   20.0f, 0.3f, "Hz" },
    { static_cast<size_t>(ParamId::ScLpf),       "sc_lpf",       "SC Low-Pass", 1000.0f, 20000.0f, 20000.0f, 0.3f, "Hz" },
    { static_cast<size_t>(ParamId::ScAudition),  "sc_audition",  "SC Audition",    0.0f,   1.0f,     0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Makeup),      "makeup",       "Makeup",       -24.0f,  24.0f,     0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::AutoGain),    "auto_gain",    "Auto Gain",      0.0f,   1.0f,     0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Mix),         "mix",          "Mix",            0.0f, 100.0f,   100.0f, 1.0f, "%" },
    { static_cast<size_t>(ParamId::Punch),       "punch",        "TS-WD Punch",    0.0f,   1.0f,     0.5f, 1.0f, "" },
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

    const juce::String getName() const override { return "Comp-X"; }
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

private:
    juce::AudioProcessorValueTreeState apvts;
    openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)> params;
    openx::dsp::CompressorEngine<float> compressor;
    openx::ui::ScrollingHistory<512> history;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace openx::comp
