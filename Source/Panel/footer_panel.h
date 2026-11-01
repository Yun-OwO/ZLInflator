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

#include "../GUI/button_component.h"
#include "../GUI/combobox_component.h"
#include "../GUI/rotary_slider_component.h"
#include "../DSP/dsp_defines.h"
#include "../State/state_definitions.h"
#include "panel_definitions.h"
#include <functional>
#include <juce_audio_processors/juce_audio_processors.h>

namespace zlpanel {
    /** The global controls that belong to no single band: the input and output
        trim, the wet mix, the oversampling choice and the auto gain switch.
        They share the header strip left of the meters, the switch stacks under
        the oversampling choice in the column right of the meters. */
    class FooterPanel : public juce::Component {
    public:
        FooterPanel(juce::AudioProcessorValueTreeState &audioParameters, zlinterface::UIBase &base);

        ~FooterPanel() override;

        void paint(juce::Graphics &g) override;

        void resized() override;

        /** the room the three knobs and the column of boxes need, so the main
            panel can line the meters up under that column */
        static float getPreferredWidth(float fontSize);

        static float getPreferredHeight(float fontSize);

    private:
        juce::AudioProcessorValueTreeState &apvts;

        std::unique_ptr<zlinterface::RotarySliderComponent> inputGain, outputGain, wet;
        std::unique_ptr<zlinterface::ComboboxComponent> overSample;
        // the knobs carry their own captions, the box takes its name from the parameter
        std::array<std::unique_ptr<zlinterface::RotarySliderComponent> *, 3> knobList{
                &inputGain, &outputGain, &wet};
        std::array<std::unique_ptr<zlinterface::ComboboxComponent> *, 1> boxList{&overSample};
        std::array<std::string, 1> boxIDs{zldsp::overSample::ID};
        juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments;
        juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> boxAttachments;

        zlinterface::UIBase *uiBase;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FooterPanel)
    };
}

#endif //ZLINFLATOR_FOOTER_PANEL_H
