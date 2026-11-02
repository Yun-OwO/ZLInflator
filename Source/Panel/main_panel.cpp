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

// ----------------------------------------------------------------------------
ChoiceButton::ChoiceButton(ZLInflatorAudioProcessor &p, const juce::String &labelText,
                           const std::string &parameterIDToUse, zlinterface::UIBase &base) :
        ButtonComponent(labelText, base),
        parameters(p.parameters), parameter(p.parameters.getParameter(parameterIDToUse)),
        parameterID(parameterIDToUse) {
    juce::ignoreUnused(parameterID);
    numChoices = parameter != nullptr ? parameter->getNumSteps() : 0;
    getButton().setClickingTogglesState(false);
    // the toggle child swallows the clicks, so step from its own callback: it
    // fires on the release inside the cell and leaves the modifier keys at hand
    getButton().onClick = [this] {
        step(juce::ModifierKeys::currentModifiers.isRightButtonDown() ? -1 : 1);
    };
    refresh();
    parameters.addParameterListener(parameterIDToUse, this);
}

ChoiceButton::~ChoiceButton() {
    parameters.removeParameterListener(parameterID, this);
}

void ChoiceButton::parameterChanged(const juce::String &, float) {
    triggerAsyncUpdate();
}

void ChoiceButton::handleAsyncUpdate() {
    refresh();
}

void ChoiceButton::mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &details) {
    step(details.deltaY >= 0.f ? 1 : -1);
}

int ChoiceButton::getChoiceIndex() const {
    if (parameter == nullptr) {
        return 0;
    }
    return juce::roundToInt(parameter->convertFrom0to1(parameter->getValue()));
}

void ChoiceButton::goTo(int index) {
    if (parameter == nullptr || numChoices <= 0) {
        return;
    }
    const auto choice = juce::jlimit(0, numChoices - 1, index);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(choice)));
    refresh();
}

void ChoiceButton::step(int direction) {
    if (parameter == nullptr || numChoices <= 0) {
        return;
    }
    goTo((getChoiceIndex() + direction + numChoices) % numChoices);
}

void ChoiceButton::refresh() {
    if (parameter == nullptr) {
        return;
    }
    const auto index = getChoiceIndex();
    // the pill carries the name of the switch while it is off and the picked
    // value once it is on, the way the A/B switch reads
    setCaption(index == 0 && offCaption.isNotEmpty() ? offCaption
                                                     : parameter->getCurrentValueAsText());
    setEditable(index != 0);
    repaint();
}

