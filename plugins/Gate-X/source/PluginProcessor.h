#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_dsp/dynamics/predictive_gate.hpp>
#include <openx_dsp/state/parameter_manager.hpp>
#include <openx_ui/dsp/scrolling_history.hpp>
#include <array>

namespace openx::gate {

enum class ParamId : size_t {
    OpenThreshold,
    CloseThreshold,
    Range,
    Attack,
    Hold,
    Release,
    Count
};

constexpr std::array<openx::state::ParameterDescriptor, static_cast<size_t>(ParamId::Count)> Descriptors{{
    { static_cast<size_t>(ParamId::OpenThreshold),  "open_thresh",  "Open Thresh",  -60.0f,   0.0f, -30.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::CloseThreshold), "close_thresh", "Close Thresh", -60.0f,   0.0f, -36.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Range),          "range",        "Range",        -80.0f,   0.0f, -60.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Attack),         "attack",       "Attack",         0.01f, 50.0f,   2.0f, 0.3f, "ms" },
    { static_cast<size_t>(ParamId::Hold),           "hold",         "Hold",           1.0f, 500.0f,  20.0f, 0.4f, "ms" },
    { static_cast<size_t>(ParamId::Release),        "release",      "Release",        5.0f, 1000.0f, 80.0f, 0.4f, "ms" },
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

    const juce::String getName() const override { return "Gate-X"; }
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
    std::array<openx::dsp::PredictiveGate<float, 256>, 2> gates;
    openx::ui::ScrollingHistory<512> history;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace openx::gate
