/*
==============================================================================
Copyright (C) 2023 - zsliu98

This file is part of ZLInflator

ZLInflator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
ZLInflator is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with ZLInflator. If not, see <https://www.gnu.org/licenses/>.
==============================================================================
*/

#ifndef ZLINFLATOR_BAND_PLOT_COMPONENT_H
#define ZLINFLATOR_BAND_PLOT_COMPONENT_H

#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "../DSP/dsp_defines.h"
#include "../DSP/ShaperFunctions.h"
#include "../DSP/SpectrumSource.h"
#include "interface_definitions.h"

namespace zlinterface {
    /** The figure of the whole plugin: a spectrum analyzer with the transfer
        curves of the three bands drawn on top of it, the two split lines with
        their drag handles, and the gain every band ends up with.

        dragging a split line or its handle writes the split parameter, so the
        plot and the knobs on its left are always in sync. */
    class BandPlotComponent : public juce::Component,
                              private juce::AudioProcessorValueTreeState::Listener,
                              private juce::AsyncUpdater,
                              private juce::Timer {
    public:
        BandPlotComponent(std::function<const shaper::Shaper<float> *(size_t)> shaperGetter,
                          SpectrumSource<float> *spectrumSource,
                          juce::AudioProcessorValueTreeState &parameters,
                          UIBase &base);

        ~BandPlotComponent() override;

        void paint(juce::Graphics &) override;

        void mouseMove(const juce::MouseEvent &) override;

        void mouseDown(const juce::MouseEvent &) override;

        void mouseDrag(const juce::MouseEvent &) override;

        void mouseUp(const juce::MouseEvent &) override;

        void mouseExit(const juce::MouseEvent &) override;

        static constexpr float xMin = 20.0f, xMax = 20000.0f;
        static constexpr float yMin = -60.0f, yMax = 12.0f;

        enum Band {
            low = 0, mid = 1, high = 2, BandNUM
        };

    private:
        void parameterChanged(const juce::String &, float) override;

        void handleAsyncUpdate() override;

        void timerCallback() override;

        void updateSpectrum();

    private:
        // coordinates
        float getX(float freq) const;

        float getY(float dB) const;

        float getFreq(float x) const;

        // helpers
        float getSplitValue(const char *ID) const;

        void setSplitValue(const char *ID, float value);

        std::pair<float, float> getBandRange(size_t band) const;

        /** the gain the transfer curve of a band adds to a -12 dB input */
        float getBandGain(size_t band) const;

        void drawSpectrum(juce::Graphics &g, juce::Rectangle<float> plotBound) const;

        /** the level every band ends up with, drawn as a line across its range */
        void drawBandCurves(juce::Graphics &g, juce::Rectangle<float> plotBound) const;

        void drawSplitLine(juce::Graphics &g, juce::Rectangle<float> plotBound, size_t index) const;

        juce::Rectangle<float> getPlotBound() const;

        juce::Rectangle<float> getHandleBound(float x) const;

        static constexpr size_t fftOrder = 11;
        static constexpr size_t fftSize = static_cast<size_t>(1) << fftOrder;
        static constexpr size_t minHandlesDistance = 24;

        std::function<const shaper::Shaper<float> *(size_t)> getShaper;
        SpectrumSource<float> *spectrum;
        juce::AudioProcessorValueTreeState &apvts;
        UIBase &uiBase;

        juce::dsp::FFT fftForward{static_cast<int>(fftOrder)};
        juce::dsp::WindowingFunction<float> window{fftSize, juce::dsp::WindowingFunction<float>::hann};
        std::array<float, fftSize * 2> fftData{};
        std::vector<float> samples;
        std::array<float, 512> scopeData{};
        std::array<float, 2> splitValues{};
        std::array<float, BandNUM> bandGains{};

        int activeHandle = -1;
        bool isDragging = false;

        juce::Point<float> lastMousePos;

        constexpr const static std::array IDs{zldsp::lowSplit::ID, zldsp::highSplit::ID, zldsp::effectIn::ID};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandPlotComponent)
    };
}

#endif //ZLINFLATOR_BAND_PLOT_COMPONENT_H
