#include "PluginEditor.h"

using namespace abyss::ui;
namespace pid = abyss::pid;

// =============================================================================
//  LookAndFeel
// =============================================================================
LnF::LnF()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::bg);
    setColour (juce::BubbleComponent::backgroundColourId, col::panelHi);
    setColour (juce::BubbleComponent::outlineColourId, col::accent);
    setColour (juce::TooltipWindow::textColourId, col::text);
    setColour (juce::ComboBox::backgroundColourId, col::panelHi);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, col::border);
    setColour (juce::ComboBox::arrowColourId, col::accent);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::TextButton::textColourOffId, col::text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

void LnF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& sl)
{
    const float r = juce::jmin (w, h) * 0.5f - 5.0f;
    const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
    const float ang = a0 + pos * (a1 - a0);
    const bool enabled = sl.isEnabled();

    // track + value arc
    juce::Path track; track.addCentredArc (cx, cy, r, r, 0, a0, a1, true);
    g.setColour (col::border);
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool bipolar = sl.getMinimum() < 0 && sl.getMaximum() > 0;
    const float from = bipolar ? a0 + (float) sl.valueToProportionOfLength (0.0) * (a1 - a0) : a0;
    juce::Path val; val.addCentredArc (cx, cy, r, r, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
    const auto ac = enabled ? col::accent : col::dim;
    g.setColour (ac.withAlpha (0.25f));
    g.strokePath (val, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (ac);
    g.strokePath (val, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // body
    const float br = r * 0.74f;
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (cx - br, cy - br + 3.0f, br * 2, br * 2);
    juce::ColourGradient grad (juce::Colour (0xff3b3f46), cx, cy - br, juce::Colour (0xff111316), cx, cy + br, false);
    g.setGradientFill (grad);
    g.fillEllipse (cx - br, cy - br, br * 2, br * 2);

    // knurling
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    for (int i = 0; i < 28; ++i)
    {
        const float a = (float) i / 28.0f * juce::MathConstants<float>::twoPi;
        g.drawLine (cx + std::sin (a) * br * 0.86f, cy - std::cos (a) * br * 0.86f,
                    cx + std::sin (a) * br * 0.98f, cy - std::cos (a) * br * 0.98f, 1.0f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawEllipse (cx - br + 0.5f, cy - br + 0.5f, br * 2 - 1, br * 2 - 1, 1.0f);

    // pointer
    juce::Path ptr; ptr.addRoundedRectangle (-1.6f, -br * 0.9f, 3.2f, br * 0.48f, 1.6f);
    g.setColour (enabled ? col::text : col::dim);
    g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (cx, cy));
}

void LnF::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                            juce::Slider::SliderStyle, juce::Slider& sl)
{
    const float cx = (float) x + (float) w * 0.5f;
    g.setColour (col::border);
    g.fillRoundedRectangle (cx - 2.0f, (float) y, 4.0f, (float) h, 2.0f);

    // 0 dB tick marks
    const float zero = (float) sl.getPositionOfValue (0.0);
    g.setColour (col::dim.withAlpha (0.6f));
    g.drawLine (cx - 10, zero, cx + 10, zero, 1.0f);

    g.setColour (col::accent);
    g.fillRoundedRectangle (cx - 2.0f, juce::jmin (zero, pos), 4.0f, std::abs (zero - pos), 2.0f);

    const juce::Rectangle<float> thumb (cx - 12.0f, pos - 6.0f, 24.0f, 12.0f);
    juce::ColourGradient grad (juce::Colour (0xff42464e), thumb.getX(), thumb.getY(), juce::Colour (0xff17191c), thumb.getX(), thumb.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (thumb, 3.0f);
    g.setColour (col::text);
    g.drawLine (thumb.getX() + 4, pos, thumb.getRight() - 4, pos, 1.5f);
}

void LnF::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool hl, bool)
{
    const bool on = b.getToggleState();
    if (dynamic_cast<PowerButton*> (&b) != nullptr)
    {
        auto r = b.getLocalBounds().toFloat().reduced (2);
        g.setColour (col::bg);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
        const float d = r.getHeight() - 4;
        const float lx = on ? r.getRight() - d - 2 : r.getX() + 2;
        if (on)
        {
            g.setColour (col::accent.withAlpha (0.35f));
            g.fillEllipse (lx - 3, r.getY() - 1, d + 6, d + 6);
        }
        g.setColour (on ? col::accent : (hl ? col::dim : col::border.brighter (0.3f)));
        g.fillEllipse (lx, r.getY() + 2, d, d);
        return;
    }
    // LED + text toggle
    auto r = b.getLocalBounds().toFloat();
    const float d = 10.0f;
    const juce::Rectangle<float> led (r.getX() + 4, r.getCentreY() - d / 2, d, d);
    if (on) { g.setColour (col::accent.withAlpha (0.35f)); g.fillEllipse (led.expanded (3)); }
    g.setColour (on ? col::accent : col::border.brighter (0.3f));
    g.fillEllipse (led);
    g.setColour (hl ? col::text : col::text.withAlpha (0.85f));
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (b.getButtonText(), r.withTrimmedLeft (20), juce::Justification::centredLeft);
}

void LnF::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hl, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    juce::Colour c = on ? col::accent : col::panelHi;
    if (hl) c = c.brighter (0.12f);
    if (down) c = c.darker (0.15f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? col::accent.brighter (0.3f) : col::border);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void LnF::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setColour (b.getToggleState() ? juce::Colours::white : col::text);
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

void LnF::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (col::panelHi);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.hasKeyboardFocus (false) ? col::accent : col::border);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (col::accent);
    g.fillPath (arrow);
}

juce::Label* LnF::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setColour (juce::Label::textColourId, col::text);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

// =============================================================================
//  Widgets
// =============================================================================
static void setupDefaultReset (juce::Slider& sl, juce::AudioProcessorValueTreeState& s, const char* id)
{
    if (auto* p = s.getParameter (id))
        sl.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

Knob::Knob (juce::AudioProcessorValueTreeState& s, const char* paramId, juce::String cap, bool isBig)
    : caption (std::move (cap)), big (isBig)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, paramId, slider);
    setupDefaultReset (slider, s, paramId);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (18);
    const int d = juce::jmin (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (d, d));
}

