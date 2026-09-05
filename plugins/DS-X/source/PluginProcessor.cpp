#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace openx::ds {

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)>::createParameterLayout(Descriptors))
{
    params.initialize(apvts, Descriptors);
}

openx::dsp::DeEsserEngine<float>::Parameters PluginProcessor::getCurrentEngineParameters() const noexcept {
    openx::dsp::DeEsserEngine<float>::Parameters p;
    p.frequencyHz = params.get(ParamId::Frequency);
    p.thresholdDb = params.get(ParamId::Threshold);
    p.reductionDb = params.get(ParamId::Reduction);
    p.bandwidthQ = params.get(ParamId::BandwidthQ);
    p.useLpcResidualSubtraction = (params.get(ParamId::UseLpc) > 0.5f);
    p.detectionMode = (params.get(ParamId::DetectionMode) > 0.5f)
        ? openx::dsp::DeEsserEngine<float>::DetectionMode::Allround
        : openx::dsp::DeEsserEngine<float>::DetectionMode::SingleVocal;
    p.bandMode = (params.get(ParamId::BandMode) > 0.5f)
        ? openx::dsp::DeEsserEngine<float>::ProcessingBandMode::SplitBand
        : openx::dsp::DeEsserEngine<float>::ProcessingBandMode::WideBand;
    p.filterType = (params.get(ParamId::FilterType) > 0.5f)
        ? openx::dsp::DeEsserEngine<float>::SidechainFilterType::Highpass
        : openx::dsp::DeEsserEngine<float>::SidechainFilterType::Bandpass;
    const float audVal = params.get(ParamId::Audition);
    p.auditionMode = (audVal > 1.5f)
        ? openx::dsp::DeEsserEngine<float>::AuditionMode::Delta
        : ((audVal > 0.5f)
            ? openx::dsp::DeEsserEngine<float>::AuditionMode::Sidechain
            : openx::dsp::DeEsserEngine<float>::AuditionMode::Normal);
    p.lookaheadMs = params.get(ParamId::Lookahead);
    p.stereoLink = params.get(ParamId::StereoLink) * 0.01f;
    const float smVal = params.get(ParamId::StereoMode);
    p.stereoMode = (smVal > 1.5f)
        ? openx::dsp::DeEsserEngine<float>::StereoProcessingMode::Side
        : ((smVal > 0.5f)
            ? openx::dsp::DeEsserEngine<float>::StereoProcessingMode::Mid
            : openx::dsp::DeEsserEngine<float>::StereoProcessingMode::Stereo);
    return p;
}

void PluginProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    const auto p = getCurrentEngineParameters();
    for (auto& deesser : deessers) {
        deesser.prepare(static_cast<float>(sampleRate));
        deesser.setParameters(p);
    }
    const int latency = static_cast<int>(deessers[0].getLatencySamples());
    setLatencySamples(latency);
}

