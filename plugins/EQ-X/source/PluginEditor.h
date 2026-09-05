#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_ui/components/interactive_eq_curve.hpp>
#include <openx_ui/theme/openx_lookandfeel.hpp>
#include "PluginProcessor.h"

namespace openx::eq {

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
    openx::ui::InteractiveEqCurve eqCurve;

    juce::Slider freqSlider, gainSlider, qSlider, dynGainSlider, threshSlider;
    juce::Label freqLabel, gainLabel, qLabel, dynGainLabel, threshLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynGainAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> threshAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace openx::eq
