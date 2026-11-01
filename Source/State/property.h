// ==============================================================================
// Copyright (C) 2023 - zsliu98
// This file is part of ZLInflator
//
// ZLInflator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// ZLInflator is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with ZLInflator. If not, see <https://www.gnu.org/licenses/>.
// ==============================================================================

#ifndef ZLINFLATOR_PROPERTY_H
#define ZLINFLATOR_PROPERTY_H

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "state_definitions.h"

namespace zlstate {

    /**
        Storage of the UI states: the ui.xml file keeps the editor size and
        style. It only ever stores the parameters of the "ZLInflatorStates"
        tree, so it can never overwrite the audio parameters of a preset.
    */
    class Property {
    public:
        Property();

        explicit Property(juce::AudioProcessorValueTreeState &apvts);

        /** applies the UI states stored in ui.xml to the given states tree */
        void loadAPVTS(juce::AudioProcessorValueTreeState &apvts);

        /** writes the given states tree into ui.xml */
        void saveAPVTS(juce::AudioProcessorValueTreeState &apvts);

    private:
        /** creates the preset directory, ui.xml itself is only written on demand */
        static void createDirectory();

        juce::File uiFile;
        mutable juce::ReadWriteLock readWriteLock;

        inline auto static const path =
                juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                        .getChildFile("Audio")
                        .getChildFile("Presets")
                        .getChildFile(JucePlugin_Manufacturer)
                        .getChildFile(JucePlugin_Name);
        inline auto static const uiPath =
                path.getChildFile("ui.xml");
    };

} // namespace zlstate

#endif // ZLINFLATOR_PROPERTY_H
