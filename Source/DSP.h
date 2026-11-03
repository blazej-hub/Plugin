#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>

// =============================================================================
//  Abyss Tone – DSP building blocks
//  Voicing reference: thrash/metal rhythm chain
//  (TS-style filter boost -> high-gain amp -> 4x12 V30, SM57 + MD421 blend)
// =============================================================================
namespace abyss
{
constexpr float kPi = 3.14159265358979f;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

// ---------------------------------------------------------------- one-pole (TPT)
struct OnePole
{
    float G = 0.0f, s = 0.0f;
    void set (float fc, float fs)
    {
        fc = juce::jlimit (1.0f, 0.49f * fs, fc);
        const float g = std::tan (kPi * fc / fs);
        G = g / (1.0f + g);
    }
    void reset() { s = 0.0f; }
    inline float lp (float x) { const float v = (x - s) * G; const float y = v + s; s = y + v; return y; }
    inline float hp (float x) { return x - lp (x); }
};

// ---------------------------------------------------------------- RBJ biquad
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }
    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void setNorm (float B0, float B1, float B2, float A0, float A1, float A2)
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    static float w0 (float f, float fs) { return 2.0f * kPi * juce::jlimit (5.0f, 0.48f * fs, f) / fs; }

    void lowPass (float f, float q, float fs)
    {
        const float w = w0 (f, fs), c = std::cos (w), al = std::sin (w) / (2 * q);
        setNorm ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highPass (float f, float q, float fs)
    {
        const float w = w0 (f, fs), c = std::cos (w), al = std::sin (w) / (2 * q);
        setNorm ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void bandPass (float f, float q, float fs)   // 0 dB peak gain
    {
        const float w = w0 (f, fs), c = std::cos (w), al = std::sin (w) / (2 * q);
        setNorm (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
    void peak (float f, float q, float db, float fs)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = w0 (f, fs), c = std::cos (w), al = std::sin (w) / (2 * q);
        setNorm (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (float f, float db, float fs, float S = 1.0f)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = w0 (f, fs), c = std::cos (w), s = std::sin (w);
        const float al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2), sq = 2 * std::sqrt (A) * al;
        setNorm (A * ((A + 1) - (A - 1) * c + sq), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sq),
                 (A + 1) + (A - 1) * c + sq, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sq);
    }
    void highShelf (float f, float db, float fs, float S = 1.0f)
    {
        const float A = std::pow (10.0f, db / 40.0f), w = w0 (f, fs), c = std::cos (w), s = std::sin (w);
        const float al = s / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2), sq = 2 * std::sqrt (A) * al;
        setNorm (A * ((A + 1) + (A - 1) * c + sq), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sq),
                 (A + 1) - (A - 1) * c + sq, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sq);
    }
};

// ---------------------------------------------------------------- noise gate
class NoiseGate
{
public:
    void prepare (double fs)
    {
        sr = (float) fs;
        envAtt = std::exp (-1.0f / (0.002f * sr));
        envRel = std::exp (-1.0f / (0.012f * sr));
        gOpen  = std::exp (-1.0f / (0.0008f * sr));
        gClose = std::exp (-1.0f / (0.070f * sr));
        holdSamples = (int) (0.030f * sr);
        reset();
    }
    void reset() { env = 0; gain = 0; open = false; hold = 0; }
    void setThreshold (float db) { thrOpen = dbToGain (2.0f * db); thrClose = dbToGain (2.0f * (db - 6.0f)); }   // squared domain

    // detection on 'key', gain applied to 'x'
    inline float process (float x)
    {
        const float a = x * x;                       // RMS-style detection: ignores noise spikes
        env = a > env ? envAtt * env + (1 - envAtt) * a : envRel * env + (1 - envRel) * a;
        if (env > thrOpen) { open = true; hold = holdSamples; }
        else if (env < thrClose) { if (hold > 0) --hold; else open = false; }
        const float target = open ? 1.0f : 0.0f;
        const float c = target > gain ? gOpen : gClose;
        gain = c * gain + (1 - c) * target;
        return x * gain;
    }

private:
    float sr = 48000, env = 0, gain = 0, envAtt = 0, envRel = 0, gOpen = 0, gClose = 0;
    float thrOpen = 0.001f, thrClose = 0.0005f;
    bool open = false; int hold = 0, holdSamples = 1000;
};