void PluginProcessor::releaseResources() {
    for (auto& deesser : deessers) {
        deesser.reset();
    }
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0) return;

    const auto p = getCurrentEngineParameters();
    for (auto& deesser : deessers) {
        deesser.setParameters(p);
    }

    const int currentLatency = static_cast<int>(deessers[0].getLatencySamples());
    if (getLatencySamples() != currentLatency) {
        setLatencySamples(currentLatency);
    }

    if (totalNumInputChannels >= 2 && p.stereoMode == openx::dsp::DeEsserEngine<float>::StereoProcessingMode::Mid) {
        // Mid-only de-essing: Center lead vocal targeted, sides delayed for perfect phase match
        auto* chL = buffer.getWritePointer(0);
        auto* chR = buffer.getWritePointer(1);

        for (int i = 0; i < numSamples; ++i) {
            const float inL = chL[i];
            const float inR = chR[i];
            const float mid = 0.5f * (inL + inR);
            const float side = 0.5f * (inL - inR);

            const float midOut = deessers[0].processSample(mid);
            const float sideOut = deessers[1].applyGainWithEffectiveEnvelope(side, 0.0f, 0.0f);

            chL[i] = midOut + sideOut;
            chR[i] = midOut - sideOut;

            spectrumAnalyzer.pushSample(midOut);
        }
    } else if (totalNumInputChannels >= 2 && p.stereoMode == openx::dsp::DeEsserEngine<float>::StereoProcessingMode::Side) {
        // Side-only de-essing: Backing vocals & wide reverb targeted, mid content preserved
        auto* chL = buffer.getWritePointer(0);
        auto* chR = buffer.getWritePointer(1);

        for (int i = 0; i < numSamples; ++i) {
            const float inL = chL[i];
            const float inR = chR[i];
            const float mid = 0.5f * (inL + inR);
            const float side = 0.5f * (inL - inR);

            const float midOut = deessers[0].applyGainWithEffectiveEnvelope(mid, 0.0f, 0.0f);
            const float sideOut = deessers[1].processSample(side);

            if (p.auditionMode != openx::dsp::DeEsserEngine<float>::AuditionMode::Normal) {
                // In audition mode, present auditioned sidechain or delta in-phase to avoid phase cancellation
                chL[i] = sideOut;
                chR[i] = sideOut;
                spectrumAnalyzer.pushSample(sideOut);
            } else {
                chL[i] = midOut + sideOut;
                chR[i] = midOut - sideOut;
                spectrumAnalyzer.pushSample(0.5f * (chL[i] + chR[i]));
            }
        }
    } else if (totalNumInputChannels >= 2) {
        // Stereo mode with variable stereo linking
        auto* chL = buffer.getWritePointer(0);
        auto* chR = buffer.getWritePointer(1);
        const float link = p.stereoLink;

        for (int i = 0; i < numSamples; ++i) {
            float scL{0}, envL{0};
            float scR{0}, envR{0};

            deessers[0].processSidechain(chL[i], scL, envL);
            deessers[1].processSidechain(chR[i], scR, envR);

            const float maxEnv = std::max(envL, envR);
            const float effL = (1.0f - link) * envL + link * maxEnv;
            const float effR = (1.0f - link) * envR + link * maxEnv;

            const float outL = deessers[0].applyGainWithEffectiveEnvelope(chL[i], scL, effL);
            const float outR = deessers[1].applyGainWithEffectiveEnvelope(chR[i], scR, effR);

            chL[i] = outL;
            chR[i] = outR;

            spectrumAnalyzer.pushSample(0.5f * (outL + outR));
        }
    } else if (totalNumInputChannels == 1) {
        // Mono mode
        auto* ch0 = buffer.getWritePointer(0);
        for (int i = 0; i < numSamples; ++i) {
            const float out = deessers[0].processSample(ch0[i]);
            ch0[i] = out;
            spectrumAnalyzer.pushSample(out);
        }
        if (totalNumOutputChannels > 1) {
            buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
        }
    }

    // Capture block dynamics for GUI polling
    float blockMinGrDb = deessers[0].getCurrentGainReductionDb();
    float blockMaxSib = deessers[0].getSibilanceActivity();
    float blockMaxScDb = deessers[0].getSidechainLevelDb();
    if (totalNumInputChannels >= 2) {
        blockMinGrDb = std::min(blockMinGrDb, deessers[1].getCurrentGainReductionDb());
        blockMaxSib = std::max(blockMaxSib, deessers[1].getSibilanceActivity());
        blockMaxScDb = std::max(blockMaxScDb, deessers[1].getSidechainLevelDb());
    }

    currentGainReductionDb.store(blockMinGrDb, std::memory_order_relaxed);
    currentSibilanceActivity.store(blockMaxSib, std::memory_order_relaxed);
    currentSidechainLevelDb.store(blockMaxScDb, std::memory_order_relaxed);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor() {
    return new PluginEditor(*this);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

} // namespace openx::ds

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new openx::ds::PluginProcessor();
}
