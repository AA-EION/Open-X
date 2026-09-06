#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_dsp/spectral/deesser_engine.hpp>
#include <openx_dsp/state/parameter_manager.hpp>
#include <openx_ui/dsp/spectrum_analyzer.hpp>
#include <array>
#include <atomic>

namespace openx::ds {

enum class ParamId : size_t {
    Frequency,
    Threshold,
    Reduction,
    BandwidthQ,
    UseLpc,
    DetectionMode,
    BandMode,
    FilterType,
    Audition,
    Lookahead,
    StereoLink,
    StereoMode,
    Count
};

constexpr std::array<openx::state::ParameterDescriptor, static_cast<size_t>(ParamId::Count)> Descriptors{{
    { static_cast<size_t>(ParamId::Frequency),     "freq",        "Frequency",      1000.0f, 18000.0f,  6000.0f, 0.35f, "Hz" },
    { static_cast<size_t>(ParamId::Threshold),     "threshold",   "Threshold",       -60.0f,     0.0f,   -24.0f, 1.00f, "dB" },
    { static_cast<size_t>(ParamId::Reduction),     "reduction",   "Max Reduct",      -36.0f,     0.0f,   -12.0f, 1.00f, "dB" },
    { static_cast<size_t>(ParamId::BandwidthQ),    "q",           "Bandwidth Q",       0.5f,     8.0f,     2.0f, 0.50f, "" },
    { static_cast<size_t>(ParamId::UseLpc),        "use_lpc",     "VT-LPSE Residual",  0.0f,     1.0f,     1.0f, 1.00f, "" },
    { static_cast<size_t>(ParamId::DetectionMode), "mode",        "Detection Mode",    0.0f,     1.0f,     0.0f, 1.00f, "" },
    { static_cast<size_t>(ParamId::BandMode),      "band_mode",   "Band Mode",         0.0f,     1.0f,     1.0f, 1.00f, "" },
    { static_cast<size_t>(ParamId::FilterType),    "filter_type", "Filter Type",       0.0f,     1.0f,     0.0f, 1.00f, "" },
    { static_cast<size_t>(ParamId::Audition),      "audition",    "Audition",          0.0f,     2.0f,     0.0f, 1.00f, "" },
    { static_cast<size_t>(ParamId::Lookahead),     "lookahead",   "Lookahead",         0.0f,    15.0f,     5.0f, 1.00f, "ms" },
    { static_cast<size_t>(ParamId::StereoLink),    "stereo_link", "Stereo Link",       0.0f,   100.0f,   100.0f, 1.00f, "%" },
    { static_cast<size_t>(ParamId::StereoMode),    "stereo_mode", "Stereo Mode",       0.0f,     2.0f,     0.0f, 1.00f, "" },
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

    [[nodiscard]] float getGainReductionDb() const noexcept { return currentGainReductionDb.load(std::memory_order_relaxed); }
    [[nodiscard]] float getSibilanceActivity() const noexcept { return currentSibilanceActivity.load(std::memory_order_relaxed); }
    [[nodiscard]] float getSidechainLevelDb() const noexcept { return currentSidechainLevelDb.load(std::memory_order_relaxed); }

private:
    [[nodiscard]] openx::dsp::DeEsserEngine<float>::Parameters getCurrentEngineParameters() const noexcept;

    juce::AudioProcessorValueTreeState apvts;
    openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)> params;
    std::array<openx::dsp::DeEsserEngine<float>, 2> deessers;
    openx::ui::SpectrumAnalyzer<11> spectrumAnalyzer;

    std::atomic<float> currentGainReductionDb{0.0f};
    std::atomic<float> currentSibilanceActivity{0.0f};
    std::atomic<float> currentSidechainLevelDb{-100.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace openx::ds
