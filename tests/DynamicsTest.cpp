// Test fuer Autogain, Transpose und Limiter (ohne JUCE)
// g++ -O2 -std=c++17 -I Source tests/DynamicsTest.cpp -o dyntest && ./dyntest
#include "Dynamics.h"
#include <cstdio>

using namespace drift;
static const double sr = 48000.0, pi = 3.14159265358979;
static bool ok = true;
static void check (bool c, const char* what) { std::printf ("%-55s %s\n", what, c ? "OK" : "FAIL"); ok &= c; }

static double rmsDb (const std::vector<float>& v, size_t from)
{
    double s = 0; for (size_t i = from; i < v.size(); ++i) s += (double) v[i] * v[i];
    return 10.0 * std::log10 (s / (double) (v.size() - from) + 1e-20);
}

// Anteil der Energie im Band +/-4 % um die Soll-Frequenz (Goertzel-Summe ueber das Band)
static double bandShare (const std::vector<float>& v, size_t from, double want)
{
    const size_t N = v.size() - from;
    double inBand = 0, total = 0;
    for (size_t i = from; i < v.size(); ++i) total += (double) v[i] * v[i];
    const double df = sr / (double) N;
    for (double f = std::ceil (want * 0.96 / df) * df; f <= want * 1.04; f += df)
    {
        const double w = 2 * pi * f / sr, cw = 2 * std::cos (w);
        double s1 = 0, s2 = 0;
        for (size_t i = from; i < v.size(); ++i)
        {
            const double s0 = v[i] + cw * s1 - s2; s2 = s1; s1 = s0;
        }
        inBand += 2.0 / (double) N * (s1 * s1 + s2 * s2 - cw * s1 * s2);
    }
    return inBand / total;
}

int main()
{
    // Autogain: -40 dBFS und +6 dBFS (uebersteuert) landen beide nahe -18 dBFS RMS
    for (float inDb : { -40.0f, -20.0f, 0.0f, 6.0f })
    {
        InputAutoGain ag; ag.prepare (sr);
        const float a = dbToGain (inDb) * std::sqrt (2.0f);
        std::vector<float> out;
        float peak = 0;
        for (int i = 0; i < (int) sr * 12; ++i)
        {
            float l = a * (float) std::sin (2 * pi * 220 * i / sr), r = l;
            ag.process (l, r, 0.0f);
            out.push_back (l); peak = std::max (peak, std::abs (l));
        }
        const double lvl = rmsDb (out, (size_t) sr * 10);
        char msg[96]; std::snprintf (msg, sizeof msg, "Autogain in %+5.1f dB -> %6.2f dB RMS, peak %.3f", inDb, lvl, peak);
        check (std::abs (lvl + 18.0) < 1.5 && peak < 0.96f, msg);
    }

    // Transpose: 440 Hz -> -12 = 220, +7 = 659, -24 = 110, +12 = 880
    for (int st : { 0, -12, 7, -24, 12 })
    {
        PitchShifter ps; ps.setSemitones (st); ps.prepare (sr);
        std::vector<float> out;
        for (int i = 0; i < (int) sr * 3; ++i)
        {
            float l = 0.5f * (float) std::sin (2 * pi * 440 * i / sr), r = l;
            ps.process (l, r); out.push_back (l);
        }
        const double want = 440.0 * std::pow (2.0, st / 12.0);
        const double share = bandShare (out, (size_t) sr, want);
        char msg[96]; std::snprintf (msg, sizeof msg, "Transpose %+3d st: %.0f %% Energie bei %.1f Hz", st, share * 100, want);
        check (share > 0.85, msg);
    }

    // Limiter: nie ueber -1 dBFS, ganz offen keine Reduktion bei leisem Signal
    for (float thr : { 0.0f, -6.0f, -18.0f })
    {
        OutputLimiter lim; lim.prepare (sr);  lim.setInitialThreshold (thr);
        float peak = 0, grMax = 0;
        for (int i = 0; i < (int) sr * 4; ++i)
        {
            const float env = (i / 6000) % 2 ? 1.6f : 0.25f;   // laute Stoesse
            float l = env * (float) std::sin (2 * pi * 110 * i / sr), r = l * 0.8f;
            lim.process (l, r, thr);
            peak = std::max (peak, std::max (std::abs (l), std::abs (r)));
            grMax = std::max (grMax, lim.takeGr());
        }
        char msg[96]; std::snprintf (msg, sizeof msg, "Limiter thr %5.1f dB: peak %.2f dBFS, GR max %.1f dB", thr, gainToDb (peak), grMax);
        check (peak <= dbToGain (-1.0f) + 1e-5f && grMax > 0.5f, msg);
    }
    {
        OutputLimiter lim; lim.prepare (sr); lim.setInitialThreshold (0.0f);
        float grMax = 0;
        for (int i = 0; i < (int) sr; ++i)
        {
            float l = 0.5f * (float) std::sin (2 * pi * 110 * i / sr), r = l;
            lim.process (l, r, 0.0f); grMax = std::max (grMax, lim.takeGr());
        }
        check (grMax < 0.01f, "Limiter ganz offen, Signal -6 dBFS: keine Reduktion");
    }

    std::printf (ok ? "ALL OK\n" : "SOME FAILED\n");
    return ok ? 0 : 1;
}
