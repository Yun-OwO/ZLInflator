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
        // the oversampling choice reads as one more box shaped control, capped
        // so the knobs under it keep room for caption, dial and value
        const auto boxRow = juce::jmin(bound.getHeight() * .42f,
                                       fontSize * zlinterface::ControlRowHeight);
        overSample->setBounds(bound.removeFromTop(boxRow).toNearestInt());
        bound.removeFromTop(fontSize * .12f);
        // the knob component sizes its caption against the cell height and its
        // dial against min(width, height), so a squat cell gives a tiny dial:
        // only the caption row is squeezed, the dial keeps the room it needs
        const auto knobSize = juce::jmin(fontSize * zlinterface::ControlRowHeight * 1.4f,
                                         bound.getWidth() / 3.f);
        if (knobSize <= 0.f) {
            return;
        }
        auto knobBound = juce::Rectangle<float>(bound.getX(), bound.getY(), knobSize * 3.f, knobSize);
        const auto unitWidth = knobSize;
        inputGain->setBounds(knobBound.removeFromLeft(unitWidth).toNearestInt());
        outputGain->setBounds(knobBound.removeFromLeft(unitWidth).toNearestInt());
        wet->setBounds(knobBound.toNearestInt());
    }
}
