#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "../dsp/spectrum_analyzer.hpp"
#include <functional>
#include <cmath>
#include <array>
#include <algorithm>
#include <string>

namespace openx::ui {

class ReverbDecayCurve : public juce::Component {
public:
    enum class DragNode {
        None,
        LowDecay,
        MidDecay,
        HighDecay,
        HighDamping,
        LowCut
    };

    struct CurveState {
        float baseDecayTime{2.5f};
        float dampingHz{6000.0f};
        float lowCutHz{30.0f};

        float decayRateLow{1.0f};
        float decayRateLowFreq{200.0f};

        float decayRateMid{1.0f};
        float decayRateMidFreq{1200.0f};
        float decayRateMidQ{0.7071f};

        float decayRateHigh{1.0f};
        float decayRateHighFreq{6000.0f};

        // Post EQ
        float postLowGainDb{0.0f};
        float postLowFreq{150.0f};
        float postMidGainDb{0.0f};
        float postMidFreq{1500.0f};
        float postHighGainDb{0.0f};
        float postHighFreq{8000.0f};
    };

    // Callback signatures
    std::function<void(float freq, float mult)> onLowDecayChanged;
    std::function<void(float freq, float mult, float q)> onMidDecayChanged;
    std::function<void(float freq, float mult)> onHighDecayChanged;
    std::function<void(float freq)> onDampingChanged;
    std::function<void(float freq)> onLowCutChanged;
    std::function<void(DragNode)> onGestureStarted;
    std::function<void(DragNode)> onGestureEnded;

    ReverbDecayCurve() {
        setRepaintsOnMouseActivity(true);
    }

    void setCurveState(const CurveState& state) noexcept {
        if (isDragging) {
            const auto savedLowFreq = curveState.decayRateLowFreq;
            const auto savedLowRate = curveState.decayRateLow;
            const auto savedMidFreq = curveState.decayRateMidFreq;
            const auto savedMidRate = curveState.decayRateMid;
            const auto savedMidQ    = curveState.decayRateMidQ;
            const auto savedHighFreq= curveState.decayRateHighFreq;
            const auto savedHighRate= curveState.decayRateHigh;
            const auto savedDamping = curveState.dampingHz;
            const auto savedLowCut  = curveState.lowCutHz;

            curveState = state;

            switch (activeNode) {
                case DragNode::LowDecay:
                    curveState.decayRateLowFreq = savedLowFreq;
                    curveState.decayRateLow = savedLowRate;
                    break;
                case DragNode::MidDecay:
                    curveState.decayRateMidFreq = savedMidFreq;
                    curveState.decayRateMid = savedMidRate;
                    curveState.decayRateMidQ = savedMidQ;
                    break;
                case DragNode::HighDecay:
                    curveState.decayRateHighFreq = savedHighFreq;
                    curveState.decayRateHigh = savedHighRate;
                    break;
                case DragNode::HighDamping:
                    curveState.dampingHz = savedDamping;
                    break;
                case DragNode::LowCut:
                    curveState.lowCutHz = savedLowCut;
                    break;
                case DragNode::None:
                    break;
            }
        } else {
            curveState = state;
        }
        repaint();
    }

    void updateSpectrum(const float* scopeData, size_t numBins, float sampleRate) noexcept {
        if (scopeData != nullptr && numBins > 0) {
            spectrumBins = std::min(numBins, spectrumData.size());
            std::copy_n(scopeData, spectrumBins, spectrumData.begin());
            hasSpectrum = true;
        } else {
            hasSpectrum = false;
        }
        sr = sampleRate > 0 ? sampleRate : 48000.0f;
        repaint();
    }

