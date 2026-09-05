#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_dsp/reverb/fdn_reverb.hpp>
#include <openx_dsp/state/parameter_manager.hpp>
#include <openx_ui/dsp/spectrum_analyzer.hpp>
#include <array>

namespace openx::verb {

enum class ParamId : size_t {
    DecayTime,
    Damping,
    ChaosMod,
    Mix,
    Count
};

constexpr std::array<openx::state::ParameterDescriptor, static_cast<size_t>(ParamId::Count)> Descriptors{{
    { static_cast<size_t>(ParamId::DecayTime), "decay",   "Decay Time",    0.2f,  20.0f,    2.5f, 0.4f, "s" },
    { static_cast<size_t>(ParamId::Damping),   "damping", "Damping Freq", 500.0f, 20000.0f, 6000.0f, 0.3f, "Hz" },
    { static_cast<size_t>(ParamId::ChaosMod),  "chaos",   "Chaos Mod",     0.0f,   1.0f,    0.25f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Mix),       "mix",     "Mix",           0.0f,   1.0f,    0.35f, 1.0f, "%" },
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

    const juce::String getName() const override { return "Verb-X"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

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
    openx::dsp::FdnReverb<float, 16> reverb;
    openx::ui::SpectrumAnalyzer<11> spectrumAnalyzer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace openx::verb
