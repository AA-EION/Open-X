#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/scrolling_history.hpp"
#include <algorithm>
#include <cmath>

namespace openx::ui {

class DynamicsScope : public juce::Component {
public:
    DynamicsScope() = default;

    void updateHistory(const std::array<ScrollingHistory<512>::Frame, 512>& historyFrames) noexcept {
        frames = historyFrames;
        isGateMode = false;
        repaint();
    }

    void updateHistory(const std::array<ScrollingHistory<512>::Frame, 512>& historyFrames,
                       float currentThresholdDb,
                       float currentKneeDb,
                       bool audition = false) noexcept
    {
        frames = historyFrames;
        thresholdDb = currentThresholdDb;
        kneeDb = currentKneeDb;
        auditionActive = audition;
        isGateMode = false;
        isLimiter = false;
        repaint();
    }

    void updateHistory(const std::array<ScrollingHistory<512>::Frame, 512>& historyFrames,
                       float currentCeilingDb,
                       bool limiterMode) noexcept
    {
        frames = historyFrames;
        thresholdDb = currentCeilingDb;
        kneeDb = 0.0f;
        isLimiter = limiterMode;
        isGateMode = false;
        auditionActive = false;
        repaint();
    }

    void updateHistory(const std::array<ScrollingHistory<512>::Frame, 512>& historyFrames,
                       float currentOpenThresholdDb,
                       float currentCloseThresholdDb,
                       int currentGateState,
                       int currentMode,
                       bool audition = false,
                       float currentKneeDb = 0.0f) noexcept
    {
        frames = historyFrames;
        thresholdDb = currentOpenThresholdDb;
        closeThresholdDb = currentCloseThresholdDb;
        gateState = currentGateState;
        mode = currentMode;
        auditionActive = audition;
        kneeDb = currentKneeDb;
        isGateMode = true;
        repaint();
    }

    void setLimiterMode(bool enabled) noexcept {
        isLimiter = enabled;
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        if (bounds.getWidth() <= 10.0f || bounds.getHeight() <= 10.0f)
            return;

        // 1. Dark vignette background
        juce::ColourGradient bgGradient(
            juce::Colour(0xff101318), bounds.getCentreX(), bounds.getY(),
            juce::Colour(0xff161a22), bounds.getCentreX(), bounds.getBottom(),
            false
        );
        g.setGradientFill(bgGradient);
        g.fillRoundedRectangle(bounds, 6.0f);

        // Scope inner display area with padding for axis labels
        constexpr float leftMargin = 38.0f;
        constexpr float rightMargin = 34.0f;
        constexpr float topMargin = 8.0f;
        constexpr float bottomMargin = 12.0f;

        const auto plotArea = bounds.withTrimmedLeft(leftMargin)
                                    .withTrimmedRight(rightMargin)
                                    .withTrimmedTop(topMargin)
                                    .withTrimmedBottom(bottomMargin);

        // 2. Horizontal decibel grid lines and scale labels
        drawGrid(g, plotArea, bounds);

        if (isGateMode) {
            // Dual-Threshold Hysteresis Zone and Guides
            drawGateThresholds(g, plotArea);
            if (mode == 2 && kneeDb > 0.5f) {
                drawKneeBand(g, plotArea);
            }
        } else {
            // Knee range visualization (if soft knee active)
            if (kneeDb > 0.5f) {
                drawKneeBand(g, plotArea);
            }
            // Animated threshold line
            drawThreshold(g, plotArea);
        }

        // Gain Reduction ribbon descending from ceiling
        drawGainReduction(g, plotArea);

        // Input & Output waveform envelopes
        drawWaveforms(g, plotArea);

        if (isGateMode) {
            // Gate Status Badge (Open, Hold, Closed, Ducking, Audition)
            drawGateStatusBadge(g, plotArea);
            // Readout HUD pill (In, Out, GR)
            drawHud(g, plotArea);
        } else {
            // Readout HUD pill (In, Out, GR)
            drawHud(g, plotArea);
        }

        // Scope border
        g.setColour(juce::Colour(0xff2d333b));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
    }

private:
    std::array<ScrollingHistory<512>::Frame, 512> frames{};
    float thresholdDb{-20.0f};
    float closeThresholdDb{-30.0f};
    float kneeDb{0.0f};
    int gateState{0};
    int mode{0};
    bool isGateMode{false};
    bool isLimiter{false};
    bool auditionActive{false};

