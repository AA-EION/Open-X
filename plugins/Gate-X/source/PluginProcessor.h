#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_dsp/dynamics/predictive_gate.hpp>
#include <openx_dsp/state/parameter_manager.hpp>
#include <openx_ui/dsp/scrolling_history.hpp>
#include <array>

namespace openx::gate {

enum class ParamId : size_t {
    Mode,
    Style,
    OpenThreshold,
    CloseThreshold,
    Ratio,
    Range,
    Knee,
    Attack,
    Hold,
    Release,
    Lookahead,
    ScLowCut,
    ScHighCut,
    ScAudition,
    ScSource,
    DryWet,
    OutputGain,
    StereoLink,
    Count
};

constexpr std::array<openx::state::ParameterDescriptor, static_cast<size_t>(ParamId::Count)> Descriptors{{
    { static_cast<size_t>(ParamId::Mode),           "mode",         "Mode",            0.0f,     2.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::Style),          "style",        "Style",           0.0f,     2.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::OpenThreshold),  "open_thresh",  "Open Thresh",   -60.0f,     0.0f,  -30.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::CloseThreshold), "close_thresh", "Close Thresh",  -60.0f,     0.0f,  -36.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Ratio),          "ratio",        "Ratio",           1.0f,    20.0f,    4.0f, 0.5f, ":1" },
    { static_cast<size_t>(ParamId::Range),          "range",        "Range",         -80.0f,     0.0f,  -60.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Knee),           "knee",         "Knee",            0.0f,    24.0f,    3.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::Attack),         "attack",       "Attack",          0.01f,  100.0f,    2.0f, 0.3f, "ms" },
    { static_cast<size_t>(ParamId::Hold),           "hold",         "Hold",            0.0f,  1000.0f,   20.0f, 0.4f, "ms" },
    { static_cast<size_t>(ParamId::Release),        "release",      "Release",         5.0f,  2000.0f,   80.0f, 0.4f, "ms" },
    { static_cast<size_t>(ParamId::Lookahead),      "lookahead",    "Lookahead",       0.0f,    20.0f,    5.0f, 0.5f, "ms" },
    { static_cast<size_t>(ParamId::ScLowCut),       "sc_lowcut",    "SC Low Cut",     10.0f, 10000.0f,   20.0f, 0.25f, "Hz" },
    { static_cast<size_t>(ParamId::ScHighCut),      "sc_highcut",   "SC High Cut",   100.0f, 22000.0f, 20000.0f, 0.25f, "Hz" },
    { static_cast<size_t>(ParamId::ScAudition),     "sc_audition",  "SC Audition",     0.0f,     1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::ScSource),       "sc_source",    "SC Source",       0.0f,     1.0f,    0.0f, 1.0f, "" },
    { static_cast<size_t>(ParamId::DryWet),         "dry_wet",      "Dry/Wet",         0.0f,   100.0f,  100.0f, 1.0f, "%" },
    { static_cast<size_t>(ParamId::OutputGain),     "out_gain",     "Output Gain",   -24.0f,    24.0f,    0.0f, 1.0f, "dB" },
    { static_cast<size_t>(ParamId::StereoLink),     "stereo_link",  "Stereo Link",     0.0f,   100.0f,  100.0f, 1.0f, "%" },
}};

class PluginProcessor final : public juce::AudioProcessor {
public:
    PluginProcessor();
    ~PluginProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

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
    int getLastGateState() const noexcept { return lastGateState.load(std::memory_order_relaxed); }

private:
    juce::AudioProcessorValueTreeState apvts;
    openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)> params;
    std::array<openx::dsp::PredictiveGate<float, 8192>, 2> gates;
    openx::ui::ScrollingHistory<512> history;
    std::atomic<int> lastGateState{0};
    int currentReportedLatency{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};

} // namespace openx::gate
