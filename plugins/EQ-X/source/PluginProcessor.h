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

    // Band 1
    Band1Freq, Band1Gain, Band1Q, Band1DynGain, Band1Threshold, Band1Type, Band1Bypass, Band1Solo,
    // Band 2
    Band2Freq, Band2Gain, Band2Q, Band2DynGain, Band2Threshold, Band2Type, Band2Bypass, Band2Solo,
    // Band 3
    Band3Freq, Band3Gain, Band3Q, Band3DynGain, Band3Threshold, Band3Type, Band3Bypass, Band3Solo,
    // Band 4
    Band4Freq, Band4Gain, Band4Q, Band4DynGain, Band4Threshold, Band4Type, Band4Bypass, Band4Solo,
    // Band 5
    Band5Freq, Band5Gain, Band5Q, Band5DynGain, Band5Threshold, Band5Type, Band5Bypass, Band5Solo,
    // Band 6
    Band6Freq, Band6Gain, Band6Q, Band6DynGain, Band6Threshold, Band6Type, Band6Bypass, Band6Solo,
    // Band 7
    Band7Freq, Band7Gain, Band7Q, Band7DynGain, Band7Threshold, Band7Type, Band7Bypass, Band7Solo,
    // Band 8
    Band8Freq, Band8Gain, Band8Q, Band8DynGain, Band8Threshold, Band8Type, Band8Bypass, Band8Solo,

    Count
};

static constexpr size_t NumBands = 8;
static constexpr size_t ParamsPerBand = 8;

constexpr ParamId getBandFreqId(size_t bandIdx) noexcept {
    return static_cast<ParamId>(2 + bandIdx * ParamsPerBand + 0);
}
constexpr ParamId getBandGainId(size_t bandIdx) noexcept {
    return static_cast<ParamId>(2 + bandIdx * ParamsPerBand + 1);
}
constexpr ParamId getBandQId(size_t bandIdx) noexcept {
    return static_cast<ParamId>(2 + bandIdx * ParamsPerBand + 2);
}
constexpr ParamId getBandDynGainId(size_t bandIdx) noexcept {
    return static_cast<ParamId>(2 + bandIdx * ParamsPerBand + 3);
}
constexpr ParamId getBandThresholdId(size_t bandIdx) noexcept {
    return static_cast<ParamId>(2 + bandIdx * ParamsPerBand + 4);
}
constexpr ParamId getBandTypeId(size_t bandIdx) noexcept {
    return static_cast<ParamId>(2 + bandIdx * ParamsPerBand + 5);
}
constexpr ParamId getBandBypassId(size_t bandIdx) noexcept {
    return static_cast<ParamId>(2 + bandIdx * ParamsPerBand + 6);
}
constexpr ParamId getBandSoloId(size_t bandIdx) noexcept {
    return static_cast<ParamId>(2 + bandIdx * ParamsPerBand + 7);
}

inline juce::String getBandFreqIdStr(size_t b) { return "b" + juce::String(static_cast<int>(b + 1)) + "_freq"; }
inline juce::String getBandGainIdStr(size_t b) { return "b" + juce::String(static_cast<int>(b + 1)) + "_gain"; }
inline juce::String getBandQIdStr(size_t b) { return "b" + juce::String(static_cast<int>(b + 1)) + "_q"; }
inline juce::String getBandDynGainIdStr(size_t b) { return "b" + juce::String(static_cast<int>(b + 1)) + "_dyngain"; }
inline juce::String getBandThresholdIdStr(size_t b) { return "b" + juce::String(static_cast<int>(b + 1)) + "_thresh"; }
inline juce::String getBandTypeIdStr(size_t b) { return "b" + juce::String(static_cast<int>(b + 1)) + "_type"; }
inline juce::String getBandBypassIdStr(size_t b) { return "b" + juce::String(static_cast<int>(b + 1)) + "_bypass"; }
inline juce::String getBandSoloIdStr(size_t b) { return "b" + juce::String(static_cast<int>(b + 1)) + "_solo"; }

