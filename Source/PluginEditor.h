#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace abyss::ui
{
namespace col
{
    const juce::Colour bg       { 0xff0d0e10 };
    const juce::Colour panel    { 0xff17191c };
    const juce::Colour panelHi  { 0xff1f2226 };
    const juce::Colour border   { 0xff2b2f35 };
    const juce::Colour accent   { 0xffd94a2b };   // ember
    const juce::Colour accent2  { 0xffe8a04a };   // amber
    const juce::Colour text     { 0xffe9e4dc };
    const juce::Colour dim      { 0xff8c877f };
}

class LnF : public juce::LookAndFeel_V4
{
public:
    LnF();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool hl, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool hl, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool hl, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::Font (juce::FontOptions (13.0f)); }
    juce::Font getPopupMenuFont() override { return juce::Font (juce::FontOptions (14.0f)); }
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// rotary knob + caption
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& s, const char* paramId, juce::String caption, bool big = false);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
private:
    juce::String caption; bool big;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class EqBand : public juce::Component
{
public:
    EqBand (juce::AudioProcessorValueTreeState& s, const char* paramId, juce::String caption);
    void resized() override;
    void paint (juce::Graphics&) override;
    juce::Slider slider;
private:
    juce::String caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class PowerButton : public juce::ToggleButton
{
public:
    PowerButton (juce::AudioProcessorValueTreeState& s, const char* paramId);
private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
};

class Meter : public juce::Component
{
public:
    void setLevel (float lin) { level = lin; repaint(); }
    void paint (juce::Graphics&) override;
private:
    float level = 0;
};

class TunerView : public juce::Component
{
public:
    void update (float freqHz, float clarity);
    void paint (juce::Graphics&) override;
private:
    float freq = 0, smoothedCents = 0; int note = -1; float conf = 0;
};

struct Section
{
    juce::String title;
    juce::Rectangle<int> bounds;
    PowerButton* power = nullptr;
    std::vector<juce::Component*> kids;
};
} // namespace abyss::ui

// =============================================================================
class MainView : public juce::Component, private juce::Timer
{
public:
    explicit MainView (AbyssToneProcessor&);
    ~MainView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int W = 1180, H = 700;

private:
    void timerCallback() override;
    void layoutSection (abyss::ui::Section&);
    void refreshVoiceButtons();
    void refreshPresetBox();

    AbyssToneProcessor& proc;
    juce::AudioProcessorValueTreeState& s;
    abyss::ui::LnF lnf;

    using Knob = abyss::ui::Knob;
    using Power = abyss::ui::PowerButton;

    // header
    juce::ComboBox presetBox;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, loadBtn { "LOAD" }, tunerBtn { "TUNER" };
    Knob inKnob, outKnob;
    abyss::ui::Meter inMeter, outMeter;

    // modules
    Power gateOn, odOn, cabOn, tightOn, eqOn, dblOn, dlyOn, revOn;
    Knob gateThr, odDrive, odTone, odLevel;
    juce::TextButton voiceBtn[3] { juce::TextButton ("CLEAN"), juce::TextButton ("THRASH"), juce::TextButton ("MODERN") };
    std::unique_ptr<juce::ParameterAttachment> voiceAtt;
    Knob ampGain, ampBass, ampMid, ampTreble, ampPres, ampDepth, ampMaster, ampLayer;
    juce::ToggleButton wideBtn { "WIDE L/R" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> wideAtt;
    juce::Label layerInfo;

    juce::ComboBox cabBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cabAtt;
    Knob cabMic;
    juce::TextButton irBtn { "LOAD IR..." };
    juce::Label irLabel;

    Knob tightAmt;
    abyss::ui::Meter tightMeter;
    std::vector<std::unique_ptr<abyss::ui::EqBand>> eqBands;
    Knob dblAmt;
    Knob dlyTime, dlyFb, dlyMix;
    juce::ComboBox syncBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncAtt;
    Knob revSize, revDamp, revMix;

    abyss::ui::TunerView tunerView;
    std::vector<abyss::ui::Section> sections;
    juce::Rectangle<int> ampArea;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastIrVersion = -1;
    std::vector<float> tunerBuf;
};

class AbyssToneEditor : public juce::AudioProcessorEditor
{
public:
    explicit AbyssToneEditor (AbyssToneProcessor&);
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (abyss::ui::col::bg); }
private:
    MainView view;
};
