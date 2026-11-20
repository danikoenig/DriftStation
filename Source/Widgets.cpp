#include "Widgets.h"
#include "PluginProcessor.h"
#include <cmath>

using namespace DriftColours;

namespace
{
    const char* algoTitles[4] = { "2 DELAYS", "4 DELAYS", "GIANT REVERB", "GRANULAR" };
    const char* algoInfo[4][3] = {
        { "2 drifting delays  ~2.5 s", "2 drifting delays  ~9.5 s", "2 drifting delays  ~22 s" },
        { "4 drifting delays  ~2.5 s", "4 drifting delays  ~8.5 s", "4 drifting delays  ~11.5 s" },
        { "huge hall", "super-huge hall", "insanely-super-huge hall" },
        { "small delay + grains", "medium delay + grains", "large + centre grain delay" } };

}

//==============================================================================
SegmentSwitch::SegmentSwitch (juce::RangedAudioParameter& p, juce::StringArray l, juce::Colour c)
    : param (p),
      attachment (p, [this] (float v) { index = juce::roundToInt (v); repaint(); }),
      labels (l), colour (c)
{
    attachment.sendInitialUpdate();
}

void SegmentSwitch::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (inset);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (cardBorder.brighter (0.1f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    const int n = labels.size();
    const float w = r.getWidth() / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto seg = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight()).reduced (2.5f);
        const bool on = i == index;
        if (on)
        {
            g.setColour (i == 0 ? textDim.withAlpha (0.35f) : colour);
            g.fillRoundedRectangle (seg, 4.0f);
        }
        g.setColour (on ? (i == 0 ? text : bgBottom) : textDim);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawFittedText (labels[i], seg.toNearestInt(), juce::Justification::centred, 1, 0.6f);
    }
}

void SegmentSwitch::mouseDown (const juce::MouseEvent& e)
{
    const int n = labels.size();
    const int i = juce::jlimit (0, n - 1, (int) ((float) e.x / (float) getWidth() * (float) n));
    attachment.setValueAsCompleteGesture ((float) i);
}

//==============================================================================
MomentaryButton::MomentaryButton (juce::RangedAudioParameter& p, juce::String t, juce::Colour c)
    : param (p), text (t), colour (c)
{
    startTimerHz (30);
}

void MomentaryButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (shown ? colour : inset);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (shown ? colour : (isMouseOver() ? colour.withAlpha (0.8f) : colour.withAlpha (0.4f)));
    g.drawRoundedRectangle (r, 7.0f, 1.3f);
    g.setColour (shown ? bgBottom : DriftColours::text);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (text, r, juce::Justification::centred);
}

void MomentaryButton::mouseDown (const juce::MouseEvent&)
{
    param.beginChangeGesture();
    param.setValueNotifyingHost (1.0f);
    shown = true;
    repaint();
}

void MomentaryButton::mouseUp (const juce::MouseEvent&)
{
    param.setValueNotifyingHost (0.0f);
    param.endChangeGesture();
    shown = false;
    repaint();
}

//==============================================================================
void ActionButton::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (down ? colour.withAlpha (0.6f) : inset);
    g.fillRoundedRectangle (r, 7.0f);
    g.setColour (isMouseOver() ? colour : colour.withAlpha (0.4f));
    g.drawRoundedRectangle (r, 7.0f, 1.3f);
    g.setColour (DriftColours::text);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (text, r, juce::Justification::centred);
}

//==============================================================================
AlgoSelector::AlgoSelector (juce::AudioProcessorValueTreeState& apvts)
    : algoParam (*apvts.getParameter (ParamIDs::algo)),
      sizeParam (*apvts.getParameter (ParamIDs::size)),
      algoAtt (algoParam, [this] (float v) { algo = juce::roundToInt (v); repaint(); }),
      sizeAtt (sizeParam, [this] (float v) { size = juce::roundToInt (v); repaint(); })
{
    algoAtt.sendInitialUpdate();
    sizeAtt.sendInitialUpdate();
}

juce::Rectangle<float> AlgoSelector::rowBounds (int row) const
{
    auto r = getLocalBounds().toFloat().withTrimmedTop (24.0f);
    const float h = r.getHeight() / 4.0f;
    return juce::Rectangle<float> (r.getX(), r.getY() + h * (float) row, r.getWidth(), h).reduced (0.0f, 3.0f);
}

