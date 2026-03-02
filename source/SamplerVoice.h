#pragma once
#include <JuceHeader.h>
#include "DSPProcessor.h"

//==============================================================================
/**
 * Shared sample data used by all LusionSamplerVoices.
 * Loaded on a background thread; audio thread only reads after atomicReady is set.
 */
class LusionSamplerSound : public juce::SynthesiserSound
{
public:
    LusionSamplerSound() = default;

    void loadBuffer (const juce::AudioBuffer<float>& src, double sourceSampleRate);
    void clear();

    bool appliesToNote    (int) override { return true; }
    bool appliesToChannel (int) override { return true; }

    const juce::AudioBuffer<float>& getBuffer()    const noexcept { return audioBuffer; }
    double                          getSourceRate() const noexcept { return sourceSampleRate; }
    bool                            isReady()       const noexcept { return atomicReady.load(); }

    /** Detected MIDI root note (0-127, or -1 if not detected). Written from loader thread,
        read from audio thread — use atomic. */
    std::atomic<int> detectedRoot { -1 };

private:
    juce::AudioBuffer<float> audioBuffer;
    double                   sourceSampleRate = 44100.0;
    std::atomic<bool>        atomicReady      { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LusionSamplerSound)
};

//==============================================================================
/**
 * Single polyphonic voice.
 * Pitch-ratio interpolation, ADSR, portamento (glide), per-voice DSP.
 */
class LusionSamplerVoice : public juce::SynthesiserVoice
{
public:
    LusionSamplerVoice();

    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void startNote    (int midiNoteNumber, float velocity,
                       juce::SynthesiserSound* sound,
                       int currentPitchWheelPosition) override;
    void stopNote     (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newValue) override;
    void controllerMoved  (int, int)    override {}
    void renderNextBlock  (juce::AudioBuffer<float>& outputBuffer,
                           int startSample, int numSamples) override;
    void prepareToPlay    (double sampleRate, int samplesPerBlock, int numOutputChannels);

    //── Atomic parameters (written from message thread, read from audio thread) ─
    std::atomic<float> paramAttack      { 0.01f };
    std::atomic<float> paramDecay       { 0.1f  };
    std::atomic<float> paramSustain     { 0.8f  };
    std::atomic<float> paramRelease     { 0.5f  };
    std::atomic<float> paramGlide       { 0.0f  };
    std::atomic<float> paramDrive       { 0.0f  };
    std::atomic<float> paramGain        { 1.0f  };
    std::atomic<float> paramFineTune    { 0.0f  };
    std::atomic<int>   paramRootOverride { -1   };  // -1 = use auto-detected

private:
    enum class AdsrState { Idle, Attack, Decay, Sustain, Release };
    AdsrState adsrState  = AdsrState::Idle;
    float     adsrLevel  = 0.f;
    float     attackCoeff  = 0.f;
    float     decayCoeff   = 0.f;
    float     releaseCoeff = 0.f;

    double playPosition        = 0.0;
    double pitchRatio          = 1.0;
    double currentMidiNote     = 60.0;
    double targetMidiNote      = 60.0;
    double pitchWheelSemitones = 0.0;
    float  velocityGain        = 1.f;

    DSPProcessor             dspProc;
    juce::AudioBuffer<float> scratchBuffer;

    void   recalcAdsr     (double sr);
    double calcPitchRatio (double sr) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LusionSamplerVoice)
};