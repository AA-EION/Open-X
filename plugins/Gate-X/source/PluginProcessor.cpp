#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace openx::gate {

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                     .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)>::createParameterLayout(Descriptors))
{
    params.initialize(apvts, Descriptors);
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (mainOut != mainIn)
        return false;

    const auto& scIn = layouts.getChannelSet(true, 1);
    if (scIn != juce::AudioChannelSet::disabled()
        && scIn != juce::AudioChannelSet::mono()
        && scIn != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void PluginProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    for (auto& gate : gates) {
        gate.prepare(static_cast<float>(sampleRate));
    }

    openx::dsp::PredictiveGate<float, 8192>::Parameters p;
    p.mode             = static_cast<int>(params.get(ParamId::Mode));
    p.style            = static_cast<int>(params.get(ParamId::Style));
    p.openThresholdDb  = params.get(ParamId::OpenThreshold);
    p.closeThresholdDb = params.get(ParamId::CloseThreshold);
    p.ratio            = params.get(ParamId::Ratio);
    p.rangeDb          = params.get(ParamId::Range);
    p.kneeDb           = params.get(ParamId::Knee);
    p.attackMs         = params.get(ParamId::Attack);
    p.holdMs           = params.get(ParamId::Hold);
    p.releaseMs        = params.get(ParamId::Release);
    p.lookaheadMs      = params.get(ParamId::Lookahead);
    p.scLowCutHz       = params.get(ParamId::ScLowCut);
    p.scHighCutHz      = params.get(ParamId::ScHighCut);
    p.dryWet           = params.get(ParamId::DryWet) * 0.01f;

    for (auto& gate : gates) {
        gate.setParameters(p);
    }
    currentReportedLatency = static_cast<int>(gates[0].getLatencySamples());
    setLatencySamples(currentReportedLatency);
}

void PluginProcessor::releaseResources() {
    for (auto& gate : gates) {
        gate.reset();
    }
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    auto mainIn  = getBusBuffer(buffer, true, 0);
    auto mainOut = getBusBuffer(buffer, false, 0);
    auto scBus   = getBusBuffer(buffer, true, 1);

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    openx::dsp::PredictiveGate<float, 8192>::Parameters p;
    p.mode             = static_cast<int>(params.get(ParamId::Mode));
    p.style            = static_cast<int>(params.get(ParamId::Style));
    p.openThresholdDb  = params.get(ParamId::OpenThreshold);
    p.closeThresholdDb = params.get(ParamId::CloseThreshold);
    p.ratio            = params.get(ParamId::Ratio);
    p.rangeDb          = params.get(ParamId::Range);
    p.kneeDb           = params.get(ParamId::Knee);
    p.attackMs         = params.get(ParamId::Attack);
    p.holdMs           = params.get(ParamId::Hold);
    p.releaseMs        = params.get(ParamId::Release);
    p.lookaheadMs      = params.get(ParamId::Lookahead);
    p.scLowCutHz       = params.get(ParamId::ScLowCut);
    p.scHighCutHz      = params.get(ParamId::ScHighCut);
    p.dryWet           = params.get(ParamId::DryWet) * 0.01f;

    const bool audition = params.get(ParamId::ScAudition) > 0.5f;
    const bool useExtSc = (params.get(ParamId::ScSource) > 0.5f) && (scBus.getNumChannels() > 0);
    const float outGainLinear = std::pow(10.0f, params.get(ParamId::OutputGain) / 20.0f);

    for (auto& gate : gates) {
        gate.setParameters(p);
    }

    const int latencyNow = static_cast<int>(gates[0].getLatencySamples());
    if (latencyNow != currentReportedLatency) {
        currentReportedLatency = latencyNow;
        setLatencySamples(currentReportedLatency);
    }

    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = std::min(mainOut.getNumChannels(), 2);

    float maxIn = 0.0f;
    float maxOut = 0.0f;

    if (channelsToProcess == 2) {
        auto* channelData0 = mainOut.getWritePointer(0);
        auto* channelData1 = mainOut.getWritePointer(1);
        const auto* inData0 = mainIn.getReadPointer(0);
        const auto* inData1 = (mainIn.getNumChannels() > 1) ? mainIn.getReadPointer(1) : inData0;

        const float* scData0 = inData0;
        const float* scData1 = inData1;
        if (useExtSc) {
            scData0 = scBus.getReadPointer(0);
            scData1 = (scBus.getNumChannels() > 1) ? scBus.getReadPointer(1) : scData0;
        }

        const float stereoLink = params.get(ParamId::StereoLink) * 0.01f;

        for (int i = 0; i < numSamples; ++i) {
            const float in0 = inData0[i];
            const float in1 = inData1[i];
            const float sc0 = scData0[i];
            const float sc1 = scData1[i];

            const float filtSc0 = gates[0].filterSidechain(sc0);
            const float filtSc1 = gates[1].filterSidechain(sc1);

            const float level0 = gates[0].detectLevel(filtSc0);
            const float level1 = gates[1].detectLevel(filtSc1);

            const float maxLevel = std::max(level0, level1);
            const float linkedLevel0 = (1.0f - stereoLink) * level0 + stereoLink * maxLevel;
            const float linkedLevel1 = (1.0f - stereoLink) * level1 + stereoLink * maxLevel;

            float out0 = gates[0].processWithLevel(in0, linkedLevel0, filtSc0, audition);
            float out1 = gates[1].processWithLevel(in1, linkedLevel1, filtSc1, audition);

            if (!audition) {
                out0 *= outGainLinear;
                out1 *= outGainLinear;
            }

            channelData0[i] = out0;
            channelData1[i] = out1;

            if (std::abs(in0) > maxIn) maxIn = std::abs(in0);
            if (std::abs(in1) > maxIn) maxIn = std::abs(in1);
            if (std::abs(out0) > maxOut) maxOut = std::abs(out0);
            if (std::abs(out1) > maxOut) maxOut = std::abs(out1);
        }
    } else if (channelsToProcess == 1) {
        auto* channelData = mainOut.getWritePointer(0);
        const auto* inData = mainIn.getReadPointer(0);
        const float* scData = inData;
        if (useExtSc) {
            scData = scBus.getReadPointer(0);
        }

        auto& gate = gates[0];

        for (int i = 0; i < numSamples; ++i) {
            const float in = inData[i];
            const float sc = scData[i];
            float out = gate.processSample(in, sc, audition);
            if (!audition) {
                out *= outGainLinear;
            }
            channelData[i] = out;

            if (std::abs(in) > maxIn) maxIn = std::abs(in);
            if (std::abs(out) > maxOut) maxOut = std::abs(out);
        }

        if (mainOut.getNumChannels() > 1) {
            mainOut.copyFrom(1, 0, mainOut, 0, 0, numSamples);
        }
    }

    const int gateState = static_cast<int>(gates[0].getGateState());
    lastGateState.store(gateState, std::memory_order_relaxed);

    const float inDb = juce::Decibels::gainToDecibels(maxIn, -100.0f);
    const float outDb = juce::Decibels::gainToDecibels(maxOut, -100.0f);
    const float grDb = gates[0].getGainReductionDb();
    history.push(inDb, outDb, grDb, static_cast<float>(gateState));
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

} // namespace openx::gate

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new openx::gate::PluginProcessor();
}