juce::Rectangle<float> AlgoSelector::dotBounds (int row, int col) const
{
    auto r = rowBounds (row);
    const float cx = r.getRight() - 78.0f + (float) col * 26.0f;
    const float d = 9.0f + 4.0f * (float) col;
    return juce::Rectangle<float> (d, d).withCentre ({ cx, r.getCentreY() - 6.0f });
}

bool AlgoSelector::hit (juce::Point<float> p, int& row, int& col) const
{
    for (int r = 0; r < 4; ++r)
        if (rowBounds (r).contains (p))
        {
            row = r;
            col = -1;
            for (int c = 0; c < 3; ++c)
                if (dotBounds (r, c).expanded (8.0f).contains (p)) col = c;
            return true;
        }
    return false;
}

void AlgoSelector::paint (juce::Graphics& g)
{
    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
    g.drawText ("ALGORITHM", getLocalBounds().removeFromTop (18), juce::Justification::centredLeft);
    g.drawText ("S   M   L", getLocalBounds().removeFromTop (18).withTrimmedRight (14), juce::Justification::centredRight);

    for (int row = 0; row < 4; ++row)
    {
        auto r = rowBounds (row);
        const auto col = DriftColours::algo (row);
        const bool sel = row == algo;
        g.setColour (sel ? col.withAlpha (0.12f) : (row == hoverRow ? card.brighter (0.06f) : card));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (sel ? col : cardBorder);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, sel ? 1.4f : 1.0f);

        // Farbstreifen
        g.setColour (col.withAlpha (sel ? 1.0f : 0.5f));
        g.fillRoundedRectangle (r.withWidth (4.0f).reduced (0.0f, 10.0f).translated (8.0f, 0.0f), 2.0f);

        auto txt = r.withTrimmedLeft (20.0f);
        g.setColour (sel ? text : text.withAlpha (0.75f));
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (algoTitles[row], txt.withTrimmedBottom (r.getHeight() * 0.42f), juce::Justification::bottomLeft);
        g.setColour (sel ? col : textDim);
        g.setFont (juce::FontOptions (10.5f));
        const int sz = sel ? size : (row == hoverRow && hoverCol >= 0 ? hoverCol : size);
        g.drawText (algoInfo[row][sz], txt.withTrimmedTop (r.getHeight() * 0.6f).withTrimmedRight (6.0f),
                    juce::Justification::topLeft);

        for (int c = 0; c < 3; ++c)
        {
            auto d = dotBounds (row, c);
            const bool on = sel && c == size;
            const bool hov = row == hoverRow && c == hoverCol;
            g.setColour (on ? col : inset);
            g.fillEllipse (d);
            g.setColour (on ? col : (hov ? col : col.withAlpha (sel ? 0.6f : 0.3f)));
            g.drawEllipse (d, 1.3f);
            if (on)
            {
                g.setColour (col.withAlpha (0.25f));
                g.drawEllipse (d.expanded (3.0f), 2.0f);
            }
        }
    }
}

void AlgoSelector::mouseMove (const juce::MouseEvent& e)
{
    int r = -1, c = -1;
    if (! hit (e.position, r, c)) r = c = -1;
    if (r != hoverRow || c != hoverCol) { hoverRow = r; hoverCol = c; repaint(); }
}

void AlgoSelector::mouseDown (const juce::MouseEvent& e)
{
    int r = -1, c = -1;
    if (! hit (e.position, r, c)) return;
    if (r != algo) algoAtt.setValueAsCompleteGesture ((float) r);
    if (c >= 0 && c != size) sizeAtt.setValueAsCompleteGesture ((float) c);
}

//==============================================================================
void LevelBar::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    g.drawText (title, r.removeFromLeft (38.0f), juce::Justification::centredLeft);
    auto bar = r.withSizeKeepingCentre (r.getWidth(), 8.0f);
    g.setColour (inset);
    g.fillRoundedRectangle (bar, 4.0f);

    const float db = juce::Decibels::gainToDecibels (value, -60.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    const juce::Colour c = isRed ? clip : (db > -9.0f ? warn : ok);
    if (frac > 0.0f)
    {
        g.setColour (c);
        g.fillRoundedRectangle (bar.withWidth (juce::jmax (8.0f, bar.getWidth() * frac)), 4.0f);
    }
}

