// ==============================================================================
// Copyright (C) 2023 - zsliu98
// This file is part of ZLInflator
//
// ZLInflator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// ZLInflator is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with ZLInflator. If not, see <https://www.gnu.org/licenses/>.
// ==============================================================================

#include "band_panel.h"

namespace {
    void layoutBandBlock(juce::Rectangle<float> bandBound, zlinterface::ComboboxComponent &box,
                         zlinterface::LinearSliderComponent &slider, float fontSize) {
        if (bandBound.isEmpty()) {
            return;
        }
        const auto boxHeight = bandBound.getHeight() * 0.34f;
        box.setBounds(bandBound.removeFromTop(boxHeight).reduced(0.f, boxHeight * .08f).toNearestInt());
        auto sliderBound = bandBound.reduced(0.f, fontSize * .1f);
        // the linear slider keeps a 0.45 aspect ratio, so leave it enough room
        const auto wantedHeight = juce::jmax(fontSize * 2.4f, sliderBound.getWidth() * 0.7f);
        slider.setBounds(sliderBound.withSizeKeepingCentre(
                sliderBound.getWidth(), juce::jmin(sliderBound.getHeight(), wantedHeight)).toNearestInt());
    }

    void layoutSplitKnob(juce::Rectangle<float> knobBound, zlinterface::RotarySliderComponent &knob,
                         float fontSize) {
        if (knobBound.isEmpty()) {
            return;
        }
        knob.setBounds(knobBound.reduced(knobBound.getWidth() * .08f, fontSize * .1f).toNearestInt());
    }
}

namespace zlpanel {
    BandPanel::BandPanel(juce::AudioProcessorValueTreeState &parameters, zlinterface::UIBase &base) :
            uiBase(base) {
        zlpanel::attachBoxes(*this, std::array{&lowBox, &midBox, &highBox}, boxAttachments,
                             styleIDs, parameters, base);
        zlpanel::attachSliders(*this, std::array{&lowCurve, &midCurve, &highCurve}, sliderAttachments,
                               curveIDs, parameters, base);
        zlpanel::attachSliders(*this, std::array{&lowSplitKnob, &highSplitKnob}, knobAttachments,
                               splitIDs, parameters, base);
    }

    BandPanel::~BandPanel() = default;

    void BandPanel::paint(juce::Graphics &g) {
        juce::ignoreUnused(g);
    }

    void BandPanel::resized() {
        auto bound = getLocalBounds().toFloat();
        const auto fontSize = uiBase.getFontSize();
        if (bound.getWidth() <= 0.f) {
            return;
        }
        // the bands run left to right, each split knob sits between the two
        // bands it divides
        constexpr float bandWeight = 1.f, splitWeight = .7f;
        const auto unit = bound.getWidth() / (3.f * bandWeight + 2.f * splitWeight);
        const std::array<zlinterface::ComboboxComponent *, 3> boxes{lowBox.get(), midBox.get(), highBox.get()};
        const std::array<zlinterface::LinearSliderComponent *, 3> sliders{lowCurve.get(), midCurve.get(),
                                                                          highCurve.get()};
        const std::array<zlinterface::RotarySliderComponent *, 2> knobs{lowSplitKnob.get(), highSplitKnob.get()};
        for (size_t i = 0; i < boxes.size(); ++i) {
            layoutBandBlock(bound.removeFromLeft(unit * bandWeight).reduced(fontSize * .25f, 0.f),
                            *boxes[i], *sliders[i], fontSize);
            if (i < knobs.size()) {
                layoutSplitKnob(bound.removeFromLeft(unit * splitWeight).reduced(fontSize * .1f, 0.f),
                                *knobs[i], fontSize);
            }
        }
    }
}
