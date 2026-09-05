#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_dsp/dynamics/multiband_processor.hpp>
#include <openx_dsp/state/parameter_manager.hpp>
#include <openx_ui/dsp/spectrum_analyzer.hpp>
#include <array>

namespace openx::mb {

enum class ParamId : size_t {
    // Crossovers
    Crossover1,
    Crossover2,
    Crossover3,

    // Global
    InputGain,
    OutputGain,

    // Band 0 (Low)
    B0_Mode,
    B0_Thresh,
    B0_Range,
    B0_Ratio,
    B0_Attack,
    B0_Release,
    B0_Knee,
    B0_Gain,
    B0_Solo,
    B0_Mute,
    B0_Bypass,

    // Band 1 (Low-Mid)
    B1_Mode,
    B1_Thresh,
    B1_Range,
    B1_Ratio,
    B1_Attack,
    B1_Release,
    B1_Knee,
    B1_Gain,
    B1_Solo,
    B1_Mute,
    B1_Bypass,

    // Band 2 (High-Mid)
    B2_Mode,
    B2_Thresh,
    B2_Range,
    B2_Ratio,
    B2_Attack,
    B2_Release,
    B2_Knee,
    B2_Gain,
    B2_Solo,
    B2_Mute,
    B2_Bypass,

    // Band 3 (High)
    B3_Mode,
    B3_Thresh,
    B3_Range,
    B3_Ratio,
    B3_Attack,
    B3_Release,
    B3_Knee,
    B3_Gain,
    B3_Solo,
    B3_Mute,
    B3_Bypass,