// ---------------------------------------------------------------- TS-808 style
// Clean path + clipped mid-band path (feedback network ~720 Hz), tone LPF, level.
// With drive ~0 / tone low it works as a "tightening filter" in front of a
// high-gain amp (bass is not amplified, mids hit the amp harder).
class TubeScreamer
{
public:
    void prepare (float fsOversampled)
    {
        fs = fsOversampled;
        midHP.set (720.0f, fs);
        antiAliasLP.set (6500.0f, fs);
        update (0.0f, 0.0f, 0.0f);
        reset();
    }
    void reset() { midHP.reset(); toneLP.reset(); antiAliasLP.reset(); outHP.reset(); }
    void update (float drive01, float tone01, float levelDb)
    {
        gain  = 1.0f + 11.0f + drive01 * drive01 * 110.0f;     // 1 + (51k + 500k*pot)/4.7k approx
        toneLP.set (700.0f * std::pow (8.0f, tone01), fs);     // 700 Hz .. 5.6 kHz
        outHP.set (20.0f, fs);
        level = dbToGain (levelDb) * 0.5f;
    }
    inline float process (float x)
    {
        const float mid = antiAliasLP.lp (midHP.hp (x)) * gain;
        const float clipped = 0.45f * std::tanh (mid / 0.45f);       // silicon diode pair
        const float y = toneLP.lp (x + clipped);
        return outHP.hp (y) * level * 2.0f;
    }

private:
    float fs = 192000, gain = 12, level = 0.5f;
    OnePole midHP, toneLP, antiAliasLP, outHP;
};

// ---------------------------------------------------------------- preamp
enum class AmpVoice { Clean = 0, Thrash = 1, Modern = 2 };

struct PreampVoicing
{
    int   stages;
    float inHP;
    float gainMinDb, gainMaxDb;
    float stageGain[3];
    float interHP[3];
    float interLP[3];
    float bias[3];
};

inline const PreampVoicing& getVoicing (AmpVoice v)
{
    // Clean: one gentle stage, wide band
    static const PreampVoicing clean  { 1, 30.0f, -6.0f, 22.0f, { 1, 1, 1 }, { 20, 20, 20 }, { 14000, 14000, 14000 }, { 0.05f, 0, 0 } };
    // Thrash: 5150-inspired. Tight lows (cathode-cut between stages), aggressive upper mids.
    static const PreampVoicing thrash { 3, 50.0f, 12.0f, 42.0f, { 1, 9, 5 }, { 25, 100, 50 }, { 8000, 5500, 3800 }, { 0.12f, 0.22f, 0.08f } };
    // Modern: Rectifier-inspired. Looser bottom, more saturation, smoother top.
    static const PreampVoicing modern { 3, 30.0f, 12.0f, 44.0f, { 1, 11, 6 }, { 20, 45, 30 }, { 7500, 5000, 3600 }, { 0.20f, 0.30f, 0.15f } };
    switch (v) { case AmpVoice::Clean: return clean; case AmpVoice::Thrash: return thrash; default: return modern; }
}

