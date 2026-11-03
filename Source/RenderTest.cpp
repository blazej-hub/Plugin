// Offline test harness: renders a synthetic DI guitar through every factory preset,
// writes WAVs, prints level / spectrum / CPU metrics, and snapshots the GUI.
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <chrono>
#include <random>

static constexpr double fs = 48000.0;

// Karplus-Strong string: filtered pick excitation, pickup-position comb, loss filter
static void pluck (std::vector<float>& out, int start, float freq, float dur, float amp, float damp, std::mt19937& rng)
{
    const int period = juce::jmax (2, (int) std::lround (fs / freq));
    std::vector<float> ring ((size_t) period);
    std::uniform_real_distribution<float> u (-1.0f, 1.0f);
    // plucked-string displacement: triangle peaked at 1/7 of the string (+ a touch of pick noise),
    // differentiated because a magnetic pickup senses string velocity
    const int pk = juce::jmax (1, period / 7);
    std::vector<float> disp ((size_t) period);
    for (int i = 0; i < period; ++i)
        disp[(size_t) i] = (i < pk ? (float) i / (float) pk : (float) (period - i) / (float) (period - pk)) + 0.02f * u (rng);
    for (int i = 0; i < period; ++i) ring[(size_t) i] = disp[(size_t) ((i + 1) % period)] - disp[(size_t) i];
    float mx = 0; for (auto v : ring) mx = std::max (mx, std::abs (v));
    for (auto& v : ring) v *= amp / (mx + 1e-9f);
    const int pu = juce::jmax (1, period / 5);   // bridge pickup at 1/5 of string
    int idx = 0;
    const int n = (int) (dur * fs);
    std::vector<float> hist ((size_t) pu + 1, 0.0f); int h = 0;
    for (int i = 0; i < n && start + i < (int) out.size(); ++i)
    {
        const float cur = ring[(size_t) idx], nxt = ring[(size_t) ((idx + 1) % period)];
        ring[(size_t) idx] = damp * (0.5f * cur + 0.5f * nxt);
        idx = (idx + 1) % period;
        const float delayed = hist[(size_t) h]; hist[(size_t) h] = cur; h = (h + 1) % (pu + 1);
        const float rel = i > n - 96 ? (float) (n - i) / 96.0f : 1.0f;
        out[(size_t) (start + i)] += (cur - delayed) * rel;
    }
}

// humbucker + cable: resonant low-pass ~3.2 kHz
static void pickupFilter (std::vector<float>& x)
{
    abyss::Biquad b; b.lowPass (2800.0f, 1.3f, (float) fs);
    for (auto& v : x) v = b.process (v);
}

static std::vector<float> makeGuitar()
{
    std::mt19937 rng (7);
    std::vector<float> g ((size_t) (fs * 8.0), 0.0f);
    const float C2 = 65.41f, G2 = 98.0f, C3 = 130.81f, Eb3 = 155.56f;
    const double eighth = 60.0 / 180.0 / 2.0;
    int t = (int) (0.1 * fs);
    // gallop of palm mutes (0-3 s)
    for (int bar = 0; bar < 8; ++bar)
        for (int k = 0; k < 4; ++k)
        {
            const bool accent = (k == 0);
            pluck (g, t, C2, (float) eighth, accent ? 0.45f : 0.35f, 0.985f, rng);   // heavily damped = palm mute
            t += (int) (eighth * fs * (k == 0 ? 1.0 : 0.5));
        }
    // open power chords (3-5.5 s)
    t = (int) (3.2 * fs);
    for (float f : { C2, G2, C3 }) pluck (g, t, f, 1.2f, 0.3f, 0.9995f, rng);
    t = (int) (4.4 * fs);
    for (float f : { Eb3 / 2.0f * 1.0f, Eb3 * 0.75f, Eb3 }) pluck (g, t, f, 1.1f, 0.3f, 0.9995f, rng);
    // lead line (5.6-8 s)
    t = (int) (5.6 * fs);
    for (float f : { 392.0f, 466.16f, 523.25f, 622.25f, 587.33f, 523.25f })
    { pluck (g, t, f, 0.6f, 0.25f, 0.9993f, rng); t += (int) (0.38 * fs); }
    pickupFilter (g);
    float pk = 0; for (auto v : g) pk = std::max (pk, std::abs (v));
    for (auto& v : g) v *= 0.5f / pk;   // DI peaks around -6 dBFS
    // a little pickup hum/noise floor
    std::normal_distribution<float> nd (0.0f, 1.0f);
    for (size_t i = 0; i < g.size(); ++i) g[i] += 0.0006f * nd (rng) + 0.0004f * std::sin (2.0f * 3.14159f * 60.0f * (float) i / (float) fs);
    return g;
}

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), fs, (unsigned) b.getNumChannels(), 24, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