void ChoiceButton::setOffCaption(const juce::String &text) {
    offCaption = text;
    refresh();
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
    // without a title the pill takes the whole cell, the way the A/B switch does
    compensationButton = std::make_unique<zlinterface::ButtonComponent>("Auto Gain", uiBase);
    compensationButton->setTitleVisible(false);
    compensationButton->setCaption("Auto Gain");
    compensationButton->setCaptionFollowsToggle(true);
    compensationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            p.parameters, zldsp::autoGain::ID, compensationButton->getButton());
    addAndMakeVisible(*compensationButton);
    overSampleButton = std::make_unique<ChoiceButton>(p, "", zldsp::overSample::ID, uiBase);
    overSampleButton->setTitleVisible(false);
    overSampleButton->setOffCaption("HQ");
    addAndMakeVisible(*overSampleButton);
    // the drive knob fills the free corner under the meters, away from the row
    // of global knobs it used to crowd
    driveKnob = std::make_unique<zlinterface::RotarySliderComponent>("Drive", uiBase);
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            p.parameters, zldsp::drive::ID, driveKnob->getSlider());
    addAndMakeVisible(*driveKnob);
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

    // the strip of band controls runs along the bottom, the header row of the
    // global knobs sits at the top, and the figure fills what is left between
    // them: the meter column takes the right hand edge, the two switches the
    // right end of the figure, right over the top edge of the spectrum
    const auto stripHeight = juce::jmin(inner.getHeight() * 0.34f,
                                        fontSize * (zlinterface::ControlRowHeight
                                                    + zlinterface::ControlCaptionHeight + 1.8f));
    const auto cellGap = fontSize * .05f;
    // one switch cell is one row of the very same grid the band strip uses
    const auto switchHeight = juce::jmin(fontSize * zlinterface::ControlHeight,
                                         (inner.getHeight() - stripHeight) * .18f);
    const auto switchCellWidth = juce::jmin(inner.getWidth() * 0.15f, fontSize * 4.6f);
    // the header row has to be tall enough to hold the two stacked switches
    const auto headerHeight = juce::jmax(juce::jmin(fontSize * zlinterface::KnobRowHeight,
                                                    (inner.getHeight() - stripHeight) * .28f),
                                         switchHeight * 2.f + cellGap);
    const auto figureBottom = inner.getBottom() - stripHeight;
    // the header keeps a little air over the figure, so the shadow of the knobs
    // never reaches the rounded top edge of the cards
    const auto figureTop = inner.getY() + headerHeight + fontSize * .3f;
    const auto plotHeight = juce::jmax(0.f, figureBottom - figureTop);

    const auto meterWidth = juce::jmin(inner.getWidth() * 0.14f, fontSize * 4.8f);
    auto meterColumn = inner.removeFromRight(meterWidth);
    // the two switches and the drive knob sit right of the figure, sharing the
    // right edge of the meter column, so nothing is left dangling in the gap
    const auto plotMargin = fontSize * .2f;
    const auto plotGutter = fontSize * .5f;
    const auto plotBound = juce::Rectangle<float>(inner.getX(), figureTop,
                                                  inner.getWidth() - plotGutter, plotHeight)
            .reduced(plotMargin, 0.f);
    // the switch pills end on the right edge of the meter cards, so the whole
    // right hand side reads as one column instead of leaving a gap
    const auto meterCardInset = juce::jmax(std::round(fontSize * .5f * .25f * 1.5f), 1.f)
                                + 1.42f * fontSize * .5f;
    const auto switchLeft = meterColumn.getRight() - meterCardInset - switchCellWidth;
    auto switchColumn = juce::Rectangle<float>(switchLeft, figureTop - switchHeight * 2.f - cellGap,
                                               switchCellWidth, switchHeight * 2.f + cellGap);
    auto overSampleCell = switchColumn.removeFromTop(switchHeight);
    switchColumn.removeFromTop(cellGap);
    auto gainCell = switchColumn.removeFromTop(switchHeight);
    // the A/B switch keeps its place over the meters, centred on their column
    auto bypassCell = juce::Rectangle<float>(meterColumn.getX(), figureTop - switchHeight,
                                             meterColumn.getWidth(), switchHeight);
    // the drive knob fills the empty corner under the meters, as big as the
    // split knobs of the band strip
    auto driveBound = juce::Rectangle<float>(switchLeft, figureBottom, switchCellWidth,
                                             inner.getBottom() - figureBottom);

    overSampleButton->setBounds(overSampleCell.withSizeKeepingCentre(
            switchCellWidth, switchHeight).toNearestInt());
    compensationButton->setBounds(gainCell.withSizeKeepingCentre(
            switchCellWidth, switchHeight).toNearestInt());
    effectButton->setBounds(bypassCell.withSizeKeepingCentre(switchCellWidth, switchHeight).toNearestInt());
    driveKnob->setBounds(driveBound.reduced(fontSize * .1f).toNearestInt());
    // the cards are drawn one shadow inset inside their own bounds and the soft
    // shadow lives in that margin: the bounds stay on the figure rectangle so
    // nothing bleeds over the header or the band strip, and the sculpted edge
    // the cards are known for comes back
    meterPanel.setBounds(juce::Rectangle<float>(meterColumn.getX(), figureTop,
                                                meterColumn.getWidth(), plotHeight).toNearestInt());
    bandPlotComponent.setBounds(plotBound.toNearestInt());

    auto bandBound = inner.removeFromBottom(stripHeight);
    // the strip stops short of the drive knob, so the two never share a pixel
    bandBound.setRight(juce::jmax(bandBound.getX(), switchLeft - fontSize * .3f));
    bandPanel.setBounds(bandBound.reduced(fontSize * .2f, fontSize * .1f).toNearestInt());

    auto headerBound = inner.removeFromTop(headerHeight);
    {
        // the knobs keep a column each of the very same width the band strip
        // gives them, so the header reads as the first row of that grid; they
        // stay clear of the switch column on the right
        auto knobRow = headerBound.withTrimmedRight(switchCellWidth + fontSize * .5f);
        const auto knobWidth = juce::jmin(knobRow.getWidth(),
                                          zlpanel::FooterPanel::getPreferredWidth(fontSize));
        // the brand keeps the corner and the knob row takes the middle, both on
        // the one centre line the header shares with the figures below
        const auto logoWidth = juce::jmin(fontSize * 9.f,
                                          juce::jmax(0.f, knobRow.getWidth() - knobWidth - fontSize * .4f));
        logoPanel.setBounds(knobRow.removeFromLeft(logoWidth).toNearestInt());
        logoPanel.setJustification(juce::Justification::centred);
        footerPanel.setBounds(knobRow.withSizeKeepingCentre(
                juce::jmin(knobRow.getWidth(), knobWidth),
                juce::jmin(knobRow.getHeight(), footerPanel.getPreferredHeight(fontSize))
        ).toNearestInt());
    }

}
