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
        BandPlotComponent(std::function<const shaper::BandShaper<float> *(size_t)> shaperGetter,
                          std::function<const shaper::BandShaper<float> *(size_t)> spreadGetter,
                          SpectrumSource<float> *spectrumSource,
                          juce::AudioProcessorValueTreeState &parameters,
                          UIBase &base);

        ~BandPlotComponent() override;

        void paint(juce::Graphics &) override;

        void mouseMove(const juce::MouseEvent &) override;

        void mouseDown(const juce::MouseEvent &) override;

        void mouseDrag(const juce::MouseEvent &) override;

        void mouseUp(const juce::MouseEvent &) override;

        void mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) override;

        void mouseExit(const juce::MouseEvent &) override;

        static constexpr float xMin = 20.0f, xMax = 20000.0f;
        static constexpr float yMin = -60.0f, yMax = 12.0f;

        enum Band {
            low = 0, mid = 1, high = 2, BandNUM
        };

        // the plot coordinates, so a test can aim at the same pixels the
        // user does
        float getPlotX(float freq) const { return getX(freq); }

        float getPlotY(float dB) const { return getY(dB); }

        float getCurveY(size_t band, float x, bool isRight) const {
            return getBandCurveY(band, x, isRight);
        }

    private:
        void parameterChanged(const juce::String &, float) override;

        void handleAsyncUpdate() override;

        void timerCallback() override;

        void refreshParameters();

        void updateSpectrum();

    private:
        // coordinates
        float getX(float freq) const;

        float getY(float dB) const;

        float getFreq(float x) const;

        // helpers
        float getSplitValue(const char *ID) const;

        void setSplitValue(const char *ID, float value);

        static const char *getBandGainID(size_t band);

        float getBandGainValue(size_t band) const;

        void setBandGainValue(size_t band, float dB);

        /** 0..100, how far the two channel curves of a band sit apart */
        static const char *getSpreadID(size_t band);

        float getSpreadValue(size_t band) const;

        void setSpreadValue(size_t band, float percent);

        std::pair<float, float> getBandRange(size_t band) const;

        /** the gain the transfer curve of a band adds to a -12 dB input */
        float getBandGain(size_t band) const;

        void drawSpectrum(juce::Graphics &g, juce::Rectangle<float> plotBound) const;

        /** the level every band ends up with, drawn as a line across its range */
        void drawBandCurves(juce::Graphics &g, juce::Rectangle<float> plotBound) const;

        void drawSplitLine(juce::Graphics &g, juce::Rectangle<float> plotBound, size_t index) const;

        juce::Rectangle<float> getPlotBound() const;

        juce::Rectangle<float> getHandleBound(float x) const;

        /** The clickable strip around the level line of a band. */
        juce::Rectangle<float> getBandLineBound(size_t band) const;

        /** the y of the left or the right transfer curve of a band at this x */
        float getBandCurveY(size_t band, float x, bool isRight) const;

        /** true while the point sits on one of the curves of that band */
        bool isOnBandCurve(size_t band, juce::Point<float> position) const;

        static constexpr size_t fftOrder = 11;
        static constexpr size_t fftSize = static_cast<size_t>(1) << fftOrder;
        static constexpr size_t minHandlesDistance = 24;

        std::function<const shaper::BandShaper<float> *(size_t)> getShaper;
        std::function<const shaper::BandShaper<float> *(size_t)> getSpreadShaper;
        SpectrumSource<float> *spectrum;
        juce::AudioProcessorValueTreeState &apvts;
        UIBase &uiBase;

        juce::dsp::FFT fftForward{static_cast<int>(fftOrder)};
        juce::dsp::WindowingFunction<float> window{fftSize, juce::dsp::WindowingFunction<float>::hann};
        std::array<float, fftSize * 2> fftData{};
        std::vector<float> samples;
        std::array<float, 512> scopeData{};
        double lastSpectrumUpdate = 0.0;
        std::array<float, 2> splitValues{};
        std::array<float, BandNUM> bandGains{};
        std::array<float, BandNUM> bandGainValues{};

        int activeHandle = -1;
        int activeBand = -1;
        bool isDragging = false;
        bool isDraggingGain = false;
        bool isDraggingSpread = false;
        std::array<float, BandNUM> spreadValues{};
        float dragStartSpread = 0.f;
        bool hoveringCurve = false;
        juce::Point<float> dragStart;
        /** the little window that rides with the mouse while something is dragged */
        juce::String dragHint;
        juce::Point<float> dragHintPosition;

        juce::Point<float> lastMousePos;

        constexpr const static std::array IDs{zldsp::lowSplit::ID, zldsp::highSplit::ID, zldsp::effectIn::ID,
                                             zldsp::bandGainLow::ID, zldsp::bandGainMid::ID, zldsp::bandGainHigh::ID,
                                             zldsp::spreadLow::ID, zldsp::spreadMid::ID,
                                             zldsp::spreadHigh::ID};

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandPlotComponent)
    };
}

#endif //ZLINFLATOR_BAND_PLOT_COMPONENT_H
