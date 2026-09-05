#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace openx::mb {

PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", openx::state::ParameterManager<ParamId, static_cast<size_t>(ParamId::Count)>::createParameterLayout(Descriptors))
{
    params.initialize(apvts, Descriptors);
}

void PluginProcessor::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
    for (auto& proc : processors) {
        proc.prepare(static_cast<float>(sampleRate));
    }
}

void PluginProcessor::releaseResources() {
    for (auto& proc : processors) {
        proc.reset();
    }
}

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/) {
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    openx::dsp::MultibandProcessor3Band<float>::Parameters p;
    p.lowMidCrossoverHz = params.get(ParamId::Crossover1);
    p.midHighCrossoverHz = params.get(ParamId::Crossover2);

    p.bands[0].thresholdDb = params.get(ParamId::LowThresh);
    p.bands[0].ratio = 3.0f;
    p.bands[0].attackMs = 30.0f;
    p.bands[0].releaseMs = 150.0f;
    p.bands[0].gainDb = params.get(ParamId::LowGain);

    p.bands[1].thresholdDb = params.get(ParamId::MidThresh);
    p.bands[1].ratio = 3.0f;
    p.bands[1].attackMs = 20.0f;
    p.bands[1].releaseMs = 100.0f;
    p.bands[1].gainDb = params.get(ParamId::MidGain);

    p.bands[2].thresholdDb = params.get(ParamId::HighThresh);
    p.bands[2].ratio = 3.0f;
    p.bands[2].attackMs = 10.0f;
    p.bands[2].releaseMs = 80.0f;
    p.bands[2].gainDb = params.get(ParamId::HighGain);

    for (auto& proc : processors) {
        proc.setParameters(p);
    }

    const int numSamples = buffer.getNumSamples();
    const int channelsToProcess = std::min(totalNumInputChannels, 2);

    for (int ch = 0; ch < channelsToProcess; ++ch) {
        auto* channelData = buffer.getWritePointer(ch);
        auto& proc = processors[static_cast<size_t>(ch)];

        for (int i = 0; i < numSamples; ++i) {
            const float out = proc.processSample(channelData[i]);
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

} // namespace openx::mb

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new openx::mb::PluginProcessor();
}