static void bandReport (const juce::AudioBuffer<float>& b, int from, int to)
{
    constexpr int order = 13, N = 1 << order;
    juce::dsp::FFT fft (order);
    juce::dsp::WindowingFunction<float> win (N, juce::dsp::WindowingFunction<float>::hann);
    const float edges[] = { 20, 100, 300, 1000, 3000, 6000, 20000 };
    double band[6] {}; int frames = 0;
    std::vector<float> buf ((size_t) N * 2);
    for (int s = from; s + N < to; s += N / 2, ++frames)
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (int i = 0; i < N; ++i) buf[(size_t) i] = 0.5f * (b.getSample (0, s + i) + b.getSample (b.getNumChannels() - 1, s + i));
        win.multiplyWithWindowingTable (buf.data(), N);
        fft.performFrequencyOnlyForwardTransform (buf.data());
        for (int k = 1; k < N / 2; ++k)
        {
            const float f = (float) k * (float) fs / N;
            for (int j = 0; j < 6; ++j) if (f >= edges[j] && f < edges[j + 1]) band[j] += (double) buf[(size_t) k] * buf[(size_t) k];
        }
    }
    double tot = 0; for (double v : band) tot += v;
    const double oct[6] = { 2.32, 1.58, 1.74, 1.58, 1.0, 1.74 };
    printf ("      dB/oct rel. to 300-1k: <100:%+5.1f 100-300:%+5.1f 1-3k:%+5.1f 3-6k:%+5.1f >6k:%+5.1f\n",
            10*std::log10 (band[0]/oct[0]/(band[2]/oct[2])), 10*std::log10 (band[1]/oct[1]/(band[2]/oct[2])), 10*std::log10 (band[3]/oct[3]/(band[2]/oct[2])),
            10*std::log10 (band[4]/oct[4]/(band[2]/oct[2])), 10*std::log10 (band[5]/oct[5]/(band[2]/oct[2]) + 1e-12));
    printf ("      bands %%:  <100:%5.1f  100-300:%5.1f  300-1k:%5.1f  1-3k:%5.1f  3-6k:%5.1f  >6k:%5.1f\n",
            100 * band[0] / tot, 100 * band[1] / tot, 100 * band[2] / tot, 100 * band[3] / tot, 100 * band[4] / tot, 100 * band[5] / tot);
}

