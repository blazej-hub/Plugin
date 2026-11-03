#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP.h"
#include "Params.h"

class AbyssToneProcessor : public juce::AudioProcessor
{
public:
    AbyssToneProcessor();
    ~AbyssToneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return (int) abyss::factoryPresets().size(); }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- helpers for the editor
    void loadImpulseResponse (const juce::File& file);
    juce::String getIrName() const { return irFile.existsAsFile() ? irFile.getFileNameWithoutExtension() : juce::String(); }
    bool savePresetToFile (const juce::File& f);
    bool loadPresetFromFile (const juce::File& f);

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inLevel { 0 }, outLevel { 0 }, tightGr { 1 };
    std::atomic<bool> tunerActive { false };   // mutes output while tuning
    abyss::TunerFifo tunerFifo;
    std::atomic<float> tunerSampleRate { 12000 };
    std::atomic<int> irVersion { 0 };

private:
    void updateParameters();
    float getDelaySeconds() const;

    double sr = 48000;
    int currentProgram = 0;
    static constexpr int osFactorLog2 = 2;   // 4x oversampling for the distortion stages

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::AudioBuffer<float> pathBuffer;   // ch0 = main amp, ch1 = layer amp

    abyss::NoiseGate gate;
    abyss::TubeScreamer od[2];
    abyss::Preamp pre[2];
    abyss::PowerSection power[2];
    abyss::CabSim cab[2];
    juce::dsp::Convolution irConv;
    abyss::TightComp tight;
    abyss::BusEQ eq;
    abyss::Doubler doubler;
    abyss::StereoDelay delay;
    juce::dsp::Reverb reverb;

    juce::SmoothedValue<float> inGainSm, outGainSm, layerSm, dblSm, dlyMixSm, revMixSm;
    juce::File irFile;
    int tunerDecim = 0; float tunerAcc = 0;

    // cached parameter pointers
    std::atomic<float>* p (const char* id) const { return apvts.getRawParameterValue (id); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AbyssToneProcessor)
};
