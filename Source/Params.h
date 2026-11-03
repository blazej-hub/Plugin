#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace abyss::pid
{
inline constexpr const char* inGain = "inGain";
inline constexpr const char* outGain = "outGain";
inline constexpr const char* gateOn = "gateOn";
inline constexpr const char* gateThr = "gateThr";
inline constexpr const char* odOn = "odOn";
inline constexpr const char* odDrive = "odDrive";
inline constexpr const char* odTone = "odTone";
inline constexpr const char* odLevel = "odLevel";
inline constexpr const char* ampVoice = "ampVoice";
inline constexpr const char* ampGain = "ampGain";
inline constexpr const char* ampBass = "ampBass";
inline constexpr const char* ampMid = "ampMid";
inline constexpr const char* ampTreble = "ampTreble";
inline constexpr const char* ampPresence = "ampPresence";
inline constexpr const char* ampDepth = "ampDepth";
inline constexpr const char* ampMaster = "ampMaster";
inline constexpr const char* ampLayer = "ampLayer";
inline constexpr const char* ampWide = "ampWide";
inline constexpr const char* cabOn = "cabOn";
inline constexpr const char* cabModel = "cabModel";
inline constexpr const char* cabMic = "cabMic";
inline constexpr const char* tightOn = "tightOn";
inline constexpr const char* tightAmt = "tightAmt";
inline constexpr const char* eqOn = "eqOn";
inline constexpr const char* eqBand[6] = { "eq0", "eq1", "eq2", "eq3", "eq4", "eq5" };
inline constexpr const char* dblOn = "dblOn";
inline constexpr const char* dblAmt = "dblAmt";
inline constexpr const char* dlyOn = "dlyOn";
inline constexpr const char* dlyTime = "dlyTime";
inline constexpr const char* dlySync = "dlySync";
inline constexpr const char* dlyFb = "dlyFb";
inline constexpr const char* dlyMix = "dlyMix";
inline constexpr const char* revOn = "revOn";
inline constexpr const char* revSize = "revSize";
inline constexpr const char* revDamp = "revDamp";
inline constexpr const char* revMix = "revMix";
} // namespace abyss::pid

namespace abyss
{
inline const juce::StringArray eqBandNames { "SUB", "300", "700", "2.3K", "BITE", "AIR" };

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    using P = std::unique_ptr<RangedAudioParameter>;
    std::vector<P> p;
    auto fl = [&] (const char* id, const char* name, float lo, float hi, float def, const char* unit = "", float step = 0.01f)
    { p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, step), def, AudioParameterFloatAttributes().withLabel (unit))); };
    // on/off switches as 2-step choices: always snap to exactly 0 or 1 (state restores exactly, automation can't sit "in between")
    auto bl = [&] (const char* id, const char* name, bool def) { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, StringArray { "Off", "On" }, def ? 1 : 0)); };
    auto ch = [&] (const char* id, const char* name, StringArray items, int def) { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };

    fl (pid::inGain, "Input", -24, 24, 0, "dB", 0.1f);
    fl (pid::outGain, "Output", -36, 12, 0, "dB", 0.1f);

    bl (pid::gateOn, "Gate On", true);
    fl (pid::gateThr, "Gate Threshold", -90, -10, -55, "dB", 0.5f);

    bl (pid::odOn, "Overdrive On", true);
    fl (pid::odDrive, "OD Drive", 0, 10, 0);
    fl (pid::odTone, "OD Tone", 0, 10, 0);
    fl (pid::odLevel, "OD Level", -12, 12, 0, "dB", 0.1f);

    ch (pid::ampVoice, "Amp Channel", { "Clean", "Thrash", "Modern" }, 1);
    fl (pid::ampGain, "Gain", 0, 10, 6.5f);
    fl (pid::ampBass, "Bass", 0, 10, 5.0f);
    fl (pid::ampMid, "Mid", 0, 10, 4.0f);
    fl (pid::ampTreble, "Treble", 0, 10, 4.5f);
    fl (pid::ampPresence, "Presence", 0, 10, 8.0f);
    fl (pid::ampDepth, "Depth", 0, 10, 3.0f);
    fl (pid::ampMaster, "Master", 0, 10, 5.0f);
    fl (pid::ampLayer, "Layer Amp", 0, 1, 0.0f);
    bl (pid::ampWide, "Layer Wide", false);

    bl (pid::cabOn, "Cab On", true);
    ch (pid::cabModel, "Cabinet", { "Modern 4x12 V30", "Vintage 4x12 V30", "Open 2x12", "Custom IR" }, 0);
    fl (pid::cabMic, "Mic Blend 57/421", 0, 1, 0.15f);

    bl (pid::tightOn, "Tight On", true);
    fl (pid::tightAmt, "Tight Amount", 0, 1, 0.5f);

    bl (pid::eqOn, "EQ On", true);
    const float eqDef[6] = { 2.0f, -1.0f, -1.0f, -2.0f, 1.5f, 1.5f };
    for (int i = 0; i < 6; ++i)
        fl (pid::eqBand[i], ("EQ " + eqBandNames[i]).toRawUTF8(), -12, 12, eqDef[i], "dB", 0.1f);

    bl (pid::dblOn, "Doubler On", false);
    fl (pid::dblAmt, "Doubler Amount", 0, 1, 0.6f);

    bl (pid::dlyOn, "Delay On", false);
    fl (pid::dlyTime, "Delay Time", 20, 2000, 360, "ms", 1.0f);
    ch (pid::dlySync, "Delay Sync", { "Free", "1/2", "1/4", "1/4 D", "1/8", "1/8 D", "1/16" }, 0);
    fl (pid::dlyFb, "Delay Feedback", 0, 0.9f, 0.3f);
    fl (pid::dlyMix, "Delay Mix", 0, 1, 0.12f);

    bl (pid::revOn, "Reverb On", false);
    fl (pid::revSize, "Reverb Size", 0, 1, 0.45f);
    fl (pid::revDamp, "Reverb Damping", 0, 1, 0.5f);
    fl (pid::revMix, "Reverb Mix", 0, 1, 0.15f);

    return { p.begin(), p.end() };
}

