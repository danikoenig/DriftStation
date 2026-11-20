#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DriftEngine.h"
#include "Dynamics.h"

namespace ParamIDs
{
    inline const juce::String algo      { "algo" };
    inline const juce::String size      { "size" };
    inline const juce::String inGain    { "in" };
    inline const juce::String blur      { "blur" };
    inline const juce::String drift     { "drift" };
    inline const juce::String transpose { "transpose" };
    inline const juce::String supcom    { "supcom" };
    inline const juce::String fb        { "fb" };
    inline const juce::String mix       { "mix" };
    inline const juce::String limit     { "limit" };
    inline const juce::String hpf       { "hpf" };
    inline const juce::String lpf       { "lpf" };
    inline const juce::String record    { "record" };
    inline const juce::String reverse   { "reverse" };
    inline const juce::String erase     { "erase" };
    inline const juce::String halfSpeed { "halfspeed" };
    inline const juce::String gate      { "gate" };
    inline const juce::String monoIn    { "monoin" };
}

class DriftStationProcessor : public juce::AudioProcessor
{
public:
    DriftStationProcessor();
    ~DriftStationProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Drift Station"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 30.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    drift::Engine engine;

    std::atomic<float> inLevel { 0.0f };
    std::atomic<bool>  inClip { false };
    std::atomic<float> outLevel { 0.0f };
    std::atomic<float> limiterGr { 0.0f };     // dB, fuer das Knopf-Licht

    void clearMemory() { engine.requestClear(); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    juce::AudioBuffer<float> wet, engIn;
    float gateGain = 1.0f, gateEnv = 0.0f;
    float mixSm = 0.5f, trimSm = 0.0f;
    drift::InputAutoGain autoGain;
    drift::PitchShifter shifter;
    drift::OutputLimiter limiter;
    int clipHold = 0;
    double sr = 48000.0;

    std::atomic<float>* pAlgo = nullptr; std::atomic<float>* pSize = nullptr;
    std::atomic<float>* pIn = nullptr; std::atomic<float>* pBlur = nullptr;
    std::atomic<float>* pDrift = nullptr; std::atomic<float>* pTranspose = nullptr;
    std::atomic<float>* pSupcom = nullptr; std::atomic<float>* pFb = nullptr;
    std::atomic<float>* pMix = nullptr; std::atomic<float>* pLimit = nullptr;
    std::atomic<float>* pHpf = nullptr; std::atomic<float>* pLpf = nullptr;
    std::atomic<float>* pRecord = nullptr; std::atomic<float>* pReverse = nullptr;
    std::atomic<float>* pErase = nullptr; std::atomic<float>* pHalf = nullptr;
    std::atomic<float>* pGate = nullptr; std::atomic<float>* pMono = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DriftStationProcessor)
};
