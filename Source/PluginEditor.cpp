#include "PluginEditor.h"
#include "Colours.h"

using namespace DriftColours;

namespace
{
    constexpr int W = DriftPanel::baseW, H = DriftPanel::baseH;
    constexpr int margin = 14, gap = 12, headerH = 44, mainH = 300;
}

DriftPanel::DriftPanel (DriftStationProcessor& p)
    : processor (p),
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
    kIn.setup (*this, s, ParamIDs::inGain, "In  Auto", text);
    kIn.slider.getProperties().set ("bipolar", true);
    kBlur.setup (*this, s, ParamIDs::blur, "Blur", accent);
    kDrift.setup (*this, s, ParamIDs::drift, "Drift", accent);
    kTranspose.setup (*this, s, ParamIDs::transpose, "Transpose", warn);
    kTranspose.slider.getProperties().set ("bipolar", true);
    kSupcom.setup (*this, s, ParamIDs::supcom, "Sup | Com", accent2);
    kSupcom.slider.getProperties().set ("bipolar", true);
    kFb.setup (*this, s, ParamIDs::fb, "Feedback", accent);
    kFb.slider.getProperties().set ("mark", 1.0 / 1.25);
    kMix.setup (*this, s, ParamIDs::mix, "Mix", accent);
    kMix.label.setColour (juce::Label::textColourId, text);
    kLimit.setup (*this, s, ParamIDs::limit, "Limiter", text);
    kLimit.slider.getProperties().set ("grLight", 0.0);

    kIn.slider.setTooltip ("Auto gain: the input is always brought to a good working level, loud signals are pulled down, "
                           "quiet ones lifted. The knob trims the target by +/-12 dB. Centre = 0.");
    kBlur.slider.setTooltip ("Delays: cross-feedback between the delay lines, the content slowly blurs. "
                             "Granular: noise modulation of the grain position. No effect in Giant Reverb.");
    kDrift.slider.setTooltip ("Delays: asynchronous LFO, modulated by a slow chaotic signal, moves the delays in the stereo field. "
                              "Granular: the same LFO moves the grain positions. No effect in Giant Reverb.");
    kTranspose.slider.setTooltip ("Transposes the memory output in semitones. Centre = 0, left down to -24, right up to +12.");
    kSupcom.slider.setTooltip ("Centre: off. Left: Suppressor, loud new sounds push out the recorded ones. "
                               "Right: Compressor, evens out the level of the memory content.");
    kFb.slider.setTooltip ("Feedback. Below 1.0 the sounds fade out, at 1.0 they stay, above 1.0 they grow "
                           "(the tape-style limiter keeps them from clipping). White tick = 1.0.");
    kMix.slider.setTooltip ("Balance between input and memory. Centre = equal.");
    kLimit.slider.setTooltip ("Output limiter with auto gain, ceiling -1 dBFS. Fully open (right) = only catches peaks. "
                             "Turn it back to lower the threshold: the limiter works harder and the output gets denser. "
                             "The ring lights up red with the gain reduction.");

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

DriftPanel::~DriftPanel()
{
    setLookAndFeel (nullptr);
}

void DriftPanel::setupToggle (juce::ToggleButton& b, const juce::String& t, juce::Colour c,
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

void DriftPanel::timerCallback()
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

    // Limiter-Licht: 0..12 dB Gain Reduction -> 0..1
    const float gr = juce::jlimit (0.0f, 1.0f, processor.limiterGr.load() / 12.0f);
    const float old = (float) kLimit.slider.getProperties()["grLight"];
    if (std::abs (gr - old) > 0.005f)
    {
        kLimit.slider.getProperties().set ("grLight", gr);
        kLimit.slider.repaint();
    }
}

void DriftPanel::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f, bgBottom, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // Logo: Umlaufbahn mit driftendem Punkt, Titel, by Dani Koenig
    auto header = juce::Rectangle<int> (margin, margin, getWidth() - 2 * margin, headerH);
    {
        const float cx = (float) header.getX() + 11.0f, cy = (float) header.getCentreY();
        g.setColour (accent.withAlpha (0.35f));
        g.drawEllipse (cx - 10.0f, cy - 10.0f, 20.0f, 20.0f, 1.6f);
        g.setColour (accent);
        g.fillEllipse (cx - 4.0f, cy - 4.0f, 8.0f, 8.0f);
        g.fillEllipse (cx + 5.0f, cy - 10.5f, 5.0f, 5.0f);
    }
    g.setColour (text);
    g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
    g.drawText ("DRIFT STATION", header.getX() + 34, header.getY() + 4, 220, 22, juce::Justification::centredLeft);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String::fromUTF8 ("by Dani Koenig  \xc2\xb7  v") + JucePlugin_VersionString,
                header.getX() + 34, header.getY() + 24, 220, 14, juce::Justification::centredLeft);

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

void DriftPanel::resized()
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
    Knob::layoutRow (kc.removeFromTop (rowH).reduced (0, 4), { &kIn, &kBlur, &kDrift, &kTranspose });
    Knob::layoutRow (kc.reduced (0, 4), { &kSupcom, &kFb, &kLimit, &kMix });

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
    const int bw = 116;
    reverseButton.setBounds (row2.removeFromLeft (bw));
    row2.removeFromLeft (10);
    halfButton.setBounds (row2.removeFromLeft (bw));
    row2.removeFromLeft (10);
    eraseButton.setBounds (row2.removeFromLeft (bw));
    row2.removeFromLeft (10);
    clearButton.setBounds (row2.removeFromLeft (bw));

    auto right = row2;
    monoButton.setBounds (right.removeFromRight (104));
    right.removeFromRight (12);
    auto g = right.removeFromRight (222);
    gateLabel = g.removeFromLeft (40);
    gateSwitch.setBounds (g);

    bc.removeFromTop (10);
}

//==============================================================================
DriftStationEditor::DriftStationEditor (DriftStationProcessor& p)
    : AudioProcessorEditor (&p), proc (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    setResizeLimits ((int) (W * 0.7), (int) (H * 0.7), W * 2, H * 2);
    getConstrainer()->setFixedAspectRatio ((double) W / (double) H);
    const float s = juce::jlimit (0.7f, 2.0f, p.uiScale.load());
    setSize (juce::roundToInt (W * s), juce::roundToInt (H * s));
}

void DriftStationEditor::paint (juce::Graphics& g)
{
    g.fillAll (bgBottom);
}

void DriftStationEditor::resized()
{
    const float s = (float) getWidth() / (float) W;
    panel.setBounds (0, 0, W, H);
    panel.setTransform (juce::AffineTransform::scale (s));
    proc.uiScale.store (s);
}