void Knob::paint (juce::Graphics& g)
{
    g.setColour (col::text.withAlpha (0.9f));
    g.setFont (juce::FontOptions (big ? 13.0f : 11.5f, juce::Font::bold));
    g.drawText (caption, getLocalBounds().removeFromBottom (18), juce::Justification::centred);
}

EqBand::EqBand (juce::AudioProcessorValueTreeState& s, const char* paramId, juce::String cap) : caption (std::move (cap))
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (slider);
    att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, paramId, slider);
    setupDefaultReset (slider, s, paramId);
    slider.onValueChange = [this] { repaint(); };
}

void EqBand::resized() { slider.setBounds (getLocalBounds().withTrimmedTop (18).withTrimmedBottom (20)); }

void EqBand::paint (juce::Graphics& g)
{
    const double v = slider.getValue();
    g.setColour (std::abs (v) > 0.05 ? col::accent2 : col::dim);
    g.setFont (juce::FontOptions (10.5f));
    g.drawText ((v > 0 ? "+" : "") + juce::String (v, 1), getLocalBounds().removeFromTop (16), juce::Justification::centred);
    g.setColour (col::text.withAlpha (0.9f));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (caption, getLocalBounds().removeFromBottom (18), juce::Justification::centred);
}

PowerButton::PowerButton (juce::AudioProcessorValueTreeState& s, const char* paramId)
{
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, paramId, *this);
    setTooltip ("On / Off");
}

void Meter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::bg);
    g.fillRoundedRectangle (r, 2.0f);
    const float db = juce::Decibels::gainToDecibels (level, -60.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    auto fill = r.reduced (1.5f);
    fill = fill.removeFromBottom (fill.getHeight() * frac);
    juce::ColourGradient grad (col::accent2, 0, r.getBottom(), juce::Colours::red, 0, r.getY(), false);
    grad.addColour (0.75, col::accent);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (fill, 1.5f);
    g.setColour (col::border);
    g.drawRoundedRectangle (r, 2.0f, 1.0f);
}

void TunerView::update (float f, float clarity)
{
    if (f > 25.0f && f < 1500.0f && clarity > 0.75f)
    {
        const float midi = 69.0f + 12.0f * std::log2 (f / 440.0f);
        const int n = (int) std::lround (midi);
        const float cents = (midi - (float) n) * 100.0f;
        smoothedCents = n == note ? 0.65f * smoothedCents + 0.35f * cents : cents;
        note = n; freq = f; conf = 1.0f;
    }
    else conf *= 0.92f;
    repaint();
}

