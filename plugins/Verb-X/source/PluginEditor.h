#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_ui/components/interactive_eq_curve.hpp>
#include <openx_ui/theme/openx_lookandfeel.hpp>
#include "PluginProcessor.h"

namespace openx::verb {

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
    openx::ui::InteractiveEqCurve decayCurve;

    juce::Slider decaySlider, dampingSlider, chaosSlider, mixSlider;
    juce::Label decayLabel, dampingLabel, chaosLabel, mixLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dampingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chaosAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace openx::verb
