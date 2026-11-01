/*
==============================================================================
Copyright (C) 2023 - zsliu98

This file is part of ZLInflator

ZLInflator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
ZLInflator is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with ZLInflator. If not, see <https://www.gnu.org/licenses/>.
==============================================================================
*/

#include "band_plot_component.h"

namespace zlinterface {
    BandPlotComponent::BandPlotComponent(std::function<const shaper::Shaper<float> *(size_t)> shaperGetter,
                                         SpectrumSource<float> *spectrumSource,
                                         juce::AudioProcessorValueTreeState &parameters,
                                         UIBase &base) :
            getShaper(std::move(shaperGetter)),
            spectrum(spectrumSource),
            apvts(parameters),
            uiBase(base) {
        samples.resize(fftSize);
        // start at the floor, so the release does not have to fall there first
        scopeData.fill(yMin);
        for (const auto &ID: IDs) {
            apvts.addParameterListener(ID, this);
            parameterChanged(ID, getSplitValue(ID));
        }
        startTimerHz(30);
    }

    BandPlotComponent::~BandPlotComponent() {
        stopTimer();
        for (const auto &ID: IDs) {
            apvts.removeParameterListener(ID, this);
        }
    }

    void BandPlotComponent::paint(juce::Graphics &g) {
        auto bound = getLocalBounds().toFloat();
        const auto fontSize = uiBase.getFontSize();
        auto innerBound = uiBase.fillRoundedShadowRectangle(g, bound, fontSize * .5f,
                                                            {.mainColour=uiBase.getBackgroundColor()});
        innerBound = uiBase.fillRoundedInnerShadowRectangle(g, innerBound, fontSize * .5f,
                                                            {.blurRadius=0.45f, .flip=true});
        const auto plotBound = getPlotBound();
        if (plotBound.isEmpty()) {
            return;
        }

        // band backgrounds, separated by the split lines
        for (size_t i = 0; i < BandNUM; ++i) {
            const auto [fLow, fHigh] = getBandRange(i);
            juce::Rectangle<float> bandRect(getX(fLow), plotBound.getY(),
                                            getX(fHigh) - getX(fLow), plotBound.getHeight());
            g.setColour(uiBase.getTextColor().withAlpha(0.03f + 0.015f * static_cast<float>(i)));
            g.fillRect(bandRect);
        }

        // frequency and level grid
        {
            g.setColour(uiBase.getTextHideColor().withAlpha(0.35f));
            constexpr std::array gridFreqs{50.0f, 100.0f, 500.0f, 1000.0f, 5000.0f, 10000.0f};
            for (const auto freq: gridFreqs) {
                const auto x = getX(freq);
                g.drawLine(x, plotBound.getY(), x, plotBound.getBottom(), .5f);
            }
            for (const auto dB: {-48.0f, -36.0f, -24.0f, -12.0f}) {
                const auto y = getY(dB);
                g.drawLine(plotBound.getX(), y, plotBound.getRight(), y, .5f);
            }
        }

        // spectrum, below everything else
        drawSpectrum(g, plotBound);

        // unity line of the -12 dB reference level
        {
            g.setColour(uiBase.getTextHideColor());
            const auto y = getY(-12.f);
            g.drawLine(plotBound.getX(), y, plotBound.getRight(), y, .5f);
        }

        // transfer curves of the three bands
        drawBandCurves(g, plotBound);

        // the spectrum sits on top of the curves, the way a FabFilter plot reads
        drawSpectrum(g, plotBound);

        // the gain every band ends up with, at the top of its own band
        for (size_t i = 0; i < BandNUM; ++i) {
            const auto [fLow, fHigh] = getBandRange(i);
            const auto rectWidth = fontSize * 4.f;
            // all three readings sit in one row, so the card over the figure never covers them
            const auto midX = juce::jlimit(plotBound.getX(), plotBound.getRight() - rectWidth * 3.2f,
                                           (getX(fLow) + getX(fHigh)) * .5f - rectWidth * .5f);
            g.setFont(juce::Font(fontSize * .8f));
            g.setColour(uiBase.getTextInactiveColor());
            g.drawText(juce::String(zlinterface::formatFloat(bandGains[i], 1)) + " dB",
                       juce::Rectangle<float>(midX, plotBound.getY() + fontSize * .15f,
                                              rectWidth, fontSize * 1.2f).toNearestInt(),
                       juce::Justification::centred, false);
        }

        // split lines, drawn above the curves
        for (size_t i = 0; i < splitValues.size(); ++i) {
            drawSplitLine(g, plotBound, i);
        }
    }

