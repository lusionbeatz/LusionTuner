#include "DSPProcessor.h"

void DSPProcessor::prepare (const juce::dsp::ProcessSpec& spec)
{
    numChannels = (int) juce::jmin ((uint32_t) 2, (uint32_t) spec.numChannels);
    reset();
}

void DSPProcessor::reset()
{
    for (int ch = 0; ch < 2; ++ch) { dcX1[ch] = 0.f; dcY1[ch] = 0.f; }
}

// Rational Padé approximation of tanh valid for |x| < 4
// Error < 0.003, safe for audio use
float DSPProcessor::softClip (float x, float driveGain) noexcept
{
    const float driven = x * (1.f + driveGain * 3.f);
    // Hard-limit input to avoid instability
    const float clamped = juce::jlimit (-4.f, 4.f, driven);
    const float x2 = clamped * clamped;
    // tanh Padé (3,2): tanh(x) ≈ x*(27+x²)/(27+9x²)
    return clamped * (27.f + x2) / (27.f + 9.f * x2);
}

void DSPProcessor::process (juce::AudioBuffer<float>& buffer,
                             float drive,
                             float gainLin) noexcept
{
    juce::FloatVectorOperations::disableDenormalisedNumberSupport();

    const int numCh  = juce::jmin (buffer.getNumChannels(), numChannels);
    const int numSmp = buffer.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        float  x1   = dcX1[ch];
        float  y1   = dcY1[ch];

        for (int i = 0; i < numSmp; ++i)
        {
            float s = data[i];

            // Drive / saturation
            if (drive > 0.001f)
                s = softClip (s, drive);

            // DC blocker: y[n] = x[n] - x[n-1] + 0.9975*y[n-1]
            const float y = s - x1 + 0.9975f * y1;
            x1 = s;
            y1 = y;
            s  = y;

            data[i] = s * gainLin;
        }

        dcX1[ch] = x1;
        dcY1[ch] = y1;
    }
}