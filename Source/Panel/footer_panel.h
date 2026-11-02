// ==============================================================================
// Copyright (C) 2023 - zsliu98
// This file is part of ZLInflator
//
// ZLInflator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// ZLInflator is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with ZLInflator. If not, see <https://www.gnu.org/licenses/>.
// ==============================================================================

#ifndef ZLINFLATOR_FOOTER_PANEL_H
#define ZLINFLATOR_FOOTER_PANEL_H

#include "../GUI/rotary_slider_component.h"
#include "../DSP/dsp_defines.h"
#include "../State/state_definitions.h"
#include "panel_definitions.h"
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>

namespace zlpanel {
    /** The global trim of every band at once: the input and output gain, the
        drive every band shaper is fed with, the wet mix and how far the two
        stereo sides are processed. They share the header strip left of the
        meters. */
    class FooterPanel : public juce::Component {
    public:
        FooterPanel(juce::AudioProcessorValueTreeState &audioParameters, zlinterface::UIBase &base);

        ~FooterPanel() override;

        void paint(juce::Graphics &g) override;

        void resized() override;

        /** the room the three knobs need, so the main panel can line the
            header up with them */
        static float getPreferredWidth(float fontSize);

        static float getPreferredHeight(float fontSize);

    private:
        juce::AudioProcessorValueTreeState &apvts;

        /** in, out, drive, wet and the mid/side focus share one row */
        constexpr static size_t NumKnobs = 5;
        std::unique_ptr<zlinterface::RotarySliderComponent> inputGain, outputGain, drive, wet, midSide;
        std::array<std::unique_ptr<zlinterface::RotarySliderComponent> *, NumKnobs> knobList{
                &inputGain, &outputGain, &drive, &wet, &midSide};
        juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments;

        zlinterface::UIBase *uiBase;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FooterPanel)
    };
}

#endif //ZLINFLATOR_FOOTER_PANEL_H
