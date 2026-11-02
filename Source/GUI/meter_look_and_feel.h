// ==============================================================================
// Copyright (C) 2023 - zsliu98
// This file is part of ZLInflator
//
// ZLInflator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// ZLInflator is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with ZLInflator. If not, see <https://www.gnu.org/licenses/>.
// ==============================================================================

#ifndef ZLINFLATOR_METERLOOKANDFEEL_H
#define ZLINFLATOR_METERLOOKANDFEEL_H

#include <juce_gui_basics/juce_gui_basics.h>
#include "interface_definitions.h"

namespace zlinterface {

    class MeterLookAndFeel : public juce::LookAndFeel_V4 {
    public:
        explicit MeterLookAndFeel(UIBase &base) {
            uiBase = &base;
        }

        void drawMeters(juce::Graphics &g, const juce::Rectangle<float> &bounds,
                        const std::vector<float> &rms,
                        const std::vector<float> &peak,
                        const std::vector<float> &peakMax) {

            // the card is drawn one inset inside the bounds, the scale has to
            // start inside it as well, otherwise the numbers cut through the
            // rounded top of the card
            const auto inset = uiBase->getRoundedShadowRectangleArea(bounds.toFloat(),
                                                                    uiBase->getFontSize() * .5f,
                                                                    {.blurRadius=.25f});
            auto bound = inset.withTrimmedTop(uiBase->getFontSize() * .12f);
            auto numberBound = bound;

            const auto meterWidth = bound.getWidth() / static_cast<float>(rms.size());
            numberBound = numberBound.withTrimmedTop(.05f * meterWidth);
            numberBound = numberBound.withTrimmedBottom(.05f * meterWidth);
            // the peak reading takes the head of the column, so the scale starts
            // under it instead of crossing it
            numberBound = numberBound.withTrimmedTop(.18f * numberBound.getHeight());
            drawMeterNumbers(g, numberBound);
            for (size_t i = 0; i < peakMax.size(); ++i) {
                auto localBound = bound.removeFromLeft(meterWidth);
                localBound = localBound.withSizeKeepingCentre(.9f * localBound.getWidth(),
                                                              localBound.getHeight() - 0.1f * localBound.getWidth());
                auto peakBound = localBound.removeFromTop(localBound.getHeight() * 0.12f);
                drawMeterPeakMax(g, peakMax[i], peakBound);
                auto curveTL = false, curveTR = false, curveBL = false, curveBR = false;
                if (i == 0) {
                    curveBL = true;
                } else if (i == peakMax.size() - 1) {
                    curveBR = true;
                }
                g.setColour(uiBase->getTextHideColor());
                drawMeterValue(g, peak[i], localBound, curveTL, curveTR, curveBL, curveBR);

                g.setColour(uiBase->getTextHideColor());
                drawMeterValue(g, rms[i], localBound, curveTL, curveTR, curveBL, curveBR);
            }
        }

        void setRMSRange(float minV, float maxV) {
            minRMS = minV;
            maxRMS = maxV;
        }

    private:
        const static int numNumbers = 5;
        float minRMS = -24.0f, maxRMS = 0.0f;

        UIBase *uiBase;

        void drawMeterPeakMax(juce::Graphics &g, float peakMax, const juce::Rectangle<float> &bound) {
            if (peakMax > 0) {
                g.setColour(uiBase->getTextColor());
            } else {
                g.setColour(uiBase->getTextInactiveColor());
            }
            if (uiBase->getFontSize() > 0) {
                g.setFont(uiBase->getFontSize() * FontTiny);
            } else {
                g.setFont(bound.getHeight() * 0.6f);
            }
            std::string peakMaxDisplay;
            if (peakMax < minRMS) {
                peakMaxDisplay = "-inf";
            } else {
                peakMaxDisplay = formatFloat(peakMax, 1);
            }
            // the channel column can be narrow, shrink the reading rather than let neighbours collide
            const auto textWidth = g.getCurrentFont().getStringWidthFloat(peakMaxDisplay);
            if (textWidth > bound.getWidth() * 0.96f && textWidth > 0) {
                g.setFont(g.getCurrentFont().withHeight(g.getCurrentFont().getHeight() * bound.getWidth() * 0.96f / textWidth));
            }
            g.drawSingleLineText(peakMaxDisplay,
                                 juce::roundToInt(bound.getCentre().x + g.getCurrentFont().getHorizontalScale()),
                                 juce::roundToInt(bound.getCentre().y + g.getCurrentFont().getDescent()),
                                 juce::Justification::horizontallyCentred);
        }

        void drawMeterValue(juce::Graphics &g, float value, const juce::Rectangle<float> &bound,
                            bool curveTL, bool curveTR, bool curveBL, bool curveBR) {
            value = juce::jlimit(minRMS, maxRMS, value);
            auto scale = (value - minRMS) / (maxRMS - minRMS);
            auto valueBound = bound;
            valueBound = valueBound.withTrimmedTop((1 - scale) * bound.getHeight());

            juce::Path path;
            path.addRoundedRectangle(valueBound.getX(), valueBound.getY(),
                                     valueBound.getWidth(), valueBound.getHeight(),
                                     uiBase->getFontSize() * .5f, uiBase->getFontSize() * .5f,
                                     curveTL, curveTR, curveBL, curveBR);
            g.fillPath(path);
        }

        void drawMeterNumbers(juce::Graphics &g, const juce::Rectangle<float> &bound) {
            auto period = static_cast<float>(static_cast<int>(maxRMS - minRMS)) / numNumbers;
            auto localNumber = maxRMS;
            auto numberBound = bound;
            g.setColour(uiBase->getTextColor());
            if (uiBase->getFontSize() > 0) {
                g.setFont(uiBase->getFontSize() * FontNormal);
            } else {
                g.setFont(bound.getHeight() * 0.6f);
            }
            auto centerX = numberBound.getCentreX();
            float step = 0.f;
            for (int i = 0; i < numNumbers; ++i) {
                auto scale = (maxRMS - localNumber) / (maxRMS - minRMS);
                auto centerY = numberBound.getY() + scale * numberBound.getHeight();
                if (i > 0 && centerY - step < g.getCurrentFont().getHeight() * 1.05f) {
                    // no room left for another label, the smallest values are the least useful
                    break;
                }
                step = centerY;
                g.drawSingleLineText("-" + formatFloat(std::abs(localNumber), 0) + "-",
                                     juce::roundToInt(centerX + g.getCurrentFont().getHorizontalScale()),
                                     juce::roundToInt(centerY + g.getCurrentFont().getDescent()),
                                     juce::Justification::horizontallyCentred);
                localNumber -= period;
            }
        }
    };
}
#endif //ZLINFLATOR_METERLOOKANDFEEL_H
