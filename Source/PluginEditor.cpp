#include "PluginEditor.h"
#include "Colours.h"

using namespace DriftColours;

namespace
{
    constexpr int W = 904, H = 516;
    constexpr int margin = 14, gap = 12, headerH = 44, mainH = 300;
}

DriftStationEditor::DriftStationEditor (DriftStationProcessor& p)
    : AudioProcessorEditor (&p), processor (p),
      algoSelector (p.apvts), memoryView (p),
      hpfSwitch (*p.apvts.getParameter (ParamIDs::hpf), { "OFF", "40", "120", "300" }, accent),
      lpfSwitch (*p.apvts.getParameter (ParamIDs::lpf), { "OFF", "1.5k", "4k", "9k" }, accent),
      recSwitch (*p.apvts.getParameter (ParamIDs::record), { "OFF", "GRP 1", "GRP 2", "ALL" }, rec),
      gateSwitch (*p.apvts.getParameter (ParamIDs::gate), { "OFF", "NORMAL", "SENS" }, textDim.brighter (0.4f)),
      eraseButton (*p.apvts.getParameter (ParamIDs::erase), "ERASE", warn)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (algoSelector);
    addAndMakeVisible (memoryView);
    addAndMakeVisible (inBar);
    addAndMakeVisible (loopBar);

    auto& s = p.apvts;
    kIn.setup (*this, s, ParamIDs::inGain, "In", text);
    kBlur.setup (*this, s, ParamIDs::blur, "Blur", accent);
    kDrift.setup (*this, s, ParamIDs::drift, "Drift", accent);
    kDrive.setup (*this, s, ParamIDs::drive, "Drive", warn);
    kSupcom.setup (*this, s, ParamIDs::supcom, "Sup | Com", accent2);
    kSupcom.slider.getProperties().set ("bipolar", true);
    kFb.setup (*this, s, ParamIDs::fb, "Feedback", accent);
    kFb.slider.getProperties().set ("mark", 1.0 / 1.25);
    kMix.setup (*this, s, ParamIDs::mix, "Mix", text);
    kOut.setup (*this, s, ParamIDs::outGain, "Out", text);

    kIn.slider.setTooltip ("Input level. Keep the IN meter out of the red.");
    kBlur.slider.setTooltip ("Delays: cross-feedback between the delay lines, the content slowly blurs. "
                             "Granular: noise modulation of the grain position. No effect in Giant Reverb.");
    kDrift.slider.setTooltip ("Delays: asynchronous LFO, modulated by a slow chaotic signal, moves the delays in the stereo field. "
                              "Granular: the same LFO moves the grain positions. No effect in Giant Reverb.");
    kDrive.slider.setTooltip ("Gentle overdrive inside the loop, like a guitar overdrive.");
    kSupcom.slider.setTooltip ("Centre: off. Left: Suppressor, loud new sounds push out the recorded ones. "
                               "Right: Compressor, evens out the level of the memory content.");
    kFb.slider.setTooltip ("Feedback. Below 1.0 the sounds fade out, at 1.0 they stay, above 1.0 they grow "
                           "(the tape-style limiter keeps them from clipping). White tick = 1.0.");
    kMix.slider.setTooltip ("Balance between input and memory. Centre = equal.");
    kOut.slider.setTooltip ("Output level.");

    for (auto* sw : { &hpfSwitch, &lpfSwitch, &recSwitch, &gateSwitch })
        addAndMakeVisible (*sw);
    hpfSwitch.setTooltip ("High-pass filter in the loop (Hz). Higher = thinner with every repeat.");
    lpfSwitch.setTooltip ("Low-pass filter in the loop. 1.5k = darkest, 9k = subtle.");
    recSwitch.setTooltip ("OFF: play the memory, input is only mixed in (good for soloing). "
                          "GRP 1 / GRP 2: record into one group of delay lines (L / R input). ALL: record into all lines.");
    gateSwitch.setTooltip ("Noise gate on the input, keeps long recordings clean. SENS = 20 dB lower threshold.");

    setupToggle (reverseButton, "REVERSE", accent2, ParamIDs::reverse, reverseAtt, "Play the memory backwards.");
    setupToggle (halfButton, "HALF SPEED", accent, ParamIDs::halfSpeed, halfAtt,
                 "Delays only: the memory plays at half speed, one octave down.");
    setupToggle (monoButton, "MONO IN", textDim.brighter (0.4f), ParamIDs::monoIn, monoAtt,
                 "Sum the left and right input and send it to both groups.");

    addAndMakeVisible (eraseButton);
    eraseButton.setTooltip ("Hold to fade out the memory.");
    addAndMakeVisible (clearButton);
    clearButton.setTooltip ("Clear the whole memory instantly.");
    clearButton.onClick = [this] { processor.clearMemory(); };

    setSize (W, H);
    startTimerHz (20);
}