class Preamp   // runs at the oversampled rate
{
public:
    void prepare (float fsOversampled) { fs = fsOversampled; configure (voice, gain01); reset(); }
    void reset() { inHP.reset(); dc.reset(); for (auto& f : hp) f.reset(); for (auto& f : lp) f.reset(); }
    void configure (AmpVoice v, float gainKnob01)
    {
        voice = v; gain01 = gainKnob01;
        const auto& p = getVoicing (v);
        inHP.set (p.inHP, fs);
        for (int i = 0; i < 3; ++i) { hp[i].set (p.interHP[i], fs); lp[i].set (p.interLP[i], fs); }
        dc.set (15.0f, fs);
        preGain = dbToGain (juce::jmap (gainKnob01, p.gainMinDb, p.gainMaxDb));
    }
    inline float process (float x)
    {
        const auto& p = getVoicing (voice);
        float y = inHP.hp (x) * preGain;
        for (int i = 0; i < p.stages; ++i)
        {
            if (i > 0) y *= p.stageGain[i];
            y = hp[i].hp (y);
            const float b = p.bias[i];
            y = std::tanh (y + b) - std::tanh (b);     // asymmetric triode-ish clip
            y = lp[i].lp (y);
        }
        return dc.hp (y);
    }

private:
    float fs = 192000, preGain = 1, gain01 = 0.5f;
    AmpVoice voice = AmpVoice::Thrash;
    OnePole inHP, dc, hp[3], lp[3];
};

// ---------------------------------------------------------------- tone stack + power amp (base rate)
class PowerSection
{
public:
    void prepare (float sampleRate) { fs = sampleRate; dirty = true; reset(); }
    void reset() { for (auto& b : bq) b.reset(); sagEnv = 0; }

    void set (AmpVoice v, float bass, float mid, float treble, float presence, float depth, float master)
    {
        if (! dirty && v == voice && bass == cb && mid == cm && treble == ct && presence == cp && depth == cd && master == cmas) return;
        dirty = false; voice = v; cb = bass; cm = mid; ct = treble; cp = presence; cd = depth; cmas = master;

        auto knob = [] (float k) { return (k - 5.0f) / 5.0f; };   // -1..+1
        // presence does little until past half (like the real thing)
        const float presDb  = -3.0f + 10.0f * std::pow (presence / 10.0f, 2.0f);
        const float depthDb = 9.0f * depth / 10.0f;

        switch (v)
        {
            case AmpVoice::Clean:
                bq[0].lowShelf  (110.0f, 10.0f * knob (bass), fs);
                bq[1].peak      (450.0f, 0.7f, 9.0f * knob (mid), fs);
                bq[2].highShelf (2500.0f, 10.0f * knob (treble), fs);
                break;
            case AmpVoice::Thrash:
                bq[0].lowShelf  (120.0f, 9.0f * knob (bass), fs);
                bq[1].peak      (520.0f, 0.8f, 10.0f * knob (mid), fs);
                bq[2].highShelf (1800.0f, 10.0f * knob (treble), fs, 0.8f);
                break;
            case AmpVoice::Modern:
                bq[0].lowShelf  (90.0f, 10.0f * knob (bass), fs);
                bq[1].peak      (700.0f, 0.7f, 11.0f * knob (mid), fs);
                bq[2].highShelf (2400.0f, 10.0f * knob (treble), fs, 0.8f);
                break;
        }
        bq[3].highShelf (4200.0f, presDb, fs);
        bq[4].peak (90.0f, 1.0f, depthDb, fs);
        bq[5].highPass (35.0f, 0.707f, fs);

        drive  = v == AmpVoice::Modern ? 1.7f : (v == AmpVoice::Thrash ? 1.3f : 0.9f);
        sagAmt = v == AmpVoice::Modern ? 0.35f : (v == AmpVoice::Thrash ? 0.12f : 0.05f);
        masterGain = dbToGain (juce::jmap (master, 0.0f, 10.0f, -30.0f, 6.0f));
        sagCoef = std::exp (-1.0f / (0.060f * fs));
    }

    inline float process (float x)
    {
        for (int i = 0; i < 6; ++i) x = bq[i].process (x);
        // power amp with a little sag (looser feel on the Modern voice)
        sagEnv = sagCoef * sagEnv + (1 - sagCoef) * std::abs (x);
        const float d = drive / (1.0f + sagAmt * sagEnv);
        const float y = std::tanh (d * x) / std::tanh (d);
        return y * masterGain;
    }

private:
    float fs = 48000, drive = 1.3f, sagAmt = 0.1f, sagEnv = 0, sagCoef = 0.999f, masterGain = 1;
    float cb = -1, cm = -1, ct = -1, cp = -1, cd = -1, cmas = -1;
    bool dirty = true;
    AmpVoice voice = AmpVoice::Thrash;
    Biquad bq[6];
};

