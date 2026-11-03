/*
==============================================================================
Copyright (C) 2023 - zsliu98
This file is part of ZLInflator

ZLInflator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
ZLInflator is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with ZLInflator. If not, see <https://www.gnu.org/licenses/>.
==============================================================================
*/

#pragma once

#ifndef ZLWAVESHAPER
#define ZLWAVESHAPER

#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_dsp/juce_dsp.h"
#include "ShaperFunctions.h"

template<typename FloatType>
class LRFilters {
public:
    LRFilters() {
        filters[0].setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
        filters[1].setType(juce::dsp::LinkwitzRileyFilterType::highpass);
        filters[2].setType(juce::dsp::LinkwitzRileyFilterType::allpass);
    }

    void setCutoffFrequency(float freq) {
        if (!juce::approximatelyEqual(freq, filters[0].getCutoffFrequency())) {
            for (auto &f: filters) {
                f.setCutoffFrequency(freq);
            }
        }
    }

    void reset() {
        for (auto &f: filters) {
            f.reset();
        }
    }

    /** returns false when prepare() has not been called yet, so the caller can
        keep the requested factor and apply it later. */
    bool update(const int factor) {
        if (!isReady) {
            return false;
        }
        const auto safeFactor = juce::jlimit (0, 5, factor);
        const auto rate = 1u << static_cast<unsigned int> (safeFactor);
        juce::dsp::ProcessSpec spec{dupSpec.sampleRate * static_cast<double> (rate),
                                    dupSpec.numChannels,
                                    dupSpec.maximumBlockSize * rate};
        for (auto &f: filters) {
            f.prepare(spec);
            f.reset();
        }
        return true;
    }

    void prepare(const juce::dsp::ProcessSpec &spec) {
        dupSpec.sampleRate = spec.sampleRate;
        dupSpec.numChannels = spec.numChannels;
        dupSpec.maximumBlockSize = spec.maximumBlockSize;
        isReady = true;
        update(0);
    }

    template<typename ProcessContext>
    void processLow(ProcessContext &context) {
        filters[0].process(context);
    }

    template<typename ProcessContext>
    void processHigh(ProcessContext &context) {
        filters[1].process(context);
    }

    template<typename ProcessContext>
    void processAll(ProcessContext &context) {
        filters[2].process(context);
    }

private:
    std::array<juce::dsp::LinkwitzRileyFilter<FloatType>, 3> filters{};
    juce::dsp::ProcessSpec dupSpec{44100, 0, 2};
    bool isReady = false;
};

template<typename FloatType>
class WaveShaper {
public:
    explicit WaveShaper(juce::AudioProcessor &processor) {
        processorRef = &processor;
        filters[0].setCutoffFrequency(zldsp::lowSplit::defaultV);
        filters[1].setCutoffFrequency(zldsp::highSplit::defaultV);
    }

    /** the shaper of one band, bands are ordered low, mid, high */
    shaper::BandShaper<FloatType> *getBandShaper(size_t band) {
        return &bandShapers[juce::jmin(band, static_cast<size_t>(numBands) - 1)];
    }

    /** the curve the right hand channel runs, which is the one above opened by
        the spread of that band */
    shaper::BandShaper<FloatType> *getBandSpreadShaper(size_t band) {
        return &bandShapersSpread[juce::jmin(band, static_cast<size_t>(numBands) - 1)];
    }

    void setWet(FloatType wet) {
        m_wet.store(wet);
        m_dry.store(static_cast<FloatType>(1) - wet);
    }

    void setBandCurve(size_t band, FloatType curve, bool compensation) {
        const auto index = juce::jmin(band, static_cast<size_t>(numBands) - 1);
        bandCurves[index] = curve;
        bandCompensations[index] = compensation;
        updateSpreadCurve(index);
    }

    void setBandType(size_t band, size_t type) {
        const auto index = juce::jmin(band, static_cast<size_t>(numBands) - 1);
        bandShapers[index].setType(type);
        bandShapersSpread[index].setType(type);
    }

