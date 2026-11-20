#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Colours.h"

class DriftStationProcessor;

// Segment-Schalter fuer Choice-Parameter (HPF, LPF, RECORD, GATE)
class SegmentSwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    SegmentSwitch (juce::RangedAudioParameter& p, juce::StringArray labels, juce::Colour c);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setAccent (juce::Colour c) { colour = c; repaint(); }

private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    juce::StringArray labels;
    juce::Colour colour;
    int index = 0;
};

// Taster, der nur gedrueckt aktiv ist (ERASE)
class MomentaryButton : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    MomentaryButton (juce::RangedAudioParameter& p, juce::String text, juce::Colour c);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override { const bool on = param.getValue() > 0.5f; if (on != shown) { shown = on; repaint(); } }
    juce::RangedAudioParameter& param;
    juce::String text;
    juce::Colour colour;
    bool shown = false;
};

// Einfacher Klick-Taster (CLEAR ALL)
class ActionButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    ActionButton (juce::String t, juce::Colour c) : text (t), colour (c) {}
    std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { down = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        down = false; repaint();
        if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    juce::String text;
    juce::Colour colour;
    bool down = false;
};

// Algorithmus-Wahl: 4 Typen x 3 Groessen
class AlgoSelector : public juce::Component
{
public:
    AlgoSelector (juce::AudioProcessorValueTreeState& apvts);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverRow = hoverCol = -1; repaint(); }

private:
    juce::Rectangle<float> rowBounds (int row) const;
    juce::Rectangle<float> dotBounds (int row, int col) const;
    bool hit (juce::Point<float> p, int& row, int& col) const;

    juce::RangedAudioParameter& algoParam;
    juce::RangedAudioParameter& sizeParam;
    juce::ParameterAttachment algoAtt, sizeAtt;
    int algo = 0, size = 1, hoverRow = -1, hoverCol = -1;
};

// Pegel des Speicherinhalts: Gruen / Gelb / Rot (Limiter)
class LevelBar : public juce::Component
{
public:
    explicit LevelBar (juce::String t) : title (t) {}
    void set (float level, bool red)
    {
        if (std::abs (level - value) > 0.002f || red != isRed) { value = level; isRed = red; repaint(); }
    }
    void paint (juce::Graphics&) override;

private:
    juce::String title;
    float value = 0.0f;
    bool isRed = false;
};

// Visualisierung: jede Delay-Linie als Ring mit umlaufendem Kopf
class MemoryView : public juce::Component, private juce::Timer
{
public:
    explicit MemoryView (DriftStationProcessor& p);
    void paint (juce::Graphics&) override;
    void refresh() { timerCallback(); }

private:
    void timerCallback() override;
    DriftStationProcessor& processor;
    float phase = 0.0f;
    std::array<float, 8> heads {}, levels {}, lengths {};
    int numLines = 2, algo = 0, grains = 0;
    float loop = 0.0f;
    bool limiting = false, recording = true;
};
