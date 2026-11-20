#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

// Drift Station: Eingangs-Autogain, Transpose (Pitch-Shifter) und Ausgangs-Limiter.
// Ohne JUCE, damit tests/EngineTest.cpp es direkt pruefen kann.

namespace drift
{
    inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
    inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

    //==========================================================================
    // Eingangs-Autogain: haelt den Eingang auf ca. -18 dBFS RMS.
    // Laute Signale werden schnell zurueckgenommen, leise langsam angehoben.
    // In Pausen (unter -60 dBFS) bleibt die Verstaerkung stehen, damit kein Rauschen hochgezogen wird.
    // Danach faengt ein weicher Sicherheits-Saettiger einzelne Spitzen ab.
    struct InputAutoGain
    {
        float targetDb = -18.0f, minDb = -24.0f, maxDb = 30.0f;
        float ms = 0.0f, gainDb = 0.0f;
        float kMs = 0.0f, kAtt = 0.0f, kRel = 0.0f;
        float silenceMs = dbToGain (-60.0f) * dbToGain (-60.0f);

        void prepare (double sr)
        {
            const auto k = [sr] (double t) { return (float) (1.0 - std::exp (-1.0 / (t * sr))); };
            kMs = k (0.3); kAtt = k (0.12); kRel = k (2.5);
            ms = 0.0f; gainDb = 0.0f;
        }

        // l, r: Eingang (wird ersetzt), trimDb: IN-Regler
        inline void process (float& l, float& r, float trimDb)
        {
            const float p = 0.5f * (l * l + r * r);
            ms += (p - ms) * kMs;
            if (ms > silenceMs)
            {
                const float want = std::clamp (targetDb - 10.0f * std::log10 (ms), minDb, maxDb);
                gainDb += (want - gainDb) * (want < gainDb ? kAtt : kRel);
            }
            const float g = dbToGain (gainDb + trimDb);
            l = safety (l * g);
            r = safety (r * g);
        }

        // linear bis 0.5, darüber weich gegen 0.95 (ca. -0.5 dBFS)
        static inline float safety (float x)
        {
            const float a = std::abs (x);
            if (a <= 0.5f) return x;
            const float y = 0.5f + 0.45f * std::tanh ((a - 0.5f) / 0.45f);
            return x < 0.0f ? -y : y;
        }
    };

    //==========================================================================
    // Transpose: Pitch-Shifter mit zwei ueberlappenden Lese-Koepfen (sin²-Ueberblendung).
    // Fenster ca. 200 ms: weich und flaechig, passend fuer Ambient. Rechts um 1/4 Fenster versetzt,
    // das macht es breiter und verteilt das Schweben der Ueberblendung auf beide Seiten.
    struct PitchShifter
    {
        std::vector<float> bufL, bufR;
        int mask = 0, w = 0, writePos = 0;
        double phase = 0.0;
        float ratio = 1.0f, wetAmt = 0.0f, kAmt = 0.0f;
        int semis = 0;

        void prepare (double sr)
        {
            w = (int) (0.2 * sr);
            int size = 1;
            while (size < w + 8) size <<= 1;
            bufL.assign ((size_t) size, 0.0f);
            bufR.assign ((size_t) size, 0.0f);
            mask = size - 1;
            writePos = 0; phase = 0.0; wetAmt = semis != 0 ? 1.0f : 0.0f;
            kAmt = (float) (1.0 - std::exp (-1.0 / (0.03 * sr)));
        }

        void setSemitones (int st)
        {
            semis = st;
            ratio = std::pow (2.0f, (float) st / 12.0f);
        }

        inline float tap (const std::vector<float>& b, double delay) const
        {
            const double rp = (double) writePos - delay;
            const int i = (int) std::floor (rp);
            const float f = (float) (rp - (double) i);
            const float y0 = b[(size_t) ((i - 1) & mask)], y1 = b[(size_t) (i & mask)];
            const float y2 = b[(size_t) ((i + 1) & mask)], y3 = b[(size_t) ((i + 2) & mask)];
            // kubisch (Hermite)
            const float c1 = 0.5f * (y2 - y0);
            const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
            const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        }

