#pragma once

#include <vector>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

// Drift Station DSP-Engine
// Ein grosser gemeinsamer Speicher (Pool). Jeder Algorithmus legt seine Delay-Linien
// ab Adresse 0 in diesen Pool. Beim Wechsel des Algorithmus bleibt der Inhalt erhalten
// und wird vom neuen Algorithmus weiterverarbeitet.

namespace drift
{
    enum Algo { twoDelays = 0, fourDelays = 1, giantReverb = 2, granular = 3 };
    constexpr int maxLines = 8;
    constexpr int maxGrains = 64;

    struct Params
    {
        int algo = twoDelays, size = 1;
        float blur = 0.0f, drift = 0.3f, drive = 0.0f, supcom = 0.0f, fb = 0.95f;
        int hpf = 0, lpf = 0, record = 3;
        bool reverse = false, erase = false, halfSpeed = false;
    };

    // TPT State Variable Filter (12 dB)
    struct SVF
    {
        float g = 0.0f, k = 1.41421356f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
        void set (float fc, float sr)
        {
            g = std::tan (3.14159265f * fc / sr);
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }
        void reset() { ic1 = ic2 = 0.0f; }
        // mode 0 = LP, 1 = HP
        inline float process (float v0, int mode)
        {
            const float v3 = v0 - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            return mode == 0 ? v2 : v0 - k * v1 - v2;
        }
    };

    // Lese-/Schreibkopf auf einem Bereich des Pools (wie ein Bandloop)
    struct TapeHead
    {
        int offset = 0, length = 1;
        double pos = 0.0;

        inline float read (const float* pool, double speed) const
        {
            if (speed >= 0.0)
            {
                const int i0 = (int) pos;
                const float f = (float) (pos - (double) i0);
                const int i1 = (i0 + 1 >= length) ? 0 : i0 + 1;
                return pool[offset + i0] * (1.0f - f) + pool[offset + i1] * f;
            }
            int c = (int) std::ceil (pos);
            const float f = (float) ((double) c - pos);
            if (c >= length) c -= length;
            const int cm = (c - 1 < 0) ? length - 1 : c - 1;
            return pool[offset + c] * (1.0f - f) + pool[offset + cm] * f;
        }

        // Kopf bewegen und die ueberfahrenen Stellen mit v beschreiben
        inline void advanceWrite (float* pool, double speed, float v)
        {
            const double np = pos + speed;
            if (speed >= 0.0)
            {
                const long a = (long) std::floor (pos), b = (long) std::floor (np);
                for (long i = a; i < b; ++i)
                    pool[offset + wrap (i)] = v;
            }
            else
            {
                const long a = (long) std::ceil (pos), b = (long) std::ceil (np);
                for (long i = a; i > b; --i)
                    pool[offset + wrap (i)] = v;
            }
            pos = np;
            while (pos >= (double) length) pos -= (double) length;
            while (pos < 0.0) pos += (double) length;
        }

        inline int wrap (long i) const
        {
            long r = i % length;
            return (int) (r < 0 ? r + length : r);
        }
    };

    struct Grain
    {
        bool active = false;
        int buf = 0;          // 0 = L, 1 = R, 2 = Centre
        double pos = 0.0;     // relativ im Bereich
        double inc = 1.0;
        int len = 1, age = 0;
        float panL = 0.7f, panR = 0.7f;
    };

    class Engine
    {
    public:
        void prepare (double sampleRate);
        void setParams (const Params& p) { target = p; }
        void requestClear() { clearRequested.store (true); }

        // in*: Eingang (bereits mit IN-Gain), wet*: Ausgang nur Effekt
        void process (const float* inL, const float* inR, float* wetL, float* wetR, int n);

        // Anzeige
        std::atomic<float> loopLevel { 0.0f };       // Pegel im Speicher (linear, Peak)
        std::atomic<bool>  limiting { false };
        std::atomic<int>   uiNumLines { 2 };
        std::atomic<int>   uiAlgo { 0 };
        std::array<std::atomic<float>, maxLines> uiHead {};     // 0..1
        std::array<std::atomic<float>, maxLines> uiLevel {};
        std::array<std::atomic<float>, maxLines> uiLength {};   // Sekunden
        std::atomic<int>   uiGrains { 0 };

        static const char* algoName (int a);
        static double repeatSeconds (int algo, int size);

    private:
        void configure (int algo, int size);
        void resetFilters();
        void smoothSeam (int poolIndex);
        inline float processLoop (int line, float x);
        inline float softLimit (float x);
        void updateChaos();
        void spawnGrain (int buf);
        static int nearestPrime (int n);

        double sr = 48000.0;
        std::vector<float> pool;
        Params target, cur;

        // aktuelle Konfiguration
        int algo = twoDelays, size = 1, numLines = 2;
        std::array<TapeHead, maxLines> heads;
        std::array<int, maxLines> group {};          // 0 = Gruppe 1 (L), 1 = Gruppe 2 (R), 2 = beide
        std::array<float, maxLines> basePan {};

        // Reverb-Allpaesse (eigener Speicher)
        std::array<std::vector<float>, maxLines> apBuf;
        std::array<int, maxLines> apLen {}, apIdx {};

        // Granular
        std::array<Grain, maxGrains> grains;
        std::array<double, 3> grainDelay {};
        std::array<int, 3> grainCountdown {};
        int grainLen = 4096;
        int numGrainBufs = 2;

        // Filter, Pegel
        std::array<SVF, maxLines> hp, lp, aa;
        std::array<float, maxLines> lineEnv {};
        int hpfMode = 0, lpfMode = 0;

        // Modulation
        double lx = 0.1, ly = 0.0, lz = 20.0;  // Lorenz
        float chaosA = 0.0f, chaosB = 0.0f;
        std::array<double, maxLines> lfoPhase {};
        std::array<float, maxLines> lfoVal {};

        // vorberechnete Laengen (Primzahlen in Samples) [algo][size][line]
        int lenTable[4][3][maxLines] {};
        int apTable[3][maxLines] {};
        int chaosCounter = 0;
        uint32_t rng = 0x12345678u;
        inline float rnd() { rng = rng * 1664525u + 1013904223u; return (float) (rng >> 8) * (1.0f / 16777216.0f); }
        float noiseState = 0.0f;

        // Glaettung
        float fade = 1.0f, fadeTarget = 1.0f, fadeStep = 0.001f;
        float rec1 = 1.0f, rec2 = 1.0f, eraseGain = 1.0f;
        float supGain = 1.0f, compGain = 1.0f, envIn = 0.0f, envComp = 0.0f;
        float fbSm = 0.95f, blurSm = 0.0f, driftSm = 0.3f, driveSm = 0.0f;
        double speedMag = 1.0, dir = 1.0;
        int pendingAlgo = -1, pendingSize = -1;
        std::array<double, maxLines> ghostPos {}, ghostSpeed {};
        std::array<std::vector<float>, maxLines> ghostBuf;
        int xfadeLen = 1, xfadeLeft = 0;
        int seamRampLen = 1, seamRampLeft = 0;
        std::atomic<bool> clearRequested { false };
        bool clearPending = false;
        float peakHold = 0.0f;
        int limitHold = 0;
    };
}
