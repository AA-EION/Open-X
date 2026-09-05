#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_dsp/eq/dynamic_biquad_engine.hpp>
#include <openx_dsp/state/parameter_manager.hpp>
#include <openx_ui/dsp/spectrum_analyzer.hpp>
#include <array>

namespace openx::eq {

enum class ParamId : size_t {
    InputGain,
    OutputGain,
    Band1Freq,
    Band1Gain,
    Band1Q,
    Band1DynGain,
    Band1Threshold,
    Count
};

constexpr std::array<openx::state::ParameterDescriptor, static_cast<size_t>(ParamId::Count)> Descriptors{{
    { static_cast<size_t>(ParamId::InputGain),      "in_gain",     "Input Gain",      -24.0f, 24.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::OutputGain),     "out_gain",    "Output Gain",     -24.0f, 24.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band1Freq),      "b1_freq",     "Band 1 Freq",      20.0f, 20000.0f, 1000.0f, 0.25f, "Hz" },
    { static_cast<size_t>(ParamId::Band1Gain),      "b1_gain",     "Band 1 Gain",     -24.0f, 24.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band1Q),         "b1_q",        "Band 1 Q",         0.1f,  18.0f,   0.7071f, 0.5f, "" },
    { static_cast<size_t>(ParamId::Band1DynGain),   "b1_dyngain",  "Band 1 Dyn Gain", -18.0f, 18.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band1Threshold), "b1_thresh",   "Band 1 Thresh",   -60.0f, 0.0f,   -20.0f, 1.0f, "dB" },
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

    const juce::String getName() const override { return "EQ-X"; }
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
    openx::ui::SpectrumAnalyzer<11>& getSpectrumAnalyzer() noexcept { return spectrumAnalyzer; }

private:
    juce::AudioProcessorValueTreeState apvts;
    openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)> params;
    std::array<openx::dsp::DynamicBiquadEngine<float>, 2> dynBiquads;
    openx::ui::SpectrumAnalyzer<11> spectrumAnalyzer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace openx::eq
