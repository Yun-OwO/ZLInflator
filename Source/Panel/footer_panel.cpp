// ==============================================================================
// Copyright (C) 2023 - zsliu98
// This file is part of ZLInflator
//
// ZLInflator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// ZLInflator is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with ZLInflator. If not, see <https://www.gnu.org/licenses/>.
// ==============================================================================

#include "footer_panel.h"

namespace zlpanel {
    namespace {
        constexpr float KnobCellWidth = 4.6f;
        constexpr float BoxColumnWidth = 5.f;
    }

    FooterPanel::FooterPanel(juce::AudioProcessorValueTreeState &audioParameters, zlinterface::UIBase &base) :
            apvts(audioParameters), uiBase(&base) {
        inputGain = std::make_unique<zlinterface::RotarySliderComponent>("In", base);
        outputGain = std::make_unique<zlinterface::RotarySliderComponent>("Out", base);
        wet = std::make_unique<zlinterface::RotarySliderComponent>("Wet", base);
        for (auto *knob : knobList) {
            addAndMakeVisible(knob->get());
        }
        const auto addAttachment = [this](const char *id, zlinterface::RotarySliderComponent *knob) {
            sliderAttachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment(
                    apvts, id, knob->getSlider()));
        };
        addAttachment(zldsp::inputGain::ID, inputGain.get());
        addAttachment(zldsp::outputGain::ID, outputGain.get());
        addAttachment(zldsp::wet::ID, wet.get());
        zlpanel::attachBoxes(*this, boxList, boxAttachments, boxIDs, apvts, base);
    }

    FooterPanel::~FooterPanel() = default;

    void FooterPanel::paint(juce::Graphics &g) {
        juce::ignoreUnused(g);
    }

    float FooterPanel::getPreferredWidth(float fontSize) {
        return fontSize * 3.f * KnobCellWidth;
    }

    void FooterPanel::setOverSampleAnchor(float rightEdge) {
        overSampleAnchorRight = rightEdge;
        resized();
    }

    void FooterPanel::setOverSampleBounds(float x, float y, float width, float boxHeight) {
        const auto fontSize = uiBase->getFontSize();
        const auto captionHeight = fontSize * zlinterface::ControlCaptionHeight;
        overSample->setBounds(juce::Rectangle<float>(x, y, width, captionHeight + boxHeight).toNearestInt());
        if (boxHeight > 0.f && width > 0.f) {
            overSample->getLabel().setBounds(
                    juce::Rectangle<int>(0, 0, juce::roundToInt(width),
                                         juce::roundToInt(captionHeight)));
            overSample->getComboBox().setBounds(
                    juce::Rectangle<int>(0, juce::roundToInt(captionHeight),
                                         juce::roundToInt(width),
                                         juce::roundToInt(boxHeight)));
        }
    }

    float FooterPanel::getPreferredHeight(float fontSize) {
        return fontSize * zlinterface::KnobRowHeight;
    }

    void FooterPanel::resized() {
        auto bound = getLocalBounds().toFloat();
        if (bound.getWidth() <= 0.f || bound.getHeight() <= 0.f) {
            return;
        }
        const auto fontSize = uiBase->getFontSize();
        // the strip ends where the column above the meters begins: the
        // oversampling cell is its last one, its caption on the caption line
        const auto rightEdge = overSampleAnchorRight > 0.f
                               ? juce::jmin(overSampleAnchorRight, bound.getRight()) : bound.getRight();
        const auto cellWidth = juce::jmin(fontSize * BoxColumnWidth, juce::jmax(0.f, rightEdge - bound.getX()));
        const auto captionHeight = fontSize * zlinterface::ControlCaptionHeight;
        const auto boxHeight = juce::jmin(fontSize * zlinterface::ControlHeight,
                                          juce::jmax(0.f, bound.getHeight() - captionHeight));
        setOverSampleBounds(rightEdge - cellWidth, bound.getY(), cellWidth, boxHeight);
        const auto knobsWidth = juce::jmax(0.f, rightEdge - cellWidth - bound.getX());
        // the knobs may spill over their share rather than get crushed, the box
        // already holds the last column of the row
        const auto knobBlockWidth = juce::jmin(juce::jmax(knobsWidth, fontSize * 3.f * KnobCellWidth),
                                               getPreferredWidth(fontSize));
        auto knobBlock = juce::Rectangle<float>(bound.getX() + (knobsWidth - knobBlockWidth) * .5f,
                                                bound.getY(), knobBlockWidth,
                                                juce::jmin(bound.getHeight(), getPreferredHeight(fontSize)));
        const auto unitWidth = knobBlock.getWidth() / 3.f;
        inputGain->setBounds(knobBlock.removeFromLeft(unitWidth).toNearestInt());
        outputGain->setBounds(knobBlock.removeFromLeft(unitWidth).toNearestInt());
        wet->setBounds(knobBlock.toNearestInt());
    }
}
