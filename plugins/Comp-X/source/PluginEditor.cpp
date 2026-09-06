#include "PluginEditor.h"

namespace openx::comp {

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);

    addAndMakeVisible(scope);

    // 1. Header controls (Style selector and Sidechain Audition)
    styleLabel.setText("Style", juce::dontSendNotification);
    styleLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    styleLabel.setColour(juce::Label::textColourId, openx::ui::OpenXLookAndFeel::TextMuted);
    styleLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(styleLabel);

    styleBox.addItem("Clean", 1);
    styleBox.addItem("Classic", 2);
    styleBox.addItem("Opto", 3);
    styleBox.addItem("Vocal", 4);
    styleBox.addItem("Mastering", 5);
    styleBox.addItem("Punch", 6);
    styleBox.addItem("Bus", 7);
    styleBox.addItem("Pumping", 8);
    addAndMakeVisible(styleBox);

    scAuditionToggle.setButtonText("SC Listen");
    scAuditionToggle.setColour(juce::ToggleButton::textColourId, openx::ui::OpenXLookAndFeel::AccentAmber);
    addAndMakeVisible(scAuditionToggle);

    // 2. Rotary Slider setup helper
    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 62, 16);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(label);
    };

    // Dynamics Knobs
    setupSlider(threshSlider,  threshLabel,  "Threshold");
    setupSlider(ratioSlider,   ratioLabel,   "Ratio");
    setupSlider(kneeSlider,    kneeLabel,    "Knee");
    setupSlider(attackSlider,  attackLabel,  "Attack");
    setupSlider(releaseSlider, releaseLabel, "Release");
    setupSlider(holdSlider,    holdLabel,    "Hold");

    autoRelToggle.setButtonText("Auto");
    autoRelToggle.setColour(juce::ToggleButton::textColourId, openx::ui::OpenXLookAndFeel::AccentCyan);
    addAndMakeVisible(autoRelToggle);

    // Sidechain & Character Knobs
    setupSlider(scHpfSlider,     scHpfLabel,     "SC High-Pass");
    setupSlider(scLpfSlider,     scLpfLabel,     "SC Low-Pass");
    setupSlider(lookaheadSlider, lookaheadLabel, "Lookahead");
    setupSlider(punchSlider,     punchLabel,     "TS-WD Punch");

    // Output Knobs
    setupSlider(makeupSlider, makeupLabel, "Makeup");
    setupSlider(mixSlider,    mixLabel,    "Dry / Wet");

    autoGainToggle.setButtonText("Auto");
    autoGainToggle.setColour(juce::ToggleButton::textColourId, openx::ui::OpenXLookAndFeel::AccentCyan);
    addAndMakeVisible(autoGainToggle);

    // 3. APVTS Attachments
    auto& apvts = processor.getApvts();
    styleAttach      = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "style", styleBox);
    scAuditionAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "sc_audition", scAuditionToggle);
    autoRelAttach    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "auto_release", autoRelToggle);
    autoGainAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "auto_gain", autoGainToggle);

    threshAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "threshold", threshSlider);
    ratioAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "ratio", ratioSlider);
    kneeAttach       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "knee", kneeSlider);
    attackAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "attack", attackSlider);
    releaseAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "release", releaseSlider);
    holdAttach       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "hold", holdSlider);

    scHpfAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "sc_hpf", scHpfSlider);
    scLpfAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "sc_lpf", scLpfSlider);
    lookaheadAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "lookahead", lookaheadSlider);
    punchAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "punch", punchSlider);

    makeupAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "makeup", makeupSlider);
    mixAttach        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mix", mixSlider);

    setSize(960, 640);
    startTimerHz(60);
}

PluginEditor::~PluginEditor() {
    setLookAndFeel(nullptr);
}

void PluginEditor::timerCallback() {
    std::array<openx::ui::ScrollingHistory<512>::Frame, 512> frames{};
    processor.getHistory().readOrdered(frames);
    scope.updateHistory(frames,
                        static_cast<float>(threshSlider.getValue()),
                        static_cast<float>(kneeSlider.getValue()),
                        scAuditionToggle.getToggleState());
}

