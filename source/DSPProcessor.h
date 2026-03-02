#pragma once
#include <JuceHeader.h>

//==============================================================================
/**
 * Inline DSP chain applied per-voice in processBlock:
 *   soft saturation (drive) → output gain
 * All state is maintained here; no allocations after prepare().
 */
class DSPProcessor
{
public:
    DSPProcessor() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** Process a stereo block in-place.
     *  drive  : 0..1  (0 = bypass, 1 = heavy saturation)
     *  gainLin: linear output gain multiplier
     */
    void process (juce::AudioBuffer<float>& buffer,
                  float drive,
                  float gainLin) noexcept;

private:
    // DC blocker state per channel (max 2)
    float dcX1[2] = {}, dcY1[2] = {};

    int   numChannels = 0;

    // Soft-clip via tanh approximation (Padé approximant, no std::tanh in RT)
    static float softClip (float x, float driveGain) noexcept;
};