    /** 0 leaves both channels on the same curve, 1 gives the right channel the
        most difference the band can take */
    void setBandSpread(size_t band, FloatType amount) {
        const auto index = juce::jmin(band, static_cast<size_t>(numBands) - 1);
        spreadAmounts[index].store(juce::jlimit(static_cast<FloatType>(0), static_cast<FloatType>(1),
                                                amount));
        updateSpreadCurve(index);
        updateEmphasis();
    }

    void setCutoffFrequency(FloatType lowFreq, FloatType highFreq) {
        const juce::GenericScopedLock<juce::CriticalSection> processLock(processorRef->getCallbackLock());
        filters[0].setCutoffFrequency(lowFreq);
        filters[1].setCutoffFrequency(highFreq);
        lowSplitFrequency.store(lowFreq);
        highSplitFrequency.store(highFreq);
        updateEmphasis();
    }

    void setBandGain(size_t band, FloatType dB) {
        bandGains[juce::jmin(band, static_cast<size_t>(numBands) - 1)].store(
                std::pow(static_cast<FloatType>(10), dB * static_cast<FloatType>(0.05)));
    }

    /** -1 saturates the mid only, +1 the side only, 0 saturates both channels
        as they are */
    void setMidSide(FloatType amount) {
        midSideAmount.store(juce::jlimit(static_cast<FloatType>(-1), static_cast<FloatType>(1),
                                         amount));
    }

    /** every band shaper is fed with the drive, the dry path is not */
    void setDrive(FloatType dB) {
        const auto gain = std::pow(static_cast<FloatType>(10), dB * static_cast<FloatType>(0.05));
        driveGain.store(gain > static_cast<FloatType>(0) ? gain : static_cast<FloatType>(1));
    }

    void setEffectFlag(bool effectFlag) {
        effect.store(effectFlag);
    }

    void setOverSampleFactor(int overSampleFactor) {
        const juce::GenericScopedLock<juce::CriticalSection> processLock(processorRef->getCallbackLock());
        // remember the requested factor: prepare() may not have created the
        // over samplers yet (hosts are allowed to restore the state first)
        idxSampler = static_cast<size_t>(juce::jlimit(0, numSamplers - 1, overSampleFactor));
        applyOverSampleSettings();
    }

    /** one pole coefficients for the per channel emphasis: each band splits
        around its own centre. */
    void updateEmphasis() {
        const auto rate = sampleRate.load() * static_cast<double>(size_t(1) << idxSampler.load());
        const auto low = lowSplitFrequency.load();
        const auto high = highSplitFrequency.load();
        const auto centres = std::array<FloatType, 3>{
                static_cast<FloatType>(std::sqrt(20.0 * juce::jmax(20.0f, low))),
                static_cast<FloatType>(std::sqrt(juce::jmax(20.0f, low) * juce::jmax(20.0f, high))),
                static_cast<FloatType>(std::sqrt(juce::jmax(20.0f, high) * 20000.0))};
        for (size_t band = 0; band < centres.size(); ++band) {
            const auto spread = spreadAmounts[band].load();
            tiltAmount[band].store(spread * static_cast<FloatType>(0.5));
            const auto ratio = juce::jlimit(0.0, 0.45, static_cast<double>(centres[band]) / rate);
            emphasisCoefficient[band].store(static_cast<FloatType>(1.0 - std::exp(-2.0 * juce::MathConstants<double>::pi * ratio)));
        }
    }

    void reset() noexcept {
        for (auto &bandState: emphasisState) {
            bandState.fill(static_cast<FloatType>(0));
        }
        for (size_t i = 0; i < numBands - 1; ++i) {
            filters[i].reset();
        }
        for (size_t i = 0; i < numSamplers; i++) {
            if (overSamplers[i] != nullptr)
                overSamplers[i]->reset();
        }
    }

    bool isPrepared() const noexcept {
        return overSamplers[idxSampler] != nullptr;
    }

