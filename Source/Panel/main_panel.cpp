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
    // the bypass is the one global switch that has to stay in reach
    effectButton = std::make_unique<zlinterface::ButtonComponent>("Effect In", uiBase);
    effectAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            p.parameters, zldsp::effectIn::ID, effectButton->getButton());
    addAndMakeVisible(*effectButton);
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

    // the band controls are a strip along the bottom, the split knobs sit
    // between the bands they divide
    // one row of style boxes plus the flat drag bars under them
    const auto stripHeight = juce::jmin(inner.getHeight() * 0.42f,
                                        fontSize * (zlinterface::ControlRowHeight
                                                    + zlinterface::ControlCaptionHeight + 1.5f + .3f));
    auto bandBound = inner.removeFromBottom(stripHeight);
    bandPanel.setBounds(bandBound.reduced(fontSize * .2f, fontSize * .1f).toNearestInt());

    // the meters keep the right edge, from the header down to the figure, so
    // they read as the two long bars of the layout sketch
    auto meterBound = inner.removeFromRight(juce::jmin(inner.getWidth() * 0.14f, fontSize * 5.f));
    meterPanel.setBounds(meterBound.toNearestInt());

    // the header holds the brand and the bypass switch, the global controls
    // fill the strip between them and the meters
    const auto headerHeight = juce::jmin(inner.getHeight() * 0.3f,
                                         fontSize * (zlinterface::ControlRowHeight
                                                     + zlinterface::ControlCaptionHeight
                                                     + zlinterface::KnobRowHeight));
    auto headerBound = inner.removeFromTop(headerHeight);
    logoPanel.setBounds(headerBound.removeFromLeft(
            juce::jmin(headerBound.getWidth() * 0.22f, fontSize * 9.f)).toNearestInt());
    effectButton->setBounds(headerBound.removeFromLeft(
            juce::jmin(headerBound.getWidth() * 0.16f, fontSize * 3.6f)).toNearestInt());
    footerPanel.setBounds(headerBound.reduced(fontSize * .2f, 0.f).toNearestInt());

    bandPlotComponent.setBounds(inner.reduced(fontSize * .2f, 0.f).toNearestInt());
}