    [[nodiscard]] static float dbToY(float db, const juce::Rectangle<float>& bounds) noexcept {
        constexpr float minDb = -60.0f;
        constexpr float maxDb = 0.0f;
        const float norm = (std::clamp(db, minDb, maxDb) - minDb) / (maxDb - minDb);
        return bounds.getBottom() - norm * bounds.getHeight();
    }

    void drawGrid(juce::Graphics& g, const juce::Rectangle<float>& plotArea, const juce::Rectangle<float>& fullBounds) const noexcept {
        g.setFont(juce::FontOptions(10.0f, juce::Font::plain));

        static constexpr float Dbs[] = { 0.0f, -6.0f, -12.0f, -18.0f, -24.0f, -36.0f, -48.0f, -60.0f };
        for (float db : Dbs) {
            const float y = dbToY(db, plotArea);

            // Grid line
            g.setColour(db == 0.0f ? juce::Colour(0x4430363d) : juce::Colour(0x2230363d));
            g.drawHorizontalLine(static_cast<int>(y), plotArea.getX(), plotArea.getRight());

            // Left axis labels (Signal level dBFS)
            g.setColour(juce::Colour(0x888b949e));
            const juce::String labelStr = juce::String(static_cast<int>(db));
            g.drawText(labelStr,
                       juce::Rectangle<float>(fullBounds.getX(), y - 7.0f, plotArea.getX() - fullBounds.getX() - 4.0f, 14.0f),
                       juce::Justification::centredRight, false);
        }

        // Right axis labels (Gain Reduction dB)
        if (isGateMode) {
            static constexpr float GateGrDbs[] = { -6.0f, -12.0f, -24.0f, -48.0f, -72.0f };
            g.setColour(juce::Colour(0x99ff5252));
            for (float grDb : GateGrDbs) {
                const float grNorm = std::clamp(std::abs(grDb) / 80.0f, 0.0f, 1.0f);
                const float y = plotArea.getY() + grNorm * (plotArea.getHeight() * 0.75f);
                const juce::String grStr = juce::String(static_cast<int>(grDb));
                g.drawText(grStr,
                           juce::Rectangle<float>(plotArea.getRight() + 4.0f, y - 7.0f, fullBounds.getRight() - plotArea.getRight() - 6.0f, 14.0f),
                           juce::Justification::centredLeft, false);
            }
        } else {
            static constexpr float GrDbs[] = { -3.0f, -6.0f, -12.0f, -18.0f, -24.0f };
            g.setColour(juce::Colour(0x99ff5252));
            for (float grDb : GrDbs) {
                const float grNorm = std::clamp(std::abs(grDb) / 24.0f, 0.0f, 1.0f);
                const float y = plotArea.getY() + grNorm * (plotArea.getHeight() * 0.65f);
                const juce::String grStr = juce::String(static_cast<int>(grDb));
                g.drawText(grStr,
                           juce::Rectangle<float>(plotArea.getRight() + 4.0f, y - 7.0f, fullBounds.getRight() - plotArea.getRight() - 6.0f, 14.0f),
                           juce::Justification::centredLeft, false);
            }
        }
    }

