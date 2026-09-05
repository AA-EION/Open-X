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

void PluginProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    for (auto& deesser : deessers) {
        deesser.prepare(static_cast<float>(sampleRate));
    }
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

    openx::dsp::DeEsserEngine<float>::Parameters p;
    p.frequencyHz = params.get(ParamId::Frequency);
    p.thresholdDb = params.get(ParamId::Threshold);
    p.reductionDb = params.get(ParamId::Reduction);
    p.bandwidthQ = params.get(ParamId::BandwidthQ);
    p.useLpcResidualSubtraction = (params.get(ParamId::UseLpc) > 0.5f);

    for (auto& deesser : deessers) {
        deesser.setParameters(p);
    }

    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = std::min(totalNumInputChannels, 2);

    for (int ch = 0; ch < channelsToProcess; ++ch) {
        auto* channelData = buffer.getWritePointer(ch);
        auto& deesser = deessers[static_cast<size_t>(ch)];

        for (int i = 0; i < numSamples; ++i) {
            const float out = deesser.processSample(channelData[i]);
            channelData[i] = out;

            if (ch == 0) {
                spectrumAnalyzer.pushSample(out);
            }
        }
    }

    if (totalNumInputChannels == 1 && totalNumOutputChannels > 1) {
        buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
    }
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
