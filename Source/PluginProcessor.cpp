#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace abyss;

AbyssToneProcessor::AbyssToneProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AbyssTone", createLayout())
{
}

bool AbyssToneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void AbyssToneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int maxBlock = juce::jmax (16, samplesPerBlock);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2, osFactorLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler->initProcessing ((size_t) maxBlock);
    setLatencySamples ((int) std::lround (oversampler->getLatencyInSamples()));

    pathBuffer.setSize (2, maxBlock, false, true, true);

    const float fs = (float) sampleRate;
    const float fsOS = fs * (float) (1 << osFactorLog2);
    gate.prepare (sampleRate);
    for (int i = 0; i < 2; ++i)
    {
        od[i].prepare (fsOS);
        pre[i].prepare (fsOS);
        power[i].prepare (fs);
        cab[i].prepare (fs);
    }
    irConv.prepare ({ sampleRate, (juce::uint32) maxBlock, 2 });
    tight.prepare (fs);
    eq.prepare (fs);
    doubler.prepare (fs);
    delay.prepare (fs);
    reverb.prepare ({ sampleRate, (juce::uint32) maxBlock, 2 });
    reverb.reset();

    for (auto* s : { &inGainSm, &outGainSm, &layerSm, &dblSm, &dlyMixSm, &revMixSm })
        s->reset (sampleRate, 0.03);
    inGainSm.setCurrentAndTargetValue (dbToGain (p (pid::inGain)->load()));
    outGainSm.setCurrentAndTargetValue (dbToGain (p (pid::outGain)->load()));
    layerSm.setCurrentAndTargetValue (p (pid::ampLayer)->load());

    const int decim = juce::jmax (1, (int) std::lround (sampleRate / 12000.0));
    tunerSampleRate = (float) (sampleRate / decim);
    tunerDecim = 0; tunerAcc = 0;

    delay.setTime (getDelaySeconds(), true);
    updateParameters();
}

float AbyssToneProcessor::getDelaySeconds() const
{
    const int sync = (int) p (pid::dlySync)->load();
    if (sync == 0) return p (pid::dlyTime)->load() / 1000.0f;
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;
    static const float beats[] = { 0, 2.0f, 1.0f, 1.5f, 0.5f, 0.75f, 0.25f };
    return juce::jlimit (0.02f, 2.0f, (float) (beats[juce::jlimit (0, 6, sync)] * 60.0 / bpm));
}

void AbyssToneProcessor::updateParameters()
{
    gate.setThreshold (p (pid::gateThr)->load());

    for (int i = 0; i < 2; ++i)
        od[i].update (p (pid::odDrive)->load() / 10.0f, p (pid::odTone)->load() / 10.0f, p (pid::odLevel)->load());

    const auto voice = (AmpVoice) (int) p (pid::ampVoice)->load();
    const auto layerVoice = voice == AmpVoice::Thrash ? AmpVoice::Modern : AmpVoice::Thrash;
    const float gain01 = p (pid::ampGain)->load() / 10.0f;
    pre[0].configure (voice, gain01);
    pre[1].configure (layerVoice, gain01);

    const float bass = p (pid::ampBass)->load(), mid = p (pid::ampMid)->load(), tre = p (pid::ampTreble)->load();
    const float pres = p (pid::ampPresence)->load(), dep = p (pid::ampDepth)->load(), mas = p (pid::ampMaster)->load();
    power[0].set (voice, bass, mid, tre, pres, dep, mas);
    power[1].set (layerVoice, bass, mid, tre, pres, dep, mas);

    const auto model = (CabModel) (int) p (pid::cabModel)->load();
    const auto iirModel = model == CabModel::CustomIR ? CabModel::Modern412 : model;
    for (auto& c : cab) c.set (iirModel, p (pid::cabMic)->load());

    tight.setAmount (p (pid::tightAmt)->load());

    float g[BusEQ::numBands];
    for (int i = 0; i < BusEQ::numBands; ++i) g[i] = p (pid::eqBand[i])->load();
    eq.set (g);

    juce::dsp::Reverb::Parameters rp;
    const float mix = p (pid::revMix)->load();
    rp.roomSize = 0.3f + 0.68f * p (pid::revSize)->load();
    rp.damping = p (pid::revDamp)->load();
    // juce::Reverb scales wet by 3 and dry by 2 internally
    rp.wetLevel = mix * 0.22f;
    rp.dryLevel = 0.5f * (1.0f - 0.35f * mix);
    rp.width = 1.0f;
    reverb.setParameters (rp);
}

void AbyssToneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numOut = buffer.getNumChannels();
    if (numSamples == 0 || numOut == 0) return;

    updateParameters();
    delay.setTime (getDelaySeconds(), false);

    inGainSm.setTargetValue (dbToGain (p (pid::inGain)->load()));
    outGainSm.setTargetValue (dbToGain (p (pid::outGain)->load()));
    layerSm.setTargetValue (p (pid::ampLayer)->load());
    dblSm.setTargetValue (p (pid::dblOn)->load() > 0.5f ? p (pid::dblAmt)->load() : 0.0f);
    dlyMixSm.setTargetValue (p (pid::dlyOn)->load() > 0.5f ? p (pid::dlyMix)->load() : 0.0f);

    const bool gateOn = p (pid::gateOn)->load() > 0.5f;
    const bool odOn = p (pid::odOn)->load() > 0.5f;
    const bool cabOn = p (pid::cabOn)->load() > 0.5f;
    const bool useIr = p (pid::cabModel)->load() > 2.5f && irFile.existsAsFile();
    const bool tightOn = p (pid::tightOn)->load() > 0.5f;
    const bool eqOn = p (pid::eqOn)->load() > 0.5f;
    const bool revOn = p (pid::revOn)->load() > 0.5f;
    const bool wide = p (pid::ampWide)->load() > 0.5f;
    const float dlyFb = p (pid::dlyFb)->load();
    const bool tuning = tunerActive.load();
    const int decim = juce::jmax (1, (int) std::lround (sr / 12000.0));

    float peakIn = 0, peakOut = 0;
    const int maxChunk = pathBuffer.getNumSamples();

    for (int start = 0; start < numSamples; start += maxChunk)
    {
        const int len = juce::jmin (maxChunk, numSamples - start);
        const float* in = buffer.getReadPointer (0, start);
        float* a = pathBuffer.getWritePointer (0);
        float* b = pathBuffer.getWritePointer (1);

        // 1) input gain, tuner feed, gate
        for (int i = 0; i < len; ++i)
        {
            float x = in[i] * inGainSm.getNextValue();
            peakIn = juce::jmax (peakIn, std::abs (x));
            tunerAcc += x;
            if (++tunerDecim >= decim) { tunerFifo.push (tunerAcc / (float) decim); tunerAcc = 0; tunerDecim = 0; }
            if (gateOn) x = gate.process (x);
            a[i] = b[i] = x;
        }

        const bool layerActive = layerSm.getCurrentValue() > 1.0e-4f || layerSm.isSmoothing();

        // 2) overdrive + preamp, oversampled
        {
            juce::dsp::AudioBlock<float> block (pathBuffer);
            auto sub = block.getSubBlock (0, (size_t) len);
            auto os = oversampler->processSamplesUp (sub);
            const int nOS = (int) os.getNumSamples();
            for (int c = 0; c < (layerActive ? 2 : 1); ++c)
            {
                float* d = os.getChannelPointer ((size_t) c);
                for (int i = 0; i < nOS; ++i)
                {
                    float x = d[i];
                    if (odOn) x = od[c].process (x);
                    d[i] = pre[c].process (x);
                }
            }
            oversampler->processSamplesDown (sub);
        }

        // 3) tone stack / power amp
        for (int i = 0; i < len; ++i) a[i] = power[0].process (a[i]);
        if (layerActive) for (int i = 0; i < len; ++i) b[i] = power[1].process (b[i]);

        // 4) cabinet
        if (cabOn)
        {
            if (useIr)
            {
                juce::dsp::AudioBlock<float> block (pathBuffer);
                auto sub = block.getSubBlock (0, (size_t) len);
                irConv.process (juce::dsp::ProcessContextReplacing<float> (sub));
                for (int c = 0; c < 2; ++c) juce::FloatVectorOperations::multiply (pathBuffer.getWritePointer (c), 0.7f, len);
            }
            else
            {
                for (int i = 0; i < len; ++i) a[i] = cab[0].process (a[i]);
                if (layerActive) for (int i = 0; i < len; ++i) b[i] = cab[1].process (b[i]);
            }
        }

        // 5) combine paths -> stereo, post FX
        float* outL = buffer.getWritePointer (0, start);
        float* outR = numOut > 1 ? buffer.getWritePointer (1, start) : nullptr;

        for (int i = 0; i < len; ++i)
        {
            const float lay = layerSm.getNextValue();
            float l, r;
            if (wide) { l = a[i]; r = layerActive ? (1.0f - lay) * a[i] + lay * b[i] : a[i]; }
            else      { l = r = layerActive ? a[i] * (1.0f - 0.5f * lay) + b[i] * 0.5f * lay : a[i]; }

            if (tightOn) tight.process (l, r);
            if (eqOn) { l = eq.process (0, l); r = eq.process (1, r); }

            const float dAmt = dblSm.getNextValue();
            if (dAmt > 1.0e-4f) doubler.process (l, r, dAmt);

            const float dMix = dlyMixSm.getNextValue();
            delay.process (l, r, dMix > 1.0e-4f ? dlyFb : 0.0f, dMix);

            outL[i] = l;
            if (outR) outR[i] = r;
        }
    }

    for (int ch = 2; ch < numOut; ++ch) buffer.clear (ch, 0, numSamples);

    // 6) reverb (stereo)
    if (revOn && numOut > 1)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        auto st = block.getSubsetChannelBlock (0, 2);
        reverb.process (juce::dsp::ProcessContextReplacing<float> (st));
    }

    // 7) output
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = tuning ? 0.0f : outGainSm.getNextValue();
        for (int ch = 0; ch < juce::jmin (2, numOut); ++ch)
        {
            float& s = buffer.getWritePointer (ch)[i];
            s = juce::jlimit (-4.0f, 4.0f, s * g);   // safety
            peakOut = juce::jmax (peakOut, std::abs (s));
        }
    }

    inLevel = juce::jmax (peakIn, inLevel.load() * 0.85f);
    outLevel = juce::jmax (peakOut, outLevel.load() * 0.85f);
    tightGr = tight.lastGr;
}

