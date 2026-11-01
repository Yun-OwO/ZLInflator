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

    void FooterPanel::resized() {
        auto bound = getLocalBounds().toFloat();
        if (bound.getWidth() <= 0.f || bound.getHeight() <= 0.f) {
            return;
        }
        const auto fontSize = uiBase->getFontSize();
        // one row of captions over one row of controls: the lowest edge of the
        // oversampling box meets the knob dials, the captions line the title up
        const auto knobRow = juce::jmin(fontSize * zlinterface::KnobRowHeight,
                                        bound.getHeight(),
                                        bound.getWidth() / 3.f * zlinterface::KnobRowHeight / zlinterface::KnobHeight);
        const auto boxRow = juce::jmin(fontSize * zlinterface::ControlRowHeight,
                                       juce::jmax(0.f, knobRow * zlinterface::ControlHeight / zlinterface::KnobHeight));
        if (boxRow <= 0.f || knobRow <= 0.f) {
            return;
        }
        // the knob cells are as wide as the caption of the oversampling box, so
        // the four titles all sit on one line above their controls
        const auto columnWidth = juce::jmin(bound.getWidth() / 4.f, fontSize * 5.f);
        auto strip = juce::Rectangle<float>(
                bound.getRight() - columnWidth * 4.f, bound.getY(), columnWidth * 4.f, knobRow);
        overSample->setBounds(
                strip.removeFromLeft(columnWidth).removeFromBottom(boxRow + fontSize * zlinterface::ControlCaptionHeight).toNearestInt());
        auto knobBound = strip;
        const auto unitWidth = knobBound.getWidth() / 3.f;
        inputGain->setBounds(knobBound.removeFromLeft(unitWidth).toNearestInt());
        outputGain->setBounds(knobBound.removeFromLeft(unitWidth).toNearestInt());
        wet->setBounds(knobBound.toNearestInt());
    }
}