void PluginEditor::paint(juce::Graphics& g) {
    g.fillAll(openx::ui::OpenXLookAndFeel::BackgroundDark);

    // Header branding
    g.setColour(openx::ui::OpenXLookAndFeel::TextPrimary);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("OPEN-X  |  COMP-X", 20, 10, 200, 30, juce::Justification::left);

    g.setColour(openx::ui::OpenXLookAndFeel::TextMuted);
    g.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    g.drawText("Precision Analytic Mastering Compressor", 215, 12, 280, 30, juce::Justification::left);

    // Section group background panels
    const auto localBounds = getLocalBounds().reduced(16);
    auto bottomArea = localBounds.withTrimmedTop(localBounds.getHeight() - 236);

    g.setColour(juce::Colour(0x221c2128));
    g.fillRoundedRectangle(bottomArea.toFloat(), 6.0f);
    g.setColour(openx::ui::OpenXLookAndFeel::OutlineColour);
    g.drawRoundedRectangle(bottomArea.toFloat(), 6.0f, 1.0f);
}

void PluginEditor::resized() {
    auto bounds = getLocalBounds().reduced(16);

    // 1. Header controls
    auto headerArea = bounds.removeFromTop(36);
    scAuditionToggle.setBounds(headerArea.removeFromRight(95).reduced(2, 6));
    headerArea.removeFromRight(12);
    styleBox.setBounds(headerArea.removeFromRight(110).reduced(2, 6));
    styleLabel.setBounds(headerArea.removeFromRight(45).reduced(2, 6));

    bounds.removeFromTop(8);

    // 2. Bottom controls panel (height 230 px)
    auto bottomArea = bounds.removeFromBottom(234);

    // 3. Central Scope visualizer
    bounds.removeFromBottom(8);
    scope.setBounds(bounds);

    // 4. Two rows of 6 controls in bottom panel
    const int rowHeight = bottomArea.getHeight() / 2;
    auto row1 = bottomArea.removeFromTop(rowHeight).reduced(4, 2);
    auto row2 = bottomArea.reduced(4, 2);

    const int colWidth = row1.getWidth() / 6;

    auto placeKnob = [](juce::Slider& s, juce::Label& l, juce::Rectangle<int> r) {
        l.setBounds(r.removeFromTop(18));
        s.setBounds(r);
    };

    // Row 1: Threshold, Ratio, Knee, Attack, Release (+ Auto toggle), Hold
    placeKnob(threshSlider,  threshLabel,  row1.removeFromLeft(colWidth).reduced(3));
    placeKnob(ratioSlider,   ratioLabel,   row1.removeFromLeft(colWidth).reduced(3));
    placeKnob(kneeSlider,    kneeLabel,    row1.removeFromLeft(colWidth).reduced(3));
    placeKnob(attackSlider,  attackLabel,  row1.removeFromLeft(colWidth).reduced(3));

    auto relCol = row1.removeFromLeft(colWidth).reduced(3);
    releaseLabel.setBounds(relCol.removeFromTop(18));
    autoRelToggle.setBounds(relCol.removeFromBottom(18).reduced(8, 0));
    releaseSlider.setBounds(relCol);

    placeKnob(holdSlider,    holdLabel,    row1.removeFromLeft(colWidth).reduced(3));

    // Row 2: SC HPF, SC LPF, Lookahead, TS-WD Punch, Makeup (+ Auto toggle), Mix
    placeKnob(scHpfSlider,     scHpfLabel,     row2.removeFromLeft(colWidth).reduced(3));
    placeKnob(scLpfSlider,     scLpfLabel,     row2.removeFromLeft(colWidth).reduced(3));
    placeKnob(lookaheadSlider, lookaheadLabel, row2.removeFromLeft(colWidth).reduced(3));
    placeKnob(punchSlider,     punchLabel,     row2.removeFromLeft(colWidth).reduced(3));

    auto makeupCol = row2.removeFromLeft(colWidth).reduced(3);
    makeupLabel.setBounds(makeupCol.removeFromTop(18));
    autoGainToggle.setBounds(makeupCol.removeFromBottom(18).reduced(8, 0));
    makeupSlider.setBounds(makeupCol);

    placeKnob(mixSlider,       mixLabel,       row2.removeFromLeft(colWidth).reduced(3));
}

} // namespace openx::comp
