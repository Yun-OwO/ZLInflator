/*
==============================================================================
Copyright (C) 2023 - zsliu98
This file is part of ZLInflator

ZLInflator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
ZLInflator is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with ZLInflator. If not, see <https://www.gnu.org/licenses/>.
==============================================================================
*/

#include "meter_panel.h"

MeterPanel::MeterPanel(ZLInflatorAudioProcessor &p, zlinterface::UIBase &base) :
        inputBackground("IN", base),
        outputBackground("OUT", base),
        inputMeter(p.getInputMeterSource(), -40.0f, 0.0f, base),
        outputMeter(p.getOutputMeterSource(), -40.0f, 0.0f, base),
        uiBase(&base) {
    addAndMakeVisible(inputBackground);
    addAndMakeVisible(outputBackground);
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);

    // init buttons
    std::array<std::string, 1> buttonID{zldsp::autoGain::ID};
    zlpanel::attachButtons(*this, buttonList, buttonAttachments, buttonID, p.parameters, base);
}

MeterPanel::~MeterPanel() = default;

void MeterPanel::paint(juce::Graphics &) {}

void MeterPanel::resized() {
    auto bound = getLocalBounds().toFloat();
    if (bound.getWidth() <= 0.f || bound.getHeight() <= 0.f) {
        return;
    }
    const auto fontSize = uiBase->getFontSize();
    // auto gain is a box shaped button stacked under the oversampling choice:
    // both keep the same size and share the column of the strip above
    const auto boxWidth = juce::jmin(bound.getWidth(), fontSize * 6.f);
    const auto captionHeight = fontSize * zlinterface::ControlCaptionHeight;
    const auto boxHeight = juce::jmin(fontSize * zlinterface::ControlHeight,
                                      juce::jmax(0.f, bound.getHeight() * .34f - captionHeight));
    const auto columnRight = bound.getRight();
    const auto bottom = bound.getBottom();
    const auto blockHeight = captionHeight + boxHeight;
    if (boxWidth > 0.f && blockHeight > 0.f) {
        // the block keeps the caption row, the button lays itself out inside it
        compensationButton->setBounds(juce::Rectangle<float>(columnRight - boxWidth,
                                                             bottom - blockHeight,
                                                             boxWidth, blockHeight).toNearestInt());
    }
    // the meters run the whole height above the auto gain block
    auto meterArea = bound.withBottom(juce::jmax(bound.getY(),
                                                 bottom - blockHeight - fontSize * .2f));
    const auto meterWidth = juce::jmin(meterArea.getWidth() * .65f, fontSize * 3.6f);
    auto meterBound = juce::Rectangle<float>(columnRight - boxWidth + (boxWidth - meterWidth) * .5f,
                                             meterArea.getY(), meterWidth, meterArea.getHeight());
    auto inputBound = meterBound.removeFromLeft(meterWidth * .5f);
    inputBackground.setBounds(inputBound.toNearestInt());
    outputBackground.setBounds(meterBound.toNearestInt());
    inputMeter.setBounds(inputBound.toNearestInt());
    outputMeter.setBounds(meterBound.toNearestInt());
}
