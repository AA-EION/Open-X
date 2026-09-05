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
    for (auto& lim : limiters) {
        lim.prepare(static_cast<float>(sampleRate));
    }
    setLatencySamples(static_cast<int>(openx::dsp::BrickwallLimiter<float, 256>::LatencySamples));
}

void PluginProcessor::releaseResources() {
    for (auto& lim : limiters) {
        lim.reset();
    }
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    const float threshGain = juce::Decibels::decibelsToGain(-params.get(ParamId::Threshold));

    openx::dsp::BrickwallLimiter<float, 256>::Parameters p;
    p.ceilingDb = params.get(ParamId::Ceiling);
    p.thresholdDb = params.get(ParamId::Threshold);
    p.releaseMs = params.get(ParamId::Release);
    p.enablePhaseDispersion = (params.get(ParamId::DispersionEnable) > 0.5f);
    p.dispersionFreqHz = params.get(ParamId::DispersionFreq);
    p.dispersionQ = 0.7071f;

    for (auto& lim : limiters) {
        lim.setParameters(p);
    }

    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = std::min(totalNumInputChannels, 2);

    float maxIn = 0.0f;
    float maxOut = 0.0f;

    for (int ch = 0; ch < channelsToProcess; ++ch) {
        auto* channelData = buffer.getWritePointer(ch);
        auto& lim = limiters[static_cast<size_t>(ch)];

        for (int i = 0; i < numSamples; ++i) {
            const float in = channelData[i] * threshGain;
            const float out = lim.processSample(in);
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
    const float grDb = limiters[0].getGainReductionDb();
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

} // namespace openx::limit

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new openx::limit::PluginProcessor();
}