//==============================================================================
MemoryView::MemoryView (DriftStationProcessor& p) : processor (p)
{
    startTimerHz (30);
}

void MemoryView::timerCallback()
{
    auto& e = processor.engine;
    numLines = e.uiNumLines.load();
    algo = e.uiAlgo.load();
    grains = e.uiGrains.load();
    for (int i = 0; i < 8; ++i)
    {
        heads[(size_t) i] = e.uiHead[(size_t) i].load();
        levels[(size_t) i] = levels[(size_t) i] * 0.7f + 0.3f * e.uiLevel[(size_t) i].load();
        lengths[(size_t) i] = e.uiLength[(size_t) i].load();
    }
    loop = e.loopLevel.load();
    limiting = e.limiting.load();
    recording = (int) processor.apvts.getRawParameterValue (ParamIDs::record)->load() != 0;
    phase += 0.01f;
    repaint();
}

void MemoryView::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (card);
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (cardBorder);
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);

    const auto col = DriftColours::algo (algo);
    const auto c = b.getCentre();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 16.0f;
    const int n = juce::jmax (1, numLines);

    // sanftes Leuchten je nach Pegel
    const float glow = juce::jlimit (0.0f, 1.0f, std::sqrt (loop) * 1.4f);
    g.setGradientFill (juce::ColourGradient (col.withAlpha (0.10f + 0.18f * glow), c.x, c.y,
                                             col.withAlpha (0.0f), c.x + R, c.y, true));
    g.fillEllipse (juce::Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c));

    for (int i = 0; i < n; ++i)
    {
        const float rr = R * (0.95f - 0.55f * (float) i / (float) juce::jmax (1, n - 1 + (n == 1 ? 1 : 0)));
        const float lvl = juce::jlimit (0.0f, 1.0f, std::sqrt (levels[(size_t) i]) * 2.0f);
        g.setColour (knobTrack);
        g.drawEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (c), 2.0f);

        const float ang = heads[(size_t) i] * juce::MathConstants<float>::twoPi;
        // Spur hinter dem Kopf
        juce::Path tail;
        tail.addCentredArc (c.x, c.y, rr, rr, 0.0f, ang - 1.4f, ang, true);
        g.setColour (col.withAlpha (0.2f + 0.6f * lvl));
        g.strokePath (tail, juce::PathStrokeType (2.0f + 2.5f * lvl, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const juce::Point<float> hp (c.x + std::sin (ang) * rr, c.y - std::cos (ang) * rr);
        g.setColour (col.withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (hp));
        g.setColour (text);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (hp));
    }

    // Mitte: Algorithmus und Speicher-LED
    auto mid = juce::Rectangle<float> (R * 0.7f, R * 0.7f).withCentre (c);
    g.setColour (bgBottom.withAlpha (0.85f));
    g.fillEllipse (mid);
    const juce::Colour led = limiting ? clip : (loop > 0.35f ? warn : (loop > 0.03f ? ok : textDim.withAlpha (0.4f)));
    g.setColour (led);
    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ c.x, mid.getY() + 16.0f }));
    g.setColour (text);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText (algoTitles[juce::jlimit (0, 3, algo)], mid.withTrimmedTop (mid.getHeight() * 0.38f).withHeight (16.0f),
                juce::Justification::centred);
    g.setColour (textDim);
    g.setFont (juce::FontOptions (10.0f));
    juce::String sub;
    if (algo == 3) sub = juce::String (grains) + " grains";
    else
    {
        float total = 0.0f;
        for (int i = 0; i < n; ++i) total += lengths[(size_t) i];
        sub = juce::String (total, 1) + " s memory";
    }
    g.drawText (sub, mid.withTrimmedTop (mid.getHeight() * 0.38f + 17.0f).withHeight (14.0f), juce::Justification::centred);
    g.setColour (recording ? rec : textDim.withAlpha (0.5f));
    g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
    g.drawText (recording ? "REC" : "PLAY", mid.withTrimmedTop (mid.getHeight() * 0.72f).withHeight (12.0f), juce::Justification::centred);
}