void TunerView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::bg.withAlpha (0.96f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (col::accent.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

    g.setColour (col::dim);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("TUNER  -  OUTPUT MUTED", r.removeFromTop (34), juce::Justification::centred);

    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const bool has = note >= 0 && conf > 0.2f;
    const bool inTune = has && std::abs (smoothedCents) < 3.0f;
    const auto c = inTune ? juce::Colour (0xff4ad17a) : col::text;

    auto noteArea = r.removeFromTop (r.getHeight() * 0.55f);
    g.setColour (c.withAlpha (has ? 1.0f : 0.25f));
    g.setFont (juce::FontOptions (84.0f, juce::Font::bold));
    g.drawText (has ? juce::String (names[note % 12]) : juce::String ("-"), noteArea, juce::Justification::centred);
    if (has)
    {
        g.setFont (juce::FontOptions (20.0f));
        g.setColour (col::dim);
        g.drawText (juce::String (note / 12 - 1), noteArea.withTrimmedLeft (noteArea.getWidth() * 0.5f + 46).withTrimmedTop (40),
                    juce::Justification::centredLeft);
    }

    // cents scale
    auto scale = r.removeFromTop (40).reduced (60, 8);
    const float mid = scale.getCentreX();
    g.setColour (col::border);
    g.fillRect (scale.getX(), scale.getCentreY() - 1, scale.getWidth(), 2.0f);
    for (int i = -50; i <= 50; i += 10)
    {
        const float x = mid + (float) i / 50.0f * scale.getWidth() * 0.5f;
        const float th = i == 0 ? 12.0f : 6.0f;
        g.setColour (i == 0 ? col::text : col::dim);
        g.fillRect (x - 0.5f, scale.getCentreY() - th, 1.0f, th * 2);
    }
    if (has)
    {
        const float x = mid + juce::jlimit (-50.0f, 50.0f, smoothedCents) / 50.0f * scale.getWidth() * 0.5f;
        g.setColour (inTune ? c : col::accent);
        g.fillRoundedRectangle (x - 3, scale.getY() - 4, 6, scale.getHeight() + 8, 3);
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (juce::String (freq, 1) + " Hz   " + (smoothedCents >= 0 ? "+" : "") + juce::String ((int) std::lround (smoothedCents)) + " ct",
                    r.removeFromTop (28), juce::Justification::centred);
    }
}

// =============================================================================
//  MainView
// =============================================================================
MainView::MainView (AbyssToneProcessor& p)
    : proc (p), s (p.apvts),
      inKnob (s, pid::inGain, "IN"), outKnob (s, pid::outGain, "OUT"),
      gateOn (s, pid::gateOn), odOn (s, pid::odOn), cabOn (s, pid::cabOn), tightOn (s, pid::tightOn),
      eqOn (s, pid::eqOn), dblOn (s, pid::dblOn), dlyOn (s, pid::dlyOn), revOn (s, pid::revOn),
      gateThr (s, pid::gateThr, "THRESHOLD", true),
      odDrive (s, pid::odDrive, "DRIVE"), odTone (s, pid::odTone, "TONE"), odLevel (s, pid::odLevel, "LEVEL"),
      ampGain (s, pid::ampGain, "GAIN", true), ampBass (s, pid::ampBass, "BASS", true), ampMid (s, pid::ampMid, "MID", true),
      ampTreble (s, pid::ampTreble, "TREBLE", true), ampPres (s, pid::ampPresence, "PRESENCE", true),
      ampDepth (s, pid::ampDepth, "DEPTH", true), ampMaster (s, pid::ampMaster, "MASTER", true),
      ampLayer (s, pid::ampLayer, "LAYER"),
      cabMic (s, pid::cabMic, "SM57 <> MD421"),
      tightAmt (s, pid::tightAmt, "AMOUNT"),
      dblAmt (s, pid::dblAmt, "AMOUNT"),
      dlyTime (s, pid::dlyTime, "TIME"), dlyFb (s, pid::dlyFb, "FEEDBACK"), dlyMix (s, pid::dlyMix, "MIX"),
      revSize (s, pid::revSize, "SIZE"), revDamp (s, pid::revDamp, "DAMP"), revMix (s, pid::revMix, "MIX")
{
    setLookAndFeel (&lnf);

    // ---------------- header
    for (const auto& pr : abyss::factoryPresets())
        presetBox.addItem (pr.name, presetBox.getNumItems() + 1);
    presetBox.setTextWhenNothingSelected ("Preset");
    presetBox.onChange = [this] { if (const int id = presetBox.getSelectedId(); id > 0) proc.setCurrentProgram (id - 1); };
    addAndMakeVisible (presetBox);

    auto step = [this] (int dir)
    {
        const int n = presetBox.getNumItems();
        const int cur = juce::jmax (0, presetBox.getSelectedId() - 1);
        presetBox.setSelectedId ((cur + dir + n) % n + 1);
    };
    prevBtn.onClick = [step] { step (-1); };
    nextBtn.onClick = [step] { step (1); };

    const auto presetDir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Abyss Tone").getChildFile ("Presets");
    saveBtn.onClick = [this, presetDir]
    {
        presetDir.createDirectory();
        chooser = std::make_unique<juce::FileChooser> ("Save preset", presetDir, "*.abyss");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  auto f = fc.getResult();
                                  if (f == juce::File()) return;
                                  f = f.withFileExtension ("abyss");
                                  if (proc.savePresetToFile (f)) presetBox.setText (f.getFileNameWithoutExtension(), juce::dontSendNotification);
                              });
    };
    loadBtn.onClick = [this, presetDir]
    {
        presetDir.createDirectory();
        chooser = std::make_unique<juce::FileChooser> ("Load preset", presetDir, "*.abyss");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile() && proc.loadPresetFromFile (f))
                                      presetBox.setText (f.getFileNameWithoutExtension(), juce::dontSendNotification);
                              });
    };

    tunerBtn.setClickingTogglesState (true);
    tunerBtn.onClick = [this]
    {
        const bool on = tunerBtn.getToggleState();
        proc.tunerActive = on;
        tunerView.setVisible (on);
    };

    for (auto* c : std::initializer_list<juce::Component*> { &prevBtn, &nextBtn, &saveBtn, &loadBtn, &tunerBtn, &inKnob, &outKnob, &inMeter, &outMeter })
        addAndMakeVisible (c);

    // ---------------- amp channel buttons
    if (auto* vp = s.getParameter (pid::ampVoice))
    {
        voiceAtt = std::make_unique<juce::ParameterAttachment> (*vp, [this] (float) { refreshVoiceButtons(); }, nullptr);
        for (int i = 0; i < 3; ++i)
        {
            voiceBtn[i].setRadioGroupId (101);
            voiceBtn[i].onClick = [this, i] { voiceAtt->setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (voiceBtn[i]);
        }
    }
    wideAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (s, pid::ampWide, wideBtn);
    layerInfo.setFont (juce::FontOptions (12.0f));
    layerInfo.setColour (juce::Label::textColourId, col::dim);
    addAndMakeVisible (wideBtn);
    addAndMakeVisible (layerInfo);

    // ---------------- cab
    cabBox.addItemList (s.getParameter (pid::cabModel)->getAllValueStrings(), 1);
    cabAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, pid::cabModel, cabBox);
    irBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Load cabinet impulse response",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.wav;*.aif;*.aiff");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc) { if (fc.getResult().existsAsFile()) proc.loadImpulseResponse (fc.getResult()); });
    };
    irLabel.setFont (juce::FontOptions (11.5f));
    irLabel.setJustificationType (juce::Justification::centred);
    irLabel.setColour (juce::Label::textColourId, col::dim);
    addAndMakeVisible (cabBox);
    addAndMakeVisible (irBtn);
    addAndMakeVisible (irLabel);

    // ---------------- delay sync
    syncBox.addItemList (s.getParameter (pid::dlySync)->getAllValueStrings(), 1);
    syncAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (s, pid::dlySync, syncBox);
    addAndMakeVisible (syncBox);

    // ---------------- EQ
    for (int i = 0; i < abyss::BusEQ::numBands; ++i)
    {
        eqBands.push_back (std::make_unique<EqBand> (s, pid::eqBand[i], abyss::eqBandNames[i]));
        addAndMakeVisible (*eqBands.back());
    }

    // ---------------- all knobs: popup value display
    for (auto* k : { &inKnob, &outKnob, &gateThr, &odDrive, &odTone, &odLevel, &ampGain, &ampBass, &ampMid, &ampTreble, &ampPres,
                     &ampDepth, &ampMaster, &ampLayer, &cabMic, &tightAmt, &dblAmt, &dlyTime, &dlyFb, &dlyMix, &revSize, &revDamp, &revMix })
    {
        k->slider.setPopupDisplayEnabled (true, true, this);
        addAndMakeVisible (k);
    }
    for (auto& b : eqBands) b->slider.setPopupDisplayEnabled (true, true, this);
    addAndMakeVisible (tightMeter);

    // ---------------- sections
    std::vector<juce::Component*> eqKids;
    for (auto& b : eqBands) eqKids.push_back (b.get());
    sections = {
        { "GATE",       { 16, 84, 150, 280 },  &gateOn, { &gateThr } },
        { "OVERDRIVE",  { 176, 84, 250, 280 }, &odOn,   { &odDrive, &odTone, &odLevel } },
        { "AMPLIFIER",  { 436, 84, 728, 280 }, nullptr, {} },
        { "CABINET",    { 16, 376, 230, 308 }, &cabOn,  { &cabBox, &cabMic, &irBtn, &irLabel } },
        { "TIGHT",      { 256, 376, 122, 308 }, &tightOn, { &tightAmt, &tightMeter } },
        { "BUS EQ",     { 388, 376, 254, 308 }, &eqOn,  eqKids },
        { "DOUBLER",    { 652, 376, 122, 308 }, &dblOn, { &dblAmt } },
        { "DELAY",      { 784, 376, 196, 308 }, &dlyOn, { &dlyTime, &dlyFb, &dlyMix, &syncBox } },
        { "REVERB",     { 990, 376, 174, 308 }, &revOn, { &revSize, &revDamp, &revMix } },
    };
    for (auto& sec : sections)
        if (sec.power) addAndMakeVisible (sec.power);
    ampArea = sections[2].bounds;

    addChildComponent (tunerView);
    tunerBuf.resize (2048);

    refreshVoiceButtons();
    refreshPresetBox();
    timerCallback();
    setSize (W, H);
    startTimerHz (30);
}

