#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_ui/components/dynamics_scope.hpp>
#include <openx_ui/theme/openx_lookandfeel.hpp>
#include "PluginProcessor.h"

namespace openx::comp {

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

    juce::Slider threshSlider, ratioSlider, kneeSlider, attackSlider, releaseSlider, punchSlider;
    juce::Label threshLabel, ratioLabel, kneeLabel, attackLabel, releaseLabel, punchLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> threshAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ratioAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> kneeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> punchAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace openx::comp
