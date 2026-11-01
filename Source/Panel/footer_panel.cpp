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
    FooterPanel::FooterPanel(juce::AudioProcessorValueTreeState &parameters, zlinterface::UIBase &base) :
            uiBase(base), apvts(parameters) {
        zlpanel::attachSliders(*this, std::array{&inputGain, &outputGain, &wet}, sliderAttachments,
                               ioIDs, parameters, base);
        zlpanel::attachBoxes(*this, std::array{&overSample}, boxAttachments,
                             overSampleIDs, parameters, base);
        // the card title button, labelled by the page it opens
        foldButton = std::make_unique<SettingButton>("Settings", base);
        foldButton->getButton().onClick = [this]() {
            // the page state is stored, so the card opens the way it was left
            if (auto *parameter = apvts.getParameter(zlstate::uiFooter::ID)) {
                parameter->setValueNotifyingHost(parameter->convertTo0to1(settingPage ? 0.f : 1.f));
            }
        };
        addAndMakeVisible(*foldButton);

        apvts.addParameterListener(zlstate::uiFooter::ID, this);
        parameterChanged(zlstate::uiFooter::ID, 0.f);
    }

    FooterPanel::~FooterPanel() {
        apvts.removeParameterListener(zlstate::uiFooter::ID, this);
    }

    void FooterPanel::paint(juce::Graphics &g) {
        const auto bound = getLocalBounds().toFloat();
        const auto fontSize = uiBase.getFontSize();
        uiBase.fillRoundedShadowRectangle(g, bound, fontSize * .5f,
                                          {.blurRadius=0.6f, .mainColour=uiBase.getBackgroundColor()});
    }

    void FooterPanel::resized() {
        auto bound = getLocalBounds().toFloat();
        const auto fontSize = uiBase.getFontSize();
        auto inner = bound.reduced(fontSize * .6f, fontSize * .4f);
        if (inner.getWidth() <= 0 || inner.getHeight() <= 0) {
            return;
        }
        // a small folded card carries the title, a wide one has the settings button on its right
        const auto narrow = inner.getWidth() < fontSize * 16.f;
        auto foldBound = narrow
                         ? inner.removeFromRight(juce::jmax(fontSize * 3.6f, inner.getWidth() * .45f))
                         : inner.removeFromRight(juce::jmin(inner.getWidth() * .3f, fontSize * 6.5f));
        foldButton->setTitle(settingPage ? "Close" : "Settings");
        foldButton->setCaptionFontSize(fontSize * zlinterface::FontSmall);
        foldButton->setBounds(foldBound.withSizeKeepingCentre(foldBound.getWidth(),
                                                              juce::jmin(foldBound.getHeight(),
                                                                         foldBound.getWidth() * .45f)).toNearestInt());
        inputGain->setVisible(settingPage);
        outputGain->setVisible(settingPage);
        wet->setVisible(settingPage);
        overSample->setVisible(settingPage);
        if (!settingPage) {
            return;
        }
        inner.removeFromRight(fontSize * .4f);
        // the card is wide and low, so the four controls sit in one row
        const auto unitWidth = inner.getWidth() / 4.f;
        inputGain->setBounds(inner.removeFromLeft(unitWidth).toNearestInt());
        outputGain->setBounds(inner.removeFromLeft(unitWidth).toNearestInt());
        wet->setBounds(inner.removeFromLeft(unitWidth).toNearestInt());
        auto overSampleBound = inner.removeFromLeft(unitWidth);
        overSample->setBounds(overSampleBound.withSizeKeepingCentre(
                overSampleBound.getWidth(), juce::jmin(overSampleBound.getHeight(), fontSize * 3.f)).toNearestInt());
    }

    float FooterPanel::getPreferredHeight(float totalHeight) const {
        const auto fontSize = uiBase.getFontSize();
        if (!settingPage) {
            return juce::jmin(totalHeight * .14f, fontSize * 3.6f);
        }
        return juce::jmin(totalHeight * .3f, fontSize * 14.f);
    }

    void FooterPanel::parameterChanged(const juce::String &parameterID, float) {
        if (parameterID == zlstate::uiFooter::ID) {
            const auto value = apvts.getRawParameterValue(zlstate::uiFooter::ID);
            settingPage = value == nullptr || value->load() > .5f;
            // the title button reads "Close" while the settings are open
            if (foldButton != nullptr) {
                foldButton->setTitle(settingPage ? "Close" : "Settings");
            }
            if (onExpandChanged) {
                onExpandChanged();
            }
        }
    }
}
