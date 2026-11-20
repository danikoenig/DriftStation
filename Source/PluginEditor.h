#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Look.h"
#include "Knob.h"
#include "Widgets.h"

class DriftStationEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit DriftStationEditor (DriftStationProcessor&);
    ~DriftStationEditor() override;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DriftStationEditor)
};
