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

    void setWet(FloatType wet) {
        m_wet.store(wet);
        m_dry.store(static_cast<FloatType>(1) - wet);
    }

    void setBandCurve(size_t band, FloatType curve, bool compensation) {
        bandShapers[juce::jmin(band, static_cast<size_t>(numBands) - 1)]
                .setParameters(curve, compensation);
    }

    void setBandType(size_t band, size_t type) {
        bandShapers[juce::jmin(band, static_cast<size_t>(numBands) - 1)].setType(type);
    }

    void setCutoffFrequency(FloatType lowFreq, FloatType highFreq) {
        const juce::GenericScopedLock<juce::CriticalSection> processLock(processorRef->getCallbackLock());
        filters[0].setCutoffFrequency(lowFreq);
        filters[1].setCutoffFrequency(highFreq);
        // where the bass stops being widened follows the low crossover
        if (!juce::approximatelyEqual(lowSplitFrequency.load(), lowFreq)) {
            lowSplitFrequency = lowFreq;
            updateBassMono();
        }
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

    /** 0 keeps both channels on the same curve, 1 pulls them the furthest apart */
    void setStereoSpread(FloatType amount) {
        stereoSpreadAmount.store(juce::jlimit(static_cast<FloatType>(0), static_cast<FloatType>(1),
                                              amount));
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

    void reset() noexcept {
        for (size_t i = 0; i < numBands - 1; ++i) {
            filters[i].reset();
        }
        for (size_t i = 0; i < numSamplers; i++) {
            if (overSamplers[i] != nullptr)
                overSamplers[i]->reset();
        }
        for (auto &bandDiffusion: sideDiffusion) {
            for (auto &chain: bandDiffusion) {
                chain.reset();
            }
        }
        bassMonoState.fill(static_cast<FloatType>(0));
        bassMonoPrevious.fill(static_cast<FloatType>(0));
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
        prepareWidening();
        processorRef->setLatencySamples(
                static_cast<int>(overSamplers[idxSampler]->getLatencyInSamples()));
    }

    /** saturates one band: at the centre of the mid/side control the two
        channels are shaped as they are, off centre the band is taken apart and
        only the mid or only the side is shaped, the other one passes dry.
        The stereo spread works on the side of that arrangement: the side is
        fed a little more drive and then run through two lengths of allpass
        diffusion, which is what actually opens the image - the two channels
        stop being a copy of one another instead of just getting louder.
        Below the low crossover the side is progressively folded away, so the
        bass stays centred and the widening never makes the low end wobble. */
    void processBand(const juce::dsp::AudioBlock<FloatType> &block,
                     const shaper::BandShaper<FloatType> *bandShaper, size_t band) noexcept {
        const auto wet = m_wet.load();
        const auto dry = m_dry.load();
        const auto drive = driveGain.load();
        const auto midSide = midSideAmount.load();
        const auto spread = stereoSpreadAmount.load();
        const auto numSamples = block.getNumSamples();
        const auto numChannels = block.getNumChannels();
        const auto widen = numChannels > 1 &&
                           (std::abs(midSide) > static_cast<FloatType>(1e-4) ||
                            spread > static_cast<FloatType>(1e-4));
        if (widen) {
            const auto wetMid = wet * juce::jlimit(static_cast<FloatType>(0), static_cast<FloatType>(1),
                                                   static_cast<FloatType>(1) - midSide);
            const auto wetSide = wet * juce::jlimit(static_cast<FloatType>(0), static_cast<FloatType>(1),
                                                    static_cast<FloatType>(1) + midSide);
            const auto width = static_cast<FloatType>(1) + spread * spreadWidth[band];
            // the side is shaped a little harder than the mid, its harmonics
            // are what tell the two channels apart on top of the diffusion
            const auto sideDrive = drive * (static_cast<FloatType>(1) + spread * static_cast<FloatType>(0.6));
            const auto alpha = bassMonoAlpha[band];
            auto left = block.getChannelPointer(0);
            auto right = block.getChannelPointer(1);
            for (size_t s = 0; s < numSamples; ++s) {
                const auto midSignal = (left[s] + right[s]) * static_cast<FloatType>(0.5);
                const auto sideSignal = (left[s] - right[s]) * static_cast<FloatType>(0.5);
                const auto midOut = shape(*bandShaper, midSignal, wetMid,
                                          static_cast<FloatType>(1) - wetMid, drive);
                auto centred = shape(*bandShaper, sideSignal, wetSide,
                                     static_cast<FloatType>(1) - wetSide, sideDrive);
                if (alpha > static_cast<FloatType>(0)) {
                    // one pole high pass on the side: the low end loses its
                    // difference between the channels as the spread comes up
                    bassMonoState[band] = alpha * (bassMonoState[band] + centred - bassMonoPrevious[band]);
                    bassMonoPrevious[band] = centred;
                    centred += spread * (bassMonoState[band] - centred);
                }
                // the two chains are fed the same signal and fade the other
                // way round in each channel, so the pair the channels receive
                // loses its correlation as the spread comes up - at zero both
                // channels still get the untouched side
                const auto front = sideDiffusion[band][0].process(centred);
                const auto back = sideDiffusion[band][1].process(centred);
                const auto sideOutLeft = centred + spread * (front - centred);
                const auto sideOutRight = centred + spread * (back - centred);
                left[s] = midOut + sideOutLeft * width;
                right[s] = midOut - sideOutRight * width;
            }
            for (size_t ch = 2; ch < numChannels; ++ch) {
                auto data = block.getChannelPointer(ch);
                for (size_t s = 0; s < numSamples; ++s) {
                    data[s] = shape(*bandShaper, data[s], wet, dry, drive);
                }
            }
            return;
        }
        for (size_t ch = 0; ch < numChannels; ++ch) {
            auto data = block.getChannelPointer(ch);
            for (size_t s = 0; s < numSamples; ++s) {
                data[s] = shape(*bandShaper, data[s], wet, dry, drive);
            }
        }
    }

    /** a short chain of allpass delays: it smears the side signal in time
        without touching its level, so two different chains hand the left and
        the right channel two versions of the side that no longer line up */
    class SideDiffusion {
    public:
        constexpr static size_t stages() noexcept { return numStages; }

        void prepare(double sampleRate, FloatType newCoefficient,
                     const std::array<double, stages()> &milliSeconds) {
            coefficient = newCoefficient;
            for (size_t i = 0; i < numStages; ++i) {
                const auto length = juce::jmax(1, juce::roundToInt(
                        milliSeconds[i] * 0.001 * sampleRate));
                lines[i].assign(static_cast<size_t>(length), static_cast<FloatType>(0));
                writeIndex[i] = 0;
            }
        }

        void reset() noexcept {
            for (auto &line: lines) {
                std::fill(line.begin(), line.end(), static_cast<FloatType>(0));
            }
            writeIndex.fill(0);
        }

        FloatType process(FloatType x) noexcept {
            for (size_t i = 0; i < numStages; ++i) {
                auto &line = lines[i];
                if (line.empty()) {
                    continue;
                }
                const auto delayed = line[writeIndex[i]];
                const auto out = -coefficient * x + delayed;
                line[writeIndex[i]] = x + coefficient * out;
                if (++writeIndex[i] >= line.size()) {
                    writeIndex[i] = 0;
                }
                x = out;
            }
            return x;
        }

    private:
        constexpr static size_t numStages = 3;
        std::array<std::vector<FloatType>, numStages> lines;
        std::array<size_t, numStages> writeIndex{};
        FloatType coefficient = static_cast<FloatType>(0.62);
    };

    /** (re)build the diffusion lines at the rate the bands are actually
        processed at, and the bass mono filter for the current crossover */
    void prepareWidening() {
        const auto overSampledRate = sampleRate.load() *
                                     static_cast<double>(static_cast<size_t>(1) << idxSampler.load());
        if (overSampledRate <= 0.0) {
            return;
        }
        for (size_t band = 0; band < numBands; ++band) {
            const auto &base = diffusionMilliSeconds[band];
            std::array<double, SideDiffusion::stages()> left{}, right{};
            for (size_t i = 0; i < left.size(); ++i) {
                left[i] = base[i];
                // the right hand chain is longer, which is what makes the two
                // channels drift apart instead of staying one signal
                right[i] = base[i] * 1.41;
            }
            sideDiffusion[band][0].prepare(overSampledRate, static_cast<FloatType>(0.62), left);
            sideDiffusion[band][1].prepare(overSampledRate, static_cast<FloatType>(0.58), right);
        }
        updateBassMono();
    }

    /** the side is folded towards mono under the low crossover: bands below it
        get the filter, the top band keeps its side untouched */
    void updateBassMono() {
        const auto overSampledRate = sampleRate.load() *
                                     static_cast<double>(static_cast<size_t>(1) << idxSampler.load());
        if (overSampledRate <= 0.0) {
            return;
        }
        const auto corner = juce::jlimit(static_cast<FloatType>(0), static_cast<FloatType>(400),
                                         lowSplitFrequency.load() * static_cast<FloatType>(0.5));
        for (size_t band = 0; band < numBands; ++band) {
            if (band == numBands - 1 || corner <= static_cast<FloatType>(0)) {
                bassMonoAlpha[band] = static_cast<FloatType>(0);
                continue;
            }
            const auto omega = static_cast<double>(corner) * 2.0 * juce::MathConstants<double>::pi /
                               overSampledRate;
            bassMonoAlpha[band] = static_cast<FloatType>(1.0 / (1.0 + omega));
        }
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
    std::atomic<FloatType> stereoSpreadAmount = static_cast<FloatType>(0);
    // per band: how much of the doubling comes from width rather than diffusion
    std::array<FloatType, numBands> spreadWidth{
            {static_cast<FloatType>(0.05), static_cast<FloatType>(0.08), static_cast<FloatType>(0.12)}};
    // allpass lengths in milliseconds, the second chain runs 1.41 times longer
    std::array<std::array<double, 3>, numBands> diffusionMilliSeconds{{
            {2.9, 7.3, 13.1}, {1.7, 4.3, 9.7}, {0.9, 2.3, 5.9}}};
    std::array<std::array<SideDiffusion, 2>, numBands> sideDiffusion{};
    std::array<FloatType, numBands> bassMonoAlpha{{static_cast<FloatType>(0),
                                                   static_cast<FloatType>(0),
                                                   static_cast<FloatType>(0)}};
    std::array<FloatType, numBands> bassMonoState{{static_cast<FloatType>(0),
                                                   static_cast<FloatType>(0),
                                                   static_cast<FloatType>(0)}};
    std::array<FloatType, numBands> bassMonoPrevious{{static_cast<FloatType>(0),
                                                      static_cast<FloatType>(0),
                                                      static_cast<FloatType>(0)}};
    std::atomic<FloatType> lowSplitFrequency = static_cast<FloatType>(zldsp::lowSplit::defaultV);
    std::atomic<FloatType> driveGain = static_cast<FloatType>(1);
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

    static constexpr std::array<const char *, 18> getIDs() {
        return {zldsp::effectIn::ID, zldsp::wet::ID, zldsp::autoGain::ID,
                zldsp::lowSplit::ID, zldsp::highSplit::ID, zldsp::overSample::ID,
                zldsp::styleLow::ID, zldsp::curveLow::ID,
                zldsp::styleMid::ID, zldsp::curveMid::ID,
                zldsp::styleHigh::ID, zldsp::curveHigh::ID, zldsp::bandGainLow::ID, zldsp::bandGainMid::ID, zldsp::bandGainHigh::ID,
                zldsp::midSide::ID, zldsp::drive::ID, zldsp::stereoSpread::ID};
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
        } else if (parameterID == zldsp::stereoSpread::ID) {
            waveShaper->setStereoSpread(static_cast<FloatType>(newValue) / static_cast<FloatType>(100));
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