MainView::~MainView() { setLookAndFeel (nullptr); }

void MainView::refreshVoiceButtons()
{
    const int v = (int) s.getRawParameterValue (pid::ampVoice)->load();
    for (int i = 0; i < 3; ++i) voiceBtn[i].setToggleState (i == v, juce::dontSendNotification);
    layerInfo.setText (juce::String ("Layer blends in the ") + (v == 1 ? "MODERN" : "THRASH") + " voice (2-amp wall)", juce::dontSendNotification);
    repaint (ampArea);
}

void MainView::refreshPresetBox()
{
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
}

void MainView::resized()
{
    // header
    prevBtn.setBounds (344, 21, 30, 30);
    presetBox.setBounds (380, 21, 300, 30);
    nextBtn.setBounds (686, 21, 30, 30);
    saveBtn.setBounds (726, 21, 62, 30);
    loadBtn.setBounds (794, 21, 62, 30);
    tunerBtn.setBounds (872, 21, 76, 30);
    inKnob.setBounds (966, 6, 60, 64);
    inMeter.setBounds (1028, 14, 7, 44);
    outKnob.setBounds (1060, 6, 60, 64);
    outMeter.setBounds (1122, 14, 7, 44);

    for (auto& sec : sections)
        if (sec.power) sec.power->setBounds (sec.bounds.getX() + 9, sec.bounds.getY() + 9, 32, 18);

    auto at = [] (const juce::Rectangle<int>& b, int dx, int dy, int w, int h) { return juce::Rectangle<int> (b.getX() + dx, b.getY() + dy, w, h); };

    // gate
    { const auto b = sections[0].bounds; gateThr.setBounds (at (b, 15, 70, 120, 140)); }
    // overdrive
    {
        const auto b = sections[1].bounds;
        odDrive.setBounds (at (b, 10, 92, 76, 92));
        odTone.setBounds (at (b, 87, 92, 76, 92));
        odLevel.setBounds (at (b, 164, 92, 76, 92));
    }
    // amp
    {
        const auto b = ampArea;
        for (int i = 0; i < 3; ++i) voiceBtn[i].setBounds (at (b, 104 + i * 98, 44, 92, 28));
        Knob* ks[] = { &ampGain, &ampBass, &ampMid, &ampTreble, &ampPres, &ampDepth, &ampMaster };
        for (int i = 0; i < 7; ++i) ks[i]->setBounds (at (b, 18 + i * 99, 86, 96, 112));
        ampLayer.setBounds (at (b, 18, 204, 70, 70));
        wideBtn.setBounds (at (b, 100, 226, 110, 24));
        layerInfo.setBounds (at (b, 214, 226, 330, 24));
        tunerView.setBounds (b.reduced (6));
    }
    // cab
    {
        const auto b = sections[3].bounds;
        cabBox.setBounds (at (b, 12, 44, b.getWidth() - 24, 28));
        cabMic.setBounds (at (b, 40, 84, b.getWidth() - 80, 112));
        irBtn.setBounds (at (b, 12, 206, b.getWidth() - 24, 28));
        irLabel.setBounds (at (b, 12, 238, b.getWidth() - 24, 40));
    }
    // tight
    {
        const auto b = sections[4].bounds;
        tightAmt.setBounds (at (b, 8, 50, b.getWidth() - 16, 100));
        tightMeter.setBounds (at (b, b.getWidth() / 2 - 5, 172, 10, 90));
    }
    // eq
    {
        const auto b = sections[5].bounds;
        const int bw = (b.getWidth() - 20) / abyss::BusEQ::numBands;
        for (int i = 0; i < (int) eqBands.size(); ++i) eqBands[(size_t) i]->setBounds (at (b, 10 + i * bw, 42, bw, b.getHeight() - 56));
    }
    // doubler
    { const auto b = sections[6].bounds; dblAmt.setBounds (at (b, 8, 50, b.getWidth() - 16, 100)); }
    // delay
    {
        const auto b = sections[7].bounds;
        dlyTime.setBounds (at (b, 8, 44, 88, 100));
        dlyFb.setBounds (at (b, 100, 44, 88, 100));
        dlyMix.setBounds (at (b, 8, 160, 88, 100));
        syncBox.setBounds (at (b, 104, 206, 80, 26));
    }
    // reverb
    {
        const auto b = sections[8].bounds;
        revSize.setBounds (at (b, 4, 44, 83, 100));
        revDamp.setBounds (at (b, 87, 44, 83, 100));
        revMix.setBounds (at (b, 45, 160, 83, 100));
    }
}

