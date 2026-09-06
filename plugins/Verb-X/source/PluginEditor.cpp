#include "PluginEditor.h"

namespace openx::verb {

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);

    addAndMakeVisible(decayCurve);

    setupSlider(decaySlider,     decayLabel,     "Decay Time",   " s");
    setupSlider(spaceSlider,     spaceLabel,     "Space",        " x");
    setupSlider(predelaySlider,  predelayLabel,  "Pre-Delay",    " ms");
    setupSlider(distanceSlider,  distanceLabel,  "Distance",     " %");
    setupSlider(diffusionSlider, diffusionLabel, "Diffusion",    " %");
    setupSlider(widthSlider,      widthLabel,      "Stereo Width", " %");
    setupSlider(dampingSlider,   dampingLabel,   "High Damping", " Hz");
    setupSlider(lowCutSlider,    lowCutLabel,    "Low Cut",      " Hz");
    setupSlider(duckingSlider,   duckingLabel,   "Ducking",      " %");
    setupSlider(chaosSlider,     chaosLabel,     "Chaos Mod",    "");
    setupSlider(mixSlider,       mixLabel,       "Dry / Wet",    " %");

    predelaySyncToggle.setButtonText("Host Sync");
    predelaySyncToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffc9d1d9));
    addAndMakeVisible(predelaySyncToggle);

    predelayNoteBox.addItem("1/32",  1);
    predelayNoteBox.addItem("1/16",  2);
    predelayNoteBox.addItem("1/8 T", 3);
    predelayNoteBox.addItem("1/16 D",4);
    predelayNoteBox.addItem("1/8",   5);
    predelayNoteBox.addItem("1/4 T", 6);
    predelayNoteBox.addItem("1/8 D", 7);
    predelayNoteBox.addItem("1/4",   8);
    addAndMakeVisible(predelayNoteBox);

    auto& apvts = processor.getApvts();
    decayAttach        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "decay", decaySlider);
    spaceAttach        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "space", spaceSlider);
    predelayAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "predelay", predelaySlider);
    predelaySyncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "predelay_sync", predelaySyncToggle);
    predelayNoteAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "predelay_note", predelayNoteBox);
    distanceAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "distance", distanceSlider);
    diffusionAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "diffusion", diffusionSlider);
    widthAttach        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "width", widthSlider);
    dampingAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "damping", dampingSlider);
    lowCutAttach       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "low_cut", lowCutSlider);
    duckingAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "ducking", duckingSlider);
    chaosAttach        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "chaos", chaosSlider);
    mixAttach          = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mix", mixSlider);

    // Bidirectional interactive decay curve node binding
    decayCurve.onLowDecayChanged = [&apvts](float freq, float mult) {
        if (auto* pF = apvts.getParameter("decay_low_freq"))
            pF->setValueNotifyingHost(pF->getNormalisableRange().convertTo0to1(freq));
        if (auto* pM = apvts.getParameter("decay_low"))
            pM->setValueNotifyingHost(pM->getNormalisableRange().convertTo0to1(mult));
    };

    decayCurve.onMidDecayChanged = [&apvts](float freq, float mult, float q) {
        if (auto* pF = apvts.getParameter("decay_mid_freq"))
            pF->setValueNotifyingHost(pF->getNormalisableRange().convertTo0to1(freq));
        if (auto* pM = apvts.getParameter("decay_mid"))
            pM->setValueNotifyingHost(pM->getNormalisableRange().convertTo0to1(mult));
        if (auto* pQ = apvts.getParameter("decay_mid_q"))
            pQ->setValueNotifyingHost(pQ->getNormalisableRange().convertTo0to1(q));
    };

    decayCurve.onHighDecayChanged = [&apvts](float freq, float mult) {
        if (auto* pF = apvts.getParameter("decay_high_freq"))
            pF->setValueNotifyingHost(pF->getNormalisableRange().convertTo0to1(freq));
        if (auto* pM = apvts.getParameter("decay_high"))
            pM->setValueNotifyingHost(pM->getNormalisableRange().convertTo0to1(mult));
    };

    decayCurve.onDampingChanged = [&apvts](float freq) {
        if (auto* pD = apvts.getParameter("damping"))
            pD->setValueNotifyingHost(pD->getNormalisableRange().convertTo0to1(freq));
    };

    decayCurve.onLowCutChanged = [&apvts](float freq) {
        if (auto* pL = apvts.getParameter("low_cut"))
            pL->setValueNotifyingHost(pL->getNormalisableRange().convertTo0to1(freq));
    };

    decayCurve.onGestureStarted = [&apvts](openx::ui::ReverbDecayCurve::DragNode node) {
        switch (node) {
            case openx::ui::ReverbDecayCurve::DragNode::LowDecay:
                if (auto* p = apvts.getParameter("decay_low_freq")) p->beginChangeGesture();
                if (auto* p = apvts.getParameter("decay_low")) p->beginChangeGesture();
                break;
            case openx::ui::ReverbDecayCurve::DragNode::MidDecay:
                if (auto* p = apvts.getParameter("decay_mid_freq")) p->beginChangeGesture();
                if (auto* p = apvts.getParameter("decay_mid")) p->beginChangeGesture();
                if (auto* p = apvts.getParameter("decay_mid_q")) p->beginChangeGesture();
                break;
            case openx::ui::ReverbDecayCurve::DragNode::HighDecay:
                if (auto* p = apvts.getParameter("decay_high_freq")) p->beginChangeGesture();
                if (auto* p = apvts.getParameter("decay_high")) p->beginChangeGesture();
                break;
            case openx::ui::ReverbDecayCurve::DragNode::HighDamping:
                if (auto* p = apvts.getParameter("damping")) p->beginChangeGesture();
                break;
            case openx::ui::ReverbDecayCurve::DragNode::LowCut:
                if (auto* p = apvts.getParameter("low_cut")) p->beginChangeGesture();
                break;
            case openx::ui::ReverbDecayCurve::DragNode::None:
                break;
        }
    };

    decayCurve.onGestureEnded = [&apvts](openx::ui::ReverbDecayCurve::DragNode node) {
        switch (node) {
            case openx::ui::ReverbDecayCurve::DragNode::LowDecay:
                if (auto* p = apvts.getParameter("decay_low_freq")) p->endChangeGesture();
                if (auto* p = apvts.getParameter("decay_low")) p->endChangeGesture();
                break;
            case openx::ui::ReverbDecayCurve::DragNode::MidDecay:
                if (auto* p = apvts.getParameter("decay_mid_freq")) p->endChangeGesture();
                if (auto* p = apvts.getParameter("decay_mid")) p->endChangeGesture();
                if (auto* p = apvts.getParameter("decay_mid_q")) p->endChangeGesture();
                break;
            case openx::ui::ReverbDecayCurve::DragNode::HighDecay:
                if (auto* p = apvts.getParameter("decay_high_freq")) p->endChangeGesture();
                if (auto* p = apvts.getParameter("decay_high")) p->endChangeGesture();
                break;
            case openx::ui::ReverbDecayCurve::DragNode::HighDamping:
                if (auto* p = apvts.getParameter("damping")) p->endChangeGesture();
                break;
            case openx::ui::ReverbDecayCurve::DragNode::LowCut:
                if (auto* p = apvts.getParameter("low_cut")) p->endChangeGesture();
                break;
            case openx::ui::ReverbDecayCurve::DragNode::None:
                break;
        }
    };

    // Percentage value text formatters for 0..1 and 0..2 normalized parameters
    auto setupPercentSlider = [](juce::Slider& s) {
        s.textFromValueFunction = [](double val) {
            return juce::String(static_cast<int>(std::round(val * 100.0))) + " %";
        };
        s.valueFromTextFunction = [](const juce::String& text) {
            return text.getDoubleValue() * 0.01;
        };
    };
    setupPercentSlider(distanceSlider);
    setupPercentSlider(diffusionSlider);
    setupPercentSlider(widthSlider);
    setupPercentSlider(duckingSlider);
    setupPercentSlider(chaosSlider);
    setupPercentSlider(mixSlider);

    decaySlider.textFromValueFunction = [](double val) { return juce::String(val, 2) + " s"; };
    spaceSlider.textFromValueFunction = [](double val) { return juce::String(val, 2) + " x"; };
    predelaySlider.textFromValueFunction = [](double val) { return juce::String(val, 1) + " ms"; };

    setSize(980, 640);
    startTimerHz(60);
}

