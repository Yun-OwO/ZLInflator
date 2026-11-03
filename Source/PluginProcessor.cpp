/*
==============================================================================
Copyright (C) 2023 - zsliu98

This file is part of ZLInflator

ZLInflator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
ZLInflator is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with ZLInflator. If not, see <https://www.gnu.org/licenses/>.
==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ZLInflatorAudioProcessor::ZLInflatorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
        : AudioProcessor(
        BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
),
#endif
          dummyProcessor(),
          parameters(*this, nullptr, juce::Identifier("ZLInflatorParameters"), zldsp::getParameterLayout()),
          states(dummyProcessor, nullptr, juce::Identifier("ZLInflatorStates"), zlstate::getParameterLayout()),
          waveShaper(*this),
          waveShaperAttach(waveShaper, parameters),
          property(states) {
    inGain.setGainDecibels(zldsp::inputGain::defaultV);
    outGain.setGainDecibels(zldsp::outputGain::defaultV);
    parameters.addParameterListener(zldsp::inputGain::ID, this);
    parameters.addParameterListener(zldsp::outputGain::ID, this);
    waveShaperAttach.addListeners();
}

ZLInflatorAudioProcessor::~ZLInflatorAudioProcessor() {
    parameters.removeParameterListener(zldsp::inputGain::ID, this);
    parameters.removeParameterListener(zldsp::outputGain::ID, this);
    // the ui state (window size, style, footer) lives on the processor now, so it
    // is written back even when the host never opens the editor
    property.saveAPVTS(states);
}

//==============================================================================
const juce::String ZLInflatorAudioProcessor::getName() const {
    return JucePlugin_Name;
}

bool ZLInflatorAudioProcessor::acceptsMidi() const {
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool ZLInflatorAudioProcessor::producesMidi() const {
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool ZLInflatorAudioProcessor::isMidiEffect() const {
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double ZLInflatorAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int ZLInflatorAudioProcessor::getNumPrograms() {
    return 1;
}

int ZLInflatorAudioProcessor::getCurrentProgram() { return 0; }

void ZLInflatorAudioProcessor::setCurrentProgram(int index) {
    juce::ignoreUnused(index);
}

const juce::String ZLInflatorAudioProcessor::getProgramName(int index) {
    juce::ignoreUnused(index);
    return {};
}

void ZLInflatorAudioProcessor::changeProgramName(int index,
                                                 const juce::String &newName) {
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void ZLInflatorAudioProcessor::prepareToPlay(double sampleRate,
                                             int samplesPerBlock) {
    // Use this method as the place to do any pre-playback
    // initialisation that you need...
    reset();
    auto channels = static_cast<juce::uint32> (juce::jmin(getMainBusNumInputChannels(), getMainBusNumOutputChannels()));
    juce::dsp::ProcessSpec spec{sampleRate, static_cast<juce::uint32> (samplesPerBlock), channels};

    inGain.prepare(spec);
    outGain.prepare(spec);
    meterIn.prepare(spec);
    meterOut.prepare(spec);
    spectrum.prepare(sampleRate);
    waveShaper.prepare(spec);
    // the DSP is only updated through parameter listeners, and hosts may
    // restore the state before prepareToPlay() is called
    applyAllParameters();
}

void ZLInflatorAudioProcessor::reset() {
    inGain.reset();
    outGain.reset();
    meterIn.reset();
    meterOut.reset();
    spectrum.reset();
    waveShaper.reset();
}

void ZLInflatorAudioProcessor::releaseResources() {
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations

bool ZLInflatorAudioProcessor::isBusesLayoutSupported(
        const BusesLayout &layouts) const {
    if (layouts.getMainInputChannelSet() != layouts.getMainOutputChannelSet())
        return false;
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

#endif

void ZLInflatorAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                            juce::MidiBuffer &midiMessages) {
    juce::ignoreUnused(midiMessages);
    juce::dsp::AudioBlock<float> block(buffer);
    process(juce::dsp::ProcessContextReplacing<float>(block));
}

void ZLInflatorAudioProcessor::process(juce::dsp::ProcessContextReplacing<float> context) {
    juce::ScopedNoDenormals noDenormals;
    auto &block = context.getOutputBlock();
    if (block.getNumChannels() == 0 || block.getNumSamples() == 0) {
        return;
    }
    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        block.getSubsetChannelBlock(static_cast<size_t>(i), 1).clear();

    inGain.process(context);
    meterIn.process(context);
    waveShaper.process(context);
    outGain.process(context);
    meterOut.process(context);
    // the spectrum should show what the plugin actually outputs
    spectrum.process(context);
}

//==============================================================================
bool ZLInflatorAudioProcessor::hasEditor() const {
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *ZLInflatorAudioProcessor::createEditor() {
    return new ZLInflatorAudioProcessorEditor(*this);
//    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void ZLInflatorAudioProcessor::getStateInformation(
        juce::MemoryBlock &destData) {
    auto tempTree = juce::ValueTree("ZLInflatorParaState");
    tempTree.appendChild(parameters.copyState(), nullptr);
    // the ui state travels with the preset as well, while ui.xml keeps the last
    // used size/style for a freshly inserted instance
    tempTree.appendChild(states.copyState(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(tempTree.createXml());
    copyXmlToBinary(*xml, destData);
}

void ZLInflatorAudioProcessor::setStateInformation(const void *data,
                                                   int sizeInBytes) {
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName("ZLInflatorParaState")) {
        auto tempTree = juce::ValueTree::fromXml(*xmlState);
        auto paraState = tempTree.getChildWithName(parameters.state.getType()).createCopy();
        if (!paraState.isValid()) {
            return;
        }
        migrateLegacyParameters(paraState);
        parameters.replaceState(paraState);
        if (auto uiState = tempTree.getChildWithName(states.state.getType()); uiState.isValid()) {
            states.replaceState(uiState);
        }
        // the host is free to restore the state before prepareToPlay(), and an
        // empty state would leave the DSP on its own defaults
        applyAllParameters();
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
    return new ZLInflatorAudioProcessor();
}

MeterSource<float> *ZLInflatorAudioProcessor::getInputMeterSource() {
    return &meterIn;
}

MeterSource<float> *ZLInflatorAudioProcessor::getOutputMeterSource() {
    return &meterOut;
}

SpectrumSource<float> *ZLInflatorAudioProcessor::getSpectrumSource() {
    return &spectrum;
}

shaper::BandShaper<float> *ZLInflatorAudioProcessor::getBandShaper(size_t band) {
    return waveShaper.getBandShaper(band);
}

shaper::BandShaper<float> *ZLInflatorAudioProcessor::getBandSpreadShaper(size_t band) {
    return waveShaper.getBandSpreadShaper(band);
}

void ZLInflatorAudioProcessor::parameterChanged(const juce::String &parameterID, float newValue) {
    if (parameterID.equalsIgnoreCase(zldsp::inputGain::ID)) {
        inGain.setGainDecibels(newValue);
    } else if (parameterID.equalsIgnoreCase(zldsp::outputGain::ID)) {
        outGain.setGainDecibels(newValue);
    }
}

void ZLInflatorAudioProcessor::applyAllParameters() {
    if (auto *value = parameters.getRawParameterValue(zldsp::inputGain::ID)) {
        parameterChanged(zldsp::inputGain::ID, value->load());
    }
    if (auto *value = parameters.getRawParameterValue(zldsp::outputGain::ID)) {
        parameterChanged(zldsp::outputGain::ID, value->load());
    }
    waveShaperAttach.applyAllParameters();
}

void ZLInflatorAudioProcessor::migrateLegacyParameters(juce::ValueTree paraState) {
    const auto findChild = [&paraState](const char *id) {
        for (const auto child: paraState) {
            if (child.getProperty(juce::Identifier("id")).toString() == id) {
                return child;
            }
        }
        return juce::ValueTree();
    };
    const auto readValue = [](const juce::ValueTree &child, float fallback) {
        if (!child.isValid()) {
            return fallback;
        }
        const auto value = child.getProperty(juce::Identifier("value"));
        return value.isVoid() ? fallback : static_cast<float>(value);
    };
    const auto writeValue = [&paraState](const char *id, float value) {
        for (const auto child: paraState) {
            if (child.getProperty(juce::Identifier("id")).toString() == id) {
                return;
            }
        }
        juce::ValueTree child(juce::Identifier("PARAM"));
        child.setProperty(juce::Identifier("id"), juce::String(id), nullptr);
        child.setProperty(juce::Identifier("value"), value, nullptr);
        paraState.appendChild(child, nullptr);
    };

    // presets written by the two shaper layout have no per band children: give
    // every band the curve and the style that used to be shared
    const auto curve = readValue(findChild(zldsp::curve1::ID), zldsp::curveLow::defaultV);
    const auto style = readValue(findChild(zldsp::style1::ID),
                                 static_cast<float>(zldsp::styleLow::defaultI));
    const std::array<std::pair<const char *, const char *>, 3> bands{{
            {zldsp::curveLow::ID, zldsp::styleLow::ID},
            {zldsp::curveMid::ID, zldsp::styleMid::ID},
            {zldsp::curveHigh::ID, zldsp::styleHigh::ID}}};
    for (const auto &band: bands) {
        writeValue(band.first, curve);
        writeValue(band.second, style);
    }
}
