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
    FooterPanel::FooterPanel(juce::AudioProcessorValueTreeState &audioParameters, juce::AudioProcessorValueTreeState &uiParameters, zlinterface::UIBase &base) :
            uiBase(base), apvts(audioParameters), uiApvts(uiParameters) {
        // the card is narrow, so the three knobs carry short captions of their own
        // instead of the parameter names the shared helper would paste on
        inputGain = std::make_unique<zlinterface::RotarySliderComponent>("In", base);
        outputGain = std::make_unique<zlinterface::RotarySliderComponent>("Out", base);
        wet = std::make_unique<zlinterface::RotarySliderComponent>("Wet", base);
        addAndMakeVisible(*inputGain);
        addAndMakeVisible(*outputGain);
        addAndMakeVisible(*wet);
        sliderAttachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment(
                audioParameters, zldsp::inputGain::ID, inputGain->getSlider()));
        sliderAttachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment(
                audioParameters, zldsp::outputGain::ID, outputGain->getSlider()));
        sliderAttachments.add(new juce::AudioProcessorValueTreeState::SliderAttachment(
                audioParameters, zldsp::wet::ID, wet->getSlider()));
        zlpanel::attachBoxes(*this, std::array{&overSample}, boxAttachments,
                             overSampleIDs, audioParameters, base);
        // the card title button, labelled by the page it opens
        foldButton = std::make_unique<SettingButton>("Settings", base);
        foldButton->getButton().onClick = [this]() {
            // the page state is stored, so the card opens the way it was left
            if (auto *parameter = uiApvts.getParameter(zlstate::uiFooter::ID)) {
                parameter->setValueNotifyingHost(parameter->convertTo0to1(settingPage ? 0.f : 1.f));
            }
        };
        addAndMakeVisible(*foldButton);

        uiApvts.addParameterListener(zlstate::uiFooter::ID, this);
        parameterChanged(zlstate::uiFooter::ID, uiApvts.getRawParameterValue(zlstate::uiFooter::ID)->load());
    }

    FooterPanel::~FooterPanel() {
        uiApvts.removeParameterListener(zlstate::uiFooter::ID, this);
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
        auto inner = bound.reduced(fontSize * .7f, fontSize * .5f);
        if (inner.getWidth() <= 0 || inner.getHeight() <= 0) {
            return;
        }
        inputGain->setVisible(settingPage);
        outputGain->setVisible(settingPage);
        wet->setVisible(settingPage);
        overSample->setVisible(settingPage);

        const auto narrow = inner.getWidth() < fontSize * 16.f;
        if (!settingPage) {
            // folded: the card is just the title button
            auto foldBound = narrow
                             ? inner
                             : inner.removeFromRight(juce::jmin(inner.getWidth() * .3f, fontSize * 6.5f));
            foldButton->setTitle("Settings");
            foldButton->setCaptionFontSize(fontSize * zlinterface::FontNormal);
            foldButton->setBounds(foldBound.withSizeKeepingCentre(
                    foldBound.getWidth(),
                    juce::jmin(foldBound.getHeight(), fontSize * zlinterface::ControlRowHeight)).toNearestInt());
            return;
        }

        // open: the three global knobs take the room on the left, the sample
        // rate choice and the close button stack on the right
        auto rightBound = inner.removeFromRight(juce::jmin(inner.getWidth() * .34f, fontSize * 8.5f));
        auto foldBound = rightBound.removeFromBottom(
                juce::jmin(rightBound.getHeight() * .5f, fontSize * zlinterface::ControlRowHeight));
        foldButton->setTitle("Close");
        foldButton->setCaptionFontSize(fontSize * zlinterface::FontNormal);
        foldButton->setBounds(foldBound.toNearestInt());
        // the combo sits at the same height as the caption and box of the band panel,
        // so every button in the editor reads the same size
        overSample->setBounds(rightBound.removeFromTop(
                juce::jmin(rightBound.getHeight(), fontSize * zlinterface::ControlRowHeight)).toNearestInt());

        inner.removeFromRight(fontSize * .5f);
        const auto unitWidth = inner.getWidth() / 3.f;
        inputGain->setBounds(inner.removeFromLeft(unitWidth).toNearestInt());
        outputGain->setBounds(inner.removeFromLeft(unitWidth).toNearestInt());
        wet->setBounds(inner.toNearestInt());
    }

    float FooterPanel::getPreferredHeight(float totalHeight) const {
        const auto fontSize = uiBase.getFontSize();
        if (!settingPage) {
            return juce::jmin(totalHeight * .14f, fontSize * zlinterface::ControlRowHeight);
        }
        return juce::jmin(totalHeight * .66f, fontSize * zlinterface::ControlRowHeight * 2.3f);
    }

    void FooterPanel::parameterChanged(const juce::String &parameterID, float) {
        if (parameterID == zlstate::uiFooter::ID) {
            const auto value = uiApvts.getRawParameterValue(zlstate::uiFooter::ID);
            settingPage = value != nullptr && value->load() > .5f;
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
