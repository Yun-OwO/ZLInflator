/*
==============================================================================
Copyright (C) 2023 - zsliu98

This file is part of ZLInflator

ZLInflator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
ZLInflator is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with ZLInflator. If not, see <https://www.gnu.org/licenses/>.
==============================================================================
*/

#ifndef ZLINFLATOR_SPECTRUMSOURCE_H
#define ZLINFLATOR_SPECTRUMSOURCE_H

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

/** A lock free ring buffer of the most recent mono mix of the signal, so that
    the editor can draw a spectrum without ever blocking the audio thread. */
template<typename FloatType>
class SpectrumSource {
public:
    static constexpr size_t dataSize = static_cast<size_t>(1) << 14;

    void prepare(double newSampleRate) noexcept {
        sampleRate.store(newSampleRate);
        reset();
    }

    void reset() noexcept {
        for (auto &sample: data) {
            sample.store(0.f, std::memory_order_relaxed);
        }
        writePos.store(0, std::memory_order_release);
    }

    template<typename ProcessContext>
    void process(const ProcessContext &context) noexcept {
        if (context.usesSeparateInputAndOutputBlocks())
            context.getOutputBlock().copyFrom(context.getInputBlock());
        const auto block = context.getInputBlock();
        const auto numSamples = block.getNumSamples();
        const auto numChannels = block.getNumChannels();
        if (numSamples == 0 || numChannels == 0) {
            return;
        }
        auto pos = writePos.load(std::memory_order_relaxed);
        const auto scale = static_cast<FloatType>(1) / static_cast<FloatType>(numChannels);
        for (size_t i = 0; i < numSamples; ++i) {
            FloatType sum = 0;
            for (size_t ch = 0; ch < numChannels; ++ch) {
                sum += block.getSample(static_cast<int> (ch), static_cast<int> (i));
            }
            data[(pos + i) & (dataSize - 1)].store(sum * scale, std::memory_order_relaxed);
        }
        writePos.store(pos + numSamples, std::memory_order_release);
    }

    /** copies the newest dest.size() samples into dest, oldest first */
    void readLatest(std::vector<float> &dest) const {
        const auto numSamples = dest.size();
        if (numSamples == 0) {
            return;
        }
        const auto pos = writePos.load(std::memory_order_acquire);
        const auto start = pos - numSamples;
        for (size_t i = 0; i < numSamples; ++i) {
            dest[i] = data[(start + i) & (dataSize - 1)].load(std::memory_order_relaxed);
        }
    }

    double getSampleRate() const noexcept {
        return sampleRate.load();
    }

private:
    std::array<std::atomic<float>, dataSize> data{};
    std::atomic<size_t> writePos = 0;
    std::atomic<double> sampleRate = 44100.0;
};

#endif //ZLINFLATOR_SPECTRUM_SOURCE_H
