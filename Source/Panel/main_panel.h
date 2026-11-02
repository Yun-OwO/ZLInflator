/*
==============================================================================
Copyright (C) 2023 - zsliu98

This file is part of ZLInflator

ZLInflator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
ZLInflator is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with ZLInflator. If not, see <https://www.gnu.org/licenses/>.
==============================================================================
*/

#ifndef ZLINFLATOR_MAIN_PANEL_H
#define ZLINFLATOR_MAIN_PANEL_H

#include "../GUI/interface_definitions.h"
#include "../GUI/button_component.h"
#include "band_panel.h"
#include "footer_panel.h"
#include "logo_panel.h"
#include "meter_panel.h"
#include "../GUI/band_plot_component.h"
#include <juce_audio_processors/juce_audio_processors.h>

/** A cell that steps through a choice parameter with every click and turns
    with the mouse wheel, the way a compact multi way switch works. */
class ChoiceButton : public zlinterface::ButtonComponent,
                     private juce::AudioProcessorValueTreeState::Listener,
                     private juce::AsyncUpdater {
public:
    ChoiceButton(ZLInflatorAudioProcessor &p, const juce::String &labelText, const std::string &parameterID,
                 zlinterface::UIBase &base);

    ~ChoiceButton() override;

    void parameterChanged(const juce::String &parameterID, float newValue) override;

    void handleAsyncUpdate() override;

    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &) override;

    /** A plain click flips between these two choices instead of walking the
        whole list, the wheel still walks them all. */
    void setTogglePair(int offChoice, int onChoice);

    /** The pill reads this while the first choice is picked. */
    void setOffCaption(const juce::String &text);

private:
    void step(int direction);

    void refresh();

    int getChoiceIndex() const;

    void goTo(int index);

    juce::AudioProcessorValueTreeState &parameters;
    juce::RangedAudioParameter *parameter;
    const juce::String parameterID;
    int numChoices = 0;
    juce::String offCaption;
    int toggleOff = 0;
    int toggleOn = 1;
    bool hasTogglePair = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChoiceButton)
};

class MainPanel : public juce::Component {
public:
    explicit MainPanel(ZLInflatorAudioProcessor &p);

    ~MainPanel() override;

    void paint(juce::Graphics &g) override;

    void resized() override;

private:
    zlinterface::UIBase uiBase;
    zlpanel::LogoPanel logoPanel;
    // the bypass switch sits in the top right corner, the auto gain switch
    // right under it
    std::unique_ptr<zlinterface::ButtonComponent> effectButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> effectAttachment;
    std::unique_ptr<zlinterface::ButtonComponent> compensationButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> compensationAttachment;
    std::unique_ptr<ChoiceButton> overSampleButton;

    zlpanel::BandPanel bandPanel;
    zlinterface::BandPlotComponent bandPlotComponent;
    MeterPanel meterPanel;
    zlpanel::FooterPanel footerPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};

#endif //ZLINFLATOR_MAIN_PANEL_H
