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
    /** The card's title button: a toggle button from the shared look and feel,
        but it reads its own title instead of ON/OFF. */
    class SettingButton : public zlinterface::ButtonComponent {
    public:
        explicit SettingButton(const juce::String &title, zlinterface::UIBase &base) :
                ButtonComponent(title, base) {
            getButton().setClickingTogglesState(false);
        }

        void setTitle(const juce::String &title) {
            // the caption already reads the title, the small label above would repeat it
            getLabel().setText({}, juce::dontSendNotification);
            setCaption(title);
        }

        void setCaptionFontSize(float size) {
            ButtonComponent::setCaptionFontSize(size);
        }
    };

    /** The floating card at the bottom of the figure: the global controls that
        are not bound to one band, folded away behind a single button so that
        the plot keeps the room it needs. */
    class FooterPanel : public juce::Component,
                        private juce::AudioProcessorValueTreeState::Listener {
    public:
        /** @param audioParameters  the main audio parameters (input_gain, wet, …). @param uiParameters  the UI state tree (ui_footer). */
        FooterPanel(juce::AudioProcessorValueTreeState &audioParameters, juce::AudioProcessorValueTreeState &uiParameters, zlinterface::UIBase &base);

        ~FooterPanel() override;

        void paint(juce::Graphics &g) override;

        void resized() override;

        bool isExpanded() const { return settingPage; }

        /** The height the card wants, out of the height of the whole editor. */
        float getPreferredHeight(float totalHeight) const;

        /** Called whenever the card folds or unfolds. */
        std::function<void()> onExpandChanged;

    private:
        void parameterChanged(const juce::String &, float) override;

        zlinterface::UIBase &uiBase;
        juce::AudioProcessorValueTreeState &apvts;
        juce::AudioProcessorValueTreeState &uiApvts;

        std::unique_ptr<zlinterface::RotarySliderComponent> inputGain, outputGain, wet;
        std::unique_ptr<zlinterface::ComboboxComponent> overSample;
        std::unique_ptr<SettingButton> foldButton;
        std::array<std::string, 1> overSampleIDs{zldsp::overSample::ID};
        juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachments;
        juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> boxAttachments;

        bool settingPage = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FooterPanel)
    };
}

#endif //ZLINFLATOR_FOOTER_PANEL_H
