#include "DriftEngine.h"
#include <algorithm>

namespace drift
{
    namespace
    {
        constexpr double poolSeconds = 48.0;
        constexpr float pi = 3.14159265358979f;

        // Delay-Laengen in Sekunden (werden auf Primzahlen in Samples gerundet)
        const double twoLen[3][2]  = { { 1.171, 1.327 }, { 4.441, 5.059 }, { 10.369, 11.633 } };
        const double fourLen[3][4] = { { 0.557, 0.613, 0.647, 0.701 },
                                       { 1.877, 2.039, 2.207, 2.383 },
                                       { 2.551, 2.777, 2.953, 3.217 } };
        const double revBase[8]    = { 0.211, 0.263, 0.317, 0.379, 0.431, 0.503, 0.571, 0.647 };
        const double revScale[3]   = { 1.0, 2.3, 5.1 };
        const double apBaseMs[8]   = { 13.0, 17.0, 23.0, 29.0, 31.0, 37.0, 41.0, 43.0 };
        const double apScale[3]    = { 1.0, 1.6, 2.4 };
        constexpr float apG = 0.62f;

        const double grDelay[3][3] = { { 0.37, 0.53, 0.0 }, { 1.31, 1.73, 0.0 }, { 3.07, 3.71, 2.29 } };
        const double grLenSec[3]   = { 0.070, 0.190, 0.420 };

        const float lfoBase[8]     = { 0.043f, 0.061f, 0.079f, 0.097f, 0.113f, 0.131f, 0.149f, 0.167f };

        const float hpfHz[4] = { 0.0f, 40.0f, 120.0f, 300.0f };
        const float lpfHz[4] = { 0.0f, 1500.0f, 4000.0f, 9000.0f };   // 1 = staerkste Wirkung

        inline float onePole (double seconds, double sr)
        {
            return (float) (1.0 - std::exp (-1.0 / (seconds * sr)));
        }

        inline void hadamard8 (float* x)
        {
            for (int h = 1; h < 8; h <<= 1)
                for (int i = 0; i < 8; i += h * 2)
                    for (int j = i; j < i + h; ++j)
                    {
                        const float a = x[j], b = x[j + h];
                        x[j] = a + b;
                        x[j + h] = a - b;
                    }
            const float s = 0.35355339f;
            for (int i = 0; i < 8; ++i) x[i] *= s;
        }

        inline void rotate (float& a, float& b, float c, float s)
        {
            const float x = a, y = b;
            a = c * x + s * y;
            b = -s * x + c * y;
        }

        inline void panGains (float pan, float& l, float& r)
        {
            pan = std::clamp (pan, -1.0f, 1.0f);
            const float ang = (pan + 1.0f) * pi * 0.25f;
            l = std::cos (ang);
            r = std::sin (ang);
        }
    }

    const char* Engine::algoName (int a)
    {
        switch (a)
        {
            case twoDelays:   return "2 DELAYS";
            case fourDelays:  return "4 DELAYS";
            case giantReverb: return "GIANT REVERB";
            default:          return "GRANULAR";
        }
    }

    double Engine::repeatSeconds (int a, int s)
    {
        double t = 0.0;
        switch (a)
        {
            case twoDelays:   for (double v : twoLen[s]) t += v; break;
            case fourDelays:  for (double v : fourLen[s]) t += v; break;
            case giantReverb: for (double v : revBase) t += v * revScale[s]; break;
            default:          t = grDelay[s][1]; break;
        }
        return t;
    }

    int Engine::nearestPrime (int n)
    {
        auto isPrime = [] (int v)
        {
            if (v < 2) return false;
            if (v % 2 == 0) return v == 2;
            for (int d = 3; (long) d * d <= v; d += 2)
                if (v % d == 0) return false;
            return true;
        };
        for (int k = 0;; ++k)
        {
            if (isPrime (n + k)) return n + k;
            if (n - k > 2 && isPrime (n - k)) return n - k;
        }
    }