    void drawGateThresholds(juce::Graphics& g, const juce::Rectangle<float>& plotArea) const noexcept {
        const float effectiveClose = std::min(closeThresholdDb, thresholdDb);
        const float openY = dbToY(thresholdDb, plotArea);
        const float closeY = dbToY(effectiveClose, plotArea);

        // 1. Shaded Hysteresis Band between Close and Open threshold
        const float topY = std::min(openY, closeY);
        const float bandH = std::abs(closeY - openY);
        if (bandH > 1.0f) {
            g.setColour(juce::Colour(0x1800f0ff));
            g.fillRect(plotArea.getX(), topY, plotArea.getWidth(), bandH);
        }

        // 2. Open Threshold Line (Neon Cyan / Emerald)
        static constexpr float openDash[] = { 6.0f, 4.0f };
        g.setColour(juce::Colour(0xd000f0ff));
        g.drawDashedLine(juce::Line<float>(plotArea.getX(), openY, plotArea.getRight(), openY), openDash, 2, 1.3f);

        const float badgeW = 68.0f;
        const float badgeH = 15.0f;
        const auto openBadge = juce::Rectangle<float>(plotArea.getRight() - badgeW - 2.0f, openY - badgeH - 2.0f, badgeW, badgeH);

        g.setColour(juce::Colour(0xcc1c2128));
        g.fillRoundedRectangle(openBadge, 3.0f);
        g.setColour(juce::Colour(0xcc00f0ff));
        g.drawRoundedRectangle(openBadge, 3.0f, 1.0f);

        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText("OPEN " + juce::String(thresholdDb, 0) + " dB", openBadge, juce::Justification::centred, false);

        // 3. Close Threshold Line (Warm Amber)
        static constexpr float closeDash[] = { 3.0f, 3.0f };
        g.setColour(juce::Colour(0xd0ffab00));
        g.drawDashedLine(juce::Line<float>(plotArea.getX(), closeY, plotArea.getRight(), closeY), closeDash, 2, 1.1f);

        const auto closeBadge = juce::Rectangle<float>(plotArea.getRight() - badgeW - 2.0f, closeY + 2.0f, badgeW, badgeH);

        g.setColour(juce::Colour(0xcc1c2128));
        g.fillRoundedRectangle(closeBadge, 3.0f);
        g.setColour(juce::Colour(0xccffab00));
        g.drawRoundedRectangle(closeBadge, 3.0f, 1.0f);

        g.drawText("CLOSE " + juce::String(effectiveClose, 0) + " dB", closeBadge, juce::Justification::centred, false);
    }

    void drawKneeBand(juce::Graphics& g, const juce::Rectangle<float>& plotArea) const noexcept {
        const float halfKnee = kneeDb * 0.5f;
        const float topY = dbToY(thresholdDb + halfKnee, plotArea);
        const float botY = dbToY(thresholdDb - halfKnee, plotArea);
        const float height = std::max(botY - topY, 1.0f);

        g.setColour(juce::Colour(0x14ffab00));
        g.fillRect(plotArea.getX(), topY, plotArea.getWidth(), height);

        g.setColour(juce::Colour(0x33ffab00));
        static constexpr float kneeDash[] = { 2.0f, 4.0f };
        g.drawDashedLine(juce::Line<float>(plotArea.getX(), topY, plotArea.getRight(), topY), kneeDash, 2, 0.8f);
        g.drawDashedLine(juce::Line<float>(plotArea.getX(), botY, plotArea.getRight(), botY), kneeDash, 2, 0.8f);
    }

    void drawThreshold(juce::Graphics& g, const juce::Rectangle<float>& plotArea) const noexcept {
        const float threshY = dbToY(thresholdDb, plotArea);

        const float recentGr = std::abs(frames[511].gainReductionDb);
        const bool isCompressing = (recentGr > 0.1f);

        if (isCompressing) {
            g.setColour(juce::Colour(0x33ffc107));
            g.drawLine(plotArea.getX(), threshY, plotArea.getRight(), threshY, 3.5f);
            g.setColour(juce::Colour(0xffffc107));
        } else {
            g.setColour(juce::Colour(0x88ffab00));
        }

        static constexpr float dashPattern[] = { 6.0f, 4.0f };
        g.drawDashedLine(juce::Line<float>(plotArea.getX(), threshY, plotArea.getRight(), threshY),
                         dashPattern, 2, isCompressing ? 1.5f : 1.0f);

        const float badgeW = isLimiter ? 56.0f : 34.0f;
        const float badgeH = 15.0f;
        const auto badgeRect = juce::Rectangle<float>(plotArea.getX() + 2.0f, threshY - badgeH * 0.5f, badgeW, badgeH);

        g.setColour(juce::Colour(0xcc1c2128));
        g.fillRoundedRectangle(badgeRect, 3.0f);
        const auto strokeCol = isCompressing ? (isLimiter ? juce::Colour(0xffff385c) : juce::Colour(0xffffc107))
                                             : (isLimiter ? juce::Colour(0xccff5252) : juce::Colour(0xccffab00));
        g.setColour(strokeCol);
        g.drawRoundedRectangle(badgeRect, 3.0f, 1.0f);

        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        const juce::String badgeText = isLimiter
            ? "CEIL " + juce::String(thresholdDb, 1) + " dB"
            : juce::String(thresholdDb, 0) + " dB";
        g.drawText(badgeText, badgeRect, juce::Justification::centred, false);
    }

