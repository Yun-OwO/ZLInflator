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
        // the three knobs and the box are centred as one block, so their
        // captions share a line whether the strip is wider or narrower
        const auto blockWidth = juce::jmin(bound.getWidth(), getPreferredWidth(fontSize));
        auto block = juce::Rectangle<float>(bound.getCentreX() - blockWidth * .5f,
                                            bound.getY(), blockWidth,
                                            juce::jmin(bound.getHeight(), getPreferredHeight(fontSize)));
        auto boxBound = block.removeFromRight(juce::jmin(block.getWidth() / 4.f,
                                                         fontSize * BoxColumnWidth));
        // the caption and the box are placed by hand: the caption shares the
        // row of the knob captions, the box keeps the standard control height
        const auto captionHeight = fontSize * zlinterface::ControlCaptionHeight;
        const auto boxHeight = juce::jmin(fontSize * zlinterface::ControlHeight,
                                          juce::jmax(0.f, block.getHeight() - captionHeight));
        overSample->setBounds(juce::Rectangle<float>(boxBound.getX(), block.getY(),
                                                     boxBound.getWidth(),
                                                     captionHeight + boxHeight).toNearestInt());
        if (boxHeight > 0.f) {
            overSample->getLabel().setBounds(
                    juce::Rectangle<int>(0, 0, juce::roundToInt(boxBound.getWidth()),
                                         juce::roundToInt(captionHeight)));
            overSample->getComboBox().setBounds(
                    juce::Rectangle<int>(0, juce::roundToInt(captionHeight),
                                         juce::roundToInt(boxBound.getWidth()),
                                         juce::roundToInt(boxHeight)));
        }
        const auto unitWidth = block.getWidth() / 3.f;
        inputGain->setBounds(block.removeFromLeft(unitWidth).toNearestInt());
        outputGain->setBounds(block.removeFromLeft(unitWidth).toNearestInt());
        wet->setBounds(block.toNearestInt());
    }
}
