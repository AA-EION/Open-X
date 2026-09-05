#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_dsp/dynamics/brickwall_limiter.hpp>
#include <openx_dsp/state/parameter_manager.hpp>
#include <openx_ui/dsp/scrolling_history.hpp>
#include <array>

namespace openx::limit {

enum class ParamId : size_t {
    Ceiling,
    Threshold,
    Release,
    DispersionEnable,
    DispersionFreq,
    Count
};

constexpr std::array<openx::state::ParameterDescriptor, static_cast<size_t>(ParamId::Count)> Descriptors{{
    { static_cast<size_t>(ParamId::Ceiling),          "ceiling",     "Ceiling",         -12.0f,  0.0f,  -0.1f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Threshold),        "threshold",   "Threshold",       -24.0f,  0.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Release),          "release",     "Release",           1.0f, 500.0f,  50.0f, 0.3f, "ms" },
    { static_cast<size_t>(ParamId::DispersionEnable), "disp_enable", "Phase Dispersion",  0.0f,   1.0f,   1.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::DispersionFreq),   "disp_freq",   "Dispersion Freq",  40.0f, 500.0f, 120.0f, 0.5f, "Hz" },
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

private:
    juce::AudioProcessorValueTreeState apvts;
    openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)> params;
    std::array<openx::dsp::BrickwallLimiter<float, 256>, 2> limiters;
    openx::ui::ScrollingHistory<512> history;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace openx::limit