    void drawGainReduction(juce::Graphics& g, const juce::Rectangle<float>& plotArea) const noexcept {
        juce::Path grFill;
        juce::Path grContour;
        const float dx = plotArea.getWidth() / 512.0f;
        bool started = false;

        const float ceilingY = isLimiter ? dbToY(thresholdDb, plotArea) : plotArea.getY();
        const float maxGrSpan = plotArea.getHeight() * (isGateMode ? 0.75f : 0.65f);
        const float maxGrScale = isGateMode ? 80.0f : 24.0f;

        for (size_t i = 0; i < 512; ++i) {
            const float x = plotArea.getX() + static_cast<float>(i) * dx;
            const float grNorm = std::clamp(std::abs(frames[i].gainReductionDb) / maxGrScale, 0.0f, 1.0f);
            const float y = ceilingY + grNorm * maxGrSpan;

            if (!started) {
                grFill.startNewSubPath(x, ceilingY);
                grFill.lineTo(x, y);
                grContour.startNewSubPath(x, y);
                started = true;
            } else {
                grFill.lineTo(x, y);
                grContour.lineTo(x, y);
            }
        }

        grFill.lineTo(plotArea.getRight(), ceilingY);
        grFill.closeSubPath();

        juce::ColourGradient grGradient(
            juce::Colour(0x33ff9100), plotArea.getX(), ceilingY,
            juce::Colour(0x88ff1744), plotArea.getX(), ceilingY + maxGrSpan,
            false
        );
        g.setGradientFill(grGradient);
        g.fillPath(grFill);

        g.setColour(juce::Colour(0xffff385c));
        g.strokePath(grContour, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawWaveforms(juce::Graphics& g, const juce::Rectangle<float>& plotArea) const noexcept {
        juce::Path inPath;
        juce::Path outPath;
        juce::Path outFill;
        const float dx = plotArea.getWidth() / 512.0f;
        const float bottomY = plotArea.getBottom();

        for (size_t i = 0; i < 512; ++i) {
            const float x = plotArea.getX() + static_cast<float>(i) * dx;
            const float inY = dbToY(frames[i].inputDb, plotArea);
            const float outY = dbToY(frames[i].outputDb, plotArea);

            if (i == 0) {
                inPath.startNewSubPath(x, inY);
                outPath.startNewSubPath(x, outY);
                outFill.startNewSubPath(x, bottomY);
                outFill.lineTo(x, outY);
            } else {
                inPath.lineTo(x, inY);
                outPath.lineTo(x, outY);
                outFill.lineTo(x, outY);
            }
        }

        outFill.lineTo(plotArea.getRight(), bottomY);
        outFill.closeSubPath();

        // Subtle electric cyan glow underneath output waveform
        juce::Colour outGlow = auditionActive ? juce::Colour(0xffffab00) : juce::Colour(0xff00f0ff);
        juce::ColourGradient outFillGradient(
            outGlow.withAlpha(0.12f), plotArea.getX(), plotArea.getY(),
            outGlow.withAlpha(0.01f), plotArea.getX(), bottomY,
            false
        );
        g.setGradientFill(outFillGradient);
        g.fillPath(outFill);

        // Input waveform: Sleek slate grey
        g.setColour(juce::Colour(0x558b949e));
        g.strokePath(inPath, juce::PathStrokeType(1.2f));

        // Output waveform: Radiant cyan or amber audition
        g.setColour(outGlow);
        g.strokePath(outPath, juce::PathStrokeType(1.8f));
    }

    void drawGateStatusBadge(juce::Graphics& g, const juce::Rectangle<float>& plotArea) const noexcept {
        juce::String statusText = "GATE CLOSED";
        juce::Colour bgCol = juce::Colour(0xd01c2128);
        juce::Colour borderCol = juce::Colour(0x448b949e);
        juce::Colour dotCol = juce::Colour(0xff8b949e);

        if (auditionActive) {
            statusText = "SIDECHAIN AUDITION";
            borderCol = juce::Colour(0xffffab00);
            dotCol = juce::Colour(0xffffab00);
        } else if (mode == 1) { // Duck
            if (gateState == 3 || gateState == 2) {
                statusText = "DUCKING ACTIVE";
                borderCol = juce::Colour(0xffff1744);
                dotCol = juce::Colour(0xffff5252);
            } else {
                statusText = "DUCK PASSTHROUGH";
                borderCol = juce::Colour(0xff00f0ff);
                dotCol = juce::Colour(0xff00f0ff);
            }
        } else if (mode == 2) { // Expander
            if (gateState == 1) {
                statusText = "EXPANDER PASS";
                borderCol = juce::Colour(0xff38ef7d);
                dotCol = juce::Colour(0xff38ef7d);
            } else if (gateState == 2) {
                statusText = "EXPANDER HOLD";
                borderCol = juce::Colour(0xffffab00);
                dotCol = juce::Colour(0xffffab00);
            } else {
                statusText = "EXPANDING";
                borderCol = juce::Colour(0xff00f0ff);
                dotCol = juce::Colour(0xff00f0ff);
            }
        } else { // Gate
            if (gateState == 1) {
                statusText = "GATE OPEN";
                borderCol = juce::Colour(0xff38ef7d);
                dotCol = juce::Colour(0xff38ef7d);
            } else if (gateState == 2) {
                statusText = "GATE HOLD";
                borderCol = juce::Colour(0xffffab00);
                dotCol = juce::Colour(0xffffab00);
            } else {
                statusText = "GATE CLOSED";
                borderCol = juce::Colour(0x668b949e);
                dotCol = juce::Colour(0xff8b949e);
            }
        }

        constexpr float hudW = 150.0f;
        constexpr float hudH = 22.0f;
        const auto hudRect = juce::Rectangle<float>(plotArea.getX() + 6.0f, plotArea.getY() + 4.0f, hudW, hudH);

        g.setColour(bgCol);
        g.fillRoundedRectangle(hudRect, 4.0f);
        g.setColour(borderCol);
        g.drawRoundedRectangle(hudRect, 4.0f, 1.0f);

        // Glowing indicator dot
        g.setColour(dotCol);
        g.fillEllipse(hudRect.getX() + 8.0f, hudRect.getY() + 7.0f, 8.0f, 8.0f);

        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xffe6edf3));
        g.drawText(statusText, hudRect.withTrimmedLeft(22.0f), juce::Justification::centredLeft, false);
    }