    void Engine::prepare (double sampleRate)
    {
        sr = sampleRate;
        pool.assign ((size_t) (poolSeconds * sr) + 16, 0.0f);

        for (int s = 0; s < 3; ++s)
        {
            for (int i = 0; i < 2; ++i) lenTable[twoDelays][s][i] = nearestPrime ((int) (twoLen[s][i] * sr));
            for (int i = 0; i < 4; ++i) lenTable[fourDelays][s][i] = nearestPrime ((int) (fourLen[s][i] * sr));
            for (int i = 0; i < 8; ++i)
            {
                lenTable[giantReverb][s][i] = nearestPrime ((int) (revBase[i] * revScale[s] * sr));
                apTable[s][i] = nearestPrime ((int) (apBaseMs[i] * apScale[s] * 0.001 * sr));
            }
            for (int i = 0; i < 3; ++i)
            {
                const double d = grDelay[s][i] > 0.0 ? grDelay[s][i] : 0.5;
                lenTable[granular][s][i] = nearestPrime ((int) ((d * 1.5 + grLenSec[s] * 1.5 + 0.05) * sr));
            }
        }

        for (auto& gb : ghostBuf) gb.assign ((size_t) (0.012 * sr) + 8, 0.0f);
        const int apMax = (int) (43.0 * 2.4 * 0.001 * sr) + 64;
        for (auto& b : apBuf) b.assign ((size_t) apMax, 0.0f);

        cur = target;
        algo = -1;
        configure (target.algo, target.size);
        fade = fadeTarget = 1.0f;
        fadeStep = (float) (1.0 / (0.025 * sr));
        dir = target.reverse ? -1.0 : 1.0;
        speedMag = 1.0;
        fbSm = target.fb;
        for (auto& g : grains) g.active = false;
    }

    void Engine::smoothSeam (int b)
    {
        // kurze Delle um eine Nahtstelle im Speicher, damit sie nicht knackt
        const int m = (int) (0.002 * sr);
        const int total = (int) pool.size();
        for (int k = -m; k <= m; ++k)
        {
            const int i = b + k;
            if (i < 0 || i >= total) continue;
            const float w = 0.5f - 0.5f * std::cos (pi * (float) std::abs (k) / (float) m);
            pool[(size_t) i] *= w;
        }
    }

    void Engine::configure (int a, int s)
    {
        const bool hadConfig = algo >= 0;
        // Nahtstellen des alten Algorithmus: Bereichsgrenzen und Schreibkoepfe
        std::array<int, maxLines * 3> oldSeams {};
        const int oldCount = hadConfig ? numLines : 0;
        for (int i = 0; i < oldCount; ++i)
        {
            const auto& h = heads[(size_t) i];
            oldSeams[(size_t) (i * 3)] = h.offset;
            oldSeams[(size_t) (i * 3 + 1)] = h.offset + h.length;
            oldSeams[(size_t) (i * 3 + 2)] = h.offset + (int) h.pos;
        }
        a = std::clamp (a, 0, 3);
        s = std::clamp (s, 0, 2);
        algo = a;
        size = s;

        int offset = 0;
        auto place = [&] (int i, int len)
        {
            auto& h = heads[(size_t) i];
            const double rel = h.length > 1 ? h.pos / (double) h.length : 0.0;
            h.offset = offset;
            h.length = len;
            h.pos = std::floor (rel * (double) len);
            offset += len + 2;
        };

        switch (a)
        {
            case twoDelays:
                numLines = 2;
                for (int i = 0; i < 2; ++i) place (i, lenTable[a][s][i]);
                group = { 0, 1 };
                basePan = { -0.7f, 0.7f };
                break;
            case fourDelays:
                numLines = 4;
                for (int i = 0; i < 4; ++i) place (i, lenTable[a][s][i]);
                group = { 0, 0, 1, 1 };
                basePan = { -0.85f, 0.35f, -0.35f, 0.85f };
                break;
            case giantReverb:
                numLines = 8;
                for (int i = 0; i < 8; ++i)
                {
                    place (i, lenTable[a][s][i]);
                    apLen[(size_t) i] = apTable[s][i];
                    apIdx[(size_t) i] = 0;
                }
                group = { 0, 0, 0, 0, 1, 1, 1, 1 };
                basePan = { -0.9f, 0.6f, -0.3f, 0.75f, 0.9f, -0.6f, 0.3f, -0.75f };
                break;
            default:
                numGrainBufs = (s == 2) ? 3 : 2;
                numLines = numGrainBufs;
                for (int i = 0; i < numGrainBufs; ++i)
                {
                    place (i, lenTable[a][s][i]);
                    grainDelay[(size_t) i] = grDelay[s][i] * sr;
                    grainCountdown[(size_t) i] = 1 + i * 97;
                }
                group = { 0, 1, 2 };
                basePan = { -0.6f, 0.6f, 0.0f };
                grainLen = (int) (grLenSec[s] * sr);
                for (auto& g : grains) g.active = false;
                break;
        }

        if (hadConfig)
        {
            for (int i = 0; i < oldCount * 3; ++i) smoothSeam (oldSeams[(size_t) i]);
            for (int i = 0; i < numLines; ++i)
            {
                const auto& h = heads[(size_t) i];
                smoothSeam (h.offset);
                smoothSeam (h.offset + h.length - 1);
                smoothSeam (h.offset + (int) h.pos);
            }
        }
        xfadeLeft = 0;
        // neu geschriebenes Material an der Naht des Kopfes einblenden
        seamRampLen = seamRampLeft = hadConfig ? std::max (1, (int) (0.002 * sr)) : 0;

        uiNumLines.store (numLines);
        uiAlgo.store (algo);
        for (int i = 0; i < maxLines; ++i)
            uiLength[(size_t) i].store (i < numLines ? (float) (heads[(size_t) i].length / sr) : 0.0f);
        resetFilters();
    }

