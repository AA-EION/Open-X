#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_ui/components/dynamics_scope.hpp>
#include <openx_ui/theme/openx_lookandfeel.hpp>
#include "PluginProcessor.h"
#include <memory>

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

    // Header Controls
    juce::ComboBox styleBox;
    juce::Label styleLabel;
    juce::ToggleButton scAuditionToggle;

    // Dynamics Controls
    juce::Slider threshSlider, ratioSlider, kneeSlider, attackSlider, releaseSlider, holdSlider;
    juce::Label threshLabel, ratioLabel, kneeLabel, attackLabel, releaseLabel, holdLabel;
    juce::ToggleButton autoRelToggle;

    // Sidechain & Character Controls
    juce::Slider scHpfSlider, scLpfSlider, lookaheadSlider, punchSlider;
    juce::Label scHpfLabel, scLpfLabel, lookaheadLabel, punchLabel;

    // Output Controls
    juce::Slider makeupSlider, mixSlider;
    juce::Label makeupLabel, mixLabel;
    juce::ToggleButton autoGainToggle;

    // APVTS Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> scAuditionAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoRelAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttach;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> threshAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ratioAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> kneeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> holdAttach;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> scHpfAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> scLpfAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lookaheadAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> punchAttach;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> makeupAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace openx::comp