DriftStationEditor::~DriftStationEditor()
{
    setLookAndFeel (nullptr);
}

void DriftStationEditor::setupToggle (juce::ToggleButton& b, const juce::String& t, juce::Colour c,
                                      const juce::String& paramID,
                                      std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att,
                                      const juce::String& tip)
{
    b.setButtonText (t);
    b.getProperties().set ("style", "labelled");
    b.getProperties().set ("accent", c.toString());
    b.setTooltip (tip);
    addAndMakeVisible (b);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, paramID, b);
}

void DriftStationEditor::timerCallback()
{
    inBar.set (processor.inLevel.load(), processor.inClip.load());
    loopBar.set (processor.engine.loopLevel.load(), processor.engine.limiting.load());

    const int algo = (int) processor.apvts.getRawParameterValue (ParamIDs::algo)->load();
    const bool reverb = algo == drift::giantReverb;
    const bool delays = algo == drift::twoDelays || algo == drift::fourDelays;
    for (auto* k : { &kBlur, &kDrift })
    {
        k->slider.setAlpha (reverb ? 0.35f : 1.0f);
        k->label.setAlpha (reverb ? 0.35f : 1.0f);
    }
    halfButton.setAlpha (delays ? 1.0f : 0.35f);
}

void DriftStationEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f, bgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // Titel
    auto header = juce::Rectangle<int> (margin, margin, getWidth() - 2 * margin, headerH);
    g.setColour (text);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("DRIFT STATION", header.withWidth (220), juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String ("drifting memory processor  |  v") + JucePlugin_VersionString,
                header.withTrimmedLeft (212).withWidth (260), juce::Justification::centredLeft);

    // Karten
    for (auto r : { knobCard, bottomCard })
    {
        g.setColour (card);
        g.fillRoundedRectangle (r.toFloat(), 10.0f);
        g.setColour (cardBorder);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f, 1.0f);
    }

    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("HPF", hpfLabel, juce::Justification::centredLeft);
    g.drawText ("LPF", lpfLabel, juce::Justification::centredLeft);
    g.drawText ("RECORD", recLabel, juce::Justification::centredLeft);
    g.drawText ("GATE", gateLabel, juce::Justification::centredLeft);
}

void DriftStationEditor::resized()
{
    auto area = getLocalBounds().reduced (margin);
    auto header = area.removeFromTop (headerH);
    auto meters = header.removeFromRight (300);
    inBar.setBounds (meters.removeFromLeft (145).withSizeKeepingCentre (145, 18));
    meters.removeFromLeft (10);
    loopBar.setBounds (meters.withSizeKeepingCentre (meters.getWidth(), 18));

    area.removeFromTop (gap);
    auto main = area.removeFromTop (mainH);
    algoSelector.setBounds (main.removeFromLeft (250));
    main.removeFromLeft (gap);
    memoryView.setBounds (main.removeFromLeft (mainH));
    main.removeFromLeft (gap);
    knobCard = main;

    auto kc = knobCard.reduced (10, 12);
    const int rowH = kc.getHeight() / 2;
    Knob::layoutRow (kc.removeFromTop (rowH).reduced (0, 4), { &kIn, &kBlur, &kDrift, &kDrive });
    Knob::layoutRow (kc.reduced (0, 4), { &kSupcom, &kFb, &kMix, &kOut });

    area.removeFromTop (gap);
    bottomCard = area;
    auto bc = bottomCard.reduced (16, 14);

    // Reihe 1: HPF | LPF | RECORD
    auto row1 = bc.removeFromTop (34);
    const int colW = (row1.getWidth() - 2 * 24) / 3;
    auto place = [] (juce::Rectangle<int> r, juce::Rectangle<int>& label, juce::Component& c, int labelW)
    {
        label = r.removeFromLeft (labelW);
        c.setBounds (r);
    };
    place (row1.removeFromLeft (colW), hpfLabel, hpfSwitch, 40);
    row1.removeFromLeft (24);
    place (row1.removeFromLeft (colW), lpfLabel, lpfSwitch, 40);
    row1.removeFromLeft (24);
    place (row1, recLabel, recSwitch, 60);

    bc.removeFromTop (16);
    auto row2 = bc.removeFromTop (36);
    const int bw = 124;
    reverseButton.setBounds (row2.removeFromLeft (bw));
    row2.removeFromLeft (10);
    halfButton.setBounds (row2.removeFromLeft (bw));
    row2.removeFromLeft (10);
    eraseButton.setBounds (row2.removeFromLeft (bw));
    row2.removeFromLeft (10);
    clearButton.setBounds (row2.removeFromLeft (bw));

    auto right = row2;
    monoButton.setBounds (right.removeFromRight (110));
    right.removeFromRight (12);
    auto g = right.removeFromRight (190);
    gateLabel = g.removeFromLeft (42);
    gateSwitch.setBounds (g);

    bc.removeFromTop (10);
}
