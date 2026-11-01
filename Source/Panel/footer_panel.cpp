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
        sliderAttachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment(
                apvts, zldsp::inputGain::ID, inputGain->getSlider()));
        sliderAttachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment(
                apvts, zldsp::outputGain::ID, outputGain->getSlider()));
        sliderAttachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment(
                apvts, zldsp::wet::ID, wet->getSlider()));
        zlpanel::attachBoxes(*this, boxList, boxAttachments, boxIDs, apvts, base);
    }

    FooterPanel::~FooterPanel() = default;

    void FooterPanel::paint(juce::Graphics &g) {
        juce::ignoreUnused(g);
    }

    float FooterPanel::getPreferredWidth(float fontSize) {
        return fontSize * (3.f * KnobCellWidth + BoxColumnWidth);
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
        // the three knobs and the column of boxes are centred as one block, so
        // their captions share a line whether the strip is wider or narrower
        const auto blockWidth = juce::jmin(bound.getWidth(),
                                           getPreferredWidth(fontSize));
        auto block = juce::Rectangle<float>(bound.getCentreX() - blockWidth * .5f,
                                                  bound.getY(), blockWidth,
                                                  juce::jmin(bound.getHeight(), getPreferredHeight(fontSize)));
        // the boxes are as tall as every other control, the knobs as wide as
        // the room the block leaves for them
        const auto boxHeight = juce::jmin(fontSize * zlinterface::ControlRowHeight,
                                          block.getHeight() * .6f);
        auto boxBound = block.removeFromRight(juce::jmin(block.getWidth() / 4.f,
                                                         fontSize * BoxColumnWidth));
        const auto cellWidth = block.getWidth() / 3.f;
        // the caption and the box are placed by hand: the caption shares the
        // row of the knob captions, the box takes the standard control height
        const auto captionHeight = fontSize * zlinterface::ControlCaptionHeight;
        auto overSampleBound = juce::Rectangle<float>(boxBound.getX(), block.getY(),
                                                      boxBound.getWidth(),
                                                      captionHeight + boxHeight);
        overSample->setBounds(overSampleBound.toNearestInt());
        overSample->getLabel().setBounds(
                juce::Rectangle<int>(0, 0, juce::roundToInt(boxBound.getWidth()),
                                     juce::roundToInt(captionHeight)));
        overSample->getComboBox().setBounds(
                juce::Rectangle<int>(0, juce::roundToInt(captionHeight),
                                     juce::roundToInt(boxBound.getWidth()),
                                     juce::roundToInt(boxHeight)));
        const auto unitWidth = cellWidth;
        inputGain->setBounds(block.removeFromLeft(unitWidth).toNearestInt());
        outputGain->setBounds(block.removeFromLeft(unitWidth).toNearestInt());
        wet->setBounds(block.toNearestInt());
    }
}
