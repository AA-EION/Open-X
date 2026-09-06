#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace openx::limit {

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)>::createParameterLayout(Descriptors))
{
    params.initialize(apvts, Descriptors);
}

void PluginProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    const auto sr = static_cast<float>(sampleRate);
    for (auto& lim : limiters) {
        lim.prepare(sr);
    }
    for (auto& tp : inTpDetectors) {
        tp.prepare(sr);
        tp.setReleaseTime(100.0f);
    }
    for (auto& tp : outTpDetectors) {
        tp.prepare(sr);
        tp.setReleaseTime(100.0f);
    }
    loudnessMeter.prepare(sr);
    setLatencySamples(static_cast<int>(openx::dsp::BrickwallLimiter<float, 256>::LatencySamples));
}

void PluginProcessor::releaseResources() {
    for (auto& lim : limiters) {
        lim.reset();
    }
    for (auto& tp : inTpDetectors) {
        tp.reset();
    }
    for (auto& tp : outTpDetectors) {
        tp.reset();
    }
    loudnessMeter.reset();
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || totalNumInputChannels == 0)
        return;

    const float ceilingDb = params.get(ParamId::Ceiling);
    const float threshDb  = params.get(ParamId::Threshold);
    const float inputGain = juce::Decibels::decibelsToGain(threshDb);
    const float releaseMs = params.get(ParamId::Release);
    const bool dispEnable = (params.get(ParamId::DispersionEnable) > 0.5f);
    const float dispFreq  = params.get(ParamId::DispersionFreq);
    const int styleIdx    = static_cast<int>(std::round(params.get(ParamId::Style)));
    const float attackMs  = params.get(ParamId::Attack);
    const float lookahead = params.get(ParamId::Lookahead);
    const float transLink = params.get(ParamId::TransientLink) * 0.01f;
    const float relLink   = params.get(ParamId::ReleaseLink) * 0.01f;
    const bool tpEnable   = (params.get(ParamId::TruePeakEnable) > 0.5f);
    const bool dcEnable   = (params.get(ParamId::DcFilterEnable) > 0.5f);
    const int audition    = static_cast<int>(std::round(params.get(ParamId::AuditionMode)));

    openx::dsp::BrickwallLimiter<float, 256>::Parameters p;
    p.ceilingDb = ceilingDb;
    p.thresholdDb = 0.0f; // Threshold/drive gain applied in pre-stage
    p.releaseMs = releaseMs;
    p.enablePhaseDispersion = dispEnable;
    p.dispersionFreqHz = dispFreq;
    p.dispersionQ = 0.7071f;
    p.style = static_cast<openx::dsp::BrickwallLimiter<float, 256>::Style>(std::clamp(styleIdx, 0, 7));
    p.attackMs = attackMs;
    p.lookaheadMs = lookahead;
    p.enableTruePeak = tpEnable;
    p.enableDcFilter = dcEnable;
    p.transientLink = transLink;
    p.releaseLink = relLink;

    for (auto& lim : limiters) {
        lim.setParameters(p);
    }

    const int channelsToProcess = std::min(totalNumInputChannels, 2);
    auto* ch0 = buffer.getWritePointer(0);
    auto* ch1 = (channelsToProcess > 1) ? buffer.getWritePointer(1) : ch0;

    float maxIn = 0.0f;
    float maxOut = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
        // 1. Apply input drive gain
        const float in0 = ch0[i] * inputGain;
        const float in1 = ch1[i] * inputGain;

        // 2. Measure input true peaks for visual metering
        inTpDetectors[0].processSample(in0);
        if (channelsToProcess > 1) {
            inTpDetectors[1].processSample(in1);
        }

        // 3. Pre-filter DC offset and phase dispersion once per channel
        const float disp0 = limiters[0].preFilter(in0);
        const float disp1 = (channelsToProcess > 1) ? limiters[1].preFilter(in1) : disp0;

        // 4. Transient peak detection on pre-filtered signal
        const float peak0 = limiters[0].detectPeakFromDispersed(disp0);
        const float peak1 = (channelsToProcess > 1) ? limiters[1].detectPeakFromDispersed(disp1) : peak0;

        // 5. Transient channel linking
        const float maxPeak = std::max(peak0, peak1);
        const float effPeak0 = (1.0f - transLink) * peak0 + transLink * maxPeak;
        const float effPeak1 = (1.0f - transLink) * peak1 + transLink * maxPeak;

        // 6. Desired gain attenuation
        const float desGain0 = limiters[0].computeDesiredGain(effPeak0);
        const float desGain1 = limiters[1].computeDesiredGain(effPeak1);

        // 7. Release channel linking
        const float minDesGain = std::min(desGain0, desGain1);
        const float linkedGain0 = (1.0f - relLink) * desGain0 + relLink * minDesGain;
        const float linkedGain1 = (1.0f - relLink) * desGain1 + relLink * minDesGain;

        // 8. Lookahead limiting process
        float out0 = limiters[0].processWithTargetGain(disp0, linkedGain0);
        float out1 = (channelsToProcess > 1) ? limiters[1].processWithTargetGain(disp1, linkedGain1) : out0;

        // 9. Measure output true peaks for visual metering
        outTpDetectors[0].processSample(out0);
        if (channelsToProcess > 1) {
            outTpDetectors[1].processSample(out1);
        }

        // 10. Audition Modes:
        // 0: Normal
        // 1: Unity Gain (1:1 level-matched to input, eliminates loudness bias)
        // 2: Delta (phase-aligned difference between delayed input and limited output)
        float write0 = out0;
        float write1 = out1;

        if (audition == 1) {
            // Unity Gain: level-matched by reversing input boost
            const float invGain = (inputGain > 1e-5f) ? (1.0f / inputGain) : 1.0f;
            write0 = out0 * invGain;
            write1 = out1 * invGain;
        } else if (audition == 2) {
            // Delta: phase-aligned difference between delayed input and limited output
            const float delIn0 = limiters[0].getLastDelayedSample();
            const float delIn1 = (channelsToProcess > 1) ? limiters[1].getLastDelayedSample() : delIn0;
            write0 = delIn0 - out0;
            write1 = delIn1 - out1;
        }

        ch0[i] = write0;
        if (channelsToProcess > 1) {
            ch1[i] = write1;
        }

        // 11. Feed ITU-R BS.1770-4 Loudness Analyzer
        loudnessMeter.processSample(out0, out1);

        // 11. History tracking
        const float absIn = std::max(std::abs(in0), std::abs(in1));
        const float absOut = std::max(std::abs(out0), std::abs(out1));
        if (absIn > maxIn) maxIn = absIn;
        if (absOut > maxOut) maxOut = absOut;
    }

    if (totalNumInputChannels == 1 && totalNumOutputChannels > 1) {
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    }

    // Push frame into lock-free visualizer history
    const float inDb = juce::Decibels::gainToDecibels(maxIn, -100.0f);
    const float outDb = juce::Decibels::gainToDecibels(maxOut, -100.0f);
    const float grDb = std::max(limiters[0].getGainReductionDb(), limiters[1].getGainReductionDb());
    history.push(inDb, outDb, grDb);

    // Update real-time atomic meter metrics for GUI
    const float inTp0 = inTpDetectors[0].getCurrentPeakDb();
    const float inTp1 = (channelsToProcess > 1) ? inTpDetectors[1].getCurrentPeakDb() : inTp0;
    inputTruePeak.store(std::max(inTp0, inTp1), std::memory_order_relaxed);

    const float outTp0 = outTpDetectors[0].getCurrentPeakDb();
    const float outTp1 = (channelsToProcess > 1) ? outTpDetectors[1].getCurrentPeakDb() : outTp0;
    outputTruePeak.store(std::max(outTp0, outTp1), std::memory_order_relaxed);

    maxGainReduction.store(grDb, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor() {
    return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    if (xml != nullptr) {
        copyXmlToBinary(*xml, destData);
    }
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

} // namespace openx::limit

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new openx::limit::PluginProcessor();
}
