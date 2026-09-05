#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_ui/components/dynamics_scope.hpp>
#include <openx_ui/theme/openx_lookandfeel.hpp>
#include "PluginProcessor.h"

namespace openx::gate {

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
    openx::ui::DynamicsScope scope;

    juce::Slider openThreshSlider, closeThreshSlider, rangeSlider, attackSlider, holdSlider, releaseSlider;
    juce::Label openThreshLabel, closeThreshLabel, rangeLabel, attackLabel, holdLabel, releaseLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> openThreshAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> closeThreshAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rangeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> holdAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace openx::gate