    void paint(juce::Graphics& g) override {
        const auto bounds = getLocalBounds().toFloat();
        // Modern dark slate background
        g.fillAll(juce::Colour(0xff121418));

        // 1. Frequency & Decay Time Grid
        drawGrid(g, bounds);

        // 2. Real-time Output Spectrum with luminous gradient fill
        if (hasSpectrum && spectrumBins > 0) {
            drawSpectrum(g, bounds);
        }

        // 3. Reverb Decay Rate EQ Curve
        drawDecayCurve(g, bounds);

        // 4. Interactive Draggable Node Handles
        drawHandles(g, bounds);

        // 5. Active Node Info Tooltip
        drawActiveTooltip(g, bounds);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const auto bounds = getLocalBounds().toFloat();
        const auto pos = e.position;

        activeNode = hitTestNodes(pos, bounds);
        if (activeNode != DragNode::None) {
            isDragging = true;
            if (onGestureStarted) onGestureStarted(activeNode);
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (!isDragging || activeNode == DragNode::None) return;
        const auto bounds = getLocalBounds().toFloat();

        const float freq = std::clamp(screenToFreq(e.position.x, bounds), 20.0f, 20000.0f);
        const float targetTime = std::clamp(screenToDecay(e.position.y, bounds), 0.1f, 30.0f);
        const float mult = std::clamp(targetTime / std::max(curveState.baseDecayTime, 0.1f), 0.2f, 3.0f);

        switch (activeNode) {
            case DragNode::LowDecay:
                curveState.decayRateLowFreq = std::clamp(freq, 40.0f, 1000.0f);
                curveState.decayRateLow = mult;
                if (onLowDecayChanged) onLowDecayChanged(curveState.decayRateLowFreq, curveState.decayRateLow);
                break;
            case DragNode::MidDecay:
                curveState.decayRateMidFreq = std::clamp(freq, 150.0f, 10000.0f);
                curveState.decayRateMid = mult;
                if (onMidDecayChanged) onMidDecayChanged(curveState.decayRateMidFreq, curveState.decayRateMid, curveState.decayRateMidQ);
                break;
            case DragNode::HighDecay:
                curveState.decayRateHighFreq = std::clamp(freq, 1000.0f, 18000.0f);
                curveState.decayRateHigh = mult;
                if (onHighDecayChanged) onHighDecayChanged(curveState.decayRateHighFreq, curveState.decayRateHigh);
                break;
            case DragNode::HighDamping:
                curveState.dampingHz = std::clamp(freq, 500.0f, 20000.0f);
                if (onDampingChanged) onDampingChanged(curveState.dampingHz);
                break;
            case DragNode::LowCut:
                curveState.lowCutHz = std::clamp(freq, 20.0f, 1000.0f);
                if (onLowCutChanged) onLowCutChanged(curveState.lowCutHz);
                break;
            case DragNode::None:
                break;
        }

        repaint();
    }

    void mouseUp(const juce::MouseEvent&) override {
        if (isDragging && activeNode != DragNode::None) {
            if (onGestureEnded) onGestureEnded(activeNode);
        }
        isDragging = false;
        activeNode = DragNode::None;
        repaint();
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override {
        const auto bounds = getLocalBounds().toFloat();
        const auto midPt = getNodePos(DragNode::MidDecay, bounds);

        if (e.position.getDistanceFrom(midPt) < 30.0f) {
            curveState.decayRateMidQ = std::clamp(curveState.decayRateMidQ + wheel.deltaY * 0.4f, 0.2f, 5.0f);
            if (onMidDecayChanged) onMidDecayChanged(curveState.decayRateMidFreq, curveState.decayRateMid, curveState.decayRateMidQ);
            repaint();
        }
    }

private:
    CurveState curveState{};
    std::array<float, 1024> spectrumData{};
    bool hasSpectrum{false};
    size_t spectrumBins{0};
    float sr{48000.0f};

    DragNode activeNode{DragNode::None};
    bool isDragging{false};

    [[nodiscard]] static float freqToX(float freq, const juce::Rectangle<float>& bounds) noexcept {
        constexpr float minLog = 1.30103f; // log10(20)
        constexpr float maxLog = 4.30103f; // log10(20000)
        const float fLog = std::log10(std::clamp(freq, 20.0f, 20000.0f));
        const float norm = (fLog - minLog) / (maxLog - minLog);
        return bounds.getX() + norm * bounds.getWidth();
    }

    [[nodiscard]] static float screenToFreq(float x, const juce::Rectangle<float>& bounds) noexcept {
        constexpr float minLog = 1.30103f;
        constexpr float maxLog = 4.30103f;
        const float norm = std::clamp((x - bounds.getX()) / bounds.getWidth(), 0.0f, 1.0f);
        return std::pow(10.0f, minLog + norm * (maxLog - minLog));
    }

    [[nodiscard]] static float decayToY(float decaySec, const juce::Rectangle<float>& bounds) noexcept {
        constexpr float minLog = -1.0f;     // log10(0.1s)
        constexpr float maxLog = 1.39794f;  // log10(25s)
        const float dLog = std::log10(std::clamp(decaySec, 0.1f, 25.0f));
        const float norm = (dLog - minLog) / (maxLog - minLog);
        return bounds.getBottom() - norm * bounds.getHeight();
    }

    [[nodiscard]] static float screenToDecay(float y, const juce::Rectangle<float>& bounds) noexcept {
        constexpr float minLog = -1.0f;
        constexpr float maxLog = 1.39794f;
        const float norm = 1.0f - std::clamp((y - bounds.getY()) / bounds.getHeight(), 0.0f, 1.0f);
        return std::pow(10.0f, minLog + norm * (maxLog - minLog));
    }

    [[nodiscard]] float calcDecayMultiplierAt(float freq) const noexcept {
        float mult = 1.0f;

        // 1. Low shelf decay multiplier
        {
            const float fRatio = freq / std::max(curveState.decayRateLowFreq, 1.0f);
            const float w = 1.0f / (1.0f + fRatio * fRatio);
            mult *= (1.0f + (curveState.decayRateLow - 1.0f) * w);
        }

        // 2. Mid peaking decay multiplier
        {
            const float fRatio = freq / std::max(curveState.decayRateMidFreq, 1.0f);
            const float bw = std::max(curveState.decayRateMidQ, 0.1f);
            const float diff = std::log(std::max(fRatio, 1e-4f)) * bw;
            const float bell = std::exp(-0.5f * diff * diff);
            mult *= (1.0f + (curveState.decayRateMid - 1.0f) * bell);
        }

        // 3. High shelf decay multiplier
        {
            const float fRatio = freq / std::max(curveState.decayRateHighFreq, 1.0f);
            const float w = (fRatio * fRatio) / (1.0f + fRatio * fRatio);
            mult *= (1.0f + (curveState.decayRateHigh - 1.0f) * w);
        }

        return std::clamp(mult, 0.15f, 4.0f);
    }

    [[nodiscard]] float getDecayTimeAt(float freq) const noexcept {
        const float baseT = std::max(curveState.baseDecayTime, 0.1f);
        const float mult = calcDecayMultiplierAt(freq);

        // High damping roll-off
        const float dampRatio = freq / std::max(curveState.dampingHz, 100.0f);
        const float dampFactor = 1.0f / std::sqrt(1.0f + dampRatio * dampRatio * 1.5f);

        // Low cut roll-off
        const float lcRatio = std::max(curveState.lowCutHz, 10.0f) / std::max(freq, 1.0f);
        const float lcFactor = 1.0f / std::sqrt(1.0f + lcRatio * lcRatio * 1.5f);

        return std::clamp(baseT * mult * dampFactor * lcFactor, 0.1f, 25.0f);
    }

    [[nodiscard]] juce::Point<float> getNodePos(DragNode node, const juce::Rectangle<float>& bounds) const noexcept {
        switch (node) {
            case DragNode::LowDecay: {
                const float t = getDecayTimeAt(curveState.decayRateLowFreq);
                return { freqToX(curveState.decayRateLowFreq, bounds), decayToY(t, bounds) };
            }
            case DragNode::MidDecay: {
                const float t = getDecayTimeAt(curveState.decayRateMidFreq);
                return { freqToX(curveState.decayRateMidFreq, bounds), decayToY(t, bounds) };
            }
            case DragNode::HighDecay: {
                const float t = getDecayTimeAt(curveState.decayRateHighFreq);
                return { freqToX(curveState.decayRateHighFreq, bounds), decayToY(t, bounds) };
            }
            case DragNode::HighDamping: {
                const float t = getDecayTimeAt(curveState.dampingHz);
                return { freqToX(curveState.dampingHz, bounds), decayToY(t, bounds) };
            }
            case DragNode::LowCut: {
                const float t = getDecayTimeAt(curveState.lowCutHz);
                return { freqToX(curveState.lowCutHz, bounds), decayToY(t, bounds) };
            }
            case DragNode::None:
            default:
                return { -100.0f, -100.0f };
        }
    }

    [[nodiscard]] DragNode hitTestNodes(juce::Point<float> pos, const juce::Rectangle<float>& bounds) const noexcept {
        static constexpr DragNode NodesToCheck[] = {
            DragNode::MidDecay, DragNode::LowDecay, DragNode::HighDecay, DragNode::HighDamping, DragNode::LowCut
        };

        for (auto n : NodesToCheck) {
            const auto pt = getNodePos(n, bounds);
            if (pos.getDistanceFrom(pt) <= 16.0f) {
                return n;
            }
        }
        return DragNode::None;
    }

    void drawGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        g.setFont(juce::FontOptions(10.0f, juce::Font::plain));

        // Vertical Frequency Lines
        static constexpr float Frequencies[] = { 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2500.0f, 5000.0f, 10000.0f, 20000.0f };
        static constexpr const char* FreqLabels[] = { "50", "100", "250", "500", "1k", "2.5k", "5k", "10k", "20k" };

        for (size_t i = 0; i < std::size(Frequencies); ++i) {
            const float x = freqToX(Frequencies[i], bounds);
            g.setColour(juce::Colour(0xff1c2128));
            g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());

            g.setColour(juce::Colour(0xff484f58));
            g.drawText(FreqLabels[i], static_cast<int>(x - 18), static_cast<int>(bounds.getBottom() - 14), 36, 12, juce::Justification::centred);
        }

        // Horizontal Decay Time Lines
        static constexpr float Times[] = { 0.2f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f };
        static constexpr const char* TimeLabels[] = { "0.2s", "0.5s", "1s", "2s", "5s", "10s", "20s" };

        for (size_t i = 0; i < std::size(Times); ++i) {
            const float y = decayToY(Times[i], bounds);
            g.setColour(juce::Colour(0xff1c2128));
            g.drawHorizontalLine(static_cast<int>(y), bounds.getX(), bounds.getRight());

            g.setColour(juce::Colour(0xff484f58));
            g.drawText(TimeLabels[i], static_cast<int>(bounds.getX() + 6), static_cast<int>(y - 6), 36, 12, juce::Justification::left);
        }

        // Baseline RT60 dashed guide
        const float baseY = decayToY(curveState.baseDecayTime, bounds);
        g.setColour(juce::Colour(0x3300f0ff));
        static constexpr float dashPattern[] = { 3.0f, 4.0f };
        g.drawDashedLine(juce::Line<float>(bounds.getX(), baseY, bounds.getRight(), baseY), dashPattern, 2, 1.0f);
    }

