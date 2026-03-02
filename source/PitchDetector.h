#pragma once
#include <JuceHeader.h>

//==============================================================================
/**
 * Pitch detector tuned for 808 / kick fundamentals (20 – 200 Hz).
 * Uses the YIN algorithm with multi-threshold fallback for robustness
 * on complex transient / decaying samples.
 */
class PitchDetector
{
public:
    struct Result
    {
        float        frequencyHz = 0.f;
        int          midiNote    = -1;
        juce::String noteName;
        float        centsOffset = 0.f;   ///< cents offset from nearest semitone
        bool         valid       = false;
    };

    /** Analyse the whole buffer at the given sampleRate.
     *  Called on a background thread — allocations are fine here. */
    Result detectFundamental (const juce::AudioBuffer<float>& buffer,
                              double sampleRate) const;

    static juce::String midiNoteToName (int note);
    static int          frequencyToMidiNote (float freq);
    static float        midiNoteToFrequency (int note);

private:
    // Primary YIN threshold — lower = more accurate but may miss some signals.
    // Fallback thresholds are tried automatically inside runYin.
    static constexpr float kYinThreshold = 0.12f;

    // Frequency search bounds — covers full 808 / sub-kick range
    static constexpr float kMinHz = 20.f;
    static constexpr float kMaxHz = 250.f;

    float runYin (const float* data, int numSamples, double sampleRate) const;
};