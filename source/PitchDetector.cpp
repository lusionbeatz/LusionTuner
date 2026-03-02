#include "PitchDetector.h"
#include <cmath>
#include <vector>

//==============================================================================
// YIN algorithm: https://ygavelis.lt/yin.pdf
// Tuned for 808 / kick fundamentals with robust fallback strategies.
//==============================================================================

static float parabolaInterp (float y0, float y1, float y2)
{
    const float denom = 2.f * (y0 - 2.f * y1 + y2);
    if (std::abs (denom) < 1e-8f) return 0.f;
    return (y0 - y2) / denom;
}

float PitchDetector::runYin (const float* data, int numSamples, double sampleRate) const
{
    const int W = juce::jmin (numSamples, 65536);
    if (W < 64) return 0.f;

    const int tauMin = juce::jmax (1, (int) std::floor ((double) sampleRate / (double) kMaxHz));
    const int tauMax = (int) std::ceil  ((double) sampleRate / (double) kMinHz);
    const int halfW  = W / 2;

    if (tauMax >= halfW || tauMin >= tauMax) return 0.f;

    // ── Step 1: difference function ─────────────────────────────────────────
    std::vector<float> d (tauMax + 1, 0.f);
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        float sum = 0.f;
        for (int j = 0; j < halfW; ++j)
        {
            const float diff = data[j] - data[j + tau];
            sum += diff * diff;
        }
        d[tau] = sum;
    }

    // ── Step 2: cumulative mean normalised difference (CMND) ─────────────────
    std::vector<float> cmnd (tauMax + 1, 0.f);
    cmnd[0] = 1.f;
    float runningSum = 0.f;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        runningSum += d[tau];
        cmnd[tau] = (runningSum > 0.f) ? (d[tau] * (float) tau / runningSum) : 1.f;
    }

    // ── Step 3: find best tau using threshold with local-minimum tracking ────
    //    Use a relaxed threshold first, then tighten if needed.
    auto findBestTau = [&](float threshold) -> int
    {
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (cmnd[tau] < threshold)
            {
                // Walk to local minimum
                while (tau + 1 <= tauMax && cmnd[tau + 1] < cmnd[tau])
                    ++tau;
                return tau;
            }
        }
        return -1;
    };

    int bestTau = findBestTau (kYinThreshold);

    // Relaxed fallback: try higher thresholds if primary fails
    if (bestTau < 0) bestTau = findBestTau (0.20f);
    if (bestTau < 0) bestTau = findBestTau (0.30f);

    // Global minimum fallback
    if (bestTau < 0)
    {
        float minVal = 1e9f;
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (cmnd[tau] < minVal) { minVal = cmnd[tau]; bestTau = tau; }
        }
        // If global min is too high, signal is probably noise
        if (minVal > 0.45f) return 0.f;
    }

    // ── Step 4: parabolic refinement ─────────────────────────────────────────
    float refined = (float) bestTau;
    if (bestTau > tauMin && bestTau < tauMax)
        refined = (float) bestTau + parabolaInterp (cmnd[bestTau - 1], cmnd[bestTau], cmnd[bestTau + 1]);

    if (refined < 1.f) return 0.f;
    return (float) sampleRate / refined;
}

//==============================================================================
PitchDetector::Result PitchDetector::detectFundamental (const juce::AudioBuffer<float>& buffer,
                                                         double sampleRate) const
{
    Result r;
    if (buffer.getNumSamples() < 64) return r;

    const int numCh   = buffer.getNumChannels();
    const int numSamp = buffer.getNumSamples();

    // ── Analyse the first 2 seconds to capture fundamental at onset ──────────
    const int analyseLen = juce::jmin (numSamp, (int) (sampleRate * 2.0));

    // ── Mix to mono ───────────────────────────────────────────────────────────
    std::vector<float> mono (analyseLen, 0.f);
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* src  = buffer.getReadPointer (ch);
        const float  gain = 1.f / (float) numCh;
        for (int i = 0; i < analyseLen; ++i)
            mono[i] += src[i] * gain;
    }

    // ── Find the loudest onset window (skip silent head if any) ──────────────
    // Search in 50 ms windows for max RMS, then analyse from that onset
    const int windowSz = (int)(sampleRate * 0.05);
    int       onsetIdx = 0;
    float     maxRms   = 0.f;
    for (int start = 0; start + windowSz < analyseLen; start += windowSz / 2)
    {
        float rms = 0.f;
        for (int i = start; i < start + windowSz; ++i)
            rms += mono[i] * mono[i];
        rms = std::sqrt (rms / (float)windowSz);
        if (rms > maxRms) { maxRms = rms; onsetIdx = start; }
    }

    // Run YIN from onset
    const int yinStart = juce::jmax (0, onsetIdx);
    const int yinLen   = analyseLen - yinStart;

    if (yinLen < 128) { r.valid = false; return r; }

    r.frequencyHz = runYin (mono.data() + yinStart, yinLen, sampleRate);

    if (r.frequencyHz < kMinHz || r.frequencyHz > kMaxHz)
    {
        // Try from the very beginning as last resort
        r.frequencyHz = runYin (mono.data(), analyseLen, sampleRate);
    }

    if (r.frequencyHz < kMinHz || r.frequencyHz > kMaxHz)
    {
        r.valid = false;
        return r;
    }

    r.midiNote   = frequencyToMidiNote (r.frequencyHz);
    r.noteName   = midiNoteToName (r.midiNote);

    const float exactMidi  = 69.f + 12.f * std::log2 (r.frequencyHz / 440.f);
    r.centsOffset = (exactMidi - (float) r.midiNote) * 100.f;
    r.valid       = true;
    return r;
}

//==============================================================================
int PitchDetector::frequencyToMidiNote (float freq)
{
    if (freq <= 0.f) return -1;
    return (int) std::round (69.f + 12.f * std::log2 (freq / 440.f));
}

float PitchDetector::midiNoteToFrequency (int note)
{
    return 440.f * std::pow (2.f, (note - 69) / 12.f);
}

juce::String PitchDetector::midiNoteToName (int note)
{
    static const char* names[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    if (note < 0 || note > 127) return "?";
    // Octave: note/12 - 1  (MIDI 0 = C-1, MIDI 60 = C4)
    return juce::String (names[note % 12]) + juce::String (note / 12 - 1);
}