// ------------------------------------------------------------------ programs
void AbyssToneProcessor::setCurrentProgram (int index)
{
    const auto& presets = factoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;

    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : presets[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

const juce::String AbyssToneProcessor::getProgramName (int index)
{
    const auto& presets = factoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? juce::String (presets[(size_t) index].name) : juce::String();
}

// ------------------------------------------------------------------ IR + state
void AbyssToneProcessor::loadImpulseResponse (const juce::File& file)
{
    if (! file.existsAsFile()) return;
    irConv.loadImpulseResponse (file, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes, 0,
                                juce::dsp::Convolution::Normalise::yes);
    irFile = file;
    ++irVersion;
    if (auto* m = apvts.getParameter (pid::cabModel))
        m->setValueNotifyingHost (m->convertTo0to1 (3.0f));
}

void AbyssToneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("irPath", irFile.getFullPathName(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void AbyssToneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.hasType (apvts.state.getType())) return;
        apvts.replaceState (tree);
        currentProgram = tree.getProperty ("program", 0);
        const juce::File f (tree.getProperty ("irPath").toString());
        if (f.existsAsFile())
        {
            irConv.loadImpulseResponse (f, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes, 0,
                                        juce::dsp::Convolution::Normalise::yes);
            irFile = f; ++irVersion;
        }
    }
}

bool AbyssToneProcessor::savePresetToFile (const juce::File& f)
{
    auto state = apvts.copyState();
    state.setProperty ("irPath", irFile.getFullPathName(), nullptr);
    if (auto xml = state.createXml()) return xml->writeTo (f);
    return false;
}

bool AbyssToneProcessor::loadPresetFromFile (const juce::File& f)
{
    if (auto xml = juce::parseXML (f))
    {
        juce::MemoryBlock mb;
        copyXmlToBinary (*xml, mb);
        setStateInformation (mb.getData(), (int) mb.getSize());
        return true;
    }
    return false;
}

juce::AudioProcessorEditor* AbyssToneProcessor::createEditor() { return new AbyssToneEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AbyssToneProcessor(); }
