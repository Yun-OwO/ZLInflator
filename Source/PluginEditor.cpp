/*
==============================================================================
Copyright (C) 2023 - zsliu98

This file is part of ZLInflator

ZLInflator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
ZLInflator is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with ZLInflator. If not, see <https://www.gnu.org/licenses/>.
==============================================================================
*/

#include "PluginEditor.h"

//==============================================================================
ZLInflatorAudioProcessorEditor::ZLInflatorAudioProcessorEditor(ZLInflatorAudioProcessor &p)
        : AudioProcessorEditor(&p), processorRef(p),
          mainPanel(p) {
    for (const auto &ID: IDs) {
        p.states.addParameterListener(ID, this);
    }
    juce::LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypeface(
            juce::Typeface::createSystemTypefaceFor(BinaryData::OpenSansSemiBold_ttf,
                                                    BinaryData::OpenSansSemiBold_ttfSize));
    addAndMakeVisible(mainPanel);
    setResizeLimits(zlstate::windowW::minV, zlstate::windowH::minV,
                    zlstate::windowW::maxV, zlstate::windowH::maxV);
    getConstrainer()->setFixedAspectRatio(
            static_cast<double>(zlstate::windowW::defaultV) /
            static_cast<double>(zlstate::windowH::defaultV));
    setResizable(true, p.wrapperType != ZLInflatorAudioProcessor::wrapperType_AudioUnitv3);
    lastUIWidth.referTo(p.states.getParameterAsValue(zlstate::windowW::ID));
    lastUIHeight.referTo(p.states.getParameterAsValue(zlstate::windowH::ID));
    setSize(lastUIWidth.getValue(), lastUIHeight.getValue());
    lastUIWidth.addListener(this);
    lastUIHeight.addListener(this);
}

ZLInflatorAudioProcessorEditor::~ZLInflatorAudioProcessorEditor() {
    for (const auto &ID: IDs) {
        processorRef.states.removeParameterListener(ID, this);
    }
    lastUIWidth.removeListener(this);
    lastUIHeight.removeListener(this);
}

//==============================================================================
void ZLInflatorAudioProcessorEditor::paint(juce::Graphics &g) {
    g.fillAll(mainPanel.getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void ZLInflatorAudioProcessorEditor::resized() {
    mainPanel.setBounds(getLocalBounds());
    lastUIWidth.setValue(getWidth());
    lastUIHeight.setValue(getHeight());
}

void ZLInflatorAudioProcessorEditor::valueChanged(juce::Value &) {
    setSize(lastUIWidth.getValue(), lastUIHeight.getValue());
}

void ZLInflatorAudioProcessorEditor::parameterChanged(const juce::String &, float) {
}
