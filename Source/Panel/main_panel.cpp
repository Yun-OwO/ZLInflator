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
    // the card floats above the figure
    footerPanel.toFront(false);
    footerPanel.onExpandChanged = [this]() {
        resized();
    };
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

    // the logo sits in the top left corner
    const auto headerHeight = juce::jmin(inner.getHeight() * 0.12f, fontSize * 2.8f);
    auto headerBound = inner.removeFromTop(headerHeight);
    logoPanel.setBounds(headerBound.removeFromLeft(
            juce::jmin(headerBound.getWidth() * 0.22f, fontSize * 9.f)).toNearestInt());
    effectButton->setBounds(headerBound.removeFromLeft(
            juce::jmin(headerBound.getWidth() * 0.16f, fontSize * 3.6f)).toNearestInt());

    // the band controls are a strip along the bottom, the split knobs sit
    // between the bands they divide
    const auto stripHeight = juce::jmin(inner.getHeight() * 0.36f, fontSize * 14.f);
    auto bandBound = inner.removeFromBottom(stripHeight);
    bandPanel.setBounds(bandBound.reduced(fontSize * .2f, fontSize * .1f).toNearestInt());

    // the meters keep the right edge, the figure takes what is left
    auto meterBound = inner.removeFromRight(juce::jmin(inner.getWidth() * 0.13f, fontSize * 5.f));
    meterPanel.setBounds(meterBound.reduced(fontSize * .2f, 0.f).toNearestInt());
    bandPlotComponent.setBounds(inner.reduced(fontSize * .2f, 0.f).toNearestInt());

    // the floating card hangs over the bottom right corner of the figure,
    // small while the settings are folded away and wide when they are open
    const auto plotBound = bandPlotComponent.getBounds().toFloat();
    const auto footerHeight = footerPanel.getPreferredHeight(plotBound.getHeight());
    const auto footerWidth = footerPanel.isExpanded()
                             ? juce::jmin(plotBound.getWidth() * 0.62f, fontSize * 26.f)
                             : juce::jmin(plotBound.getWidth() * 0.3f, fontSize * 10.f);
    footerPanel.setBounds(juce::Rectangle<float>(plotBound.getRight() - footerWidth,
                                                 plotBound.getBottom() - footerHeight,
                                                 footerWidth, footerHeight).toNearestInt());
}
