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
    // the switch reads as the a/b pair: a is the dry signal, b the saturated
    // one, the picked letter lit and its mate dim
    effectButton = std::make_unique<zlinterface::ButtonComponent>("", uiBase);
    effectButton->getButton().setClickingTogglesState(true);
    effectButton->setPairCaption("A", "B");
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

    // the strip of band controls runs along the bottom, the figure sits over it
    // with the meters of the right column beside it, and the column carries the
    // oversampling choice, the auto gain switch and the bypass switch above the
    // meters, every control on the grid of a cap
    const auto stripHeight = juce::jmin(inner.getHeight() * 0.34f,
                                        fontSize * (zlinterface::ControlRowHeight
                                                    + zlinterface::ControlCaptionHeight + 1.8f));
    const auto cellHeight = juce::jmin(fontSize * zlinterface::KnobRowHeight,
                                       (inner.getHeight() - stripHeight) * .28f);
    const auto switchHeight = juce::jmin(cellHeight, (inner.getHeight() - stripHeight) * .105f);
    const auto cellGap = fontSize * .05f;

    const auto cellWidth = juce::jmin(inner.getWidth() * 0.16f, fontSize * 4.6f);
    auto rightBound = inner.removeFromRight(cellWidth);
    auto overSampleCell = rightBound.removeFromTop(cellHeight);
    auto gainCell = rightBound.removeFromTop(switchHeight + cellGap);
    auto bypassCell = rightBound.removeFromTop(switchHeight + cellGap);
    // the meters run from the top of the figure to the top of the strip
    const auto figureTop = bypassCell.getBottom();
    const auto figureBottom = inner.getBottom() - stripHeight;
    const auto plotHeight = juce::jmax(0.f, figureBottom - figureTop);
    auto meterBound = rightBound.removeFromTop(plotHeight);

    // the oversampling choice ends the header row, the auto gain switch stacks
    // right under it, so the pair reads like a style box over a curve bar
    footerPanel.setOverSampleAnchor(overSampleCell.getRight());
    compensationButton->setBounds(gainCell.withSizeKeepingCentre(
            gainCell.getWidth(), juce::jmin(gainCell.getHeight(), fontSize * zlinterface::ControlHeight)
    ).toNearestInt());
    {
        const auto buttonWidth = juce::jmin(bypassCell.getWidth(), fontSize * 3.6f);
        const auto buttonHeight = juce::jmin(bypassCell.getHeight(), fontSize * zlinterface::ControlHeight);
        effectButton->setBounds(bypassCell.withSizeKeepingCentre(buttonWidth, buttonHeight).toNearestInt());
    }

    auto bandBound = inner.removeFromBottom(stripHeight);
    bandPanel.setBounds(bandBound.reduced(fontSize * .2f, fontSize * .1f).toNearestInt());

    auto headerBound = inner.removeFromTop(cellHeight);
    logoPanel.setBounds(headerBound.removeFromLeft(
            juce::jmin(headerBound.getWidth() * 0.22f, fontSize * 9.f)).toNearestInt());
    footerPanel.setBounds(headerBound.reduced(fontSize * .2f, 0.f).toNearestInt());

    meterPanel.setBounds(meterBound.toNearestInt());
    bandPlotComponent.setBounds(juce::Rectangle<float>(inner.getX(), figureTop,
                                                       inner.getWidth(), plotHeight)
                                        .reduced(fontSize * .2f, 0.f).toNearestInt());
}
