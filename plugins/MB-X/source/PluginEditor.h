#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_ui/components/interactive_eq_curve.hpp>
#include <openx_ui/theme/openx_lookandfeel.hpp>
#include "PluginProcessor.h"

namespace openx::mb {

class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit PluginEditor(PluginProcessor& p);
    ~PluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    PluginProcessor& processor;
    openx::ui::OpenXLookAndFeel lnf;
    openx::ui::InteractiveEqCurve multibandCurve;

    juce::Slider xo1Slider, xo2Slider;
    juce::Slider lowThreshSlider, midThreshSlider, hiThreshSlider;
    juce::Slider lowGainSlider, midGainSlider, hiGainSlider;

    juce::Label xo1Label, xo2Label;
    juce::Label lowThreshLabel, midThreshLabel, hiThreshLabel;
    juce::Label lowGainLabel, midGainLabel, hiGainLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xo1Attach, xo2Attach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowThreshAttach, midThreshAttach, hiThreshAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowGainAttach, midGainAttach, hiGainAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace openx::mb
