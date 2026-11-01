/*
==============================================================================
Copyright (C) 2023 - zsliu98

This file is part of ZLInflator

ZLInflator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
ZLInflator is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with ZLInflator. If not, see <https://www.gnu.org/licenses/>.
==============================================================================
*/

#include "main_panel.h"

namespace {
    // the editor is a landscape card, so the font follows the width
    float getPanelFontSize(const juce::Rectangle<float> &bound) {
        return juce::jmin(bound.getWidth() * 0.024f, bound.getHeight() * 0.048f);
    }
}

MainPanel::MainPanel(ZLInflatorAudioProcessor &p) :
        uiBase(),
        logoPanel(p, uiBase),
        bandPanel(p.parameters, uiBase),
        bandPlotComponent([&p](size_t band) -> const shaper::Shaper<float> * {
            if (auto *bandShaper = p.getBandShaper(band)) {
                return bandShaper->getActiveShaper();
            }
            return nullptr;
        }, p.getSpectrumSource(), p.parameters, uiBase),
        meterPanel(p, uiBase),
        footerPanel(p.parameters, uiBase) {
    // the bypass is the one global switch that has to stay in reach, it reads
    // as a small A/B button in the top right corner, the auto gain switch under
    // it. Both are built before the first resized(), which the resize limits
    // above the panel already trigger.
    // the switch shows the state itself: A is the dry signal, B the saturated one
    effectButton = std::make_unique<zlinterface::ButtonComponent>("", uiBase);
    effectButton->getButton().setClickingTogglesState(true);
    effectButton->setCaption("B");
    effectButton->getButton().onStateChange = [this] {
        effectButton->setCaption(effectButton->getButton().getToggleState() ? "B" : "A");
    };
    effectAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            p.parameters, zldsp::effectIn::ID, effectButton->getButton());
    addAndMakeVisible(*effectButton);
    compensationButton = std::make_unique<zlinterface::ButtonComponent>("Auto Gain", uiBase);
    compensationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            p.parameters, zldsp::autoGain::ID, compensationButton->getButton());
    addAndMakeVisible(*compensationButton);
    addAndMakeVisible(logoPanel);
    addAndMakeVisible(bandPanel);
    addAndMakeVisible(bandPlotComponent);
    addAndMakeVisible(meterPanel);
    addAndMakeVisible(footerPanel);
}

MainPanel::~MainPanel() = default;

void MainPanel::paint(juce::Graphics &g) {
    g.fillAll(uiBase.getBackgroundColor());
    auto bound = getLocalBounds().toFloat();
    const auto fontSize = getPanelFontSize(bound);
    bound = uiBase.fillRoundedShadowRectangle(g, bound, fontSize * 0.5f, {});
    uiBase.fillRoundedInnerShadowRectangle(g, bound, fontSize * 0.5f, {.blurRadius=0.45f, .flip=true});
}

void MainPanel::resized() {
    auto bound = getLocalBounds().toFloat();
    const auto fontSize = getPanelFontSize(bound);
    bound = zlinterface::getRoundedShadowRectangleArea(bound, fontSize * 0.5f, {});
    bound = zlinterface::getRoundedShadowRectangleArea(bound, fontSize * 0.5f, {});
    uiBase.setFontSize(fontSize);
    auto inner = bound.reduced(fontSize * 0.4f);
    if (inner.getWidth() <= 0 || inner.getHeight() <= 0) {
        return;
    }

    // three rows: the header with the brand and the global controls, the
    // figure, and the strip of band controls. The right hand column repeats the
    // same rows: the bypass switch, the meters level with the figure, the auto
    // gain switch in the corner under them.
    const auto rowHeight = juce::jmin(fontSize * zlinterface::KnobRowHeight,
                                      inner.getHeight() * 0.3f);
    // one row of style boxes plus the flat drag bars under them
    const auto stripHeight = juce::jmin(inner.getHeight() * 0.42f,
                                        fontSize * (zlinterface::ControlRowHeight
                                                    + zlinterface::ControlCaptionHeight + 1.8f));
    const auto plotHeight = juce::jmax(0.f, inner.getHeight() - rowHeight - stripHeight);

    const auto cellWidth = juce::jmin(inner.getWidth() * 0.16f, fontSize * 4.6f);
    auto rightBound = inner.removeFromRight(cellWidth);
    auto cornerBound = rightBound.removeFromTop(rowHeight);
    auto meterBound = rightBound.removeFromTop(plotHeight);
    auto gainBound = rightBound.removeFromTop(
            juce::jmin(fontSize * (zlinterface::ControlRowHeight + zlinterface::ControlCaptionHeight),
                       juce::jmax(0.f, rightBound.getHeight())));

    effectButton->setBounds(cornerBound.withSizeKeepingCentre(
            juce::jmin(cornerBound.getWidth(), fontSize * 3.4f),
            juce::jmin(cornerBound.getHeight(), fontSize * zlinterface::ControlRowHeight)).toNearestInt());

    auto bandBound = inner.removeFromBottom(stripHeight);
    bandPanel.setBounds(bandBound.reduced(fontSize * .2f, fontSize * .1f).toNearestInt());

    // the auto gain switch continues the column right under the meters
    compensationButton->setBounds(gainBound.toNearestInt());

    auto headerBound = inner.removeFromTop(rowHeight);
    logoPanel.setBounds(headerBound.removeFromLeft(
            juce::jmin(headerBound.getWidth() * 0.22f, fontSize * 9.f)).toNearestInt());
    footerPanel.setBounds(headerBound.reduced(fontSize * .2f, 0.f).toNearestInt());

    meterPanel.setBounds(meterBound.toNearestInt());
    bandPlotComponent.setBounds(inner.reduced(fontSize * .2f, 0.f).toNearestInt());
}
