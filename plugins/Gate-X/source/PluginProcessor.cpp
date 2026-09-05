#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace openx::gate {

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)>::createParameterLayout(Descriptors))
{
    params.initialize(apvts, Descriptors);
}

void PluginProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    for (auto& gate : gates) {
        gate.prepare(static_cast<float>(sampleRate));
    }
    setLatencySamples(static_cast<int>(openx::dsp::PredictiveGate<float, 256>::BufferMask));
}

void PluginProcessor::releaseResources() {
    for (auto& gate : gates) {
        gate.reset();
    }
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    openx::dsp::PredictiveGate<float, 256>::Parameters p;
    p.openThresholdDb  = params.get(ParamId::OpenThreshold);
    p.closeThresholdDb = params.get(ParamId::CloseThreshold);
    p.rangeDb          = params.get(ParamId::Range);
    p.attackMs         = params.get(ParamId::Attack);
    p.holdMs           = params.get(ParamId::Hold);
    p.releaseMs        = params.get(ParamId::Release);

    for (auto& gate : gates) {
        gate.setParameters(p);
    }

    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = std::min(totalNumInputChannels, 2);

    float maxIn = 0.0f;
    float maxOut = 0.0f;

    for (int ch = 0; ch < channelsToProcess; ++ch) {
        auto* channelData = buffer.getWritePointer(ch);
        auto& gate = gates[static_cast<size_t>(ch)];

        for (int i = 0; i < numSamples; ++i) {
            const float in = channelData[i];
            const float out = gate.processSample(in);
            channelData[i] = out;

            if (std::abs(in) > maxIn) maxIn = std::abs(in);
            if (std::abs(out) > maxOut) maxOut = std::abs(out);
        }
    }

    if (totalNumInputChannels == 1 && totalNumOutputChannels > 1) {
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    }

    const float inDb = juce::Decibels::gainToDecibels(maxIn, -100.0f);
    const float outDb = juce::Decibels::gainToDecibels(maxOut, -100.0f);
    const float grDb = gates[0].getGainReductionDb();
    history.push(inDb, outDb, grDb);
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
