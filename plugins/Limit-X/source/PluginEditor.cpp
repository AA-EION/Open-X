#include "PluginEditor.h"

namespace openx::limit {

//==============================================================================
// MasterMeterTower Implementation
//==============================================================================
MasterMeterTower::MasterMeterTower(PluginProcessor& p)
    : processor(p)
{
    resetBtn.setButtonText("RESET");
    resetBtn.onClick = [this] {
        processor.resetIntegratedLoudness();
    };
    addAndMakeVisible(resetBtn);
}

void MasterMeterTower::updateMetrics() noexcept {
    currentInTp  = processor.getInputTruePeakDb();
    currentOutTp = processor.getOutputTruePeakDb();
    currentGr    = processor.getMaxGainReductionDb();

    if (currentGr > maxGrHold) {
        maxGrHold = currentGr;
    } else {
        maxGrHold = std::max(0.0f, maxGrHold - 0.05f);
    }

    auto& lm = processor.getLoudnessMeter();
    momentaryLufs  = lm.getMomentaryLufs();
    shortTermLufs  = lm.getShortTermLufs();
    integratedLufs = lm.getIntegratedLufs();

    repaint();
}

void MasterMeterTower::paint(juce::Graphics& g) {
    const auto bounds = getLocalBounds().toFloat();

    // Card background
    g.setColour(juce::Colour(0xff161b22));
    g.fillRoundedRectangle(bounds, 4.0f);
    g.setColour(juce::Colour(0xff2d333b));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    // Header Title
    g.setColour(openx::ui::OpenXLookAndFeel::TextMuted);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("OUTPUT & LOUDNESS", bounds.getX() + 6.0f, bounds.getY() + 4.0f, bounds.getWidth() - 12.0f, 16.0f, juce::Justification::centred);

    // Metering Area
    const float meterTop = bounds.getY() + 24.0f;
    const float meterHeight = bounds.getHeight() * 0.52f;
    const float colWidth = (bounds.getWidth() - 24.0f) / 2.0f;

    const auto tpRect = juce::Rectangle<float>(bounds.getX() + 8.0f, meterTop, colWidth, meterHeight);
    const auto grRect = juce::Rectangle<float>(bounds.getX() + 16.0f + colWidth, meterTop, colWidth, meterHeight);

    // 1. True Peak Meter Bar (-48 dB to +3 dBTP)
    g.setColour(juce::Colour(0xff0e1116));
    g.fillRoundedRectangle(tpRect, 2.0f);
    g.setColour(juce::Colour(0xff2d333b));
    g.drawRoundedRectangle(tpRect, 2.0f, 1.0f);

    constexpr float minTpDb = -48.0f;
    constexpr float maxTpDb = 3.0f;
    const float normTp = (std::clamp(currentOutTp, minTpDb, maxTpDb) - minTpDb) / (maxTpDb - minTpDb);
    const float fillH = normTp * (tpRect.getHeight() - 4.0f);

    if (fillH > 0.0f) {
        const auto fillRect = juce::Rectangle<float>(tpRect.getX() + 2.0f, tpRect.getBottom() - 2.0f - fillH, tpRect.getWidth() - 4.0f, fillH);
        juce::Colour barCol = (currentOutTp > 0.0f) ? openx::ui::OpenXLookAndFeel::AccentRed : openx::ui::OpenXLookAndFeel::AccentCyan;
        g.setColour(barCol);
        g.fillRoundedRectangle(fillRect, 2.0f);
    }

    // 0 dBTP line on TP meter
    const float zeroNorm = (0.0f - minTpDb) / (maxTpDb - minTpDb);
    const float zeroY = tpRect.getBottom() - 2.0f - zeroNorm * (tpRect.getHeight() - 4.0f);
    g.setColour(juce::Colour(0xaaff1744));
    g.drawHorizontalLine(static_cast<int>(zeroY), tpRect.getX(), tpRect.getRight());

    // 2. Gain Reduction Meter Bar (0 to 24 dB downwards)
    g.setColour(juce::Colour(0xff0e1116));
    g.fillRoundedRectangle(grRect, 2.0f);
    g.setColour(juce::Colour(0xff2d333b));
    g.drawRoundedRectangle(grRect, 2.0f, 1.0f);

    const float normGr = std::clamp(currentGr / 24.0f, 0.0f, 1.0f);
    const float grFillH = normGr * (grRect.getHeight() - 4.0f);

    if (grFillH > 0.0f) {
        const auto grFillRect = juce::Rectangle<float>(grRect.getX() + 2.0f, grRect.getY() + 2.0f, grRect.getWidth() - 4.0f, grFillH);
        g.setColour(openx::ui::OpenXLookAndFeel::AccentRed);
        g.fillRoundedRectangle(grFillRect, 2.0f);
    }

    // Meter text badges under bars
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.setColour(currentOutTp > 0.0f ? openx::ui::OpenXLookAndFeel::AccentRed : openx::ui::OpenXLookAndFeel::TextPrimary);
    const juce::String tpStr = (currentOutTp > -90.0f) ? juce::String(currentOutTp, 1) + " TP" : "-INF";
    g.drawText(tpStr, tpRect.getX() - 4.0f, tpRect.getBottom() + 2.0f, tpRect.getWidth() + 8.0f, 14.0f, juce::Justification::centred);

    g.setColour(currentGr > 0.05f ? openx::ui::OpenXLookAndFeel::AccentRed : openx::ui::OpenXLookAndFeel::TextMuted);
    const juce::String grStr = (currentGr > 0.05f) ? "-" + juce::String(currentGr, 1) + " GR" : "0.0 GR";
    g.drawText(grStr, grRect.getX() - 4.0f, grRect.getBottom() + 2.0f, grRect.getWidth() + 8.0f, 14.0f, juce::Justification::centred);

    // 3. Loudness Box (Lower Area)
    const float lufsTop = tpRect.getBottom() + 20.0f;
    const float lufsHeight = bounds.getBottom() - lufsTop - 6.0f;
    const auto lufsRect = juce::Rectangle<float>(bounds.getX() + 6.0f, lufsTop, bounds.getWidth() - 12.0f, lufsHeight);

    g.setColour(juce::Colour(0xff101317));
    g.fillRoundedRectangle(lufsRect, 3.0f);
    g.setColour(juce::Colour(0xff24292f));
    g.drawRoundedRectangle(lufsRect, 3.0f, 1.0f);

    auto drawMetricRow = [&](const juce::String& tag, float val, float yOffset, juce::Colour col) {
        g.setColour(openx::ui::OpenXLookAndFeel::TextMuted);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText(tag, lufsRect.getX() + 6.0f, yOffset, 20.0f, 16.0f, juce::Justification::left);

        g.setColour(col);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        const juce::String vStr = (val > -90.0f) ? juce::String(val, 1) + " LUFS" : "-INF";
        g.drawText(vStr, lufsRect.getX() + 26.0f, yOffset, lufsRect.getWidth() - 32.0f, 16.0f, juce::Justification::right);
    };

    drawMetricRow("M", momentaryLufs,  lufsRect.getY() + 4.0f,  openx::ui::OpenXLookAndFeel::TextPrimary);
    drawMetricRow("S", shortTermLufs,  lufsRect.getY() + 22.0f, openx::ui::OpenXLookAndFeel::AccentCyan);
    drawMetricRow("I", integratedLufs, lufsRect.getY() + 40.0f, openx::ui::OpenXLookAndFeel::AccentAmber);
}

void MasterMeterTower::resized() {
    const auto bounds = getLocalBounds();
    const int lufsTop = static_cast<int>(bounds.getHeight() * 0.52f + 44.0f);
    resetBtn.setBounds(bounds.getX() + 8, lufsTop + 62, bounds.getWidth() - 16, 20);
}

//==============================================================================
// PluginEditor Implementation
//==============================================================================
PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p), meterTower(p)
{
    setLookAndFeel(&lnf);

    addAndMakeVisible(scope);
    addAndMakeVisible(meterTower);

    // Style selector setup
    styleBox.addItem("1: Transparent", 1);
    styleBox.addItem("2: Punchy",      2);
    styleBox.addItem("3: Dynamic",     3);
    styleBox.addItem("4: Allround",    4);
    styleBox.addItem("5: Aggressive",  5);
    styleBox.addItem("6: Modern",      6);
    styleBox.addItem("7: Bus",         7);
    styleBox.addItem("8: Safe",        8);
    addAndMakeVisible(styleBox);

    styleBox.onChange = [this] { updateStyleDescription(); };

    styleDescLabel.setFont(juce::FontOptions(11.0f, juce::Font::plain));
    styleDescLabel.setColour(juce::Label::textColourId, openx::ui::OpenXLookAndFeel::AccentCyan);
    styleDescLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(styleDescLabel);
    updateStyleDescription();

    // Audition ComboBox
    auditionBox.addItem("Audition: Normal",     1);
    auditionBox.addItem("Audition: Unity Gain", 2);
    auditionBox.addItem("Audition: Delta",      3);
    addAndMakeVisible(auditionBox);

    // Toggle Buttons
    truePeakButton.setButtonText("True Peak Limiting");
    addAndMakeVisible(truePeakButton);

    dcFilterButton.setButtonText("DC Offset Filter");
    addAndMakeVisible(dcFilterButton);

    dispEnableButton.setButtonText("CO-PDN Phase");
    addAndMakeVisible(dispEnableButton);

    // Knobs setup helper
    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 62, 16);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        addAndMakeVisible(label);
    };

    setupSlider(threshSlider,    threshLabel,    "Gain / Thresh");
    setupSlider(ceilingSlider,   ceilingLabel,   "Ceiling");
    setupSlider(attackSlider,    attackLabel,    "Attack");
    setupSlider(releaseSlider,   releaseLabel,   "Release");
    setupSlider(lookaheadSlider, lookaheadLabel, "Lookahead");
    setupSlider(transLinkSlider, transLinkLabel, "Trans Link");
    setupSlider(relLinkSlider,   relLinkLabel,   "Rel Link");
    setupSlider(dispFreqSlider,  dispFreqLabel,  "CO-PDN Freq");

    // APVTS Attachments
    auto& apvts = processor.getApvts();
    styleAttach      = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "style", styleBox);
    auditionAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "audition", auditionBox);
    truePeakAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "true_peak", truePeakButton);
    dcFilterAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "dc_filter", dcFilterButton);
    dispEnableAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "disp_enable", dispEnableButton);

    threshAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "threshold", threshSlider);
    ceilingAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "ceiling", ceilingSlider);
    attackAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "attack", attackSlider);
    releaseAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "release", releaseSlider);
    lookaheadAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "lookahead", lookaheadSlider);
    transLinkAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "trans_link", transLinkSlider);
    relLinkAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "rel_link", relLinkSlider);
    dispFreqAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "disp_freq", dispFreqSlider);

    setSize(980, 600);
    startTimerHz(60);
}

