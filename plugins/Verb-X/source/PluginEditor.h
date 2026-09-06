#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <openx_ui/components/reverb_decay_curve.hpp>
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
    void setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& text, const juce::String& suffix);

    PluginProcessor& processor;
    openx::ui::OpenXLookAndFeel lnf;
    openx::ui::ReverbDecayCurve decayCurve;

    // Primary acoustic controls
    juce::Slider decaySlider;
    juce::Slider spaceSlider;
    juce::Slider predelaySlider;
    juce::Slider distanceSlider;
    juce::Slider diffusionSlider;
    juce::Slider widthSlider;
    juce::Slider dampingSlider;
    juce::Slider lowCutSlider;
    juce::Slider duckingSlider;
    juce::Slider chaosSlider;
    juce::Slider mixSlider;

    juce::ToggleButton predelaySyncToggle;
    juce::ComboBox predelayNoteBox;

    juce::Label decayLabel;
    juce::Label spaceLabel;
    juce::Label predelayLabel;
    juce::Label distanceLabel;
    juce::Label diffusionLabel;
    juce::Label widthLabel;
    juce::Label dampingLabel;
    juce::Label lowCutLabel;
    juce::Label duckingLabel;
    juce::Label chaosLabel;
    juce::Label mixLabel;

    // APVTS Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> decayAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spaceAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> predelayAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> predelaySyncAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> predelayNoteAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distanceAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> diffusionAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> widthAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dampingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowCutAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> duckingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> chaosAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditor)
};

} // namespace openx::verb
