#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_ui/components/interactive_eq_curve.hpp>
#include <openx_ui/theme/openx_lookandfeel.hpp>
#include "PluginProcessor.h"
#include <array>
#include <memory>

namespace openx::eq {

class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit PluginEditor(PluginProcessor& p);
    ~PluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void selectBand(size_t bandIdx);

private:
    void timerCallback() override;

    PluginProcessor& processor;
    openx::ui::OpenXLookAndFeel lnf;
    openx::ui::InteractiveEqCurve eqCurve;

    size_t selectedBand{0};
    bool isUpdatingFromTimer{false};

    // Header master controls
    juce::Slider inGainSlider, outGainSlider;
    juce::Label inGainLabel, outGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inGainAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outGainAttach;

    // Band selector buttons (1 to 8)
    juce::Label bandsBarLabel;
    std::array<juce::TextButton, NumBands> bandButtons;

    // Selected band controls
    juce::Label bandBadgeLabel;
    juce::ComboBox typeComboBox;
    juce::Label typeLabel;
    juce::ToggleButton bypassButton;
    juce::ToggleButton soloButton;

    juce::Slider freqSlider, gainSlider, qSlider, dynGainSlider, threshSlider;
    juce::Label freqLabel, gainLabel, qLabel, dynGainLabel, threshLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynGainAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> threshAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace openx::eq