    //==============================================================================
    template<typename ProcessContext>
    void process(const ProcessContext &context) noexcept {
        const auto numSamples = context.getInputBlock().getNumSamples();
        const auto numChannels = context.getInputBlock().getNumChannels();
        const auto copyInputToOutput = [&context]() {
            if (context.usesSeparateInputAndOutputBlocks())
                context.getOutputBlock().copyFrom(context.getInputBlock());
        };
        if (context.isBypassed || overSamplers[idxSampler] == nullptr) {
            // process() should only run after prepare(), but never crash if it
            // does
            copyInputToOutput();
            return;
        }
        auto oversampled_block =
                overSamplers[idxSampler]->processSamplesUp(context.getInputBlock());
        auto oversampled_context =
                juce::dsp::ProcessContextReplacing<FloatType>(oversampled_block);
        if (effect.load()) {
            // with the effect off the up sampled block still holds the input,
            // so it simply travels down untouched - it is not the low band
            auto sepBlock =
                    juce::dsp::AudioBlock<FloatType>(bufferSeparation)
                            .getSubBlock(0, static_cast<size_t> (
                                    (static_cast<size_t>(1) << idxSampler.load()) *
                                    static_cast<size_t>(numSamples)));
            juce::dsp::AudioBlock<FloatType> blocks[numBands];
            std::vector<juce::dsp::ProcessContextReplacing<FloatType>> contexts;
            for (size_t i = 0; i < numBands; ++i) {
                blocks[i] = sepBlock.getSubsetChannelBlock(i * numChannels,
                                                           (size_t) numChannels);
                blocks[i].copyFrom(oversampled_context.getInputBlock());
                contexts.push_back(
                        juce::dsp::ProcessContextReplacing<FloatType>(blocks[i]));
            }

            // low / mid / high split, every band keeps its own shaper
            filters[0].processLow(contexts[0]);
            filters[1].processAll(contexts[0]);

            filters[0].processHigh(contexts[1]);
            contexts[2].getOutputBlock().copyFrom(contexts[1].getOutputBlock());

            filters[1].processLow(contexts[1]);
            filters[1].processHigh(contexts[2]);

            for (size_t i = 0; i < numBands; ++i) {
                processBand(blocks[i], &bandShapers[i], i);
            }
            for (size_t i = 1; i < numBands; ++i) {
                // the band trim follows the shaper, so dragging the level line
                // of a band on the figure is what sets it
                blocks[i].multiplyBy(bandGains[i].load());
                blocks[0].add(blocks[i]);
            }

            oversampled_context.getOutputBlock().copyFrom(blocks[0]);
        }
        overSamplers[idxSampler]->processSamplesDown(context.getOutputBlock());
    }

    void prepare(const juce::dsp::ProcessSpec &spec) {
        reset();
        sampleRate = spec.sampleRate;
        bufferSeparation.setSize((int) spec.numChannels * numBands,
                                 int(16 * spec.maximumBlockSize), false, false,
                                 true);
        for (size_t i = 0; i < numSamplers; ++i) {
            overSamplers[i] = std::make_unique<juce::dsp::Oversampling<FloatType >>(
                    spec.numChannels, i,
                    juce::dsp::Oversampling<FloatType>::filterHalfBandFIREquiripple, true,
                    true);
            overSamplers[i]->initProcessing(spec.maximumBlockSize);
        }
        // the split filters run at the current over sampling rate, and the
        // latency has to follow the factor that may have been restored before
        // prepare() was called
        for (auto &f: filters) {
            f.prepare(spec);
        }
        applyOverSampleSettings();
    }

private:
    // must be called with the processor callback lock held
    void applyOverSampleSettings() {
        if (overSamplers[idxSampler] == nullptr) {
            return;
        }
        for (size_t i = 0; i < numBands - 1; ++i) {
            filters[i].update(static_cast<int>(idxSampler));
        }
        processorRef->setLatencySamples(
                static_cast<int>(overSamplers[idxSampler]->getLatencyInSamples()));
        updateEmphasis();
    }

