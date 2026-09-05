#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace openx::comp {

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)>::createParameterLayout(Descriptors))
{
    params.initialize(apvts, Descriptors);
}

void PluginProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    for (auto& comp : compressors) {
        comp.prepare(static_cast<float>(sampleRate));
    }
}

void PluginProcessor::releaseResources() {
    for (auto& comp : compressors) {
        comp.reset();
    }
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    openx::dsp::CompressorEngine<float>::Parameters p;
    p.thresholdDb     = params.get(ParamId::Threshold);
    p.ratio           = params.get(ParamId::Ratio);
    p.kneeDb          = params.get(ParamId::Knee);
    p.attackMs        = params.get(ParamId::Attack);
    p.releaseMs       = params.get(ParamId::Release);
    p.makeupGainDb    = params.get(ParamId::Makeup);
    p.transientPunch  = params.get(ParamId::TransientPunch);

    for (auto& comp : compressors) {
        comp.setParameters(p);
    }

    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = std::min(totalNumInputChannels, 2);

    float maxInLinear = 0.0f;
    float maxOutLinear = 0.0f;

    for (int ch = 0; ch < channelsToProcess; ++ch) {
        auto* channelData = buffer.getWritePointer(ch);
        auto& comp = compressors[static_cast<size_t>(ch)];

        for (int i = 0; i < numSamples; ++i) {
            const float in = channelData[i];
            const float out = comp.processSample(in);
            channelData[i] = out;

            if (std::abs(in) > maxInLinear) maxInLinear = std::abs(in);
            if (std::abs(out) > maxOutLinear) maxOutLinear = std::abs(out);
        }
    }

    if (totalNumInputChannels == 1 && totalNumOutputChannels > 1) {
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    }

    const float inDb = juce::Decibels::gainToDecibels(maxInLinear, -100.0f);
    const float outDb = juce::Decibels::gainToDecibels(maxOutLinear, -100.0f);
    const float grDb = compressors[0].getGainReductionDb();
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

} // namespace openx::comp

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new openx::comp::PluginProcessor();
}