// ---------------------------------------------------------------- cabinet (IIR model)
// Two mic "chains" on the same speaker, blended: dynamic SM57-ish + brighter MD421-ish
enum class CabModel { Modern412 = 0, Vintage412 = 1, Open212 = 2, CustomIR = 3 };

class CabSim
{
public:
    void prepare (float sampleRate) { fs = sampleRate; model = -1; reset(); }
    void reset() { for (auto& b : speaker) b.reset(); for (auto& b : m57) b.reset(); for (auto& b : m421) b.reset(); }

    void set (CabModel m, float micBlend01)
    {
        blend = micBlend01;
        if ((int) m == model) return;
        model = (int) m;
        switch (m)
        {
            case CabModel::Vintage412:   // 2000-era V30: more low-mid honk, darker top
                speaker[0].highPass (70.0f, 0.85f, fs);
                speaker[1].peak (115.0f, 1.0f, 6.0f, fs);
                speaker[2].peak (800.0f, 0.9f, 2.0f, fs);
                speaker[3].peak (1800.0f, 1.2f, 3.0f, fs);
                speaker[4].lowPass (4300.0f, 0.7f, fs);
                speaker[5].lowPass (4800.0f, 0.9f, fs);
                speaker[6].peak (650.0f, 0.9f, 1.5f, fs);
                break;
            case CabModel::Open212:
                speaker[0].highPass (65.0f, 0.7f, fs);
                speaker[1].peak (100.0f, 1.2f, 2.0f, fs);
                speaker[2].peak (500.0f, 1.0f, -2.5f, fs);
                speaker[3].peak (3000.0f, 1.2f, 3.0f, fs);
                speaker[4].lowPass (6500.0f, 0.75f, fs);
                speaker[5].lowPass (7500.0f, 1.0f, fs);
                speaker[6].peak (1000.0f, 0.7f, 0.0f, fs);
                break;
            default:                     // Modern closed-back 4x12 V30
                speaker[0].highPass (70.0f, 0.8f, fs);
                speaker[1].peak (110.0f, 1.0f, 6.0f, fs);
                speaker[2].peak (450.0f, 0.9f, -1.5f, fs);
                speaker[3].peak (2300.0f, 1.4f, 2.5f, fs);
                speaker[4].lowPass (4500.0f, 0.7f, fs);
                speaker[5].lowPass (5200.0f, 0.9f, fs);
                speaker[6].peak (800.0f, 0.8f, 2.5f, fs);    // V30 upper-mid honk
                break;
        }
        // SM57 on the cone edge: rolls off fizz around 6 kHz, proximity bump
        m57[0].peak (180.0f, 0.8f, 1.5f, fs);
        m57[1].lowPass (6000.0f, 0.8f, fs);
        m57[2].peak (3800.0f, 1.3f, -3.5f, fs);
        // MD421: fuller lows, spike ~4.3 kHz
        m421[0].lowShelf (150.0f, 2.5f, fs);
        m421[1].peak (4300.0f, 2.0f, 6.0f, fs);
    }

    inline float process (float x)
    {
        for (auto& b : speaker) x = b.process (x);
        float a = x; for (auto& b : m57) a = b.process (a);
        float c = x; for (auto& b : m421) c = b.process (c);
        return (a * (1.0f - blend) + c * blend) * 1.4f;
    }

private:
    float fs = 48000, blend = 0.15f;
    int model = -1;
    Biquad speaker[7], m57[3], m421[2];
};

