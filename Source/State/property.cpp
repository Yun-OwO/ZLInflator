// ==============================================================================
// Copyright (C) 2023 - zsliu98
// This file is part of ZLInflator
//
// ZLInflator is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version. ZLInflator is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// ZLInflator. If not, see <https://www.gnu.org/licenses/>.
// ==============================================================================

#include "property.h"

namespace zlstate {

    Property::Property() : uiFile(uiPath) {
        createDirectory();
    }

    Property::Property(juce::AudioProcessorValueTreeState &apvts) : uiFile(uiPath) {
        createDirectory();
        loadAPVTS(apvts);
    }

    void Property::createDirectory() {
        if (!path.isDirectory()) {
            path.createDirectory();
        }
    }

    void Property::loadAPVTS(juce::AudioProcessorValueTreeState &apvts) {
        const juce::ScopedReadLock myScopedLock(readWriteLock);
        if (!uiFile.existsAsFile()) {
            return;
        }
        auto xml = juce::XmlDocument::parse(uiFile);
        // ui.xml may be missing, corrupt or written by an older version: it is
        // only applied when it really holds this state tree
        if (xml == nullptr || xml->getTagName() != apvts.state.getType().toString()) {
            return;
        }
        auto newState = juce::ValueTree::fromXml(*xml);
        if (!newState.hasType(apvts.state.getType())) {
            return;
        }
        apvts.replaceState(newState);
    }

    void Property::saveAPVTS(juce::AudioProcessorValueTreeState &apvts) {
        const juce::ScopedWriteLock myScopedLock(readWriteLock);
        auto xml = apvts.copyState().createXml();
        if (xml != nullptr) {
            xml->writeTo(uiFile);
        }
    }
} // namespace zlstate
