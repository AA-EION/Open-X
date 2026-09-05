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
    Makeup,
    TransientPunch,
    Count
};

constexpr std::array<openx::state::ParameterDescriptor, static_cast<size_t>(ParamId::Count)> Descriptors{{
    { static_cast<size_t>(ParamId::Threshold),      "threshold", "Threshold", -60.0f, 0.0f,   -20.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Ratio),          "ratio",     "Ratio",       1.0f, 20.0f,    4.0f, 0.5f, ":1" },
    { static_cast<size_t>(ParamId::Knee),           "knee",      "Knee",        0.0f, 24.0f,    6.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Attack),         "attack",    "Attack",      0.1f, 100.0f,  15.0f, 0.3f, "ms" },
    { static_cast<size_t>(ParamId::Release),        "release",   "Release",    10.0f, 1000.0f, 120.0f, 0.3f, "ms" },
    { static_cast<size_t>(ParamId::Makeup),         "makeup",    "Makeup",    -12.0f, 24.0f,    0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::TransientPunch), "punch",     "Punch",       0.0f,  1.0f,    0.5f, 1.0f, "" },
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
    std::array<openx::dsp::CompressorEngine<float>, 2> compressors;
    openx::ui::ScrollingHistory<512> history;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace openx::comp