    void Engine::resetFilters()
    {
        for (auto& f : hp) f.reset();
        for (auto& f : lp) f.reset();
        for (auto& f : aa) f.reset();
    }

    inline float Engine::softLimit (float x)
    {
        // Band-artige Saettigung statt hartem Clipping
        constexpr float t = 0.6f;
        const float a = std::abs (x);
        if (a <= t) return x;
        if (a > 0.92f) limitHold = (int) (0.15 * sr);
        const float y = t + (1.0f - t) * std::tanh ((a - t) / (1.0f - t));
        return x < 0.0f ? -y : y;
    }

    inline float Engine::processLoop (int line, float x)
    {
        if (hpfMode > 0) x = hp[(size_t) line].process (x, 1);
        if (lpfMode > 0) x = lp[(size_t) line].process (x, 0);

        if (driveSm > 0.001f)
        {
            // sanfter Overdrive: Kleinsignal-Verstaerkung 1, laute Anteile saettigen
            const float pre = 1.0f + 11.0f * driveSm * driveSm;
            const float bias = 0.2f * driveSm;
            const float y = (std::tanh (pre * x + bias) - std::tanh (bias)) / pre;
            x += (y - x) * std::min (1.0f, driveSm * 4.0f);
        }

        x *= compGain;
        x = softLimit (x);
        peakHold = std::max (peakHold, std::abs (x));
        return x;
    }

    void Engine::updateChaos()
    {
        // Lorenz-Attraktor, sehr langsam
        constexpr int stepSamples = 16;
        const double dt = 0.15 * stepSamples / sr;
        const double dx = 10.0 * (ly - lx);
        const double dy = lx * (28.0 - lz) - ly;
        const double dz = lx * ly - (8.0 / 3.0) * lz;
        lx += dx * dt; ly += dy * dt; lz += dz * dt;
        chaosA = (float) std::clamp (lx / 20.0, -1.0, 1.0);
        chaosB = (float) std::clamp ((lz - 25.0) / 15.0, -1.0, 1.0);

        const float speed = 0.6f + 0.8f * driftSm;
        for (int i = 0; i < maxLines; ++i)
        {
            const float c = (i & 1) ? chaosA : chaosB;
            const double f = lfoBase[i] * speed * (1.0f + 0.7f * c);
            lfoPhase[(size_t) i] += f * stepSamples / sr;
            if (lfoPhase[(size_t) i] >= 1.0) lfoPhase[(size_t) i] -= 1.0;
            lfoVal[(size_t) i] = std::sin (2.0f * pi * (float) lfoPhase[(size_t) i]);
        }
    }