    void drawHud(juce::Graphics& g, const juce::Rectangle<float>& plotArea) const noexcept {
        const auto& latest = frames[511];

        const float inDb = std::clamp(latest.inputDb, -100.0f, 12.0f);
        const float outDb = std::clamp(latest.outputDb, -100.0f, 12.0f);
        const float grDb = -std::abs(latest.gainReductionDb);

        constexpr float hudW = 220.0f;
        constexpr float hudH = 22.0f;
        const auto hudRect = juce::Rectangle<float>(plotArea.getRight() - hudW - 4.0f, plotArea.getY() + 4.0f, hudW, hudH);

        g.setColour(juce::Colour(0xd0161a22));
        g.fillRoundedRectangle(hudRect, 4.0f);
        g.setColour(juce::Colour(0x4430363d));
        g.drawRoundedRectangle(hudRect, 4.0f, 1.0f);

        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));

        const float partW = hudW / 3.0f;
        const auto rIn = juce::Rectangle<float>(hudRect.getX(), hudRect.getY(), partW, hudH);
        const auto rOut = juce::Rectangle<float>(hudRect.getX() + partW, hudRect.getY(), partW, hudH);
        const auto rGr = juce::Rectangle<float>(hudRect.getX() + partW * 2.0f, hudRect.getY(), partW, hudH);

        // IN
        g.setColour(juce::Colour(0xff8b949e));
        g.drawText("IN " + juce::String(inDb, 1), rIn, juce::Justification::centred, false);

        // OUT
        g.setColour(juce::Colour(0xff00f0ff));
        g.drawText("OUT " + juce::String(outDb, 1), rOut, juce::Justification::centred, false);

        // GR
        g.setColour(juce::Colour(0xffff5252));
        g.drawText("GR " + juce::String(grDb, 1), rGr, juce::Justification::centred, false);
    }
};

} // namespace openx::ui