PluginEditor::~PluginEditor() {
    setLookAndFeel(nullptr);
}

void PluginEditor::updateStyleDescription() {
    static constexpr const char* Descriptions[] = {
        "Clean, ultra-low distortion with transparent dual-stage release",
        "Preserves transient snap and punch for drums & rock material",
        "Crest-adaptive release preserving natural macro-dynamics",
        "Versatile, balanced general mastering limiter for all material",
        "Maximum loudness with soft-knee saturation near ceiling",
        "Fast transient recovery tailored for competitive modern masters",
        "Smooth musical glue for drum buses and instrument groups",
        "Broadcast safe, distortion-free multi-stage envelope"
    };

    const int idx = std::clamp(styleBox.getSelectedId() - 1, 0, 7);
    styleDescLabel.setText(Descriptions[idx], juce::dontSendNotification);
}

void PluginEditor::timerCallback() {
    std::array<openx::ui::ScrollingHistory<512>::Frame, 512> frames{};
    processor.getHistory().readOrdered(frames);
    scope.updateHistory(frames, static_cast<float>(ceilingSlider.getValue()), true);
    meterTower.updateMetrics();
}

void PluginEditor::paint(juce::Graphics& g) {
    g.fillAll(openx::ui::OpenXLookAndFeel::BackgroundDark);

    // Header bar
    g.setColour(openx::ui::OpenXLookAndFeel::PanelDark);
    g.fillRect(0, 0, getWidth(), 44);
    g.setColour(openx::ui::OpenXLookAndFeel::OutlineColour);
    g.drawHorizontalLine(44, 0.0f, static_cast<float>(getWidth()));

    // Title & Subtitle
    g.setColour(openx::ui::OpenXLookAndFeel::TextPrimary);
    g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    g.drawText("OPEN-X  |  LIMIT-X", 18, 7, 180, 20, juce::Justification::left);

    g.setColour(openx::ui::OpenXLookAndFeel::TextMuted);
    g.setFont(juce::FontOptions(10.5f, juce::Font::plain));
    g.drawText("True Peak Brickwall Limiter & Loudness Analyzer", 19, 25, 270, 16, juce::Justification::left);

    // Bottom controls background panel
    const auto bottomRect = juce::Rectangle<int>(12, 420, getWidth() - 24, getHeight() - 432).toFloat();
    g.setColour(openx::ui::OpenXLookAndFeel::PanelDark);
    g.fillRoundedRectangle(bottomRect, 4.0f);
    g.setColour(openx::ui::OpenXLookAndFeel::OutlineColour);
    g.drawRoundedRectangle(bottomRect, 4.0f, 1.0f);
}

