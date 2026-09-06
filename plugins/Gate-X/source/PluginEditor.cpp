#include "PluginEditor.h"

namespace openx::gate {

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);

    addAndMakeVisible(scope);

    // Setup Dropdowns
    auto setupBox = [this](juce::ComboBox& box, juce::Label& label, const juce::String& text) {
        addAndMakeVisible(box);
        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centredRight);
        label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(label);
    };

    modeBox.addItem("Gate", 1);
    modeBox.addItem("Duck", 2);
    modeBox.addItem("Expander", 3);
    setupBox(modeBox, modeLabel, "Mode:");

    styleBox.addItem("Clean", 1);
    styleBox.addItem("Classic", 2);
    styleBox.addItem("Vocal", 3);
    setupBox(styleBox, styleLabel, "Style:");

    // Setup Buttons
    scAuditionButton.setButtonText("SC Audition");
    addAndMakeVisible(scAuditionButton);

    scSourceButton.setButtonText("Ext SC");
    addAndMakeVisible(scSourceButton);

    // Setup Rotary Sliders
    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 16);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(label);
    };

    // Dynamics Section
    setupSlider(openThreshSlider,  openThreshLabel,  "Open Thresh");
    setupSlider(closeThreshSlider, closeThreshLabel, "Close Thresh");
    setupSlider(rangeSlider,       rangeLabel,       "Floor Range");
    setupSlider(ratioSlider,       ratioLabel,       "Ratio");
    setupSlider(kneeSlider,        kneeLabel,        "Knee");

    // Time Section
    setupSlider(attackSlider,      attackLabel,      "Attack");
    setupSlider(holdSlider,        holdLabel,        "Hold");
    setupSlider(releaseSlider,     releaseLabel,     "Release");
    setupSlider(lookaheadSlider,   lookaheadLabel,   "Lookahead");

    // Sidechain & Output Section
    setupSlider(scLowCutSlider,    scLowCutLabel,    "SC Low Cut");
    setupSlider(scHighCutSlider,   scHighCutLabel,   "SC High Cut");
    setupSlider(stereoLinkSlider,  stereoLinkLabel,  "Stereo Link");
    setupSlider(dryWetSlider,      dryWetLabel,      "Dry / Wet");
    setupSlider(outGainSlider,     outGainLabel,     "Output Gain");

    auto& apvts = processor.getApvts();
    modeAttach        = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "mode", modeBox);
    styleAttach       = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "style", styleBox);
    scAuditionAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "sc_audition", scAuditionButton);
    scSourceAttach    = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "sc_source", scSourceButton);

    openThreshAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "open_thresh", openThreshSlider);
    closeThreshAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "close_thresh", closeThreshSlider);
    rangeAttach       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "range", rangeSlider);
    ratioAttach       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "ratio", ratioSlider);
    kneeAttach        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "knee", kneeSlider);

    attackAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "attack", attackSlider);
    holdAttach        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "hold", holdSlider);
    releaseAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "release", releaseSlider);
    lookaheadAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "lookahead", lookaheadSlider);

    scLowCutAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "sc_lowcut", scLowCutSlider);
    scHighCutAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "sc_highcut", scHighCutSlider);
    stereoLinkAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "stereo_link", stereoLinkSlider);
    dryWetAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "dry_wet", dryWetSlider);
    outGainAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "out_gain", outGainSlider);

    setSize(960, 580);
    startTimerHz(60);
}

PluginEditor::~PluginEditor() {
    setLookAndFeel(nullptr);
}

void PluginEditor::timerCallback() {
    std::array<openx::ui::ScrollingHistory<512>::Frame, 512> frames{};
    processor.getHistory().readOrdered(frames);

    const float openThresh = static_cast<float>(openThreshSlider.getValue());
    const float closeThresh = static_cast<float>(closeThreshSlider.getValue());
    const float kneeDb = static_cast<float>(kneeSlider.getValue());
    const int gateState = processor.getLastGateState();
    const int mode = std::max(0, modeBox.getSelectedId() - 1);
    const bool audition = scAuditionButton.getToggleState();

    scope.updateHistory(frames, openThresh, closeThresh, gateState, mode, audition, kneeDb);
}