constexpr std::array<openx::state::ParameterDescriptor, static_cast<size_t>(ParamId::Count)> Descriptors{{
    { static_cast<size_t>(ParamId::InputGain),      "in_gain",     "Input Gain",       -30.0f, 30.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::OutputGain),     "out_gain",    "Output Gain",      -30.0f, 30.0f,   0.0f, 1.0f, "dB" },

    // Band 1: Low Shelf at 30 Hz
    { static_cast<size_t>(ParamId::Band1Freq),      "b1_freq",     "Band 1 Freq",       20.0f, 20000.0f, 30.0f, 0.25f, "Hz" },
    { static_cast<size_t>(ParamId::Band1Gain),      "b1_gain",     "Band 1 Gain",      -30.0f, 30.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band1Q),         "b1_q",        "Band 1 Q",          0.1f,  18.0f,   0.7071f, 0.5f, "" },
    { static_cast<size_t>(ParamId::Band1DynGain),   "b1_dyngain",  "Band 1 Dyn Gain",  -30.0f, 30.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band1Threshold), "b1_thresh",   "Band 1 Thresh",    -60.0f, 0.0f,   -20.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band1Type),      "b1_type",     "Band 1 Type",        0.0f, 5.0f,    1.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Band1Bypass),    "b1_bypass",   "Band 1 Bypass",      0.0f, 1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Band1Solo),      "b1_solo",     "Band 1 Solo",        0.0f, 1.0f,    0.0f, 1.0f, "" },

    // Band 2: Bell at 80 Hz
    { static_cast<size_t>(ParamId::Band2Freq),      "b2_freq",     "Band 2 Freq",       20.0f, 20000.0f, 80.0f, 0.25f, "Hz" },
    { static_cast<size_t>(ParamId::Band2Gain),      "b2_gain",     "Band 2 Gain",      -30.0f, 30.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band2Q),         "b2_q",        "Band 2 Q",          0.1f,  18.0f,   0.7071f, 0.5f, "" },
    { static_cast<size_t>(ParamId::Band2DynGain),   "b2_dyngain",  "Band 2 Dyn Gain",  -30.0f, 30.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band2Threshold), "b2_thresh",   "Band 2 Thresh",    -60.0f, 0.0f,   -20.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band2Type),      "b2_type",     "Band 2 Type",        0.0f, 5.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Band2Bypass),    "b2_bypass",   "Band 2 Bypass",      0.0f, 1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Band2Solo),      "b2_solo",     "Band 2 Solo",        0.0f, 1.0f,    0.0f, 1.0f, "" },

    // Band 3: Bell at 200 Hz
    { static_cast<size_t>(ParamId::Band3Freq),      "b3_freq",     "Band 3 Freq",       20.0f, 20000.0f, 200.0f, 0.25f, "Hz" },
    { static_cast<size_t>(ParamId::Band3Gain),      "b3_gain",     "Band 3 Gain",      -30.0f, 30.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band3Q),         "b3_q",        "Band 3 Q",          0.1f,  18.0f,   0.7071f, 0.5f, "" },
    { static_cast<size_t>(ParamId::Band3DynGain),   "b3_dyngain",  "Band 3 Dyn Gain",  -30.0f, 30.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band3Threshold), "b3_thresh",   "Band 3 Thresh",    -60.0f, 0.0f,   -20.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band3Type),      "b3_type",     "Band 3 Type",        0.0f, 5.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Band3Bypass),    "b3_bypass",   "Band 3 Bypass",      0.0f, 1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Band3Solo),      "b3_solo",     "Band 3 Solo",        0.0f, 1.0f,    0.0f, 1.0f, "" },

    // Band 4: Bell at 600 Hz
    { static_cast<size_t>(ParamId::Band4Freq),      "b4_freq",     "Band 4 Freq",       20.0f, 20000.0f, 600.0f, 0.25f, "Hz" },
    { static_cast<size_t>(ParamId::Band4Gain),      "b4_gain",     "Band 4 Gain",      -30.0f, 30.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band4Q),         "b4_q",        "Band 4 Q",          0.1f,  18.0f,   0.7071f, 0.5f, "" },
    { static_cast<size_t>(ParamId::Band4DynGain),   "b4_dyngain",  "Band 4 Dyn Gain",  -30.0f, 30.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band4Threshold), "b4_thresh",   "Band 4 Thresh",    -60.0f, 0.0f,   -20.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band4Type),      "b4_type",     "Band 4 Type",        0.0f, 5.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Band4Bypass),    "b4_bypass",   "Band 4 Bypass",      0.0f, 1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Band4Solo),      "b4_solo",     "Band 4 Solo",        0.0f, 1.0f,    0.0f, 1.0f, "" },

    // Band 5: Bell at 1500 Hz
    { static_cast<size_t>(ParamId::Band5Freq),      "b5_freq",     "Band 5 Freq",       20.0f, 20000.0f, 1500.0f, 0.25f, "Hz" },
    { static_cast<size_t>(ParamId::Band5Gain),      "b5_gain",     "Band 5 Gain",      -30.0f, 30.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band5Q),         "b5_q",        "Band 5 Q",          0.1f,  18.0f,   0.7071f, 0.5f, "" },
    { static_cast<size_t>(ParamId::Band5DynGain),   "b5_dyngain",  "Band 5 Dyn Gain",  -30.0f, 30.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band5Threshold), "b5_thresh",   "Band 5 Thresh",    -60.0f, 0.0f,   -20.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band5Type),      "b5_type",     "Band 5 Type",        0.0f, 5.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Band5Bypass),    "b5_bypass",   "Band 5 Bypass",      0.0f, 1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Band5Solo),      "b5_solo",     "Band 5 Solo",        0.0f, 1.0f,    0.0f, 1.0f, "" },

    // Band 6: Bell at 4000 Hz
    { static_cast<size_t>(ParamId::Band6Freq),      "b6_freq",     "Band 6 Freq",       20.0f, 20000.0f, 4000.0f, 0.25f, "Hz" },
    { static_cast<size_t>(ParamId::Band6Gain),      "b6_gain",     "Band 6 Gain",      -30.0f, 30.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band6Q),         "b6_q",        "Band 6 Q",          0.1f,  18.0f,   0.7071f, 0.5f, "" },
    { static_cast<size_t>(ParamId::Band6DynGain),   "b6_dyngain",  "Band 6 Dyn Gain",  -30.0f, 30.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band6Threshold), "b6_thresh",   "Band 6 Thresh",    -60.0f, 0.0f,   -20.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band6Type),      "b6_type",     "Band 6 Type",        0.0f, 5.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Band6Bypass),    "b6_bypass",   "Band 6 Bypass",      0.0f, 1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Band6Solo),      "b6_solo",     "Band 6 Solo",        0.0f, 1.0f,    0.0f, 1.0f, "" },

    // Band 7: Bell at 9000 Hz
    { static_cast<size_t>(ParamId::Band7Freq),      "b7_freq",     "Band 7 Freq",       20.0f, 20000.0f, 9000.0f, 0.25f, "Hz" },
    { static_cast<size_t>(ParamId::Band7Gain),      "b7_gain",     "Band 7 Gain",      -30.0f, 30.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band7Q),         "b7_q",        "Band 7 Q",          0.1f,  18.0f,   0.7071f, 0.5f, "" },
    { static_cast<size_t>(ParamId::Band7DynGain),   "b7_dyngain",  "Band 7 Dyn Gain",  -30.0f, 30.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band7Threshold), "b7_thresh",   "Band 7 Thresh",    -60.0f, 0.0f,   -20.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band7Type),      "b7_type",     "Band 7 Type",        0.0f, 5.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Band7Bypass),    "b7_bypass",   "Band 7 Bypass",      0.0f, 1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Band7Solo),      "b7_solo",     "Band 7 Solo",        0.0f, 1.0f,    0.0f, 1.0f, "" },

    // Band 8: High Shelf at 16000 Hz
    { static_cast<size_t>(ParamId::Band8Freq),      "b8_freq",     "Band 8 Freq",       20.0f, 20000.0f, 16000.0f, 0.25f, "Hz" },
    { static_cast<size_t>(ParamId::Band8Gain),      "b8_gain",     "Band 8 Gain",      -30.0f, 30.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band8Q),         "b8_q",        "Band 8 Q",          0.1f,  18.0f,   0.7071f, 0.5f, "" },
    { static_cast<size_t>(ParamId::Band8DynGain),   "b8_dyngain",  "Band 8 Dyn Gain",  -30.0f, 30.0f,   0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band8Threshold), "b8_thresh",   "Band 8 Thresh",    -60.0f, 0.0f,   -20.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Band8Type),      "b8_type",     "Band 8 Type",        0.0f, 5.0f,    2.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Band8Bypass),    "b8_bypass",   "Band 8 Bypass",      0.0f, 1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Band8Solo),      "b8_solo",     "Band 8 Solo",        0.0f, 1.0f,    0.0f, 1.0f, "" },
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

    [[nodiscard]] float getDynamicGainOffset(size_t bandIdx) const noexcept {
        if (bandIdx < NumBands) {
            return dynamicGainOffsets[bandIdx].load(std::memory_order_relaxed);
        }
        return 0.0f;
    }

    [[nodiscard]] const openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)>& getParamManager() const noexcept {
        return params;
    }

private:
    juce::AudioProcessorValueTreeState apvts;
    openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)> params;
    std::array<std::array<openx::dsp::DynamicBiquadEngine<float>, NumBands>, 2> dynBiquads;
    std::array<std::atomic<float>, NumBands> dynamicGainOffsets{};
    openx::ui::SpectrumAnalyzer<11> spectrumAnalyzer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace openx::eq