PluginEditor::~PluginEditor() {
    setLookAndFeel(nullptr);
}

void PluginEditor::setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& text, const juce::String& suffix) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
    if (suffix.isNotEmpty()) {
        slider.setTextValueSuffix(suffix);
    }
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    addAndMakeVisible(label);
}

void PluginEditor::timerCallback() {
    auto& analyzer = processor.getSpectrumAnalyzer();
    const float sr = static_cast<float>(processor.getSampleRate());
    analyzer.update(sr > 0 ? sr : 48000.0f);

    auto scopeData = analyzer.getScopeData();
    decayCurve.updateSpectrum(scopeData.data(), scopeData.size(), sr > 0 ? sr : 48000.0f);

    const auto& params = processor.getParams();
    openx::ui::ReverbDecayCurve::CurveState state;
    state.baseDecayTime = params.get(ParamId::DecayTime);
    state.dampingHz = params.get(ParamId::Damping);
    state.lowCutHz = params.get(ParamId::LowCut);
    state.decayRateLow = params.get(ParamId::DecayLow);
    state.decayRateLowFreq = params.get(ParamId::DecayLowFreq);
    state.decayRateMid = params.get(ParamId::DecayMid);
    state.decayRateMidFreq = params.get(ParamId::DecayMidFreq);
    state.decayRateMidQ = params.get(ParamId::DecayMidQ);
    state.decayRateHigh = params.get(ParamId::DecayHigh);
    state.decayRateHighFreq = params.get(ParamId::DecayHighFreq);

    decayCurve.setCurveState(state);
}

