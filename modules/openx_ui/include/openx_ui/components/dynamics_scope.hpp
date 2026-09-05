#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/scrolling_history.hpp"

namespace openx::ui {

class DynamicsScope : public juce::Component {
public:
    DynamicsScope() = default;

    void updateHistory(const std::array<ScrollingHistory<512>::Frame, 512>& historyFrames,
                       float currentThresholdDb = -20.0f) noexcept
    {
        frames = historyFrames;
        thresholdDb = currentThresholdDb;
        repaint();
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        g.fillAll(juce::Colour(0xff121418));

        // 1. Grid lines (-30, -20, -10, 0 dB)
        drawGrid(g, bounds);

        // 2. Threshold guide
        const float threshY = dbToY(thresholdDb, bounds);
        g.setColour(juce::Colour(0x66ffab00));
        static constexpr float dashPattern[] = { 4.0f, 4.0f };
        g.drawDashedLine(juce::Line<float>(bounds.getX(), threshY, bounds.getRight(), threshY),
                         dashPattern, 2, 1.0f);

        // 3. Gain Reduction fill ribbon (Amber/Red from ceiling down)
        drawGainReduction(g, bounds);

        // 4. Input & Output waveform envelopes
        drawWaveforms(g, bounds);
    }

private:
    std::array<ScrollingHistory<512>::Frame, 512> frames{};
    float thresholdDb{-20.0f};

    [[nodiscard]] static float dbToY(float db, const juce::Rectangle<float>& bounds) noexcept {
        constexpr float minDb = -60.0f;
        constexpr float maxDb = 0.0f;
        const float norm = (std::clamp(db, minDb, maxDb) - minDb) / (maxDb - minDb);
        return bounds.getBottom() - norm * bounds.getHeight();
    }

    void drawGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        g.setColour(juce::Colour(0xff1c2128));
        static constexpr float Dbs[] = { -48.0f, -36.0f, -24.0f, -12.0f, 0.0f };
        for (float db : Dbs) {
            const float y = dbToY(db, bounds);
            g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());
        }
    }

    void drawGainReduction(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        juce::Path grPath;
        const float dx = bounds.getWidth() / 512.0f;
        bool started = false;

        for (size_t i = 0; i < 512; ++i) {
            const float x = bounds.getX() + static_cast<float>(i) * dx;
            // Map gain reduction [0 dB, -24 dB] -> [ceiling Y, lower Y]
            const float grLinear = std::clamp(std::abs(frames[i].gainReductionDb) / 24.0f, 0.0f, 1.0f);
            const float y = bounds.getY() + grLinear * (bounds.getHeight() * 0.6f);

            if (!started) {
                grPath.startNewSubPath(x, bounds.getY());
                grPath.lineTo(x, y);
                started = true;
            } else {
                grPath.lineTo(x, y);
            }
        }

        grPath.lineTo(bounds.getRight(), bounds.getY());
        grPath.closeSubPath();

        g.setColour(juce::Colour(0x33ff1744));
        g.fillPath(grPath);
        g.setColour(juce::Colour(0xaaff1744));
        g.strokePath(grPath, juce::PathStrokeType(1.2f));
    }

    void drawWaveforms(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        juce::Path inPath;
        juce::Path outPath;
        const float dx = bounds.getWidth() / 512.0f;

        for (size_t i = 0; i < 512; ++i) {
            const float x = bounds.getX() + static_cast<float>(i) * dx;
            const float inY = dbToY(frames[i].inputDb, bounds);
            const float outY = dbToY(frames[i].outputDb, bounds);

            if (i == 0) {
                inPath.startNewSubPath(x, inY);
                outPath.startNewSubPath(x, outY);
            } else {
                inPath.lineTo(x, inY);
                outPath.lineTo(x, outY);
            }
        }

        g.setColour(juce::Colour(0x448b949e));
        g.strokePath(inPath, juce::PathStrokeType(1.0f));
        g.setColour(juce::Colour(0xff00f0ff));
        g.strokePath(outPath, juce::PathStrokeType(1.5f));
    }
};

} // namespace openx::ui