// ---------------------------------------------------------------- factory presets
struct Preset { const char* name; std::vector<std::pair<const char*, float>> values; };

inline const std::vector<Preset>& factoryPresets()
{
    using namespace pid;
    static const std::vector<Preset> presets {
        { "Rhythm | Thrash Wall", {} },   // = defaults
        { "Rhythm | Layered Wide (2 amps)", { { ampLayer, 1.0f }, { ampWide, 1.0f }, { ampGain, 6.8f }, { odTone, 0.5f }, { dblOn, 1 }, { dblAmt, 0.35f } } },
        { "Rhythm | Modern Chug", { { ampVoice, 2 }, { odDrive, 1.5f }, { odTone, 2.5f }, { ampGain, 6.0f }, { ampBass, 4.5f },
                                    { ampMid, 4.5f }, { ampTreble, 5.0f }, { ampPresence, 7.0f }, { cabModel, 1 }, { ampBass, 5.5f } } },
        { "Lead | Melodic", { { ampGain, 7.5f }, { ampMid, 6.0f }, { ampTreble, 5.0f }, { odDrive, 1.0f }, { odTone, 3.0f },
                              { tightOn, 0 }, { eqBand[2], 0.0f }, { eqBand[3], -1.0f },
                              { dlyOn, 1 }, { dlyTime, 360 }, { dlyFb, 0.3f }, { dlyMix, 0.15f },
                              { revOn, 1 }, { revSize, 0.5f }, { revMix, 0.15f }, { outGain, -4.5f } } },
        { "Clean | Atmospheric", { { ampVoice, 0 }, { ampMaster, 6.8f }, { ampGain, 4.0f }, { ampBass, 5.0f }, { ampMid, 5.0f }, { ampTreble, 6.0f },
                                   { ampPresence, 5.0f }, { ampDepth, 2.0f }, { odOn, 0 }, { gateThr, -72 }, { cabModel, 2 },
                                   { tightOn, 0 }, { eqBand[0], 0 }, { eqBand[1], 0 }, { eqBand[2], 0 }, { eqBand[3], 0 }, { eqBand[4], 0 }, { eqBand[5], 1.5f },
                                   { dlyOn, 1 }, { dlySync, 3 }, { dlyFb, 0.4f }, { dlyMix, 0.25f },
                                   { revOn, 1 }, { revSize, 0.8f }, { revMix, 0.3f }, { dblOn, 1 }, { dblAmt, 0.5f } } },
        { "Doom | Slow Crush", { { ampVoice, 2 }, { ampGain, 8.0f }, { ampBass, 6.0f }, { ampMid, 3.5f }, { ampDepth, 6.0f },
                                 { odTone, 1.5f }, { cabModel, 1 }, { tightAmt, 0.3f }, { dblOn, 1 }, { dblAmt, 0.4f } } },
    };
    return presets;
}
} // namespace abyss