    void drawSpectrum(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        juce::Path spectrumPath;
        const float binWidth = sr / static_cast<float>(spectrumBins * 2);
        bool firstPoint = true;

        for (size_t i = 1; i < spectrumBins; ++i) {
            const float freq = static_cast<float>(i) * binWidth;
            if (freq < 20.0f || freq > 20000.0f) continue;

            const float x = freqToX(freq, bounds);
            // Map [-90 dB, 0 dB] -> screen Y
            const float norm = std::clamp((spectrumData[i] + 90.0f) / 90.0f, 0.0f, 1.0f);
            const float y = bounds.getBottom() - norm * bounds.getHeight();

            if (firstPoint) {
                spectrumPath.startNewSubPath(x, bounds.getBottom());
                spectrumPath.lineTo(x, y);
                firstPoint = false;
            } else {
                spectrumPath.lineTo(x, y);
            }
        }

        if (firstPoint) return;

        spectrumPath.lineTo(bounds.getRight(), bounds.getBottom());
        spectrumPath.closeSubPath();

        // Glowing luminous gradient fill
        juce::ColourGradient grad(juce::Colour(0x2200f0ff), bounds.getCentreX(), bounds.getY(),
                                  juce::Colour(0x0200f0ff), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill(grad);
        g.fillPath(spectrumPath);

        g.setColour(juce::Colour(0x4400f0ff));
        g.strokePath(spectrumPath, juce::PathStrokeType(1.0f));
    }

    void drawDecayCurve(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        juce::Path curvePath;
        juce::Path fillPath;

        const int numPoints = static_cast<int>(bounds.getWidth());
        for (int i = 0; i < numPoints; ++i) {
            const float x = bounds.getX() + static_cast<float>(i);
            const float freq = screenToFreq(x, bounds);
            const float decayTime = getDecayTimeAt(freq);
            const float y = decayToY(decayTime, bounds);

            if (i == 0) {
                curvePath.startNewSubPath(x, y);
                fillPath.startNewSubPath(x, bounds.getBottom());
                fillPath.lineTo(x, y);
            } else {
                curvePath.lineTo(x, y);
                fillPath.lineTo(x, y);
            }
        }

        fillPath.lineTo(bounds.getRight(), bounds.getBottom());
        fillPath.closeSubPath();

        // Shaded luminous cyan fill underneath decay curve
        juce::ColourGradient fillGrad(juce::Colour(0x2800f0ff), bounds.getCentreX(), bounds.getY() + 30,
                                     juce::Colour(0x0000f0ff), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill(fillGrad);
        g.fillPath(fillPath);

        // Core sharp neon curve stroke
        g.setColour(juce::Colour(0xff00f0ff));
        g.strokePath(curvePath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawHandles(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        auto drawNode = [&](DragNode node, juce::Colour col, bool ring) {
            const auto pt = getNodePos(node, bounds);
            const bool isActive = (activeNode == node);

            if (isActive) {
                g.setColour(col.withAlpha(0.35f));
                g.fillEllipse(pt.x - 12.0f, pt.y - 12.0f, 24.0f, 24.0f);
            }

            g.setColour(col);
            g.fillEllipse(pt.x - 6.0f, pt.y - 6.0f, 12.0f, 12.0f);
            g.setColour(juce::Colours::white);
            g.drawEllipse(pt.x - 6.0f, pt.y - 6.0f, 12.0f, 12.0f, 1.5f);

            if (ring) {
                g.setColour(col.withAlpha(0.6f));
                g.drawEllipse(pt.x - 10.0f, pt.y - 10.0f, 20.0f, 20.0f, 1.0f);
            }
        };

        // Low Decay Node (Amber/Cyan)
        drawNode(DragNode::LowDecay, juce::Colour(0xffffab00), false);

        // Mid Decay Node (Cyan, with Q ring)
        drawNode(DragNode::MidDecay, juce::Colour(0xff00f0ff), true);

        // High Decay Node (Amber/Cyan)
        drawNode(DragNode::HighDecay, juce::Colour(0xffffab00), false);

        // High Damping Cutoff Node (Purple/Violet)
        drawNode(DragNode::HighDamping, juce::Colour(0xffa371f7), false);

        // Low Cut Damping Node (Red/Orange)
        drawNode(DragNode::LowCut, juce::Colour(0xffff5c57), false);
    }

    void drawActiveTooltip(juce::Graphics& g, const juce::Rectangle<float>& bounds) const noexcept {
        if (!isDragging || activeNode == DragNode::None) return;

        juce::String tip;
        juce::Point<float> pt = getNodePos(activeNode, bounds);

        switch (activeNode) {
            case DragNode::LowDecay:
                tip = juce::String::formatted("Low Decay: %.2fx @ %.0f Hz", curveState.decayRateLow, curveState.decayRateLowFreq);
                break;
            case DragNode::MidDecay:
                tip = juce::String::formatted("Mid Decay: %.2fx @ %.0f Hz (Q: %.2f)", curveState.decayRateMid, curveState.decayRateMidFreq, curveState.decayRateMidQ);
                break;
            case DragNode::HighDecay:
                tip = juce::String::formatted("High Decay: %.2fx @ %.0f Hz", curveState.decayRateHigh, curveState.decayRateHighFreq);
                break;
            case DragNode::HighDamping:
                tip = juce::String::formatted("High Damping: %.0f Hz", curveState.dampingHz);
                break;
            case DragNode::LowCut:
                tip = juce::String::formatted("Low Cut: %.0f Hz", curveState.lowCutHz);
                break;
            default:
                break;
        }

        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        const int tipW = g.getCurrentFont().getStringWidth(tip) + 16;
        constexpr int tipH = 22;
        int tipX = static_cast<int>(pt.x - tipW * 0.5f);
        int tipY = static_cast<int>(pt.y - 32.0f);

        tipX = std::clamp(tipX, static_cast<int>(bounds.getX() + 4), static_cast<int>(bounds.getRight() - tipW - 4));
        tipY = std::max(tipY, static_cast<int>(bounds.getY() + 4));

        g.setColour(juce::Colour(0xee161b22));
        g.fillRoundedRectangle(static_cast<float>(tipX), static_cast<float>(tipY), static_cast<float>(tipW), static_cast<float>(tipH), 4.0f);
        g.setColour(juce::Colour(0xff30363d));
        g.drawRoundedRectangle(static_cast<float>(tipX), static_cast<float>(tipY), static_cast<float>(tipW), static_cast<float>(tipH), 4.0f, 1.0f);

        g.setColour(juce::Colours::white);
        g.drawText(tip, tipX, tipY, tipW, tipH, juce::Justification::centred);
    }
};

} // namespace openx::ui