void PluginEditor::resized() {
    const int w = getWidth();

    // Top Header items
    auditionBox.setBounds(w - 170, 8, 155, 26);
    truePeakButton.setBounds(w - 325, 8, 145, 26);
    dcFilterButton.setBounds(w - 450, 8, 115, 26);
    dispEnableButton.setBounds(w - 580, 8, 120, 26);

    // Center Display Area
    const int centerTop = 50;
    const int centerHeight = 360;
    const int meterWidth = 180;
    const int scopeWidth = w - 24 - meterWidth - 10;

    scope.setBounds(12, centerTop, scopeWidth, centerHeight);
    meterTower.setBounds(12 + scopeWidth + 10, centerTop, meterWidth, centerHeight);

    // Bottom Controls Area
    const int bottomY = 428;
    styleBox.setBounds(20, bottomY, 150, 24);
    styleDescLabel.setBounds(180, bottomY, w - 210, 24);

    const int knobY = bottomY + 32;
    const int knobAreaW = w - 40;
    const int knobW = knobAreaW / 8;
    const int knobH = 106;

    auto placeKnob = [&](juce::Slider& s, juce::Label& l, int col) {
        const int x = 20 + col * knobW;
        l.setBounds(x, knobY, knobW, 16);
        s.setBounds(x, knobY + 16, knobW, knobH - 16);
    };

    placeKnob(threshSlider,    threshLabel,    0);
    placeKnob(ceilingSlider,   ceilingLabel,   1);
    placeKnob(attackSlider,    attackLabel,    2);
    placeKnob(releaseSlider,   releaseLabel,   3);
    placeKnob(lookaheadSlider, lookaheadLabel, 4);
    placeKnob(transLinkSlider, transLinkLabel, 5);
    placeKnob(relLinkSlider,   relLinkLabel,   6);
    placeKnob(dispFreqSlider,  dispFreqLabel,  7);
}

} // namespace openx::limit
