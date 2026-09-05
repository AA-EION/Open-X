#include "PluginEditor.h"

namespace openx::verb {

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);

    addAndMakeVisible(decayCurve);

    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        addAndMakeVisible(label);
    };

    setupSlider(decaySlider,   decayLabel,   "Decay Time");
    setupSlider(dampingSlider, dampingLabel, "Damping Freq");
    setupSlider(chaosSlider,   chaosLabel,   "SH-WCP Chaos");
    setupSlider(mixSlider,     mixLabel,     "Dry / Wet");

    auto& apvts = processor.getApvts();
    decayAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "decay", decaySlider);
    dampingAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "damping", dampingSlider);
    chaosAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "chaos", chaosSlider);
    mixAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mix", mixSlider);

    setSize(860, 520);
    startTimerHz(60);
}

PluginEditor::~PluginEditor() {
    setLookAndFeel(nullptr);
}

void PluginEditor::timerCallback() {
    auto& analyzer = processor.getSpectrumAnalyzer();
    const float sr = static_cast<float>(processor.getSampleRate());
    analyzer.update(sr > 0 ? sr : 48000.0f);

    auto scopeData = analyzer.getScopeData();
    decayCurve.updateSpectrum(scopeData.data(), scopeData.size(), sr > 0 ? sr : 48000.0f);

    openx::ui::InteractiveEqCurve::FilterBandState state;
    state.frequency = static_cast<float>(dampingSlider.getValue());
    state.gainDb = -6.0f; // Graphical damping shelf representation
    state.q = 0.7071f;
    state.isDynamic = false;
    decayCurve.setBandState(state);
}

void PluginEditor::paint(juce::Graphics& g) {
    g.fillAll(openx::ui::OpenXLookAndFeel::BackgroundDark);

    g.setColour(openx::ui::OpenXLookAndFeel::TextPrimary);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("OPEN-X  |  VERB-X", 20, 10, 200, 30, juce::Justification::left);

    g.setColour(openx::ui::OpenXLookAndFeel::TextMuted);
    g.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    g.drawText("16-Channel Orthogonal Symplectic Reverb", 220, 12, 280, 30, juce::Justification::left);
}

void PluginEditor::resized() {
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(32);

    auto bottomPanel = area.removeFromBottom(110);
    decayCurve.setBounds(area);

    const int knobWidth = bottomPanel.getWidth() / 4;
    auto placeKnob = [&](juce::Slider& s, juce::Label& l, juce::Rectangle<int> r) {
        l.setBounds(r.removeFromTop(20));
        s.setBounds(r);
    };

    placeKnob(decaySlider,   decayLabel,   bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(dampingSlider, dampingLabel, bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(chaosSlider,   chaosLabel,   bottomPanel.removeFromLeft(knobWidth).reduced(4));
    placeKnob(mixSlider,     mixLabel,     bottomPanel.removeFromLeft(knobWidth).reduced(4));
}

} // namespace openx::verb