    /** saturates one band. Both channels run through the same shaper while the
        spread is at zero; as it comes up the right hand channel is handed a
        curve that saturates a little earlier and its own drive, so one band
        leaves the shapers as two different harmonic series. That difference is
        the width - it needs no side signal, so a mono input widens as well. */
    void processBand(const juce::dsp::AudioBlock<FloatType> &block,
                     const shaper::BandShaper<FloatType> *bandShaper, size_t band) noexcept {
        const auto wet = m_wet.load();
        const auto dry = m_dry.load();
        const auto drive = driveGain.load();
        const auto midSide = midSideAmount.load();
        const auto spread = spreadAmounts[band].load();
        const auto numSamples = block.getNumSamples();
        const auto numChannels = block.getNumChannels();
        if (numChannels > 1 && std::abs(midSide) > static_cast<FloatType>(1e-4)) {
            const auto wetMid = wet * juce::jlimit(static_cast<FloatType>(0), static_cast<FloatType>(1),
                                                   static_cast<FloatType>(1) - midSide);
            const auto wetSide = wet * juce::jlimit(static_cast<FloatType>(0), static_cast<FloatType>(1),
                                                    static_cast<FloatType>(1) + midSide);
            auto left = block.getChannelPointer(0);
            auto right = block.getChannelPointer(1);
            for (size_t s = 0; s < numSamples; ++s) {
                const auto midSignal = (left[s] + right[s]) * static_cast<FloatType>(0.5);
                const auto sideSignal = (left[s] - right[s]) * static_cast<FloatType>(0.5);
                const auto midOut = shape(*bandShaper, midSignal, wetMid,
                                          static_cast<FloatType>(1) - wetMid, drive);
                const auto sideOut = shape(*bandShaper, sideSignal, wetSide,
                                           static_cast<FloatType>(1) - wetSide, drive);
                left[s] = midOut + sideOut;
                right[s] = midOut - sideOut;
            }
            for (size_t ch = 2; ch < numChannels; ++ch) {
                auto data = block.getChannelPointer(ch);
                for (size_t s = 0; s < numSamples; ++s) {
                    data[s] = shape(*bandShaper, data[s], wet, dry, drive);
                }
            }
            return;
        }
        // the two channels run the same curve and the same drive; the spread
        // tilts one towards the bottom of the band and the other towards the
        // top before the shaper, so the same note comes out of the left and the
        // right hand shaper with different harmonics out of reach of any DC
        // offset - a mono input widens too
        for (size_t ch = 0; ch < numChannels; ++ch) {
            auto data = block.getChannelPointer(ch);
            const auto isRight = numChannels > 1 && (ch % 2) == 1;
            const auto *channelShaper = isRight ? &bandShapersSpread[band] : bandShaper;
            auto &emphasis = emphasisState[band][isRight ? 1 : 0];
            const auto coefficient = emphasisCoefficient[band].load();
            // the two tilts mirror each other, and their tilted halves sit on
            // opposite sides of the frequency the band is centred on, so
            // neither channel gains more of the signal than the other
            const auto amount = tiltAmount[band].load();
            const auto highGain = isRight ? static_cast<FloatType>(1) + amount : static_cast<FloatType>(1) - amount;
            const auto lowGain = isRight ? static_cast<FloatType>(1) - amount : static_cast<FloatType>(1) + amount;
            for (size_t s = 0; s < numSamples; ++s) {
                const auto sample = data[s];
                emphasis += coefficient * (sample - emphasis);
                const auto pushed = emphasis * lowGain + (sample - emphasis) * highGain;
                data[s] = shape(*bandShaper, pushed, wet, dry, drive);
            }
        }
    }

    /** keeps the curves the figure draws in step with the parameter values:
        both channels run the same curve, the spread tilts them apart in
        processBand(). */
    void updateSpreadCurve(size_t band) {
        const auto spread = spreadAmounts[band].load();
        const auto curve = bandCurves[band];
        const auto compensation = bandCompensations[band];
        const auto eased = curve * (static_cast<FloatType>(1) -
                                    spread * static_cast<FloatType>(0.35));
        const auto pushed = curve + spread * (static_cast<FloatType>(1) - curve);
        bandShapers[band].setParameters(juce::jlimit(static_cast<FloatType>(0),
                                                     static_cast<FloatType>(1), eased), compensation);
        bandShapersSpread[band].setParameters(juce::jlimit(static_cast<FloatType>(0),
                                                           static_cast<FloatType>(1), pushed),
                                              compensation);
    }