    void BandPlotComponent::drawSpectrum(juce::Graphics &g, juce::Rectangle<float> plotBound) const {
        juce::Path spectrumPath;
        spectrumPath.startNewSubPath(plotBound.getX(), plotBound.getBottom());
        const auto scopeSize = static_cast<int>(scopeData.size());
        for (size_t i = 0; i < scopeData.size(); ++i) {
            const auto proportion = static_cast<float>(i) / static_cast<float>(scopeSize - 1);
            const auto freq = xMin * std::pow(xMax / xMin, proportion);
            // scopeData already holds dB, so no second conversion here
            spectrumPath.lineTo(getX(freq), getY(juce::jmax(scopeData[i], yMin - 12.f)));
        }
        spectrumPath.lineTo(plotBound.getRight(), plotBound.getBottom());
        spectrumPath.closeSubPath();
        juce::ColourGradient gradient(uiBase.getTextColor().withAlpha(0.28f), plotBound.getX(), plotBound.getY(),
                                      uiBase.getTextColor().withAlpha(0.02f), plotBound.getX(), plotBound.getBottom(),
                                      false);
        g.setGradientFill(gradient);
        g.fillPath(spectrumPath);
        g.setColour(uiBase.getTextColor().withAlpha(0.65f));
        g.strokePath(spectrumPath, juce::PathStrokeType(1.4f));
    }