    void Engine::spawnGrain (int b)
    {
        Grain* g = nullptr;
        for (auto& gr : grains)
            if (! gr.active) { g = &gr; break; }
        if (g == nullptr) return;

        const auto& h = heads[(size_t) b];
        const int len = std::max (64, (int) ((float) grainLen * (0.8f + 0.4f * rnd())));
        const double D = grainDelay[(size_t) b];
        const float noise = rnd() * 2.0f - 1.0f;
        const double mod = blurSm * 0.45 * D * noise + driftSm * 0.4 * D * lfoVal[(size_t) b];
        const double d = std::clamp (D + mod, (double) len + 64.0, (double) h.length - (double) len - 64.0);
        const bool rev = dir < 0.0;

        double start = h.pos - d + (rev ? (double) len : 0.0);
        while (start < 0.0) start += h.length;
        while (start >= h.length) start -= h.length;

        float pan = basePan[(size_t) b];
        pan += (b == 2 ? 0.3f : 0.4f) * (rnd() * 2.0f - 1.0f);
        g->active = true;
        g->buf = b;
        g->pos = start;
        g->inc = rev ? -1.0 : 1.0;
        g->len = len;
        g->age = 0;
        panGains (pan, g->panL, g->panR);
    }

    void Engine::process (const float* inL, const float* inR, float* wetL, float* wetR, int n)
    {
        float* P0 = pool.data();
        // Uebergaenge anstossen (Ausblenden, umschalten, Einblenden)
        if (pendingAlgo < 0 && (target.algo != algo || target.size != size))
        {
            pendingAlgo = target.algo;
            pendingSize = target.size;
            fadeTarget = 0.0f;
        }
        if ((target.reverse != (dir < 0.0)) && pendingAlgo < 0)
        {
            // Richtungswechsel: neuer Kopf laeuft rueckwaerts, alter Kopf liest kurz
            // weiter und wird weich ausgeblendet (kein Sprung im Lese-/Schreibsignal)
            const double oldSpeed = dir * (algo == giantReverb || algo == granular ? 1.0 : speedMag);
            const int seamLen = (int) (0.02 * sr);
            const int xLen = (int) (0.012 * sr);
            const float g = algo == giantReverb ? 0.0f : std::clamp (fbSm * eraseGain * supGain, 0.0f, 1.0f);
            for (int i = 0; i < numLines; ++i)
            {
                auto& h = heads[(size_t) i];
                ghostPos[(size_t) i] = 0.0;
                ghostSpeed[(size_t) i] = std::abs (oldSpeed);
                if (algo == granular) continue;
                const long base = (long) std::floor (h.pos);
                const long sgn = oldSpeed >= 0.0 ? 1 : -1;
                // alten Inhalt in Laufrichtung sichern (davon liest der ausblendende Kopf)
                auto& gb = ghostBuf[(size_t) i];
                for (int k = 0; k < (int) gb.size() && k <= xLen + 2; ++k)
                    gb[(size_t) k] = P0[h.offset + h.wrap (base + sgn * k)];
                // Naht vorbereiten: alter Inhalt auf der anderen Seite des Kopfes wird sanft
                // auf den Feedback-Pegel gebracht (Reverb: kurze Delle), damit die Stelle
                // beim naechsten Durchlauf nicht knackt
                for (int k = 1; k <= seamLen; ++k)
                {
                    const float w = g + (1.0f - g) * (float) k / (float) seamLen;
                    P0[h.offset + h.wrap (base + sgn * k)] *= w;
                }
            }
            dir = target.reverse ? -1.0 : 1.0;
            xfadeLen = xfadeLeft = (algo == granular) ? 0 : (int) (0.012 * sr);
        }
        if (clearRequested.exchange (false))
        {
            clearPending = true;
            fadeTarget = 0.0f;
        }

        cur = target;
        hpfMode = cur.hpf;
        lpfMode = cur.lpf;
        if (hpfMode > 0) for (auto& f : hp) f.set (hpfHz[hpfMode], (float) sr);
        if (lpfMode > 0) for (auto& f : lp) f.set (lpfHz[lpfMode], (float) sr);

        const float kSlow   = onePole (0.08, sr);
        const float kRec    = onePole (0.02, sr);
        const float kErase  = cur.erase ? onePole (0.35, sr) : onePole (0.03, sr);
        const float kSpeed  = onePole (0.35, sr);
        const float kAttIn  = onePole (0.002, sr), kRelIn = onePole (0.15, sr);
        const float kAttC   = onePole (0.02, sr),  kRelC  = onePole (0.4, sr);
        const float kGain   = onePole (0.01, sr);
        const float kEnvL   = onePole (0.1, sr);

        const float rec1T = (cur.record == 1 || cur.record == 3) ? 1.0f : 0.0f;
        const float rec2T = (cur.record == 2 || cur.record == 3) ? 1.0f : 0.0f;
        const float supAmt = std::max (0.0f, -cur.supcom);
        const float comAmt = std::max (0.0f, cur.supcom);
        const double speedT = (cur.halfSpeed && (algo == twoDelays || algo == fourDelays)) ? 0.5 : 1.0;

        float wetNorm = 1.0f;
        if (algo == fourDelays) wetNorm = 0.8f;
        else if (algo == giantReverb) wetNorm = 0.45f;

        float* P = pool.data();

        for (int s = 0; s < n; ++s)
        {
            // Ueberblendung
            if (std::abs (fade - fadeTarget) > 0.0f)
            {
                fade += fadeTarget > fade ? fadeStep : -fadeStep;
                fade = std::clamp (fade, 0.0f, 1.0f);
                if (std::abs (fade - fadeTarget) < fadeStep) fade = fadeTarget;
            }
            if (fade <= 0.0f && fadeTarget <= 0.0f)
            {
                if (clearPending)
                {
                    std::fill (pool.begin(), pool.end(), 0.0f);
                    for (auto& b : apBuf) std::fill (b.begin(), b.end(), 0.0f);
                    for (auto& g : grains) g.active = false;
                    resetFilters();
                    clearPending = false;
                }
                if (pendingAlgo >= 0)
                {
                    configure (pendingAlgo, pendingSize);
                    pendingAlgo = pendingSize = -1;
                    if (algo == fourDelays) wetNorm = 0.8f;
                    else if (algo == giantReverb) wetNorm = 0.45f;
                    else wetNorm = 1.0f;
                }
                fadeTarget = 1.0f;
            }

            if ((chaosCounter++ & 15) == 0) updateChaos();

            // Parameter glaetten
            fbSm    += (cur.fb - fbSm) * kSlow;
            blurSm  += (cur.blur - blurSm) * kSlow;
            driftSm += (cur.drift - driftSm) * kSlow;
            driveSm += (cur.drive - driveSm) * kSlow;
            rec1    += (rec1T - rec1) * kRec;
            rec2    += (rec2T - rec2) * kRec;
            eraseGain += ((cur.erase ? 0.0f : 1.0f) - eraseGain) * kErase;
            speedMag += (speedT - speedMag) * kSpeed;

            const float xl = inL[s], xr = inR[s];
            const float inMono = 0.5f * (std::abs (xl) + std::abs (xr));

            // Suppressor: lauter Eingang unterdrueckt den Speicherinhalt
            envIn += (inMono - envIn) * (inMono > envIn ? kAttIn : kRelIn);
            {
                const float db = 20.0f * std::log10 (envIn + 1.0e-9f);
                const float f = std::clamp ((db + 48.0f) / 36.0f, 0.0f, 1.0f);
                const float t = 1.0f - supAmt * f;
                supGain += (t - supGain) * kGain;
            }

            float seamRamp = 1.0f;
            if (seamRampLeft > 0)
            {
                seamRamp = 1.0f - (float) seamRampLeft / (float) seamRampLen;
                --seamRampLeft;
            }

            const float g1 = xl * rec1, g2 = xr * rec2;
            const float fbG = fbSm * eraseGain * supGain;
            float outL = 0.0f, outR = 0.0f, loopSum = 0.0f;

            if (algo != granular)
            {
                const double spd = dir * (algo == giantReverb ? 1.0 : speedMag);
                // Bei halber Geschwindigkeit wird seltener geschrieben: vorher tiefpassen (Aliasing)
                const float aaMix = algo == giantReverb ? 0.0f : (float) std::clamp ((1.0 - speedMag) * 20.0, 0.0, 1.0);
                if ((s & 31) == 0)
                    for (int i = 0; i < numLines; ++i) aa[(size_t) i].set ((float) (0.42 * sr * speedMag), (float) sr);
                float r[maxLines], m[maxLines];
                for (int i = 0; i < numLines; ++i)
                    r[i] = heads[(size_t) i].read (P, spd);

                float seamIn = 1.0f;
                if (xfadeLeft > 0)
                {
                    const float x = 0.5f - 0.5f * std::cos (pi * (1.0f - (float) xfadeLeft / (float) xfadeLen));
                    for (int i = 0; i < numLines; ++i)
                    {
                        const auto& gb = ghostBuf[(size_t) i];
                        const double gk = ghostPos[(size_t) i];
                        const int k0 = std::min ((int) gk, (int) gb.size() - 2);
                        const float fk = (float) (gk - (double) k0);
                        const float g = gb[(size_t) k0] * (1.0f - fk) + gb[(size_t) k0 + 1] * fk;
                        r[i] = g * (1.0f - x) + r[i] * x;
                        if (algo == giantReverb) seamIn = x;
                        ghostPos[(size_t) i] = gk + ghostSpeed[(size_t) i];
                    }
                    --xfadeLeft;
                }
                for (int i = 0; i < numLines; ++i)
                    loopSum += std::abs (r[i]);

                if (algo == giantReverb)
                {
                    // Allpass-Diffusion in jeder Linie: Echos verschwimmen langsam zu Rauschen
                    for (int i = 0; i < 8; ++i)
                    {
                        auto& buf = apBuf[(size_t) i];
                        int& idx = apIdx[(size_t) i];
                        const float d = buf[(size_t) idx];
                        const float y = -apG * r[i] + d;
                        buf[(size_t) idx] = r[i] + apG * y;
                        if (++idx >= apLen[(size_t) i]) idx = 0;
                        r[i] = y;
                        m[i] = y;
                    }
                    hadamard8 (m);
                }
                else
                {
                    for (int i = 0; i < numLines; ++i) m[i] = r[i];
                    // Blur: Kreuz-Feedback als Rotation (energieerhaltend)
                    const float th = blurSm * pi * 0.25f;
                    const float c = std::cos (th), sn = std::sin (th);
                    if (algo == twoDelays)
                        rotate (m[0], m[1], c, sn);
                    else
                    {
                        rotate (m[0], m[2], c, sn); rotate (m[1], m[3], c, sn);
                        rotate (m[0], m[1], c, sn); rotate (m[2], m[3], c, sn);
                    }
                }

                for (int i = 0; i < numLines; ++i)
                {
                    float pan = basePan[(size_t) i];
                    if (algo != giantReverb)
                        pan += driftSm * 0.9f * lfoVal[(size_t) i] * (pan < 0 ? 1.0f : -1.0f);
                    float pl, pr;
                    panGains (pan, pl, pr);
                    outL += r[i] * pl;
                    outR += r[i] * pr;
                    lineEnv[(size_t) i] += (std::abs (r[i]) - lineEnv[(size_t) i]) * kEnvL;
                }

                // Kompressor: Steuersignal = Eingang + Speicherinhalt
                {
                    const float ctl = inMono + loopSum / (float) numLines;
                    envComp += (ctl - envComp) * (ctl > envComp ? kAttC : kRelC);
                    float gdb = 0.0f;
                    if (comAmt > 0.0f)
                    {
                        const float edb = 20.0f * std::log10 (envComp + 1.0e-9f);
                        gdb = comAmt * 0.85f * (-14.0f - edb);
                        if (edb < -54.0f) gdb = std::min (gdb, 0.0f);
                        gdb = std::clamp (gdb, -24.0f, 12.0f);
                    }
                    const float tg = std::pow (10.0f, gdb / 20.0f);
                    compGain += (tg - compGain) * kGain;
                }

                for (int i = 0; i < numLines; ++i)
                {
                    const float in = group[(size_t) i] == 0 ? g1 : g2;
                    float w = processLoop (i, in + fbG * m[i]);
                    { const float f = aa[(size_t) i].process (w, 0); w += (f - w) * aaMix; }
                    w *= seamIn * seamRamp;
                    heads[(size_t) i].advanceWrite (P, spd, w);
                }
            }
            else
            {
                float gOut[3] = { 0.0f, 0.0f, 0.0f };
                int active = 0;
                for (auto& g : grains)
                {
                    if (! g.active) continue;
                    ++active;
                    const auto& h = heads[(size_t) g.buf];
                    const int i0 = (int) g.pos;
                    const float f = (float) (g.pos - (double) i0);
                    const int i1 = (i0 + 1 >= h.length) ? 0 : i0 + 1;
                    const float v = P[h.offset + i0] * (1.0f - f) + P[h.offset + i1] * f;
                    const float env = 0.5f - 0.5f * std::cos (2.0f * pi * (float) g.age / (float) g.len);
                    const float sv = v * env;
                    gOut[g.buf] += sv;
                    outL += sv * g.panL;
                    outR += sv * g.panR;
                    g.pos += g.inc;
                    if (g.pos >= h.length) g.pos -= h.length;
                    if (g.pos < 0.0) g.pos += h.length;
                    if (++g.age >= g.len) g.active = false;
                }

                for (int b = 0; b < numGrainBufs; ++b)
                {
                    if (--grainCountdown[(size_t) b] <= 0)
                    {
                        spawnGrain (b);
                        grainCountdown[(size_t) b] = std::max (16, (int) ((float) grainLen * 0.5f * (0.5f + rnd())));
                    }
                    loopSum += std::abs (gOut[b]);
                    lineEnv[(size_t) b] += (std::abs (gOut[b]) - lineEnv[(size_t) b]) * kEnvL;
                }

                {
                    const float ctl = inMono + loopSum / (float) numGrainBufs;
                    envComp += (ctl - envComp) * (ctl > envComp ? kAttC : kRelC);
                    float gdb = 0.0f;
                    if (comAmt > 0.0f)
                    {
                        const float edb = 20.0f * std::log10 (envComp + 1.0e-9f);
                        gdb = comAmt * 0.85f * (-14.0f - edb);
                        if (edb < -54.0f) gdb = std::min (gdb, 0.0f);
                        gdb = std::clamp (gdb, -24.0f, 12.0f);
                    }
                    compGain += (std::pow (10.0f, gdb / 20.0f) - compGain) * kGain;
                }

                for (int b = 0; b < numGrainBufs; ++b)
                {
                    const float in = b == 0 ? g1 : b == 1 ? g2 : 0.5f * (g1 + g2);
                    const float w = processLoop (b, in + fbG * gOut[b]) * seamRamp;
                    heads[(size_t) b].advanceWrite (P, 1.0, w);
                }
                if ((s & 63) == 0) uiGrains.store (active);
            }

            const float gOutAll = fade * wetNorm;
            wetL[s] = outL * gOutAll;
            wetR[s] = outR * gOutAll;
        }

        // Anzeige
        const float lvl = std::max (peakHold, loopLevel.load() * 0.85f);
        loopLevel.store (lvl);
        peakHold = 0.0f;
        if (limitHold > 0) limitHold -= n;
        limiting.store (limitHold > 0);
        for (int i = 0; i < numLines; ++i)
        {
            const auto& h = heads[(size_t) i];
            uiHead[(size_t) i].store ((float) (h.pos / (double) h.length));
            uiLevel[(size_t) i].store (lineEnv[(size_t) i]);
        }
    }
}
