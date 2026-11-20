#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    using APF = juce::AudioParameterFloat;
    using APC = juce::AudioParameterChoice;
    using APB = juce::AudioParameterBool;

    juce::String pct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
    juce::String db (float v, int)  { return v <= -59.9f ? "-inf dB" : juce::String (v, 1) + " dB"; }
}

juce::AudioProcessorValueTreeState::ParameterLayout DriftStationProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    auto attr = [] (auto fn) { return juce::AudioParameterFloatAttributes().withStringFromValueFunction (fn); };

    l.add (std::make_unique<APC> (juce::ParameterID { ParamIDs::algo, 1 }, "Algorithm",
                                  juce::StringArray { "2 Delays", "4 Delays", "Giant Reverb", "Granular" }, 0));
    l.add (std::make_unique<APC> (juce::ParameterID { ParamIDs::size, 1 }, "Size",
                                  juce::StringArray { "Small", "Medium", "Large" }, 1));
    l.add (std::make_unique<APF> (juce::ParameterID { ParamIDs::inGain, 1 }, "In",
                                  juce::NormalisableRange<float> (-60.0f, 12.0f, 0.1f, 2.0f), 0.0f, attr (db)));
    l.add (std::make_unique<APF> (juce::ParameterID { ParamIDs::blur, 1 }, "Blur",
                                  juce::NormalisableRange<float> (0.0f, 1.0f), 0.2f, attr (pct)));
    l.add (std::make_unique<APF> (juce::ParameterID { ParamIDs::drift, 1 }, "Drift",
                                  juce::NormalisableRange<float> (0.0f, 1.0f), 0.4f, attr (pct)));
    l.add (std::make_unique<APF> (juce::ParameterID { ParamIDs::drive, 1 }, "Drive",
                                  juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f, attr (pct)));
    l.add (std::make_unique<APF> (juce::ParameterID { ParamIDs::supcom, 1 }, "Sup|Com",
                                  juce::NormalisableRange<float> (-1.0f, 1.0f), 0.0f,
                                  attr ([] (float v, int)
                                  {
                                      if (std::abs (v) < 0.005f) return juce::String ("OFF");
                                      return (v < 0 ? juce::String ("SUP ") : juce::String ("COM "))
                                             + juce::String (juce::roundToInt (std::abs (v) * 100.0f)) + " %";
                                  })));
    l.add (std::make_unique<APF> (juce::ParameterID { ParamIDs::fb, 1 }, "Feedback",
                                  juce::NormalisableRange<float> (0.0f, 1.25f, 0.001f), 0.95f,
                                  attr ([] (float v, int) { return juce::String (v, 2); })));
    l.add (std::make_unique<APF> (juce::ParameterID { ParamIDs::mix, 1 }, "Mix",
                                  juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f, attr (pct)));
    l.add (std::make_unique<APF> (juce::ParameterID { ParamIDs::outGain, 1 }, "Out",
                                  juce::NormalisableRange<float> (-60.0f, 12.0f, 0.1f, 2.0f), 0.0f, attr (db)));
    l.add (std::make_unique<APC> (juce::ParameterID { ParamIDs::hpf, 1 }, "HPF",
                                  juce::StringArray { "Off", "40 Hz", "120 Hz", "300 Hz" }, 0));
    l.add (std::make_unique<APC> (juce::ParameterID { ParamIDs::lpf, 1 }, "LPF",
                                  juce::StringArray { "Off", "1.5 kHz", "4 kHz", "9 kHz" }, 0));
    l.add (std::make_unique<APC> (juce::ParameterID { ParamIDs::record, 1 }, "Record",
                                  juce::StringArray { "Off", "Group 1", "Group 2", "All" }, 3));
    l.add (std::make_unique<APB> (juce::ParameterID { ParamIDs::reverse, 1 }, "Reverse", false));
    l.add (std::make_unique<APB> (juce::ParameterID { ParamIDs::erase, 1 }, "Erase", false));
    l.add (std::make_unique<APB> (juce::ParameterID { ParamIDs::halfSpeed, 1 }, "Half Speed", false));
    l.add (std::make_unique<APC> (juce::ParameterID { ParamIDs::gate, 1 }, "Noise Gate",
                                  juce::StringArray { "Off", "Normal", "Sensitive" }, 1));
    l.add (std::make_unique<APB> (juce::ParameterID { ParamIDs::monoIn, 1 }, "Mono In", false));
    return l;
}

DriftStationProcessor::DriftStationProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "DriftStation", createLayout())
{
    pAlgo = apvts.getRawParameterValue (ParamIDs::algo);
    pSize = apvts.getRawParameterValue (ParamIDs::size);
    pIn = apvts.getRawParameterValue (ParamIDs::inGain);
    pBlur = apvts.getRawParameterValue (ParamIDs::blur);
    pDrift = apvts.getRawParameterValue (ParamIDs::drift);
    pDrive = apvts.getRawParameterValue (ParamIDs::drive);
    pSupcom = apvts.getRawParameterValue (ParamIDs::supcom);
    pFb = apvts.getRawParameterValue (ParamIDs::fb);
    pMix = apvts.getRawParameterValue (ParamIDs::mix);
    pOut = apvts.getRawParameterValue (ParamIDs::outGain);
    pHpf = apvts.getRawParameterValue (ParamIDs::hpf);
    pLpf = apvts.getRawParameterValue (ParamIDs::lpf);
    pRecord = apvts.getRawParameterValue (ParamIDs::record);
    pReverse = apvts.getRawParameterValue (ParamIDs::reverse);
    pErase = apvts.getRawParameterValue (ParamIDs::erase);
    pHalf = apvts.getRawParameterValue (ParamIDs::halfSpeed);
    pGate = apvts.getRawParameterValue (ParamIDs::gate);
    pMono = apvts.getRawParameterValue (ParamIDs::monoIn);
}