void PluginEditor::paint(juce::Graphics& g) {
    g.fillAll(openx::ui::OpenXLookAndFeel::BackgroundDark);

    // Header
    g.setColour(openx::ui::OpenXLookAndFeel::TextPrimary);
    g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    g.drawText("OPEN-X  |  GATE-X", 18, 10, 180, 26, juce::Justification::left);

    g.setColour(openx::ui::OpenXLookAndFeel::TextMuted);
    g.setFont(juce::FontOptions(11.5f, juce::Font::plain));
    g.drawText("Kinematic Dual-Threshold Predictive Expander & Gate", 195, 12, 330, 24, juce::Justification::left);

    // Section Titles in Bottom Panel
    const auto area = getLocalBounds().reduced(16);
    const int bottomY = area.getBottom() - 145;

    g.setColour(openx::ui::OpenXLookAndFeel::OutlineColour);
    g.drawHorizontalLine(bottomY - 4, static_cast<float>(area.getX()), static_cast<float>(area.getRight()));

    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.setColour(openx::ui::OpenXLookAndFeel::AccentCyan.withAlpha(0.85f));
    g.drawText("DYNAMICS", area.getX() + 4, bottomY - 18, 100, 14, juce::Justification::left);

    const int dynW = (area.getWidth() * 5) / 14;
    const int timeW = (area.getWidth() * 4) / 14;

    g.drawText("TIME CONSTANTS", area.getX() + dynW + 8, bottomY - 18, 120, 14, juce::Justification::left);
    g.drawText("SIDECHAIN & OUTPUT", area.getX() + dynW + timeW + 8, bottomY - 18, 150, 14, juce::Justification::left);

    // Vertical subtle dividers between sections
    g.setColour(openx::ui::OpenXLookAndFeel::OutlineColour.withAlpha(0.5f));
    g.drawVerticalLine(area.getX() + dynW, static_cast<float>(bottomY), static_cast<float>(area.getBottom()));
    g.drawVerticalLine(area.getX() + dynW + timeW, static_cast<float>(bottomY), static_cast<float>(area.getBottom()));
}

void PluginEditor::resized() {
    auto area = getLocalBounds().reduced(16);

    // Top Header bar controls (Mode, Style, Audition, Ext SC)
    auto headerArea = area.removeFromTop(28);
    auto rightControls = headerArea.removeFromRight(440);

    modeLabel.setBounds(rightControls.removeFromLeft(45));
    modeBox.setBounds(rightControls.removeFromLeft(90).reduced(2));

    styleLabel.setBounds(rightControls.removeFromLeft(45));
    styleBox.setBounds(rightControls.removeFromLeft(85).reduced(2));

    scAuditionButton.setBounds(rightControls.removeFromLeft(95).reduced(2));
    scSourceButton.setBounds(rightControls.reduced(2));

    area.removeFromTop(6);

    // Bottom Controls Panel (height 140)
    auto bottomPanel = area.removeFromBottom(140);
    area.removeFromBottom(8);

    // Middle Scope occupies remaining bounds
    scope.setBounds(area);

    // Layout Bottom Controls:
    const int totalWidth = bottomPanel.getWidth();
    const int dynW = (totalWidth * 5) / 14;
    const int timeW = (totalWidth * 4) / 14;
    const int scW = totalWidth - dynW - timeW;

    auto dynArea  = bottomPanel.removeFromLeft(dynW).reduced(2, 0);
    auto timeArea = bottomPanel.removeFromLeft(timeW).reduced(2, 0);
    auto scArea   = bottomPanel.reduced(2, 0);

    auto placeKnob = [](juce::Slider& s, juce::Label& l, juce::Rectangle<int> r) {
        l.setBounds(r.removeFromTop(18));
        s.setBounds(r);
    };

    // 1. Dynamics: 5 knobs
    const int kDynW = dynArea.getWidth() / 5;
    placeKnob(openThreshSlider,  openThreshLabel,  dynArea.removeFromLeft(kDynW).reduced(2));
    placeKnob(closeThreshSlider, closeThreshLabel, dynArea.removeFromLeft(kDynW).reduced(2));
    placeKnob(rangeSlider,       rangeLabel,       dynArea.removeFromLeft(kDynW).reduced(2));
    placeKnob(ratioSlider,       ratioLabel,       dynArea.removeFromLeft(kDynW).reduced(2));
    placeKnob(kneeSlider,        kneeLabel,        dynArea.reduced(2));

    // 2. Time: 4 knobs
    const int kTimeW = timeArea.getWidth() / 4;
    placeKnob(attackSlider,    attackLabel,    timeArea.removeFromLeft(kTimeW).reduced(2));
    placeKnob(holdSlider,      holdLabel,      timeArea.removeFromLeft(kTimeW).reduced(2));
    placeKnob(releaseSlider,   releaseLabel,   timeArea.removeFromLeft(kTimeW).reduced(2));
    placeKnob(lookaheadSlider, lookaheadLabel, timeArea.reduced(2));

    // 3. Sidechain & Output: 5 knobs
    const int kScW = scArea.getWidth() / 5;
    placeKnob(scLowCutSlider,   scLowCutLabel,   scArea.removeFromLeft(kScW).reduced(2));
    placeKnob(scHighCutSlider,  scHighCutLabel,  scArea.removeFromLeft(kScW).reduced(2));
    placeKnob(stereoLinkSlider, stereoLinkLabel, scArea.removeFromLeft(kScW).reduced(2));
    placeKnob(dryWetSlider,     dryWetLabel,     scArea.removeFromLeft(kScW).reduced(2));
    placeKnob(outGainSlider,    outGainLabel,    scArea.reduced(2));
}

} // namespace openx::gate