    /** the odd symmetric transfer function of one band: the shaper only sees
        -1..1, and the dry signal is blended back in. */
    static FloatType shape(const shaper::BandShaper<FloatType> &bandShaper, FloatType x,
                           FloatType wet, FloatType dry, FloatType drive) {
        const auto sgn = x > 0 ? static_cast<FloatType>(1) : static_cast<FloatType>(-1);
        const auto driven = juce::jmin(static_cast<FloatType>(1), std::abs(x) * drive);
        // the shaped part is scaled back by the drive, so a band keeps its
        // level while it saturates harder
        return bandShaper(driven) * wet * sgn / drive + x * dry;
    }

    juce::AudioProcessor *processorRef;
    constexpr static const int numSamplers = 5, numBands = 3;
    std::atomic<double> sampleRate;
    std::array<shaper::BandShaper<FloatType>, numBands> bandShapers;
    std::array<std::atomic<FloatType>, numBands> bandGains{{static_cast<FloatType>(1), static_cast<FloatType>(1), static_cast<FloatType>(1)}};
    std::atomic<FloatType> m_wet = static_cast<FloatType>(zldsp::wet::formatV(zldsp::wet::defaultV));
    std::atomic<FloatType> m_dry = static_cast<FloatType>(1) -
                                   static_cast<FloatType>(zldsp::wet::formatV(zldsp::wet::defaultV));
    std::atomic<FloatType> midSideAmount = static_cast<FloatType>(0);
    std::array<std::atomic<FloatType>, numBands> spreadAmounts{
            {static_cast<FloatType>(0), static_cast<FloatType>(0), static_cast<FloatType>(0)}};
    // the right hand channel of every band runs a curve of its own, which
    // follows the curve above and the spread of that band
    std::array<shaper::BandShaper<FloatType>, numBands> bandShapersSpread;
    std::array<FloatType, numBands> bandCurves{{static_cast<FloatType>(0),
                                                static_cast<FloatType>(0),
                                                static_cast<FloatType>(0)}};
    std::array<bool, numBands> bandCompensations{{false, false, false}};
    std::atomic<FloatType> driveGain = static_cast<FloatType>(1);
    std::atomic<FloatType> lowSplitFrequency = static_cast<FloatType>(zldsp::lowSplit::defaultV);
    std::atomic<FloatType> highSplitFrequency = static_cast<FloatType>(zldsp::highSplit::defaultV);
    std::array<std::atomic<FloatType>, numBands> tiltAmount{
            {static_cast<FloatType>(0), static_cast<FloatType>(0), static_cast<FloatType>(0)}};
    std::array<std::atomic<FloatType>, numBands> emphasisCoefficient{
            {static_cast<FloatType>(0.1), static_cast<FloatType>(0.1), static_cast<FloatType>(0.1)}};
    std::array<std::array<FloatType, 2>, numBands> emphasisState{};
    std::array<std::unique_ptr<juce::dsp::Oversampling<FloatType>>, numSamplers>
            overSamplers{};
    std::atomic<size_t> idxSampler = zldsp::overSample::defaultI;
    std::atomic<bool> effect = zldsp::effectIn::defaultV;
    std::array<LRFilters<FloatType>, numBands - 1> filters{};
    juce::AudioBuffer<FloatType> bufferSeparation;
};

template<typename FloatType>
class WaveShaperAttach : public juce::AudioProcessorValueTreeState::Listener {
public:
    explicit WaveShaperAttach(WaveShaper<FloatType> &shaper, juce::AudioProcessorValueTreeState &parameters) {
        waveShaper = &shaper;
        apvts = &parameters;
    }

    static constexpr std::array<const char *, 20> getIDs() {
        return {zldsp::effectIn::ID, zldsp::wet::ID, zldsp::autoGain::ID,
                zldsp::lowSplit::ID, zldsp::highSplit::ID, zldsp::overSample::ID,
                zldsp::styleLow::ID, zldsp::curveLow::ID,
                zldsp::styleMid::ID, zldsp::curveMid::ID,
                zldsp::styleHigh::ID, zldsp::curveHigh::ID, zldsp::bandGainLow::ID, zldsp::bandGainMid::ID, zldsp::bandGainHigh::ID,
                zldsp::midSide::ID, zldsp::drive::ID,
                zldsp::spreadLow::ID, zldsp::spreadMid::ID, zldsp::spreadHigh::ID};
    }

