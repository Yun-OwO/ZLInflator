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
    BandPlotComponent::BandPlotComponent(std::function<const shaper::BandShaper<float> *(size_t)> shaperGetter,
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

    void BandPlotComponent::refreshParameters() {
        splitValues[0] = getSplitValue(zldsp::lowSplit::ID);
        splitValues[1] = getSplitValue(zldsp::highSplit::ID);
        for (size_t i = 0; i < BandNUM; ++i) {
            bandGainValues[i] = getBandGainValue(i);
            bandGains[i] = getBandGain(i);
        }
        spreadValue = getSpreadValue();
    }

    void BandPlotComponent::paint(juce::Graphics &g) {
        // the timer drives the refresh while the editor sits idle, but the first
        // frame after opening has to be right too, so paint also tops it up
        if (juce::Time::getMillisecondCounterHiRes() - lastSpectrumUpdate >= 30.0) {
            updateSpectrum();
        }
        // read the parameters straight from the tree, a repaint can outrun the
        // async callback that announces a change
        refreshParameters();
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

        // split lines, drawn above the curves
        for (size_t i = 0; i < splitValues.size(); ++i) {
            drawSplitLine(g, plotBound, i);
        }
    }

    void BandPlotComponent::drawSpectrum(juce::Graphics &g, juce::Rectangle<float> plotBound) const {
        juce::Path spectrumPath;
        spectrumPath.startNewSubPath(plotBound.getX(), plotBound.getBottom());
        const auto scopeSize = static_cast<int>(scopeData.size());
        auto previous = juce::Point<float>(plotBound.getX(), getY(juce::jmax(scopeData[0], yMin - 12.f)));
        spectrumPath.lineTo(previous);
        for (size_t i = 1; i < scopeData.size(); ++i) {
            const auto proportion = static_cast<float>(i) / static_cast<float>(scopeSize - 1);
            const auto freq = xMin * std::pow(xMax / xMin, proportion);
            // scopeData already holds dB, so no second conversion here
            const auto point = juce::Point<float>(getX(freq), getY(juce::jmax(scopeData[i], yMin - 12.f)));
            // half way between two points keeps the trace round instead of saw toothed
            const auto middle = (previous + point) * .5f;
            spectrumPath.quadraticTo(previous.x, previous.y, middle.x, middle.y);
            previous = point;
        }
        spectrumPath.lineTo(previous);
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
        const auto fontSize = uiBase.getFontSize();
        for (size_t i = 0; i < BandNUM; ++i) {
            const auto [fLow, fHigh] = getBandRange(i);
            const auto y = getY(bandGains[i]);
            const auto alpha = 0.62f + 0.16f * static_cast<float>(i);
            const auto isActive = activeBand == static_cast<int>(i) &&
                                  (isDraggingGain || getBandLineBound(i).contains(lastMousePos));

            // the level the band ends up with, drawn across its own range
            juce::Path gainPath;
            gainPath.startNewSubPath(getX(fLow), y);
            gainPath.lineTo(getX(fHigh), y);
            // a soft glow around the line, so the three bands read apart
            g.setColour(uiBase.getTextColor().withAlpha(alpha * (isActive ? .5f : .3f)));
            g.strokePath(gainPath, juce::PathStrokeType(7.f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
            g.setColour(isActive ? uiBase.getLineColor1() : uiBase.getTextColor().withAlpha(alpha));
            g.strokePath(gainPath, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));

            // the transfer curve of the shaper this band runs, laid over the part
            // of the spectrum the band covers: the horizontal axis sweeps the
            // input level from the bottom to the top of the plot
            if (getShaper != nullptr) {
                if (const auto *shaperBundle = getShaper(i)) {
                    const auto xLow = getX(fLow);
                    const auto xHigh = getX(fHigh);
                    const auto *bandShaper = shaperBundle->getActiveShaper();
                    if (bandShaper != nullptr && xHigh - xLow > 2.f) {
                        // the spread pulls the curve of the left and the right
                        // channel apart: at zero they lie on top of each other
                        const auto spread = juce::jlimit(0.f, 1.f, spreadValue * .01f);
                        const auto split = spread > .002f;
                        constexpr int steps = 72;
                        juce::Path curves[2];
                        juce::Point<float> curveEnds[2];
                        std::array<juce::Point<float>, steps + 1> points[2];
                        const auto traceCount = split ? 2 : 1;
                        for (int trace = 0; trace < traceCount; ++trace) {
                            const auto channelGain = juce::jmax(.05f, trace == 0
                                                                             ? 1.f - spread * .75f
                                                                             : 1.f + spread * .75f);
                            for (int step = 0; step <= steps; ++step) {
                                const auto t = static_cast<float>(step) / static_cast<float>(steps);
                                const auto inputDb = yMin + (0.f - yMin) * t;
                                const auto output = std::abs((*bandShaper)(
                                        juce::Decibels::decibelsToGain(inputDb) * channelGain)) / channelGain;
                                const auto outputDb = output > 0.f
                                                      ? juce::Decibels::gainToDecibels(output)
                                                      : yMin;
                                const auto point = juce::Point<float>(xLow + (xHigh - xLow) * t,
                                                                      getY(outputDb + bandGainValues[i]));
                                points[trace][static_cast<size_t>(step)] = point;
                                curveEnds[trace] = point;
                                if (step == 0) {
                                    curves[trace].startNewSubPath(point);
                                } else {
                                    curves[trace].lineTo(point);
                                }
                            }
                        }
                        if (split) {
                            // the area between the two channels is the room the
                            // curves open up, so it reads at a glance
                            juce::Path opening;
                            opening.startNewSubPath(points[0][0]);
                            for (int step = 1; step <= steps; ++step) {
                                opening.lineTo(points[0][static_cast<size_t>(step)]);
                            }
                            for (int step = steps; step >= 0; --step) {
                                opening.lineTo(points[1][static_cast<size_t>(step)]);
                            }
                            opening.closeSubPath();
                            g.setColour(uiBase.getLineColor1().withAlpha(.16f));
                            g.fillPath(opening);
                        }
                        for (int trace = 0; trace < traceCount; ++trace) {
                            const auto shade = trace == 0 ? 1.f : .72f;
                            g.setColour(uiBase.getLineColor1().withAlpha((isActive ? .55f : .28f) * shade));
                            g.strokePath(curves[trace],
                                         juce::PathStrokeType(4.5f, juce::PathStrokeType::curved,
                                                              juce::PathStrokeType::rounded));
                            g.setColour(uiBase.getLineColor1().withAlpha((isActive ? .95f : .6f) * shade));
                            g.strokePath(curves[trace],
                                         juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                                              juce::PathStrokeType::rounded));
                            if (split) {
                                g.setFont(juce::Font(fontSize * .7f));
                                g.setColour(uiBase.getTextInactiveColor());
                                g.drawText(trace == 0 ? "L" : "R",
                                           juce::Rectangle<float>(juce::jmin(curveEnds[trace].x,
                                                                             plotBound.getRight() - fontSize),
                                                                  curveEnds[trace].y - fontSize * .95f,
                                                                  fontSize, fontSize).toNearestInt(),
                                           juce::Justification::centred, false);
                            }
                        }
                    }
                }
            }

            // the trim that line stands for, right above it
            const auto text = juce::String(bandGainValues[i] > 0.f ? "+" : "") +
                              zlinterface::formatFloat(bandGainValues[i], 1) + " dB";
            const auto rectWidth = fontSize * 3.6f;
            const auto midX = juce::jlimit(getX(fLow), juce::jmax(getX(fLow), getX(fHigh) - rectWidth),
                                           (getX(fLow) + getX(fHigh)) * .5f - rectWidth * .5f);
            g.setFont(juce::Font(fontSize * .8f));
            g.setColour(isActive ? uiBase.getLineColor1() : uiBase.getTextInactiveColor());
            g.drawText(text,
                       juce::Rectangle<float>(midX, y - fontSize * 1.45f, rectWidth, fontSize * 1.2f).toNearestInt(),
                       juce::Justification::centred, false);
        }

        // while the curves are apart, say how far they have opened
        if (spreadValue > .5f) {
            g.setFont(juce::Font(fontSize * .85f));
            g.setColour(uiBase.getTextInactiveColor());
            g.drawText("Spread " + juce::String(juce::roundToInt(spreadValue)) + " %",
                       plotBound.withHeight(fontSize * 1.5f).toNearestInt(),
                       juce::Justification::centredLeft, false);
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

        const auto labelWidth = fontSize * 3.2f;
        const auto labelHeight = fontSize * 1.2f;
        // the low split reads to the left of its own line, the high one to its right,
        // so the two can never sit on top of each other
        const auto wantedX = index == 0 ? x - labelWidth - fontSize * .2f : x + fontSize * .2f;
        const auto labelX = juce::jlimit(plotBound.getX(), plotBound.getRight() - labelWidth, wantedX);
        const auto labelY = plotBound.getY() + fontSize * .3f;
        const auto labelBound = juce::Rectangle<float>(labelX, labelY, labelWidth, labelHeight);
        // a soft plate keeps the reading off the spectrum underneath
        g.setColour(uiBase.getBackgroundColor().withAlpha(.6f));
        g.fillRoundedRectangle(labelBound, fontSize * .25f);
        g.setFont(juce::Font(fontSize * FontSmall));
        g.setColour(isActive ? uiBase.getLineColor1() : uiBase.getTextColor());
        g.drawText(zlinterface::formatFloat(splitValues[index], 0) + " Hz",
                   labelBound.toNearestInt(), juce::Justification::centred, false);

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
        const auto previousBand = activeBand;
        activeHandle = -1;
        activeBand = -1;
        for (size_t i = 0; i < splitValues.size(); ++i) {
            if (getHandleBound(getX(splitValues[i])).expanded(2.f).contains(event.position)) {
                activeHandle = static_cast<int>(i);
            }
        }
        hoveringCurve = false;
        if (activeHandle < 0) {
            for (size_t i = 0; i < BandNUM && activeBand < 0; ++i) {
                if (isOnBandCurve(i, event.position)) {
                    activeBand = static_cast<int>(i);
                    hoveringCurve = true;
                } else if (getBandLineBound(i).contains(event.position)) {
                    activeBand = static_cast<int>(i);
                }
            }
        }
        setMouseCursor(activeBand >= 0 ? juce::MouseCursor::UpDownResizeCursor
                                       : juce::MouseCursor::NormalCursor);
        if (previous != activeHandle || previousBand != activeBand) {
            repaint();
        }
    }

    void BandPlotComponent::mouseDown(const juce::MouseEvent &event) {
        lastMousePos = event.position;
        // the split handles win over the level lines they sit above
        for (size_t i = 0; i < splitValues.size(); ++i) {
            if (getHandleBound(getX(splitValues[i])).expanded(2.f).contains(event.position)) {
                activeHandle = static_cast<int>(i);
                isDragging = true;
                repaint();
                return;
            }
        }
        for (size_t i = 0; i < BandNUM; ++i) {
            if (isOnBandCurve(i, event.position)) {
                activeBand = static_cast<int>(i);
                hoveringCurve = true;
                isDraggingSpread = true;
                dragStart = event.position;
                dragStartSpread = getSpreadValue();
                repaint();
                return;
            }
        }
        for (size_t i = 0; i < BandNUM; ++i) {
            if (getBandLineBound(i).contains(event.position)) {
                activeBand = static_cast<int>(i);
                isDraggingGain = true;
                dragStart = event.position;
                repaint();
                return;
            }
        }
    }

    void BandPlotComponent::mouseDrag(const juce::MouseEvent &event) {
        lastMousePos = event.position;
        if (isDraggingSpread) {
            // one plot height of travel opens the two channels fully
            const auto plotBound = getPlotBound();
            if (plotBound.getHeight() <= 0) {
                return;
            }
            setSpreadValue(dragStartSpread +
                           (dragStart.y - event.position.y) / plotBound.getHeight() * 100.f);
            repaint();
            return;
        }
        if (isDraggingGain && activeBand >= 0) {
            // one pixel of travel is one pixel on the dB scale of the plot
            const auto plotBound = getPlotBound();
            if (plotBound.getHeight() <= 0) {
                return;
            }
            const auto start = bandGainValues[static_cast<size_t>(activeBand)] -
                               (event.position.y - dragStart.y) / plotBound.getHeight() * (yMax - yMin);
            setBandGainValue(static_cast<size_t>(activeBand), start);
            repaint();
            return;
        }
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
        isDraggingGain = false;
        isDraggingSpread = false;
        repaint();
    }

    void BandPlotComponent::mouseWheelMove(const juce::MouseEvent &event,
                                           const juce::MouseWheelDetails &wheel) {
        const auto direction = wheel.deltaY >= 0.f ? 1.f : -1.f;
        for (size_t i = 0; i < splitValues.size(); ++i) {
            if (getHandleBound(getX(splitValues[i])).expanded(2.f).contains(event.position)) {
                setSplitValue(i == 0 ? zldsp::lowSplit::ID : zldsp::highSplit::ID,
                              splitValues[i] * (direction > 0.f ? 1.02f : 1.f / 1.02f));
                repaint();
                return;
            }
        }
        for (size_t i = 0; i < BandNUM; ++i) {
            if (isOnBandCurve(i, event.position)) {
                setSpreadValue(getSpreadValue() + direction * 2.f);
                repaint();
                return;
            }
        }
        for (size_t i = 0; i < BandNUM; ++i) {
            if (getBandLineBound(i).contains(event.position)) {
                setBandGainValue(i, getBandGainValue(i) + direction * .5f);
                repaint();
                return;
            }
        }
    }

    void BandPlotComponent::mouseExit(const juce::MouseEvent &) {
        if (!isDragging && !isDraggingGain && !isDraggingSpread && (activeHandle >= 0 || activeBand >= 0)) {
            activeHandle = -1;
            activeBand = -1;
            repaint();
        }
    }

    void BandPlotComponent::parameterChanged(const juce::String &, float) {
        refreshParameters();
        triggerAsyncUpdate();
    }

    void BandPlotComponent::handleAsyncUpdate() {
        refreshParameters();
        repaint();
    }

    void BandPlotComponent::timerCallback() {
        updateSpectrum();
        refreshParameters();
        repaint();
    }

    void BandPlotComponent::updateSpectrum() {
        lastSpectrumUpdate = juce::Time::getMillisecondCounterHiRes();
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
        const auto binOf = [&](float freq) {
            return juce::jlimit(1.f, static_cast<float>(numBins - 1),
                                freq * static_cast<float>(fftSize) / static_cast<float>(sampleRate));
        };
        // the analysis follows the continuous log frequency axis of the plot: every
        // point is a triangular weighted average of the bins around its frequency,
        // so a point never holds a whole bin and the low end stops looking like steps
        for (size_t i = 0; i < scopeData.size(); ++i) {
            const auto freq = xMin * std::exp(logRatio * static_cast<float>(i) / scopeSize);
            const auto centre = binOf(freq);
            const auto lower = std::floor(centre);
            const auto upper = lower + 1.f;
            const auto fraction = centre - lower;
            const auto lowerWeight = 1.f - fraction;
            const auto lowerBin = static_cast<size_t>(lower);
            const auto upperBin = static_cast<size_t>(upper);
            // normalise the hann window and the one sided spectrum
            const auto power = fftData[lowerBin] * lowerWeight + fftData[upperBin] * fraction;
            const auto magnitude = power * 4.f / static_cast<float>(fftSize);
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
        // the line the band's level sits at: what the shaper does to a -12 dB
        // input, plus the trim the line itself stands for
        return juce::jlimit(yMin, yMax,
                            juce::Decibels::gainToDecibels(output) + bandGainValues[band]);
    }

    const char *BandPlotComponent::getBandGainID(size_t band) {
        if (band == Band::mid) {
            return zldsp::bandGainMid::ID;
        }
        if (band == Band::high) {
            return zldsp::bandGainHigh::ID;
        }
        return zldsp::bandGainLow::ID;
    }

    float BandPlotComponent::getBandGainValue(size_t band) const {
        if (auto *value = apvts.getRawParameterValue(getBandGainID(band))) {
            return value->load();
        }
        return 0.f;
    }

    void BandPlotComponent::setBandGainValue(size_t band, float dB) {
        if (auto *parameter = apvts.getParameter(getBandGainID(band))) {
            parameter->setValueNotifyingHost(parameter->convertTo0to1(dB));
        }
    }

    float BandPlotComponent::getSpreadValue() const {
        if (auto *value = apvts.getRawParameterValue(zldsp::stereoSpread::ID)) {
            return value->load();
        }
        return 0.f;
    }

    void BandPlotComponent::setSpreadValue(float percent) {
        if (auto *parameter = apvts.getParameter(zldsp::stereoSpread::ID)) {
            parameter->setValueNotifyingHost(
                    parameter->convertTo0to1(juce::jlimit(0.f, 100.f, percent)));
        }
    }

    float BandPlotComponent::getBandCurveY(size_t band, float x, float channelGain) const {
        const auto [fLow, fHigh] = getBandRange(band);
        const auto xLow = getX(fLow);
        const auto xHigh = getX(fHigh);
        if (xHigh - xLow <= 2.f || x < xLow || x > xHigh || getShaper == nullptr) {
            return yMin;
        }
        const auto *bandShaper = getShaper(band)->getActiveShaper();
        if (bandShaper == nullptr) {
            return yMin;
        }
        const auto t = juce::jlimit(0.f, 1.f, (x - xLow) / (xHigh - xLow));
        const auto inputDb = yMin + (0.f - yMin) * t;
        const auto gain = juce::jmax(.05f, channelGain);
        const auto output = std::abs((*bandShaper)(juce::Decibels::decibelsToGain(inputDb) * gain)) / gain;
        return getY(output > 0.f ? juce::Decibels::gainToDecibels(output) + bandGainValues[band] : yMin);
    }

    bool BandPlotComponent::isOnBandCurve(size_t band, juce::Point<float> position) const {
        const auto [fLow, fHigh] = getBandRange(band);
        const auto xLow = getX(fLow);
        const auto xHigh = getX(fHigh);
        if (xHigh - xLow <= 2.f || position.x < xLow || position.x > xHigh) {
            return false;
        }
        const auto tolerance = juce::jmax(uiBase.getFontSize() * .5f, 6.f);
        const auto spread = juce::jlimit(0.f, 1.f, spreadValue * .01f);
        for (const auto channelGain: {1.f - spread * .75f, 1.f + spread * .75f}) {
            if (std::abs(getBandCurveY(band, position.x, channelGain) - position.y) < tolerance) {
                return true;
            }
        }
        return false;
    }

    juce::Rectangle<float> BandPlotComponent::getBandLineBound(size_t band) const {
        const auto [fLow, fHigh] = getBandRange(band);
        const auto y = getY(bandGains[band]);
        const auto xLow = getX(fLow);
        const auto xHigh = juce::jmax(xLow, getX(fHigh));
        // a strip of a comfortable height, so the line is easy to grab
        const auto touchHeight = juce::jmax(uiBase.getFontSize() * .9f, 12.f);
        return juce::Rectangle<float>(xLow, y - touchHeight * .5f, xHigh - xLow, touchHeight);
    }

    juce::Rectangle<float> BandPlotComponent::getPlotBound() const {
        const auto fontSize = uiBase.getFontSize();
        // the card takes one inset, the graph the small margin inside it: the
        // figure has to start right under the switches, so it cannot afford the
        // second, wider inset the old panel used
        auto bound = getLocalBounds().toFloat();
        bound = uiBase.getRoundedShadowRectangleArea(bound, fontSize * .5f, {});
        if (bound.getWidth() <= 0 || bound.getHeight() <= 0) {
            return {};
        }
        return bound.reduced(fontSize * 0.45f, fontSize * 0.3f);
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