bool DriftStationProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void DriftStationProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    drift::Params p;
    p.algo = (int) pAlgo->load();
    p.size = (int) pSize->load();
    p.fb = pFb->load();
    p.reverse = pReverse->load() > 0.5f;
    engine.setParams (p);
    engine.prepare (sampleRate);
    wet.setSize (2, juce::jmax (1, samplesPerBlock));
    engIn.setSize (2, juce::jmax (1, samplesPerBlock));
    inGainSm = juce::Decibels::decibelsToGain (pIn->load(), -60.0f);
    outGainSm = juce::Decibels::decibelsToGain (pOut->load(), -60.0f);
    mixSm = pMix->load();
}

void DriftStationProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    if (wet.getNumSamples() < n) wet.setSize (2, n, false, false, true);

    if (numIn == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);

    drift::Params p;
    p.algo = (int) pAlgo->load();
    p.size = (int) pSize->load();
    p.blur = pBlur->load();
    p.drift = pDrift->load();
    p.drive = pDrive->load();
    p.supcom = pSupcom->load();
    p.fb = pFb->load();
    p.hpf = (int) pHpf->load();
    p.lpf = (int) pLpf->load();
    p.record = (int) pRecord->load();
    p.reverse = pReverse->load() > 0.5f;
    p.erase = pErase->load() > 0.5f;
    p.halfSpeed = pHalf->load() > 0.5f;
    engine.setParams (p);

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    auto* wl = wet.getWritePointer (0);
    auto* wr = wet.getWritePointer (1);

    const int gateMode = (int) pGate->load();
    const float gateThr = gateMode == 2 ? juce::Decibels::decibelsToGain (-86.0f) : juce::Decibels::decibelsToGain (-66.0f);
    const bool mono = pMono->load() > 0.5f;
    const float inT = juce::Decibels::decibelsToGain (pIn->load(), -60.0f);
    const float outT = juce::Decibels::decibelsToGain (pOut->load(), -60.0f);
    const float mixT = pMix->load();
    const float kSm = 1.0f - std::exp (-1.0f / (0.02f * (float) sr));
    const float kGateA = 1.0f - std::exp (-1.0f / (0.001f * (float) sr));
    const float kGateR = 1.0f - std::exp (-1.0f / (0.08f * (float) sr));
    const float kEnvR = 1.0f - std::exp (-1.0f / (0.05f * (float) sr));

    // Eingang: Gain, Mono-Summe, Noise Gate. Trocken-Signal bleibt im buffer.
    float inPeak = 0.0f;
    if (engIn.getNumSamples() < n) engIn.setSize (2, n, false, false, true);
    auto* eL = engIn.getWritePointer (0);
    auto* eR = engIn.getWritePointer (1);
    for (int s = 0; s < n; ++s)
    {
        inGainSm += (inT - inGainSm) * kSm;
        float l = L[s] * inGainSm, r = R[s] * inGainSm;
        if (mono) { const float m = 0.5f * (l + r); l = r = m; }
        L[s] = l; R[s] = r;
        const float a = std::max (std::abs (l), std::abs (r));
        inPeak = std::max (inPeak, a);

        float g = 1.0f;
        if (gateMode > 0)
        {
            gateEnv += (a - gateEnv) * (a > gateEnv ? 0.5f : kEnvR);
            const float t = gateEnv > gateThr ? 1.0f : 0.0f;
            gateGain += (t - gateGain) * (t > gateGain ? kGateA : kGateR);
            g = gateGain;
        }
        eL[s] = l * g;
        eR[s] = r * g;
    }

    engine.process (eL, eR, wl, wr, n);

    float outPeak = 0.0f;
    for (int s = 0; s < n; ++s)
    {
        mixSm += (mixT - mixSm) * kSm;
        outGainSm += (outT - outGainSm) * kSm;
        const float dryG = std::cos (mixSm * juce::MathConstants<float>::halfPi);
        const float wetG = std::sin (mixSm * juce::MathConstants<float>::halfPi);
        L[s] = (L[s] * dryG + wl[s] * wetG) * outGainSm;
        R[s] = (R[s] * dryG + wr[s] * wetG) * outGainSm;
        outPeak = std::max (outPeak, std::max (std::abs (L[s]), std::abs (R[s])));
    }

    if (inPeak >= 0.99f) clipHold = (int) (0.2 * sr);
    else if (clipHold > 0) clipHold -= n;
    inClip.store (clipHold > 0);
    inLevel.store (std::max (inPeak, inLevel.load() * 0.9f));
    outLevel.store (std::max (outPeak, outLevel.load() * 0.9f));
}

void DriftStationProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DriftStationProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    // Erase nie dauerhaft gedrueckt laden
    if (auto* e = apvts.getParameter (ParamIDs::erase)) e->setValueNotifyingHost (0.0f);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DriftStationProcessor();
}

juce::AudioProcessorEditor* DriftStationProcessor::createEditor()
{
    return new DriftStationEditor (*this);
}