        inline void process (float& l, float& r)
        {
            bufL[(size_t) writePos] = l;
            bufR[(size_t) writePos] = r;

            const float target = semis != 0 ? 1.0f : 0.0f;
            wetAmt += (target - wetAmt) * kAmt;

            if (wetAmt > 1.0e-4f)
            {
                const double pi = 3.14159265358979;
                const auto frac = [] (double x) { return x - std::floor (x); };
                const double pL1 = phase, pL2 = frac (phase + 0.5);
                const double pR1 = frac (phase + 0.25), pR2 = frac (phase + 0.75);
                const auto win = [pi] (double p) { const float s = (float) std::sin (pi * p); return s * s; };
                const auto dly = [this] (double p) { return 3.0 + p * (double) w; };
                const float sl = tap (bufL, dly (pL1)) * win (pL1) + tap (bufL, dly (pL2)) * win (pL2);
                const float sr = tap (bufR, dly (pR1)) * win (pR1) + tap (bufR, dly (pR2)) * win (pR2);
                l += (sl - l) * wetAmt;
                r += (sr - r) * wetAmt;

                phase += (1.0 - (double) ratio) / (double) w;
                while (phase >= 1.0) phase -= 1.0;
                while (phase < 0.0) phase += 1.0;
            }
            writePos = (writePos + 1) & mask;
        }
    };

    //==========================================================================
    // Ausgangs-Limiter mit Lookahead (Stereo verlinkt).
    // Threshold 0 bis -24 dB, Autogain: das Signal wird um -Threshold angehoben,
    // die Decke liegt fest bei -1 dBFS. Ganz offen (0 dB) greift er nur ueber -1 dBFS.
    struct OutputLimiter
    {
        float ceiling = dbToGain (-1.0f);
        int la = 64;                       // Lookahead in Samples
        std::vector<float> dl, dr, req, hold;
        std::vector<int> dqIdx; std::vector<float> dqVal;
        int pos = 0, dqHead = 0, dqTail = 0;
        long n = 0;
        double boxSum = 0.0;
        std::vector<float> box;
        float rel = 1.0f, kRel = 0.0f, preSm = 1.0f, kPre = 0.0f;
        float grDb = 0.0f;                 // Max. Gain Reduction seit letzter Abfrage

        int latency() const { return la - 1; }

        void prepare (double sr)
        {
            la = std::max (8, (int) (0.0015 * sr));
            dl.assign ((size_t) la, 0.0f); dr.assign ((size_t) la, 0.0f);
            box.assign ((size_t) la, 1.0f);
            dqIdx.assign ((size_t) la + 1, 0); dqVal.assign ((size_t) la + 1, 1.0f);
            pos = 0; dqHead = dqTail = 0; n = 0;
            boxSum = (double) la; rel = 1.0f;
            kRel = (float) (1.0 - std::exp (-1.0 / (0.15 * sr)));
            kPre = (float) (1.0 - std::exp (-1.0 / (0.03 * sr)));
            grDb = 0.0f;
        }

        void setInitialThreshold (float thrDb) { preSm = dbToGain (-thrDb); }

        inline void process (float& l, float& r, float thrDb)
        {
            preSm += (dbToGain (-thrDb) - preSm) * kPre;
            const float xl = l * preSm, xr = r * preSm;
            const float pk = std::max (std::abs (xl), std::abs (xr));
            const float g = pk > ceiling ? ceiling / pk : 1.0f;

            // gleitendes Minimum ueber la Samples (monotone Warteschlange)
            const int cap = la + 1;
            while (dqHead != dqTail)
            {
                const int last = (dqTail - 1 + cap) % cap;
                if (dqVal[(size_t) last] >= g) dqTail = last; else break;
            }
            dqIdx[(size_t) dqTail] = (int) (n % 1000000000L); dqVal[(size_t) dqTail] = g;
            dqTail = (dqTail + 1) % cap;
            while (true)
            {
                const long idx = dqIdx[(size_t) dqHead];
                const long age = ((n % 1000000000L) - idx + 1000000000L) % 1000000000L;
                if (age >= la) dqHead = (dqHead + 1) % cap; else break;
            }
            const float hmin = dqVal[(size_t) dqHead];

            // Release
            rel = hmin < rel ? hmin : rel + (hmin - rel) * kRel;

            // Rechteck-Glaettung ueber la Samples
            boxSum += (double) rel - (double) box[(size_t) pos];
            box[(size_t) pos] = rel;
            const float gain = (float) (boxSum / (double) la);

            // Verzoegerung um la-1 Samples
            const int rd = (pos + 1) % la;
            dl[(size_t) pos] = xl; dr[(size_t) pos] = xr;
            float ol = dl[(size_t) rd] * gain, orr = dr[(size_t) rd] * gain;
            pos = rd;
            ++n;

            ol = std::clamp (ol, -ceiling, ceiling);
            orr = std::clamp (orr, -ceiling, ceiling);
            l = ol; r = orr;
            grDb = std::max (grDb, -gainToDb (gain));
        }

        float takeGr() { const float v = grDb; grDb = 0.0f; return v; }
    };
}