    void BandPlotComponent::drawBandCurves(juce::Graphics &g, juce::Rectangle<float> plotBound) const {
        juce::ignoreUnused(plotBound);
        if (getShaper == nullptr) {
            return;
        }
        for (size_t i = 0; i < BandNUM; ++i) {
            const auto [fLow, fHigh] = getBandRange(i);
            const auto y = getY(bandGains[i]);
            const auto alpha = 0.5f + 0.18f * static_cast<float>(i);

            // the level the band ends up with, drawn across its own range
            juce::Path gainPath;
            gainPath.startNewSubPath(getX(fLow), y);
            gainPath.lineTo(getX(fHigh), y);
            g.setColour(uiBase.getTextColor().withAlpha(alpha));
            g.strokePath(gainPath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
            // a soft glow around the line, so the three bands read apart
            g.setColour(uiBase.getTextColor().withAlpha(alpha * .25f));
            g.strokePath(gainPath, juce::PathStrokeType(7.f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
        }
    }

    void BandPlotComponent::drawSplitLine(juce::Graphics &g, juce::Rectangle<float> plotBound, size_t index) const {
        const auto fontSize = uiBase.getFontSize();
        const auto x = getX(splitValues[index]);
        const auto isActive = activeHandle == static_cast<int>(index) &&
                              (isDragging || getHandleBound(x).expanded(2.f).contains(lastMousePos));
        const auto colour = isActive ? uiBase.getLineColor1() : uiBase.getTextColor().withAlpha(0.35f);
        g.setColour(colour);
        g.drawLine(x, plotBound.getY(), x, plotBound.getBottom(), isActive ? 1.5f : 1.f);

        g.setFont(juce::Font(fontSize * FontSmall));
        g.setColour(isActive ? uiBase.getLineColor1() : uiBase.getTextColor());
        const auto labelWidth = fontSize * 3.2f;
        // the low split reads to the left of its line, the high one to the right,
        // so the two can never sit on top of each other
        const auto wantedX = index == 0 ? x - labelWidth - fontSize * .2f : x + fontSize * .2f;
        const auto labelX = juce::jlimit(plotBound.getX(), plotBound.getRight() - labelWidth, wantedX);
        const auto labelY = plotBound.getBottom() - fontSize * 1.5f;
        g.drawText(zlinterface::formatFloat(splitValues[index], 0) + " Hz",
                   juce::Rectangle<float>(labelX, labelY, labelWidth, fontSize * 1.2f).toNearestInt(),
                   juce::Justification::centred, false);

        const auto handleBound = getHandleBound(x);
        uiBase.fillRoundedShadowRectangle(g, handleBound, fontSize * .25f,
                                          {.mainColour=uiBase.getBackgroundColor()});
        uiBase.fillRoundedInnerShadowRectangle(g, handleBound, fontSize * .25f,
                                               {.blurRadius=0.35f, .flip=true});
        g.setColour(isActive ? uiBase.getLineColor1() : uiBase.getTextInactiveColor());
        g.fillRoundedRectangle(handleBound.reduced(handleBound.getWidth() * 0.35f,
                                                   handleBound.getHeight() * 0.28f),
                               fontSize * 0.12f);
    }

    void BandPlotComponent::mouseMove(const juce::MouseEvent &event) {
        lastMousePos = event.position;
        const auto previous = activeHandle;
        activeHandle = -1;
        for (size_t i = 0; i < splitValues.size(); ++i) {
            if (getHandleBound(getX(splitValues[i])).expanded(2.f).contains(event.position)) {
                activeHandle = static_cast<int>(i);
            }
        }
        if (previous != activeHandle) {
            repaint();
        }
    }

    void BandPlotComponent::mouseDown(const juce::MouseEvent &event) {
        lastMousePos = event.position;
        for (size_t i = 0; i < splitValues.size(); ++i) {
            if (getHandleBound(getX(splitValues[i])).expanded(2.f).contains(event.position)) {
                activeHandle = static_cast<int>(i);
                isDragging = true;
                repaint();
                return;
            }
        }
    }

    void BandPlotComponent::mouseDrag(const juce::MouseEvent &event) {
        lastMousePos = event.position;
        if (!isDragging || activeHandle < 0) {
            return;
        }
        const auto plotBound = getPlotBound();
        if (plotBound.isEmpty()) {
            return;
        }
        const auto x = juce::jlimit(plotBound.getX(), plotBound.getRight(), event.position.x);
        const auto freq = getFreq(x);
        // the two splits may not cross or get closer than a semitone
        constexpr auto minRatio = 1.06f;
        if (activeHandle == 0) {
            const auto value = juce::jlimit(xMin, splitValues[1] / minRatio, freq);
            setSplitValue(zldsp::lowSplit::ID, value);
        } else {
            const auto value = juce::jlimit(splitValues[0] * minRatio, xMax, freq);
            setSplitValue(zldsp::highSplit::ID, value);
        }
        repaint();
    }

    void BandPlotComponent::mouseUp(const juce::MouseEvent &) {
        isDragging = false;
        repaint();
    }

    void BandPlotComponent::mouseExit(const juce::MouseEvent &) {
        if (!isDragging && activeHandle >= 0) {
            activeHandle = -1;
            repaint();
        }
    }

    void BandPlotComponent::parameterChanged(const juce::String &, float) {
        splitValues[0] = getSplitValue(zldsp::lowSplit::ID);
        splitValues[1] = getSplitValue(zldsp::highSplit::ID);
        triggerAsyncUpdate();
    }

    void BandPlotComponent::handleAsyncUpdate() {
        for (size_t i = 0; i < BandNUM; ++i) {
            bandGains[i] = getBandGain(i);
        }
        repaint();
    }

    void BandPlotComponent::timerCallback() {
        updateSpectrum();
        for (size_t i = 0; i < BandNUM; ++i) {
            bandGains[i] = getBandGain(i);
        }
        repaint();
    }

    void BandPlotComponent::updateSpectrum() {
        if (spectrum == nullptr) {
            return;
        }
        spectrum->readLatest(samples);
        std::copy(samples.begin(), samples.end(), fftData.begin());
        std::fill(fftData.begin() + static_cast<long>(fftSize), fftData.end(), 0.f);
        window.multiplyWithWindowingTable(fftData.data(), fftSize);
        fftForward.performFrequencyOnlyForwardTransform(fftData.data());

        constexpr auto numBins = fftSize / 2;
        const auto sampleRate = juce::jmax(1.0, spectrum->getSampleRate());
        const auto scopeSize = static_cast<float>(scopeData.size());
        const auto logRatio = std::log(xMax / xMin);
        for (size_t i = 0; i < scopeData.size(); ++i) {
            const auto freqLow = xMin * std::exp(logRatio * static_cast<float>(i) / scopeSize);
            const auto freqHigh = xMin * std::exp(logRatio * static_cast<float>(i + 1) / scopeSize);
            const auto toBin = [&](float freq) {
                return juce::jlimit(1.f, static_cast<float>(numBins - 1),
                                    freq * static_cast<float>(fftSize) / static_cast<float>(sampleRate));
            };
            const auto lowerBin = static_cast<size_t>(toBin(freqLow));
            const auto upperBin = static_cast<size_t>(toBin(freqHigh));
            // a scope point can span several bins down low: take the loudest of them,
            // otherwise a narrow peak falls between two points and is never drawn
            auto magnitude = 0.f;
            for (auto bin = lowerBin; bin <= juce::jmax(lowerBin, upperBin); ++bin) {
                magnitude = juce::jmax(magnitude, fftData[bin]);
            }
            // normalise the hann window and the one sided spectrum
            magnitude = magnitude * 4.f / static_cast<float>(fftSize);
            const auto dB = juce::Decibels::gainToDecibels(magnitude, -100.f);
            // fast attack, slow release
            // never sink below the bottom of the plot, or the decay curve hugs the edge for seconds
            const auto clamped = juce::jmax(dB, yMin - 12.f);
            auto &current = scopeData[i];
            current = juce::jmax(clamped, current - 1.5f);
        }
    }

    float BandPlotComponent::getX(float freq) const {
        const auto plotBound = getPlotBound();
        const auto proportion = std::log(juce::jlimit(xMin, xMax, freq) / xMin) / std::log(xMax / xMin);
        return plotBound.getX() + proportion * plotBound.getWidth();
    }

    float BandPlotComponent::getY(float dB) const {
        const auto plotBound = getPlotBound();
        const auto proportion = juce::jlimit(0.f, 1.f, (dB - yMin) / (yMax - yMin));
        return plotBound.getBottom() - proportion * plotBound.getHeight();
    }

    float BandPlotComponent::getFreq(float x) const {
        const auto plotBound = getPlotBound();
        if (plotBound.getWidth() <= 0) {
            return xMin;
        }
        const auto proportion = juce::jlimit(0.f, 1.f, (x - plotBound.getX()) / plotBound.getWidth());
        return xMin * std::pow(xMax / xMin, proportion);
    }

    float BandPlotComponent::getSplitValue(const char *ID) const {
        if (auto *value = apvts.getRawParameterValue(ID)) {
            return value->load();
        }
        return zldsp::lowSplit::defaultV;
    }

    void BandPlotComponent::setSplitValue(const char *ID, float value) {
        if (auto *parameter = apvts.getParameter(ID)) {
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        }
    }

    std::pair<float, float> BandPlotComponent::getBandRange(size_t band) const {
        const auto low = splitValues[0], high = splitValues[1];
        if (band == Band::low) {
            return {xMin, low};
        } else if (band == Band::mid) {
            return {low, high};
        }
        return {high, xMax};
    }

    float BandPlotComponent::getBandGain(size_t band) const {
        if (getShaper == nullptr) {
            return 0.f;
        }
        const auto *bandShaper = getShaper(band);
        if (bandShaper == nullptr) {
            return 0.f;
        }
        // a fixed input level keeps the line steady while playing
        const auto input = juce::Decibels::decibelsToGain(-12.f);
        const auto output = std::abs((*bandShaper)(input));
        if (output <= 0.f) {
            return yMin;
        }
        return juce::jlimit(yMin, yMax, juce::Decibels::gainToDecibels(output));
    }

    juce::Rectangle<float> BandPlotComponent::getPlotBound() const {
        const auto fontSize = uiBase.getFontSize();
        auto bound = getLocalBounds().toFloat();
        bound = uiBase.getRoundedShadowRectangleArea(bound, fontSize * .5f, {});
        bound = uiBase.getRoundedShadowRectangleArea(bound, fontSize * .5f, {});
        if (bound.getWidth() <= 0 || bound.getHeight() <= 0) {
            return {};
        }
        return bound.reduced(fontSize * 0.6f, fontSize * 0.5f);
    }

    juce::Rectangle<float> BandPlotComponent::getHandleBound(float x) const {
        const auto fontSize = uiBase.getFontSize();
        const auto plotBound = getPlotBound();
        const auto handleWidth = juce::jmax(fontSize * 0.6f, 8.f);
        const auto handleHeight = juce::jmax(fontSize * 1.1f, 16.f);
        return juce::Rectangle<float>(x - handleWidth * 0.5f,
                                      plotBound.getY() - handleHeight * 0.5f,
                                      handleWidth, handleHeight);
    }
}