void MainView::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);

    // subtle vignette
    juce::ColourGradient vg (juce::Colour (0xff1a1410), (float) W * 0.5f, 0, col::bg, (float) W * 0.5f, (float) H * 0.7f, true);
    g.setGradientFill (vg);
    g.fillRect (getLocalBounds());

    // header
    g.setColour (col::accent);
    g.fillEllipse (22, 27, 12, 12);
    g.setColour (col::accent.withAlpha (0.3f));
    g.fillEllipse (18, 23, 20, 20);
    g.setColour (col::text);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("ABYSS TONE", 44, 14, 260, 30, juce::Justification::centredLeft);
    g.setColour (col::dim);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("METAL GUITAR SUITE  /  v1.0", 46, 42, 260, 16, juce::Justification::centredLeft);
    g.setColour (col::border);
    g.fillRect (16, 74, W - 32, 1);

    for (auto& sec : sections)
    {
        const auto r = sec.bounds.toFloat();
        const bool isAmp = sec.title == "AMPLIFIER";
        if (isAmp)
        {
            juce::ColourGradient grad (juce::Colour (0xff24262b), r.getX(), r.getY(), juce::Colour (0xff141518), r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
        }
        else g.setColour (col::panel);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

        // header strip
        const int titleX = sec.power != nullptr ? 46 : 22;
        if (sec.power == nullptr)
        {
            g.setColour (col::accent);
            g.fillRoundedRectangle (r.getX() + 12, r.getY() + 14, 3, 12, 1.5f);
        }
        g.setColour (col::text);
        g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
        g.drawText (sec.title, (int) r.getX() + titleX, (int) r.getY() + 8, 200, 24, juce::Justification::centredLeft);
        g.setColour (col::border);
        g.fillRect (r.getX() + 10, r.getY() + 34, r.getWidth() - 20, 1.0f);
    }

    // amp face details
    {
        const auto b = ampArea;
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText ("CHANNEL", b.getX() + 22, b.getY() + 44, 80, 28, juce::Justification::centredLeft);
        g.setColour (col::accent.withAlpha (0.85f));
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText ("ABYSS 120  -  4x OVERSAMPLED", b.getRight() - 300, b.getY() + 44, 280, 28, juce::Justification::centredRight);
        g.setColour (col::border);
        g.fillRect (b.getX() + 14, b.getY() + 200, b.getWidth() - 28, 1);
    }

    // overdrive hint
    {
        const auto b = sections[1].bounds;
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawFittedText ("Drive 0 + Tone 0 = tightening\nfilter in front of the amp (~720 Hz focus)",
                          b.getX() + 12, b.getY() + 200, b.getWidth() - 24, 40, juce::Justification::centred, 2);
        g.setColour (col::accent2.withAlpha (0.9f));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText ("TS-808 STYLE", b.getX() + 12, b.getY() + 46, b.getWidth() - 24, 22, juce::Justification::centred);
    }
    // gate / tight / doubler / delay hints
    {
        g.setColour (col::dim);
        g.setFont (juce::FontOptions (11.0f));
        auto b = sections[0].bounds;
        g.drawFittedText ("Hysteresis gate\nbefore the amp", b.getX() + 8, b.getY() + 222, b.getWidth() - 16, 36, juce::Justification::centred, 2);
        b = sections[4].bounds;
        g.drawFittedText ("Palm-mute low end\n65-294 Hz", b.getX() + 6, b.getY() + 268, b.getWidth() - 12, 32, juce::Justification::centred, 2);
        g.drawText ("GR", b.getX(), b.getY() + 154, b.getWidth(), 16, juce::Justification::centred);
        b = sections[6].bounds;
        g.drawFittedText ("Fake double-\ntracking for\nwide rhythms", b.getX() + 6, b.getY() + 170, b.getWidth() - 12, 50, juce::Justification::centred, 3);
        b = sections[7].bounds;
        g.drawText ("SYNC", b.getX() + 104, b.getY() + 184, 80, 18, juce::Justification::centred);
    }
}

