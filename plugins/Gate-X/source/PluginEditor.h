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

    juce::ComboBox modeBox, styleBox;
    juce::Label modeLabel, styleLabel;
    juce::ToggleButton scAuditionButton, scSourceButton;

    juce::Slider openThreshSlider, closeThreshSlider, rangeSlider, ratioSlider, kneeSlider;
    juce::Label openThreshLabel, closeThreshLabel, rangeLabel, ratioLabel, kneeLabel;

    juce::Slider attackSlider, holdSlider, releaseSlider, lookaheadSlider;
    juce::Label attackLabel, holdLabel, releaseLabel, lookaheadLabel;

    juce::Slider scLowCutSlider, scHighCutSlider, stereoLinkSlider, dryWetSlider, outGainSlider;
    juce::Label scLowCutLabel, scHighCutLabel, stereoLinkLabel, dryWetLabel, outGainLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> scAuditionAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> scSourceAttach;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> openThreshAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> closeThreshAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rangeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ratioAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> kneeAttach;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> holdAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lookaheadAttach;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> scLowCutAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> scHighCutAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stereoLinkAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dryWetAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outGainAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace openx::gate