    void addListeners() {
        for (auto &ID: getIDs()) {
            apvts->addParameterListener(ID, this);
        }
    }

    /** push every audio parameter value into the DSP. Called after prepare() so
        that the DSP always starts from the parameter values, no matter whether
        the host restored the state before or after prepareToPlay(). */
    void applyAllParameters() {
        for (auto &ID: getIDs()) {
            if (auto *value = apvts->getRawParameterValue(ID)) {
                parameterChanged(ID, value->load());
            }
        }
    }

    void parameterChanged(const juce::String &parameterID, float newValue) override {
        if (parameterID == zldsp::wet::ID) {
            waveShaper->setWet(static_cast<FloatType>(newValue) / static_cast<FloatType>(100));
        } else if (parameterID == zldsp::autoGain::ID || parameterID == zldsp::curveLow::ID ||
                   parameterID == zldsp::curveMid::ID || parameterID == zldsp::curveHigh::ID) {
            const auto compensation = loadValue(zldsp::autoGain::ID) > 0.5f;
            const auto curves = std::array<float, 3>{loadValue(zldsp::curveLow::ID),
                                                     loadValue(zldsp::curveMid::ID),
                                                     loadValue(zldsp::curveHigh::ID)};
            for (size_t i = 0; i < curves.size(); ++i) {
                waveShaper->setBandCurve(i, static_cast<FloatType>(curves[i]) /
                                            static_cast<FloatType>(100), compensation);
            }
        } else if (parameterID == zldsp::lowSplit::ID || parameterID == zldsp::highSplit::ID) {
            auto lowSplit = loadValue(zldsp::lowSplit::ID);
            auto highSplit = loadValue(zldsp::highSplit::ID);
            if (parameterID == zldsp::lowSplit::ID) {
                lowSplit = newValue;
            } else {
                highSplit = newValue;
            }
            waveShaper->setCutoffFrequency(lowSplit, highSplit);
        } else if (parameterID == zldsp::effectIn::ID) {
            waveShaper->setEffectFlag(newValue > 0.5f);
        } else if (parameterID == zldsp::styleLow::ID || parameterID == zldsp::styleMid::ID ||
                   parameterID == zldsp::styleHigh::ID) {
            size_t band = 0;
            if (parameterID == zldsp::styleMid::ID) {
                band = 1;
            } else if (parameterID == zldsp::styleHigh::ID) {
                band = 2;
            }
            waveShaper->setBandType(band, static_cast<size_t>(newValue));
        } else if (parameterID == zldsp::overSample::ID) {
            waveShaper->setOverSampleFactor(static_cast<int>(newValue));
        } else if (parameterID == zldsp::midSide::ID) {
            waveShaper->setMidSide(static_cast<FloatType>(newValue) / static_cast<FloatType>(100));
        } else if (parameterID == zldsp::drive::ID) {
            waveShaper->setDrive(static_cast<FloatType>(newValue));
        } else if (parameterID == zldsp::spreadLow::ID || parameterID == zldsp::spreadMid::ID ||
                   parameterID == zldsp::spreadHigh::ID) {
            size_t band = 0;
            if (parameterID == zldsp::spreadMid::ID) {
                band = 1;
            } else if (parameterID == zldsp::spreadHigh::ID) {
                band = 2;
            }
            waveShaper->setBandSpread(band, static_cast<FloatType>(newValue) /
                                             static_cast<FloatType>(100));
        }
    }

private:
    /** getRawParameterValue() returns nullptr for an unknown ID, and the
        returned pointer must not be dereferenced then. */
    float loadValue(const char *ID) const {
        if (auto *value = apvts->getRawParameterValue(ID)) {
            return value->load();
        }
        return 0.0f;
    }

    WaveShaper<FloatType> *waveShaper;
    juce::AudioProcessorValueTreeState *apvts;
};

#endif
