#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace openx::ui {

class OpenXLookAndFeel : public juce::LookAndFeel_V4 {
public:
    static inline const juce::Colour BackgroundDark{ 0xff121418 };
    static inline const juce::Colour PanelDark     { 0xff1c2128 };
    static inline const juce::Colour OutlineColour { 0xff2d333b };
    static inline const juce::Colour TextPrimary   { 0xffe6edf3 };
    static inline const juce::Colour TextMuted     { 0xff8b949e };
    static inline const juce::Colour AccentCyan    { 0xff00f0ff };
    static inline const juce::Colour AccentAmber   { 0xffffab00 };
    static inline const juce::Colour AccentRed     { 0xffff1744 };

    OpenXLookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId, BackgroundDark);
        setColour(juce::Slider::thumbColourId, AccentCyan);
        setColour(juce::Slider::rotarySliderFillColourId, AccentCyan);
        setColour(juce::Slider::rotarySliderOutlineColourId, OutlineColour);
        setColour(juce::Label::textColourId, TextPrimary);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& /*slider*/) override
    {
        const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        const auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        const auto center = bounds.getCentre();
        const auto lineW = juce::jmin(4.0f, radius * 0.15f);
        const auto arcRadius = radius - lineW * 0.5f;

        // Background track
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(OutlineColour);
        g.strokePath(backgroundArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active value arc
        if (sliderPosProportional > 0.0f) {
            juce::Path valueArc;
            valueArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
            g.setColour(AccentCyan);
            g.strokePath(valueArc, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Center dial
        const auto dialRadius = arcRadius - lineW * 1.5f;
        g.setColour(PanelDark);
        g.fillEllipse(center.x - dialRadius, center.y - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f);
        g.setColour(OutlineColour);
        g.drawEllipse(center.x - dialRadius, center.y - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f, 1.0f);

        // Pointer
        juce::Path p;
        const auto pointerLength = dialRadius * 0.7f;
        p.addRectangle(-1.2f, -dialRadius, 2.4f, pointerLength);
        p.applyTransform(juce::AffineTransform::rotation(toAngle).translated(center.x, center.y));
        g.setColour(TextPrimary);
        g.fillPath(p);
    }
};

} // namespace openx::ui