// ---------------------------------------------------------------- "Tight": palm-mute low-end control
// Dynamic cut of the 65–294 Hz band, only when that band gets loud (chugs).
class TightComp
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (auto& b : band) b.bandPass (140.0f, 0.65f, fs);
        att = std::exp (-1.0f / (0.004f * fs));
        rel = std::exp (-1.0f / (0.120f * fs));
        reset();
    }
    void reset() { for (auto& b : band) b.reset(); env = 0; }
    void setAmount (float a01) { amount = a01; thr = dbToGain (-12.0f - 18.0f * a01); }

    // stereo pair, linked detection
    inline void process (float& l, float& r)
    {
        const float bl = band[0].process (l), br = band[1].process (r);
        const float lvl = std::max (std::abs (bl), std::abs (br));
        env = lvl > env ? att * env + (1 - att) * lvl : rel * env + (1 - rel) * lvl;
        float g = 1.0f;
        if (env > thr)
        {
            const float overDb = 20.0f * std::log10 (env / thr);
            const float grDb = juce::jmin (12.0f * amount + 3.0f, overDb * 0.75f);   // 4:1, capped
            g = dbToGain (-grDb);
        }
        l += bl * (g - 1.0f);
        r += br * (g - 1.0f);
        lastGr = g;
    }
    float lastGr = 1.0f;

private:
    float fs = 48000, att = 0, rel = 0, env = 0, thr = 0.1f, amount = 0.5f;
    Biquad band[2];
};

// ---------------------------------------------------------------- 6-band "bus" EQ
class BusEQ
{
public:
    static constexpr int numBands = 6;
    static constexpr float freqs[numBands] = { 70.0f, 300.0f, 700.0f, 2300.0f, 3200.0f, 9000.0f };

    void prepare (float sampleRate) { fs = sampleRate; for (auto& g : last) g = 1000.0f; reset(); }
    void reset() { for (auto& c : bq) for (auto& b : c) b.reset(); }
    void set (const float* gainsDb)
    {
        for (int i = 0; i < numBands; ++i)
        {
            if (gainsDb[i] == last[i]) continue;
            last[i] = gainsDb[i];
            for (int ch = 0; ch < 2; ++ch)
            {
                auto& b = bq[ch][i];
                if (i == 0)                 b.lowShelf (freqs[i], gainsDb[i], fs);
                else if (i == numBands - 1) b.highShelf (freqs[i], gainsDb[i], fs);
                else                        b.peak (freqs[i], i == 4 ? 0.9f : 1.2f, gainsDb[i], fs);
            }
        }
    }
    inline float process (int ch, float x) { for (auto& b : bq[ch]) x = b.process (x); return x; }

private:
    float fs = 48000, last[numBands] {};
    Biquad bq[2][numBands];
};

// ---------------------------------------------------------------- fractional delay line
class DelayLine
{
public:
    void prepare (int maxSamples) { buf.assign ((size_t) juce::nextPowerOfTwo (maxSamples + 4), 0.0f); mask = (int) buf.size() - 1; w = 0; }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float read (float delaySamples) const
    {
        const float rp = (float) w - 1.0f - delaySamples;
        const int i0 = (int) std::floor (rp);
        const float fr = rp - (float) i0;
        const float a = buf[(size_t) (i0 & mask)], b = buf[(size_t) ((i0 + 1) & mask)];
        return a + (b - a) * fr;
    }
private:
    std::vector<float> buf; int mask = 0, w = 0;
};

// ---------------------------------------------------------------- doubler (fake double-tracking)
class Doubler
{
public:
    void prepare (float sampleRate) { fs = sampleRate; for (auto& d : dl) d.prepare ((int) (0.05f * fs)); reset(); }
    void reset() { for (auto& d : dl) d.reset(); ph[0] = 0; ph[1] = 0.37f; }
    inline void process (float& l, float& r, float amount)
    {
        dl[0].push (l); dl[1].push (r);
        ph[0] += 0.23f / fs; ph[1] += 0.31f / fs;
        for (auto& p : ph) if (p >= 1.0f) p -= 1.0f;
        const float dL = dl[0].read ((0.011f + 0.0012f * std::sin (2 * kPi * ph[0])) * fs);
        const float dR = dl[1].read ((0.019f + 0.0016f * std::sin (2 * kPi * ph[1])) * fs);
        l = l * (1.0f - 0.35f * amount) + dL * 0.35f * amount;
        r = r * (1.0f - amount) + dR * amount;
    }
private:
    float fs = 48000, ph[2] {};
    DelayLine dl[2];
};

