// Offline-Test der Engine ohne JUCE:
//   g++ -O2 -std=c++17 -I Source tests/EngineTest.cpp Source/DriftEngine.cpp -o /tmp/enginetest && /tmp/enginetest [outdir]
// Rendert pro Algorithmus/Groesse 20 s und prueft auf NaN, Pegel und Stabilitaet.
#include "DriftEngine.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>

static void writeWav (const std::string& path, const std::vector<float>& l, const std::vector<float>& r, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    const uint32_t n = (uint32_t) l.size(), dataBytes = n * 4;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t a = (int16_t) std::lround (std::fmax (-1.0, std::fmin (1.0, l[i])) * 32767.0);
        int16_t b = (int16_t) std::lround (std::fmax (-1.0, std::fmin (1.0, r[i])) * 32767.0);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    const int sr = 48000, block = 256;
    const double seconds = 20.0;
    const std::string outDir = argc > 1 ? argv[1] : "";
    int failures = 0;

    for (int a = 0; a < 4; ++a)
        for (int sz = 0; sz < 3; ++sz)
        {
            drift::Engine e;
            drift::Params p;
            p.algo = a; p.size = sz; p.fb = 0.97f; p.blur = 0.3f; p.drift = 0.5f;
            if (a == 2) p.fb = 0.9f;
            e.setParams (p);
            e.prepare (sr);

            const int total = (int) (seconds * sr);
            std::vector<float> outL (total), outR (total);
            std::vector<float> inL (block), inR (block), wl (block), wr (block);
            bool nan = false;
            double energyLate = 0.0, peak = 0.0;

            for (int pos = 0; pos < total; pos += block)
            {
                // Toggle-Tests: Reverse bei 8 s, Half Speed bei 12 s, Erase bei 17-18 s
                drift::Params q = p;
                const double t = pos / (double) sr;
                const char* only = std::getenv ("DS_TOGGLE");
                const std::string tg = only ? only : "rhe";
                q.reverse = tg.find ('r') != std::string::npos && (t > 8.0 && t < 11.0);
                q.halfSpeed = tg.find ('h') != std::string::npos && (t > 12.0 && t < 15.0);
                q.erase = tg.find ('e') != std::string::npos && (t > 17.0 && t < 18.0);
                e.setParams (q);

                for (int i = 0; i < block; ++i)
                {
                    const double ts = (pos + i) / (double) sr;
                    // drei Plucks (A-Moll) in den ersten 3 Sekunden
                    double v = 0.0;
                    const double onsets[3] = { 0.1, 0.9, 1.7 };
                    const double freqs[3] = { 220.0, 261.63, 329.63 };
                    for (int k = 0; k < 3; ++k)
                        if (ts > onsets[k])
                        {
                            const double tt = ts - onsets[k];
                            v += 0.3 * std::sin (2 * M_PI * freqs[k] * tt) * std::exp (-tt * 3.0);
                        }
                    inL[i] = inR[i] = (float) v;
                }
                e.process (inL.data(), inR.data(), wl.data(), wr.data(), block);
                for (int i = 0; i < block && pos + i < total; ++i)
                {
                    if (! std::isfinite (wl[i]) || ! std::isfinite (wr[i])) nan = true;
                    outL[pos + i] = wl[i] + 0.7f * inL[i];
                    outR[pos + i] = wr[i] + 0.7f * inR[i];
                    peak = std::fmax (peak, std::fabs (wl[i]));
                    if ((pos + i) / (double) sr > 3.0)
                        energyLate += wl[i] * wl[i] + wr[i] * wr[i];
                }
            }
            const double rmsLate = std::sqrt (energyLate / (17.0 * sr * 2));
            const bool ok = ! nan && peak < 1.5 && rmsLate > 1.0e-4;
            if (! ok) ++failures;
            std::printf ("%-13s size %d  peak %.3f  rms(3-20s) %.4f  %s\n",
                         drift::Engine::algoName (a), sz, peak, rmsLate, ok ? "OK" : "FAIL");

            if (! outDir.empty())
                writeWav (outDir + "/algo" + std::to_string (a) + "_size" + std::to_string (sz) + ".wav", outL, outR, sr);
        }

    std::printf ("%s\n", failures == 0 ? "ALL OK" : "FAILURES");
    return failures;
}
