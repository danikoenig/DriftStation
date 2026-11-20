// Rendert das GUI headless als PNG (docs/screenshot.png) und laesst vorher Audio durchlaufen
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    DriftStationProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    if (argc > 2)
    {
        proc.apvts.getParameter (ParamIDs::algo)->setValueNotifyingHost (proc.apvts.getParameter (ParamIDs::algo)->convertTo0to1 ((float) std::atoi (argv[2])));
    }
    // Demo-Einstellung fuer das Bild: Transpose -12, Limiter arbeitet
    auto setP = [&] (const juce::String& id, float v) { auto* p = proc.apvts.getParameter (id); p->setValueNotifyingHost (p->convertTo0to1 (v)); };
    setP (ParamIDs::transpose, -12.0f);
    setP (ParamIDs::limit, -16.0f);
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    double t = 0.0;
    for (int b = 0; b < 48000 * 6 / 512; ++b)
    {
        for (int i = 0; i < 512; ++i, t += 1.0 / 48000.0)
        {
            const float v = (float) (0.3 * std::sin (2 * juce::MathConstants<double>::pi * 220.0 * t) * std::exp (-std::fmod (t, 1.3) * 3.0));
            buf.setSample (0, i, v); buf.setSample (1, i, v);
        }
        proc.processBlock (buf, midi);
    }
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setVisible (true);
    for (int i = 0; i < 6; ++i) if (auto* de = dynamic_cast<DriftStationEditor*> (ed.get())) de->pollNow();
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
    juce::File out (argc > 1 ? argv[1] : "screenshot.png");
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
    return 0;
}
