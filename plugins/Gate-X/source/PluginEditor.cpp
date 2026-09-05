#include "PluginEditor.h"

namespace openx::gate {

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);

    addAndMakeVisible(scope);

    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::Font(12.0f, juce::Font::bold));
        addAndMakeVisible(label);
    };

    setupSlider(openThreshSlider,  openThreshLabel,  "Open Thresh");
    setupSlider(closeThreshSlider, closeThreshLabel, "Close Thresh");
    setupSlider(rangeSlider,       rangeLabel,       "Floor Range");
    setupSlider(attackSlider,      attackLabel,      "PKAS Attack");
    setupSlider(holdSlider,        holdLabel,        "Hold");
    setupSlider(releaseSlider,     releaseLabel,     "Release");

    auto& apvts = processor.getApvts();
    openThreshAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "open_thresh", openThreshSlider);
    closeThreshAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "close_thresh", closeThreshSlider);
    rangeAttach       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "range", rangeSlider);
    attackAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "attack", attackSlider);
    holdAttach        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "hold", holdSlider);
    releaseAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "release", releaseSlider);

    setSize(860, 520);
    startTimerHz(60);
}

PluginEditor::~PluginEditor() {
    setLookAndFeel(nullptr);
}

void PluginEditor::timerCallback() {
    std::array<openx::ui::ScrollingHistory<512>::Frame, 512> frames{};
    processor.getHistory().readOrdered(frames);
    scope.updateHistory(frames, static_cast<float>(openThreshSlider.getValue()));
}

void PluginEditor::paint(juce::Graphics& g) {
    g.fillAll(openx::ui::OpenXLookAndFeel::BackgroundDark);

    g.setColour(openx::ui::OpenXLookAndFeel::TextPrimary);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("OPEN-X  |  GATE-X", 20, 10, 200, 30, juce::Justification::left);

    g.setColour(openx::ui::OpenXLookAndFeel::TextMuted);
    g.setFont(juce::Font(12.0f, juce::Font::plain));
    g.drawText("Kinematic Predictive Lookahead Expander & Gate", 220, 12, 320, 30, juce::Justification::left);
}

void PluginEditor::resized() {
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(32);

    auto bottomPanel = area.removeFromBottom(110);
    scope.setBounds(area);

    const int knobWidth = bottomPanel.getWidth() / 6;
    auto placeKnob = [&](juce::Slider& s, juce::Label& l, juce::Rectangle<int> r) {
        l.setBounds(r.removeFromTop(20));
        s.setBounds(r);
    };

    placeKnob(openThreshSlider,  openThreshLabel,  bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(closeThreshSlider, closeThreshLabel, bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(rangeSlider,       rangeLabel,       bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(attackSlider,      attackLabel,      bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(holdSlider,        holdLabel,        bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(releaseSlider,     releaseLabel,     bottomPanel.removeFromLeft(knobWidth).reduced(4));
}

} // namespace openx::gate