void PluginEditor::paint(juce::Graphics& g) {
    g.fillAll(openx::ui::OpenXLookAndFeel::BackgroundDark);

    // Header Title
    g.setColour(openx::ui::OpenXLookAndFeel::TextPrimary);
    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.drawText("OPEN-X  |  VERB-X", 20, 10, 200, 30, juce::Justification::left);

    // Subtitle & Architecture descriptor
    g.setColour(openx::ui::OpenXLookAndFeel::TextMuted);
    g.setFont(juce::FontOptions(12.0f, juce::Font::plain));
    g.drawText("16-Channel Symplectic Algorithmic Reverb — Pro-R 2 Architecture", 225, 12, 450, 30, juce::Justification::left);

    // Section Titles in Bottom Control Bar
    auto area = getLocalBounds().reduced(16);
    auto bottomPanel = area.removeFromBottom(220);
    const int sectionWidth = (bottomPanel.getWidth() - 16) / 3;

    auto drawSectionBox = [&](juce::Rectangle<int> rect, const juce::String& title) {
        g.setColour(juce::Colour(0xff161b22));
        g.fillRoundedRectangle(rect.toFloat(), 6.0f);
        g.setColour(juce::Colour(0xff30363d));
        g.drawRoundedRectangle(rect.toFloat(), 6.0f, 1.0f);

        g.setColour(openx::ui::OpenXLookAndFeel::TextMuted);
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        g.drawText(title, rect.getX() + 10, rect.getY() + 4, rect.getWidth() - 20, 16, juce::Justification::left);
    };

    auto sec1 = bottomPanel.removeFromLeft(sectionWidth);
    bottomPanel.removeFromLeft(8);
    auto sec2 = bottomPanel.removeFromLeft(sectionWidth);
    bottomPanel.removeFromLeft(8);
    auto sec3 = bottomPanel;

    drawSectionBox(sec1, "TIME & SPACE");
    drawSectionBox(sec2, "ACOUSTICS & CHARACTER");
    drawSectionBox(sec3, "DAMPING & OUTPUT");
}

void PluginEditor::resized() {
    auto area = getLocalBounds().reduced(16);
    area.removeFromTop(32);

    auto bottomPanel = area.removeFromBottom(220);
    decayCurve.setBounds(area.reduced(0, 4));

    const int sectionWidth = (bottomPanel.getWidth() - 16) / 3;
    auto sec1 = bottomPanel.removeFromLeft(sectionWidth).reduced(6, 4);
    sec1.removeFromTop(18);
    bottomPanel.removeFromLeft(8);

    auto sec2 = bottomPanel.removeFromLeft(sectionWidth).reduced(6, 4);
    sec2.removeFromTop(18);
    bottomPanel.removeFromLeft(8);

    auto sec3 = bottomPanel.reduced(6, 4);
    sec3.removeFromTop(18);

    auto placeKnob = [](juce::Slider& s, juce::Label& l, juce::Rectangle<int> r) {
        l.setBounds(r.removeFromTop(18));
        s.setBounds(r);
    };

    // 1. Time & Space Section
    {
        auto row1 = sec1.removeFromTop(sec1.getHeight() / 2);
        auto row2 = sec1;

        const int colW = row1.getWidth() / 2;
        placeKnob(spaceSlider, spaceLabel, row1.removeFromLeft(colW).reduced(4));
        placeKnob(decaySlider, decayLabel, row1.reduced(4));

        auto leftR2 = row2.removeFromLeft(colW).reduced(4);
        placeKnob(predelaySlider, predelayLabel, leftR2);

        auto rightR2 = row2.reduced(4);
        rightR2.removeFromTop(8);
        predelaySyncToggle.setBounds(rightR2.removeFromTop(24));
        predelayNoteBox.setBounds(rightR2.removeFromTop(26).reduced(2, 0));
    }

    // 2. Acoustics & Character Section
    {
        auto row1 = sec2.removeFromTop(sec2.getHeight() / 2);
        auto row2 = sec2;

        const int colW = row1.getWidth() / 2;
        placeKnob(distanceSlider, distanceLabel, row1.removeFromLeft(colW).reduced(4));
        placeKnob(diffusionSlider, diffusionLabel, row1.reduced(4));

        placeKnob(chaosSlider, chaosLabel, row2.removeFromLeft(colW).reduced(4));
        placeKnob(widthSlider, widthLabel, row2.reduced(4));
    }

    // 3. Damping & Output Section
    {
        auto row1 = sec3.removeFromTop(sec3.getHeight() / 2);
        auto row2 = sec3;

        const int colW = row1.getWidth() / 2;
        placeKnob(lowCutSlider, lowCutLabel, row1.removeFromLeft(colW).reduced(4));
        placeKnob(dampingSlider, dampingLabel, row1.reduced(4));

        placeKnob(duckingSlider, duckingLabel, row2.removeFromLeft(colW).reduced(4));
        placeKnob(mixSlider, mixLabel, row2.reduced(4));
    }
}

} // namespace openx::verb