// ---------------------------------------------------------------- stereo delay
class StereoDelay
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (auto& d : dl) d.prepare ((int) (2.1f * fs));
        for (auto& f : damp) f.set (4500.0f, fs);
        for (auto& f : lowcut) f.set (150.0f, fs);
        timeSm.reset (sampleRate, 0.25);
        reset();
    }
    void reset() { for (auto& d : dl) d.reset(); for (auto& f : damp) f.reset(); for (auto& f : lowcut) f.reset(); }
    void setTime (float seconds, bool snap) { if (snap) timeSm.setCurrentAndTargetValue (seconds * fs); else timeSm.setTargetValue (seconds * fs); }

    inline void process (float& l, float& r, float feedback, float mix)
    {
        const float t = timeSm.getNextValue();
        const float yl = dl[0].read (t), yr = dl[1].read (t * 1.012f);   // slight spread
        // ping-pong-ish feedback
        const float inMono = 0.5f * (l + r);
        dl[0].push (lowcut[0].hp (damp[0].lp (inMono + yr * feedback)));
        dl[1].push (lowcut[1].hp (damp[1].lp (yl * feedback)));
        l += yl * mix;
        r += yr * mix;
    }
private:
    float fs = 48000;
    DelayLine dl[2];
    OnePole damp[2], lowcut[2];
    juce::SmoothedValue<float> timeSm;
};

// ---------------------------------------------------------------- lock-free-ish sample FIFO for the tuner
class TunerFifo
{
public:
    static constexpr int size = 4096;
    void push (float x) { const int w = writeIdx.load (std::memory_order_relaxed); data[(size_t) w] = x; writeIdx.store ((w + 1) & (size - 1), std::memory_order_release); }
    void copyLatest (float* dest, int n) const
    {
        const int w = writeIdx.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i) dest[i] = data[(size_t) ((w - n + i) & (size - 1))];
    }
private:
    std::array<float, size> data {};
    std::atomic<int> writeIdx { 0 };
};

// YIN pitch detector (runs on the message thread)
inline float detectPitch (const float* x, int n, float fs, float& clarity)
{
    const int maxTau = juce::jmin (n / 2, (int) (fs / 28.0f));
    const int minTau = (int) (fs / 1500.0f);
    const int W = n - maxTau;
    std::vector<float> d ((size_t) maxTau + 1, 0.0f);
    float energy = 0; for (int i = 0; i < W; ++i) energy += x[i] * x[i];
    clarity = 0;
    if (energy / (float) W < 1.0e-7f) return 0.0f;

    for (int tau = 1; tau <= maxTau; ++tau)
    {
        float s = 0;
        for (int i = 0; i < W; ++i) { const float v = x[i] - x[i + tau]; s += v * v; }
        d[(size_t) tau] = s;
    }
    float run = 0; d[0] = 1;
    for (int tau = 1; tau <= maxTau; ++tau) { run += d[(size_t) tau]; d[(size_t) tau] = d[(size_t) tau] * (float) tau / juce::jmax (run, 1.0e-12f); }

    int best = -1;
    for (int tau = minTau; tau < maxTau; ++tau)
        if (d[(size_t) tau] < 0.15f) { while (tau + 1 < maxTau && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau; best = tau; break; }
    if (best < 0) return 0.0f;

    const float a = d[(size_t) best - 1], b = d[(size_t) best], c = d[(size_t) best + 1];
    const float den = a - 2 * b + c;
    const float shift = std::abs (den) > 1e-9f ? 0.5f * (a - c) / den : 0.0f;
    clarity = 1.0f - b;
    return fs / ((float) best + shift);
}
} // namespace abyss
