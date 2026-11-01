// ==============================================================================
// Copyright (C) 2023 - zsliu98
// This file is part of ZLInflator
//
// ZLInflator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// ZLInflator is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with ZLInflator. If not, see <https://www.gnu.org/licenses/>.
// ==============================================================================

#ifndef ZLINFLATOR_BAND_PANEL_H
#define ZLINFLATOR_BAND_PANEL_H

#include "../DSP/dsp_defines.h"
#include "../GUI/combobox_component.h"
#include "../GUI/linear_slider_component.h"
#include "../GUI/rotary_slider_component.h"
#include "panel_definitions.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace zlpanel {
    /** The left hand side of the main figure: for every band the algorithm and
        the curve amount, and between two bands the knob of the split that
        separates them, so everything between two split knobs belongs to the
        band in between. */
    class BandPanel : public juce::Component {
    public:
        BandPanel(juce::AudioProcessorValueTreeState &parameters, zlinterface::UIBase &base);

        ~BandPanel() override;

        void paint(juce::Graphics &g) override;

        void resized() override;

        zlinterface::ComboboxComponent &getLowBox() { return *lowBox; }

    private:
        zlinterface::UIBase &uiBase;

        std::unique_ptr<zlinterface::ComboboxComponent> lowBox, midBox, highBox;
        std::unique_ptr<zlinterface::LinearSliderComponent> lowCurve, midCurve, highCurve;
        std::unique_ptr<zlinterface::RotarySliderComponent> lowSplitKnob, highSplitKnob;
        std::array<std::string, 3> styleIDs{zldsp::styleLow::ID, zldsp::styleMid::ID, zldsp::styleHigh::ID};
        std::array<std::string, 3> curveIDs{zldsp::curveLow::ID, zldsp::curveMid::ID, zldsp::curveHigh::ID};
        std::array<std::string, 2> splitIDs{zldsp::lowSplit::ID, zldsp::highSplit::ID};
        juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> boxAttachments;
        juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments;
        juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> knobAttachments;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandPanel)
    };
}

#endif //ZLINFLATOR_BAND_PANEL_H
