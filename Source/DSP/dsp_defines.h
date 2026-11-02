/*
==============================================================================
Copyright (C) 2023 - zsliu98
This file is part of ZLInflator

ZLInflator is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
ZLInflator is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with ZLInflator. If not, see <https://www.gnu.org/licenses/>.
==============================================================================
*/

#ifndef ZLINFLATOR_DSP_DEFINES_H
#define ZLINFLATOR_DSP_DEFINES_H

#include <juce_audio_processors/juce_audio_processors.h>

namespace zldsp {
    inline auto static const versionHint = 1;
    // floats
    template<class T>
    class FloatParameters {
    public:
        static std::unique_ptr<juce::AudioParameterFloat> get(bool automate = true) {
            auto attributes = juce::AudioParameterFloatAttributes().withAutomatable(automate).withLabel(T::name);
            return std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(T::ID, versionHint), T::name,
                                                               T::range, T::defaultV, attributes);
        }

        inline static float convertTo01(float x) {
            return T::range.convertTo0to1(x);
        }
    };

    class inputGain : public FloatParameters<inputGain> {
    public:
        auto static constexpr ID = "input_gain";
        auto static constexpr name = "IN Gain (dB)";
        inline auto static const range = juce::NormalisableRange<float>(-16.f, 16.f, .1f, 0.5, true);
        auto static constexpr defaultV = 0.0f;
    };

    class outputGain : public FloatParameters<outputGain> {
    public:
        auto static constexpr ID = "output_gain";
        auto static constexpr name = "OUT Gain (dB)";
        inline auto static const range = juce::NormalisableRange<float>(-16.f, 16.f, .1f, 0.5, true);
        auto static constexpr defaultV = 0.0f;
    };

    class wet : public FloatParameters<wet> {
    public:
        auto static constexpr ID = "wet";
        auto static constexpr name = "Wet (%)";
        inline auto static const range = juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f);
        auto static constexpr defaultV = 100.0f;
        static float formatV(float v) {
            return v / 100.f;
        }
    };

    class curve1 : public FloatParameters<curve1> {
    public:
        auto static constexpr ID = "curve1";
        auto static constexpr name = "Curve (%)";
        inline auto static const range = juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f);
        auto static constexpr defaultV = 25.0f;
        static float formatV(float v) {
            return v / 100.f;
        }
    };

    class curve2 : public FloatParameters<curve2> {
    public:
        auto static constexpr ID = "curve2";
        auto static constexpr name = "Curve (%)";
        inline auto static const range = juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f);
        auto static constexpr defaultV = 25.0f;
        static float formatV(float v) {
            return v / 100.f;
        }
    };

    class weight : public FloatParameters<weight> {
    public:
        auto static constexpr ID = "weight";
        auto static constexpr name = "Weight (%)";
        inline auto static const range = juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f);
        auto static constexpr defaultV = 50.0f;
        static float formatV(float v) {
            return v / 100.f;
        }
    };

    class lowSplit : public FloatParameters<lowSplit> {
    public:
        auto static constexpr ID = "low_split";
        auto static constexpr name = "Low Split (Hz)";
        inline auto static const range = juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.2f);
        auto static constexpr defaultV = 240.0f;
    };

    class highSplit : public FloatParameters<highSplit> {
    public:
        auto static constexpr ID = "high_split";
        auto static constexpr name = "High Split (Hz)";
        inline auto static const range = juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.2f);
        auto static constexpr defaultV = 2400.0f;
    };

    // bools
    template<class T>
    class BoolParameters {
    public:
        static std::unique_ptr<juce::AudioParameterBool> get(bool automate = true) {
            auto attributes = juce::AudioParameterBoolAttributes().withAutomatable(automate).withLabel(T::name);
            return std::make_unique<juce::AudioParameterBool>(juce::ParameterID(T::ID, versionHint), T::name,
                                                              T::defaultV, attributes);
        }

        static std::unique_ptr<juce::AudioParameterBool> get(juce::String label, bool automate = true) {
            auto attributes = juce::AudioParameterBoolAttributes().withAutomatable(automate).withLabel(label);
            return std::make_unique<juce::AudioParameterBool>(juce::ParameterID(T::ID, versionHint), T::name,
                                                              T::defaultV, attributes);
        }
    };

    class effectIn : public BoolParameters<effectIn> {
    public:
        auto static constexpr ID = "effect_in";
        auto static constexpr name = "Effect In";
        auto static constexpr defaultV = true;
    };

    class bandSplit : public BoolParameters<bandSplit> {
    public:
        auto static constexpr ID = "band_split";
        auto static constexpr name = "Band Split";
        auto static constexpr defaultV = false;
    };

    class autoGain : public BoolParameters<autoGain> {
    public:
        auto static constexpr ID = "auto_gain";
        auto static constexpr name = "Auto Gain";
        auto static constexpr defaultV = false;
    };

    // choices
    template<class T>
    class ChoiceParameters {
    public:
        static std::unique_ptr<juce::AudioParameterChoice> get(bool automate = true) {
            auto attributes = juce::AudioParameterChoiceAttributes().withAutomatable(automate).withLabel(T::name);
            return std::make_unique<juce::AudioParameterChoice>(
                    juce::ParameterID(T::ID, versionHint), T::name, T::choices, T::defaultI, attributes);
        }

        static std::unique_ptr<juce::AudioParameterChoice> get(juce::String label, bool automate = true) {
            auto attributes = juce::AudioParameterChoiceAttributes().withAutomatable(automate).withLabel(label);
            return std::make_unique<juce::AudioParameterChoice>(
                    juce::ParameterID(T::ID, versionHint), T::name, T::choices, T::defaultI, attributes);
        }
    };

    class overSample : public ChoiceParameters<overSample> {
    public:
        auto static constexpr ID = "over_sample";
        auto static constexpr name = "Over Sampling";
        inline auto static const choices = juce::StringArray{"OFF", "2x", "4x", "8x", "16x"};
        int static constexpr defaultI = 0;
    };

    class style1 : public ChoiceParameters<style1> {
    public:
        auto static constexpr ID = "style1";
        auto static constexpr name = "1st Style";
        inline auto static const choices = juce::StringArray{"Identity", "Quadratic", "Cubic", "Quartic", "SinMOD"};
        enum {
            Identity,
            Quadratic,
            Cubic,
            Quartic,
//            SigMOD,
            SinMOD,
            StyleNUM
        };
        int static constexpr defaultI = 3;
    };

    class style2 : public ChoiceParameters<style2> {
    public:
        auto static constexpr ID = "style2";
        auto static constexpr name = "2nd Style";
        inline auto static const choices = juce::StringArray{"Identity", "Quadratic", "Cubic", "Quartic", "SinMOD"};
        enum {
            Identity,
            Quadratic,
            Cubic,
            Quartic,
//            SigMOD,
            SinMOD,
            StyleNUM
        };
        int static constexpr defaultI = 1;
    };

    // one shaper per band
    class styleLow : public ChoiceParameters<styleLow> {
    public:
        auto static constexpr ID = "style_low";
        auto static constexpr name = "Low Style";
        inline auto static const choices = juce::StringArray{"Identity", "Quadratic", "Cubic", "Quartic", "SinMOD"};
        int static constexpr defaultI = 3;
    };

    class curveLow : public FloatParameters<curveLow> {
    public:
        auto static constexpr ID = "curve_low";
        auto static constexpr name = "Low Curve (%)";
        inline auto static const range = juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f);
        auto static constexpr defaultV = 25.0f;
        static float formatV(float v) {
            return v / 100.f;
        }
    };

    class styleMid : public ChoiceParameters<styleMid> {
    public:
        auto static constexpr ID = "style_mid";
        auto static constexpr name = "Mid Style";
        inline auto static const choices = juce::StringArray{"Identity", "Quadratic", "Cubic", "Quartic", "SinMOD"};
        int static constexpr defaultI = 3;
    };

    class curveMid : public FloatParameters<curveMid> {
    public:
        auto static constexpr ID = "curve_mid";
        auto static constexpr name = "Mid Curve (%)";
        inline auto static const range = juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f);
        auto static constexpr defaultV = 25.0f;
        static float formatV(float v) {
            return v / 100.f;
        }
    };

    class styleHigh : public ChoiceParameters<styleHigh> {
    public:
        auto static constexpr ID = "style_high";
        auto static constexpr name = "High Style";
        inline auto static const choices = juce::StringArray{"Identity", "Quadratic", "Cubic", "Quartic", "SinMOD"};
        int static constexpr defaultI = 3;
    };

    class curveHigh : public FloatParameters<curveHigh> {
    public:
        auto static constexpr ID = "curve_high";
        auto static constexpr name = "High Curve (%)";
        inline auto static const range = juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f);
        auto static constexpr defaultV = 25.0f;
        static float formatV(float v) {
            return v / 100.f;
        }
    };

    // per band trims, so the two split handles on the figure also set the
    // balance of the bands themselves
    class bandGainLow : public FloatParameters<bandGainLow> {
    public:
        auto static constexpr ID = "band_gain_low";
        auto static constexpr name = "Low Gain (dB)";
        inline auto static const range = juce::NormalisableRange<float>(-12.f, 12.f, .1f, 0.5, true);
        auto static constexpr defaultV = 0.0f;
    };

    class bandGainMid : public FloatParameters<bandGainMid> {
    public:
        auto static constexpr ID = "band_gain_mid";
        auto static constexpr name = "Mid Gain (dB)";
        inline auto static const range = juce::NormalisableRange<float>(-12.f, 12.f, .1f, 0.5, true);
        auto static constexpr defaultV = 0.0f;
    };

    class bandGainHigh : public FloatParameters<bandGainHigh> {
    public:
        auto static constexpr ID = "band_gain_high";
        auto static constexpr name = "High Gain (dB)";
        inline auto static const range = juce::NormalisableRange<float>(-12.f, 12.f, .1f, 0.5, true);
        auto static constexpr defaultV = 0.0f;
    };

    /** how far the two stereo sides are processed, left of the centre only the
        mid is saturated, right of it only the side, the centre is both */
    class midSide : public FloatParameters<midSide> {
    public:
        auto static constexpr ID = "mid_side";
        auto static constexpr name = "Mid/Side (%)";
        inline auto static const range = juce::NormalisableRange<float>(-100.f, 100.f, 0.1f);
        auto static constexpr defaultV = 0.0f;
    };

    /** the drive every band is fed with, in decibels */
    class drive : public FloatParameters<drive> {
    public:
        auto static constexpr ID = "drive";
        auto static constexpr name = "Drive (dB)";
        inline auto static const range = juce::NormalisableRange<float>(-12.f, 24.f, 0.1f, 0.6, true);
        auto static constexpr defaultV = 0.0f;
    };

    /** how far the left and the right channel of a band drift apart, in percent */
    class stereoSpread : public FloatParameters<stereoSpread> {
    public:
        auto static constexpr ID = "stereo_spread";
        auto static constexpr name = "Stereo Spread (%)";
        inline auto static const range = juce::NormalisableRange<float>(0.f, 100.f, 0.1f, 1.f, true);
        auto static constexpr defaultV = 0.0f;
    };

    inline juce::AudioProcessorValueTreeState::ParameterLayout getParameterLayout() {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        // the parameters of the two shaper mixer are kept at their former
        // positions so that presets keep resolving, but they are neither
        // automated nor used by the DSP anymore: every band has its own
        // style/curve pair now
        layout.add(inputGain::get(), outputGain::get(), wet::get(),
                   curve1::get(false), curve2::get(false), weight::get(false),
                   lowSplit::get(), highSplit::get(),
                   effectIn::get(), bandSplit::get(false), autoGain::get(),
                   overSample::get(), style1::get(false), style2::get(false),
                   styleLow::get(), curveLow::get(),
                   styleMid::get(), curveMid::get(),
                   styleHigh::get(), curveHigh::get(),
                   bandGainLow::get(), bandGainMid::get(), bandGainHigh::get(),
                   midSide::get(), drive::get(), stereoSpread::get());
        return layout;
    }
}

#endif //ZLINFLATOR_DSP_DEFINES_H
