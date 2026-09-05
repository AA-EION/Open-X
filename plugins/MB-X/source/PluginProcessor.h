#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_dsp/dynamics/multiband_processor.hpp>
#include <openx_dsp/state/parameter_manager.hpp>
#include <openx_ui/dsp/spectrum_analyzer.hpp>
#include <array>

namespace openx::mb {

enum class ParamId : size_t {
    Crossover1,
    Crossover2,
    LowThresh,
    MidThresh,
    HighThresh,
    LowGain,
    MidGain,
    HighGain,
    Count
};

constexpr std::array<openx::state::ParameterDescriptor, static_cast<size_t>(ParamId::Count)> Descriptors{{
    { static_cast<size_t>(ParamId::Crossover1), "xo1",        "Low-Mid XO",     40.0f,  1000.0f,  250.0f, 0.3f, "Hz" },
    { static_cast<size_t>(ParamId::Crossover2), "xo2",        "Mid-High XO",   800.0f, 16000.0f, 3500.0f, 0.3f, "Hz" },
    { static_cast<size_t>(ParamId::LowThresh),   "low_thresh", "Low Thresh",   -60.0f,     0.0f,  -18.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::MidThresh),   "mid_thresh", "Mid Thresh",   -60.0f,     0.0f,  -18.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::HighThresh),  "hi_thresh",  "High Thresh",  -60.0f,     0.0f,  -18.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::LowGain),     "low_gain",   "Low Gain",     -24.0f,    24.0f,    0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::MidGain),     "mid_gain",   "Mid Gain",     -24.0f,    24.0f,    0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::HighGain),    "hi_gain",    "High Gain",    -24.0f,    24.0f,    0.0f, 1.0f, "dB" },
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

    const juce::String getName() const override { return "MB-X"; }
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
    std::array<openx::dsp::MultibandProcessor3Band<float>, 2> processors;
    openx::ui::SpectrumAnalyzer<11> spectrumAnalyzer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace openx::mb