    Count
};

constexpr std::array<openx::state::ParameterDescriptor, static_cast<size_t>(ParamId::Count)> Descriptors{{
    // Crossovers
    { static_cast<size_t>(ParamId::Crossover1), "xo1", "Low-Mid XO",   30.0f,  1000.0f,  160.0f, 0.3f, "Hz" },
    { static_cast<size_t>(ParamId::Crossover2), "xo2", "Mid-High XO",  120.0f,  8000.0f, 1200.0f, 0.3f, "Hz" },
    { static_cast<size_t>(ParamId::Crossover3), "xo3", "High-Air XO",  800.0f, 18000.0f, 6000.0f, 0.3f, "Hz" },

    // Global
    { static_cast<size_t>(ParamId::InputGain),  "input_gain",  "Input Gain",  -24.0f, 24.0f, 0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::OutputGain), "output_gain", "Output Gain", -24.0f, 24.0f, 0.0f, 1.0f, "dB" },

    // Band 0 (Low)
    { static_cast<size_t>(ParamId::B0_Mode),    "b0_mode",    "Low Mode",       0.0f,    1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::B0_Thresh),  "b0_thresh",  "Low Thresh",   -60.0f,    0.0f,  -18.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::B0_Range),   "b0_range",   "Low Range",    -24.0f,   24.0f,  -12.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::B0_Ratio),   "b0_ratio",   "Low Ratio",      1.0f,   20.0f,    2.5f, 0.5f, ":1" },
    { static_cast<size_t>(ParamId::B0_Attack),  "b0_attack",  "Low Attack",     0.1f,  250.0f,   30.0f, 0.4f, "ms" },
    { static_cast<size_t>(ParamId::B0_Release), "b0_release", "Low Release",   10.0f, 1500.0f,  150.0f, 0.4f, "ms" },
    { static_cast<size_t>(ParamId::B0_Knee),    "b0_knee",    "Low Knee",       0.0f,   20.0f,    4.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::B0_Gain),    "b0_gain",    "Low Gain",     -24.0f,   24.0f,    0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::B0_Solo),    "b0_solo",    "Low Solo",       0.0f,    1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::B0_Mute),    "b0_mute",    "Low Mute",       0.0f,    1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::B0_Bypass),  "b0_bypass",  "Low Bypass",     0.0f,    1.0f,    0.0f, 1.0f, "" },

    // Band 1 (Low-Mid)
    { static_cast<size_t>(ParamId::B1_Mode),    "b1_mode",    "Low-Mid Mode",     0.0f,    1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::B1_Thresh),  "b1_thresh",  "Low-Mid Thresh", -60.0f,    0.0f,  -18.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::B1_Range),   "b1_range",   "Low-Mid Range",  -24.0f,   24.0f,  -12.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::B1_Ratio),   "b1_ratio",   "Low-Mid Ratio",    1.0f,   20.0f,    2.5f, 0.5f, ":1" },
    { static_cast<size_t>(ParamId::B1_Attack),  "b1_attack",  "Low-Mid Attack",   0.1f,  250.0f,   20.0f, 0.4f, "ms" },
    { static_cast<size_t>(ParamId::B1_Release), "b1_release", "Low-Mid Release", 10.0f, 1500.0f,  100.0f, 0.4f, "ms" },
    { static_cast<size_t>(ParamId::B1_Knee),    "b1_knee",    "Low-Mid Knee",     0.0f,   20.0f,    4.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::B1_Gain),    "b1_gain",    "Low-Mid Gain",   -24.0f,   24.0f,    0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::B1_Solo),    "b1_solo",    "Low-Mid Solo",     0.0f,    1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::B1_Mute),    "b1_mute",    "Low-Mid Mute",     0.0f,    1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::B1_Bypass),  "b1_bypass",  "Low-Mid Bypass",   0.0f,    1.0f,    0.0f, 1.0f, "" },

    // Band 2 (High-Mid)
    { static_cast<size_t>(ParamId::B2_Mode),    "b2_mode",    "High-Mid Mode",     0.0f,    1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::B2_Thresh),  "b2_thresh",  "High-Mid Thresh", -60.0f,    0.0f,  -18.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::B2_Range),   "b2_range",   "High-Mid Range",  -24.0f,   24.0f,  -12.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::B2_Ratio),   "b2_ratio",   "High-Mid Ratio",    1.0f,   20.0f,    2.5f, 0.5f, ":1" },
    { static_cast<size_t>(ParamId::B2_Attack),  "b2_attack",  "High-Mid Attack",   0.1f,  250.0f,   15.0f, 0.4f, "ms" },
    { static_cast<size_t>(ParamId::B2_Release), "b2_release", "High-Mid Release", 10.0f, 1500.0f,   80.0f, 0.4f, "ms" },
    { static_cast<size_t>(ParamId::B2_Knee),    "b2_knee",    "High-Mid Knee",     0.0f,   20.0f,    4.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::B2_Gain),    "b2_gain",    "High-Mid Gain",   -24.0f,   24.0f,    0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::B2_Solo),    "b2_solo",    "High-Mid Solo",     0.0f,    1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::B2_Mute),    "b2_mute",    "High-Mid Mute",     0.0f,    1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::B2_Bypass),  "b2_bypass",  "High-Mid Bypass",   0.0f,    1.0f,    0.0f, 1.0f, "" },

    // Band 3 (High)
    { static_cast<size_t>(ParamId::B3_Mode),    "b3_mode",    "High Mode",       0.0f,    1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::B3_Thresh),  "b3_thresh",  "High Thresh",   -60.0f,    0.0f,  -18.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::B3_Range),   "b3_range",   "High Range",    -24.0f,   24.0f,  -12.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::B3_Ratio),   "b3_ratio",   "High Ratio",      1.0f,   20.0f,    2.5f, 0.5f, ":1" },
    { static_cast<size_t>(ParamId::B3_Attack),  "b3_attack",  "High Attack",     0.1f,  250.0f,   10.0f, 0.4f, "ms" },
    { static_cast<size_t>(ParamId::B3_Release), "b3_release", "High Release",   10.0f, 1500.0f,   60.0f, 0.4f, "ms" },
    { static_cast<size_t>(ParamId::B3_Knee),    "b3_knee",    "High Knee",       0.0f,   20.0f,    4.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::B3_Gain),    "b3_gain",    "High Gain",     -24.0f,   24.0f,    0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::B3_Solo),    "b3_solo",    "High Solo",       0.0f,    1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::B3_Mute),    "b3_mute",    "High Mute",       0.0f,    1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::B3_Bypass),  "b3_bypass",  "High Bypass",     0.0f,    1.0f,    0.0f, 1.0f, "" },
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
    [[nodiscard]] float getBandGainReductionDb(size_t band) const noexcept;

private:
    juce::AudioProcessorValueTreeState apvts;
    openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)> params;
    std::array<openx::dsp::MultibandProcessor<float, 4>, 2> processors;
    openx::ui::SpectrumAnalyzer<11> spectrumAnalyzer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace openx::mb