void MainView::timerCallback()
{
    inMeter.setLevel (proc.inLevel.load());
    outMeter.setLevel (proc.outLevel.load());
    tightMeter.setLevel (juce::jlimit (0.0f, 1.0f, 1.0f - proc.tightGr.load()) * 8.0f);

    for (auto& sec : sections)
        if (sec.power)
        {
            const float a = sec.power->getToggleState() ? 1.0f : 0.4f;
            for (auto* k : sec.kids) if (k->getAlpha() != a) k->setAlpha (a);
        }

    if (const int v = proc.irVersion.load(); v != lastIrVersion)
    {
        lastIrVersion = v;
        const auto n = proc.getIrName();
        irLabel.setText (n.isEmpty() ? "No IR loaded\n(built-in cab models)" : "IR: " + n, juce::dontSendNotification);
    }

    if (presetBox.getSelectedId() > 0 && presetBox.getSelectedId() != proc.getCurrentProgram() + 1)
        refreshPresetBox();

    if (tunerView.isVisible())
    {
        proc.tunerFifo.copyLatest (tunerBuf.data(), (int) tunerBuf.size());
        float clarity = 0;
        const float f = abyss::detectPitch (tunerBuf.data(), (int) tunerBuf.size(), proc.tunerSampleRate.load(), clarity);
        tunerView.update (f, clarity);
    }
}

// =============================================================================
AbyssToneEditor::AbyssToneEditor (AbyssToneProcessor& p) : AudioProcessorEditor (p), view (p)
{
    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (MainView::W * 6 / 10, MainView::H * 6 / 10, MainView::W * 2, MainView::H * 2);
    getConstrainer()->setFixedAspectRatio ((double) MainView::W / MainView::H);
    setSize (MainView::W, MainView::H);
}

void AbyssToneEditor::resized()
{
    const float scale = (float) getWidth() / (float) MainView::W;
    view.setTransform (juce::AffineTransform::scale (scale));
    view.setBounds (0, 0, MainView::W, MainView::H);
}
