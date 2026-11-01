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

class MainPanel : public juce::Component {
public:
    explicit MainPanel(ZLInflatorAudioProcessor &p);

    ~MainPanel() override;

    void paint(juce::Graphics &g) override;

    void resized() override;

private:
    zlinterface::UIBase uiBase;
    zlpanel::LogoPanel logoPanel;
    std::unique_ptr<zlinterface::ButtonComponent> effectButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> effectAttachment;

    zlpanel::BandPanel bandPanel;
    zlinterface::BandPlotComponent bandPlotComponent;
    MeterPanel meterPanel;
    zlpanel::FooterPanel footerPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};

#endif //ZLINFLATOR_MAIN_PANEL_H
