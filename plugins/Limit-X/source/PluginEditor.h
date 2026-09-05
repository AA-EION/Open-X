#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_ui/components/dynamics_scope.hpp>
#include <openx_ui/theme/openx_lookandfeel.hpp>
#include "PluginProcessor.h"

namespace openx::limit {

class MasterMeterTower final : public juce::Component {
public:
    MasterMeterTower(PluginProcessor& p);
    ~MasterMeterTower() override = default;

    void updateMetrics() noexcept;
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    PluginProcessor& processor;
    float currentInTp{-100.0f};
    float currentOutTp{-100.0f};
    float currentGr{0.0f};
    float momentaryLufs{-100.0f};
    float shortTermLufs{-100.0f};
    float integratedLufs{-100.0f};
    float maxGrHold{0.0f};

    juce::TextButton resetBtn{ "RESET" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MasterMeterTower)
};

class PluginEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit PluginEditor(PluginProcessor& p);
    ~PluginEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateStyleDescription();

    PluginProcessor& processor;
    openx::ui::OpenXLookAndFeel lnf;
    openx::ui::DynamicsScope scope;
    MasterMeterTower meterTower;

    // Controls
    juce::ComboBox styleBox;
    juce::Label styleDescLabel;
    juce::ComboBox auditionBox;

    juce::ToggleButton truePeakButton;
    juce::ToggleButton dcFilterButton;
    juce::ToggleButton dispEnableButton;

    juce::Slider threshSlider;
    juce::Slider ceilingSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider lookaheadSlider;
    juce::Slider transLinkSlider;
    juce::Slider relLinkSlider;
    juce::Slider dispFreqSlider;

    juce::Label threshLabel;
    juce::Label ceilingLabel;
    juce::Label attackLabel;
    juce::Label releaseLabel;
    juce::Label lookaheadLabel;
    juce::Label transLinkLabel;
    juce::Label relLinkLabel;
    juce::Label dispFreqLabel;

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> styleAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> auditionAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   truePeakAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   dcFilterAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   dispEnableAttach;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> threshAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ceilingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lookaheadAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> transLinkAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> relLinkAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dispFreqAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace openx::limit