int main()
{
    setvbuf (stdout, nullptr, _IONBF, 0);
    juce::ScopedJuceInitialiser_GUI init;
    const auto outDir = juce::File::getCurrentWorkingDirectory().getChildFile ("renders");
    outDir.createDirectory();

    const auto di = makeGuitar();
    const int n = (int) di.size();
    const int block = 256;

    juce::AudioBuffer<float> diBuf (2, n);
    for (int c = 0; c < 2; ++c) diBuf.copyFrom (c, 0, di.data(), n);
    writeWav (outDir.getChildFile ("00_DI.wav"), diBuf);
    printf ("DI reference:\n"); bandReport (diBuf, (int) (0.2 * fs), (int) (3.0 * fs));

    AbyssToneProcessor proc;
    proc.setPlayConfigDetails (2, 2, fs, block);
    proc.prepareToPlay (fs, block);
    printf ("latency: %d samples\n", proc.getLatencySamples());

    juce::MidiBuffer midi;
    for (int pr = 0; pr < proc.getNumPrograms(); ++pr)
    {
        proc.setCurrentProgram (pr);
        proc.reset();
        juce::AudioBuffer<float> out (2, n);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, 0, di.data(), n);

        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int s = 0; s < n; s += block)
        {
            const int len = juce::jmin (block, n - s);
            juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, s, len);
            proc.processBlock (view, midi);
        }
        const double secs = std::chrono::duration<double> (std::chrono::high_resolution_clock::now() - t0).count();

        bool bad = false;
        for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) if (! std::isfinite (out.getSample (c, i))) bad = true;
        auto rmsDb = [&] (int a, int b2) { return juce::Decibels::gainToDecibels (0.5f * (out.getRMSLevel (0, a, b2 - a) + out.getRMSLevel (1, a, b2 - a))); };
        const float corr = [&] { double lr = 0, ll = 0, rr = 0; for (int i = 0; i < n; ++i) { const double l = out.getSample (0, i), r = out.getSample (1, i); lr += l * r; ll += l * l; rr += r * r; } return (float) (lr / std::sqrt (ll * rr + 1e-12)); }();

        const auto name = proc.getProgramName (pr);
        printf ("[%d] %-32s peak %6.1f dBFS | RMS chugs %6.1f, chords %6.1f, lead %6.1f | L/R corr %.2f | CPU %.1f%% %s\n", pr, name.toRawUTF8(),
                juce::Decibels::gainToDecibels (out.getMagnitude (0, n)), rmsDb ((int) (0.2 * fs), (int) (3.0 * fs)),
                rmsDb ((int) (3.3 * fs), (int) (5.4 * fs)), rmsDb ((int) (5.7 * fs), (int) (7.9 * fs)), corr,
                100.0 * secs / ((double) n / fs), bad ? "NaN!!" : "");
        bandReport (out, (int) (0.2 * fs), (int) (3.0 * fs));
        writeWav (outDir.getChildFile (juce::String (pr + 1).paddedLeft ('0', 2) + "_" + name.replaceCharacters ("| ()", "____").removeCharacters ("/") + ".wav"), out);
    }

    // module debug for lead
    for (const char* off : { "revOn", "dlyOn" })
    {
        proc.setCurrentProgram (3); proc.reset();
        proc.apvts.getParameter (off)->setValueNotifyingHost (0.0f);
        juce::AudioBuffer<float> out (2, n);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, 0, di.data(), n);
        for (int s = 0; s < n; s += block) { juce::AudioBuffer<float> v (out.getArrayOfWritePointers(), 2, s, juce::jmin (block, n - s)); proc.processBlock (v, midi); }
        printf ("lead w/o %s: chug RMS %.1f\n", off, juce::Decibels::gainToDecibels (out.getRMSLevel (0, (int) (0.2 * fs), (int) (2.8 * fs))));
    }
    // silence test: zero input
    {
        proc.setCurrentProgram (0); proc.reset();
        juce::AudioBuffer<float> zb (2, (int) fs); zb.clear();
        for (int s = 0; s < zb.getNumSamples(); s += block) { juce::AudioBuffer<float> v (zb.getArrayOfWritePointers(), 2, s, juce::jmin (block, zb.getNumSamples() - s)); proc.processBlock (v, midi); }
        printf ("zero-input output RMS (last 0.5s) %.1f dBFS, peak %.4f\n", juce::Decibels::gainToDecibels (zb.getRMSLevel (0, (int) (fs * 0.5), (int) (fs * 0.5)) + 1e-9f), zb.getMagnitude (0, (int) (fs*0.5), (int) (fs*0.5)));
        abyss::NoiseGate g; g.prepare (fs); g.setThreshold (-55);
        std::mt19937 rng (3); std::normal_distribution<float> nd (0, 1); double e = 0;
        for (int i = 0; i < (int) fs; ++i) { const float y = g.process (0.0006f * nd (rng)); if (i > fs / 2) e += y * y; }
        { abyss::NoiseGate g2; g2.prepare (fs); g2.setThreshold (-55); int closed = -1;
          for (int i = 0; i < (int) fs; ++i) { const float in = i < (int) (0.3 * fs) ? 0.4f * std::sin (0.01f * (float) i) : 0.0006f * nd (rng);
              const float y = g2.process (in); if (i > (int) (0.3 * fs) && closed < 0 && std::abs (y) < 1e-5f * 0.0006f * 10) closed = i; }
          printf ("gate closes %.0f ms after signal stops\n", (closed - 0.3 * fs) / fs * 1000.0); }
        printf ("standalone gate on -64 dB noise: out RMS %.1f dB\n", 10 * std::log10 (e / (fs / 2) + 1e-20));
    }
    // gate check: only noise floor in, should be ~silent out
    {
        proc.setCurrentProgram (0); proc.reset();
        juce::AudioBuffer<float> nb (2, (int) fs);
        std::mt19937 rng (3); std::normal_distribution<float> nd (0, 1);
        for (int i = 0; i < nb.getNumSamples(); ++i) { const float v = 0.0006f * nd (rng); nb.setSample (0, i, v); nb.setSample (1, i, v); }
        for (int s = 0; s < nb.getNumSamples(); s += block) { juce::AudioBuffer<float> v (nb.getArrayOfWritePointers(), 2, s, juce::jmin (block, nb.getNumSamples() - s)); proc.processBlock (v, midi); }
        printf ("gate test: noise-only output RMS %.1f dBFS (gate on)\n", juce::Decibels::gainToDecibels (nb.getRMSLevel (0, (int) (fs * 0.5), (int) (fs * 0.5)) + 1e-9f));
    }

    // TS "filter" response: drive 0, tone 0, level 0 at low input level
    {
        abyss::TubeScreamer ts; ts.prepare ((float) fs); ts.update (0, 0, 0);
        printf ("TS @drive0/tone0 response (dB):");
        for (float f : { 80.f, 200.f, 500.f, 720.f, 1000.f, 2000.f, 4000.f, 8000.f })
        {
            ts.reset(); double e = 0;
            for (int i = 0; i < (int) fs; ++i) { const float y = ts.process (0.01f * std::sin (2 * 3.14159265f * f * (float) i / (float) fs)); if (i > fs / 2) e += y * y; }
            printf ("  %g:%+.1f", f, 10 * std::log10 (e / (fs / 2) / (0.01 * 0.01 / 2)));
        }
        printf ("\n");
    }

    // GUI snapshot
    {
        proc.setCurrentProgram (1);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        ed->setSize (1180, 700);
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = outDir.getChildFile ("gui.png"); f.deleteFile();
        juce::FileOutputStream os (f); png.writeImageToStream (img, os);
        printf ("GUI snapshot written\n");
    }
    return 0;
}
