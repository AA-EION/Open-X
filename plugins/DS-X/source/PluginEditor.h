#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_ui/components/interactive_eq_curve.hpp>
#include <openx_ui/theme/openx_lookandfeel.hpp>
#include "PluginProcessor.h"

namespace openx::ds {

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
    openx::ui::InteractiveEqCurve deessCurve;

    juce::Slider freqSlider, threshSlider, reductionSlider, qSlider;
    juce::ToggleButton lpcButton;
    juce::Label freqLabel, threshLabel, reductionLabel, qLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> threshAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reductionAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lpcAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace openx::ds
