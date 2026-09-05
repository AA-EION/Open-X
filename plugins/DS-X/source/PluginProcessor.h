#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_dsp/spectral/deesser_engine.hpp>
#include <openx_dsp/state/parameter_manager.hpp>
#include <openx_ui/dsp/spectrum_analyzer.hpp>
#include <array>

namespace openx::ds {

enum class ParamId : size_t {
    Frequency,
    Threshold,
    Reduction,
    BandwidthQ,
    UseLpc,
    Count
};

constexpr std::array<openx::state::ParameterDescriptor, static_cast<size_t>(ParamId::Count)> Descriptors{{
    { static_cast<size_t>(ParamId::Frequency),  "freq",       "Frequency", 2000.0f, 16000.0f, 6000.0f, 0.3f, "Hz" },
    { static_cast<size_t>(ParamId::Threshold),  "threshold",  "Threshold",  -60.0f,     0.0f,  -24.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Reduction),  "reduction",  "Max Reduct", -24.0f,     0.0f,  -12.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::BandwidthQ), "q",          "Bandwidth",    0.5f,     8.0f,    2.0f, 0.5f, "" },
    { static_cast<size_t>(ParamId::UseLpc),     "use_lpc",    "LPC Residual", 0.0f,     1.0f,    1.0f, 1.0f, "" },
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

    const juce::String getName() const override { return "DS-X"; }
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
    std::array<openx::dsp::DeEsserEngine<float>, 2> deessers;
    openx::ui::SpectrumAnalyzer<11> spectrumAnalyzer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace openx::ds
