// ==============================================================================
// Copyright (C) 2023 - zsliu98
// This file is part of ZLInflator
//
// ZLInflator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
// ZLInflator is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along with ZLInflator. If not, see <https://www.gnu.org/licenses/>.
// ==============================================================================

#ifndef ZL_NAME_LOOK_AND_FEEL_H
#define ZL_NAME_LOOK_AND_FEEL_H

#include "interface_definitions.h"
#include "juce_gui_basics/juce_gui_basics.h"

namespace zlinterface {
    class NameLookAndFeel : public juce::LookAndFeel_V4 {
    public:
        explicit NameLookAndFeel(UIBase &base) {
            uiBase = &base;
        }

        void drawLabel(juce::Graphics &g, juce::Label &label) override {
            if (editable) {
                g.setColour(uiBase->getTextColor());
            } else {
                g.setColour(uiBase->getTextInactiveColor());
            }
            auto labelArea{label.getLocalBounds().toFloat()};
            auto center = labelArea.getCentre();
            float labelFontSize = uiBase->getFontSize() * zlinterface::FontSmall;
            if (labelFontSize <= 0) {
                labelFontSize = labelArea.getHeight() * 0.6f;
            }
            g.setFont(juce::Font(labelFontSize));
            const juce::String text{label.getText()};
            // shrink the text instead of cutting it off
            auto textWidth = g.getCurrentFont().getStringWidthFloat(text);
            if (textWidth > labelArea.getWidth() * 0.96f && textWidth > 0) {
                const auto scale = labelArea.getWidth() * 0.96f / textWidth;
                g.setFont(juce::Font(labelFontSize * scale));
            }
            g.drawSingleLineText(
                    text,
                    juce::roundToInt(center.x + g.getCurrentFont().getHorizontalScale()),
                    juce::roundToInt(center.y + g.getCurrentFont().getDescent()),
                    juce::Justification::horizontallyCentred);
        }

        void setEditable(bool f) {
            editable.store(f);
        }

    private:
        std::atomic<bool> editable = true;

        UIBase *uiBase;
    };
}

#endif //ZL_NAME_LOOK_AND_FEEL_H
