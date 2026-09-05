#include "PluginEditor.h"

namespace openx::comp {

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

    setupSlider(threshSlider,  threshLabel,  "Threshold");
    setupSlider(ratioSlider,   ratioLabel,   "Ratio");
    setupSlider(kneeSlider,    kneeLabel,    "Knee");
    setupSlider(attackSlider,  attackLabel,  "Attack");
    setupSlider(releaseSlider, releaseLabel, "Release");
    setupSlider(punchSlider,   punchLabel,   "TS-WD Punch");

    auto& apvts = processor.getApvts();
    threshAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "threshold", threshSlider);
    ratioAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "ratio", ratioSlider);
    kneeAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "knee", kneeSlider);
    attackAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "attack", attackSlider);
    releaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "release", releaseSlider);
    punchAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "punch", punchSlider);

    setSize(860, 520);
    startTimerHz(60);
}

PluginEditor::~PluginEditor() {
    setLookAndFeel(nullptr);
}

void PluginEditor::timerCallback() {
    std::array<openx::ui::ScrollingHistory<512>::Frame, 512> frames{};
    processor.getHistory().readOrdered(frames);
    scope.updateHistory(frames, static_cast<float>(threshSlider.getValue()));
}

void PluginEditor::paint(juce::Graphics& g) {
    g.fillAll(openx::ui::OpenXLookAndFeel::BackgroundDark);

    g.setColour(openx::ui::OpenXLookAndFeel::TextPrimary);
    g.setFont(juce::Font(18.0f, juce::Font::bold));
    g.drawText("OPEN-X  |  COMP-X", 20, 10, 200, 30, juce::Justification::left);

    g.setColour(openx::ui::OpenXLookAndFeel::TextMuted);
    g.setFont(juce::Font(12.0f, juce::Font::plain));
    g.drawText("Precision Analytic Dynamic Compressor", 220, 12, 260, 30, juce::Justification::left);
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

    placeKnob(threshSlider,  threshLabel,  bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(ratioSlider,   ratioLabel,   bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(kneeSlider,    kneeLabel,    bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(attackSlider,  attackLabel,  bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(releaseSlider, releaseLabel, bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(punchSlider,   punchLabel,   bottomPanel.removeFromLeft(knobWidth).reduced(4));
}

} // namespace openx::comp
