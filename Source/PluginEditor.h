#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Look.h"
#include "Knob.h"
#include "Widgets.h"

// Die eigentliche Oberflaeche in fester Groesse (904 x 516). Der Editor skaliert sie.
class DriftPanel : public juce::Component, private juce::Timer
{
public:
    static constexpr int baseW = 904, baseH = 516;
    explicit DriftPanel (DriftStationProcessor&);
    ~DriftPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void pollNow() { timerCallback(); memoryView.refresh(); }

private:
    void timerCallback() override;
    void setupToggle (juce::ToggleButton& b, const juce::String& text, juce::Colour c,
                      const juce::String& paramID, std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att,
                      const juce::String& tip);

    DriftStationProcessor& processor;
    DriftLookAndFeel lnf;

    AlgoSelector algoSelector;
    MemoryView memoryView;
    LevelBar inBar { "IN" }, loopBar { "LOOP" };

    Knob kIn, kBlur, kDrift, kTranspose, kSupcom, kFb, kMix, kLimit;

    SegmentSwitch hpfSwitch, lpfSwitch, recSwitch, gateSwitch;
    juce::ToggleButton reverseButton, halfButton, monoButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> reverseAtt, halfAtt, monoAtt;
    MomentaryButton eraseButton;
    ActionButton clearButton { "CLEAR ALL", DriftColours::clip };

    juce::Rectangle<int> knobCard, bottomCard;
    juce::Rectangle<int> hpfLabel, lpfLabel, recLabel, gateLabel;

    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DriftPanel)
};

// Plugin-Fenster: frei skalierbar (Seitenverhaeltnis fest), Groesse wird im Projekt gespeichert
class DriftStationEditor : public juce::AudioProcessorEditor
{
public:
    explicit DriftStationEditor (DriftStationProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void pollNow() { panel.pollNow(); }

private:
    DriftStationProcessor& proc;
    DriftPanel panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DriftStationEditor)
};
