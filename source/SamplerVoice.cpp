#include "SamplerVoice.h"
#include "PitchDetector.h"

//==============================================================================
// LusionSamplerSound
//==============================================================================
void LusionSamplerSound::loadBuffer (const juce::AudioBuffer<float>& src, double sr)
{
    atomicReady.store (false);
    juce::Thread::sleep (1);
    audioBuffer      = src;
    sourceSampleRate = sr;
    atomicReady.store (true);
}

void LusionSamplerSound::clear()
{
    atomicReady.store (false);
    audioBuffer.setSize (0, 0);
    detectedRoot.store (-1);
}

//==============================================================================
// LusionSamplerVoice
//==============================================================================
LusionSamplerVoice::LusionSamplerVoice()
{
    scratchBuffer.setSize (2, 4096);
}

void LusionSamplerVoice::prepareToPlay (double sampleRate, int samplesPerBlock,
                                         int numOutputChannels)
{
    const int neededSamples  = juce::jmax (samplesPerBlock, 4096);
    const int neededChannels = juce::jmax (numOutputChannels, 2);
    scratchBuffer.setSize (neededChannels, neededSamples, false, false, true);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) neededChannels;
    dspProc.prepare (spec);

    adsrState    = AdsrState::Idle;
    adsrLevel    = 0.0f;
    playPosition = 0.0;
}

bool LusionSamplerVoice::canPlaySound (juce::SynthesiserSound* s)
{
    return dynamic_cast<LusionSamplerSound*> (s) != nullptr;
}

void LusionSamplerVoice::startNote (int midiNoteNumber, float velocity,
                                     juce::SynthesiserSound* sound,
                                     int pitchWheelPos)
{
    auto* s = dynamic_cast<LusionSamplerSound*> (sound);
    if (s == nullptr || !s->isReady()) return;

    velocityGain        = velocity;
    targetMidiNote      = (double) midiNoteNumber;
    pitchWheelSemitones = (pitchWheelPos - 8192) / 8192.0 * 2.0;

    const float glide = paramGlide.load();
    if (glide < 0.005f || adsrState == AdsrState::Idle)
        currentMidiNote = targetMidiNote;

    playPosition = 0.0;
    recalcAdsr (getSampleRate());
    adsrState = AdsrState::Attack;
}

void LusionSamplerVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        if (adsrState != AdsrState::Idle)
            adsrState = AdsrState::Release;
    }
    else
    {
        adsrState    = AdsrState::Idle;
        adsrLevel    = 0.0f;
        playPosition = 0.0;
        clearCurrentNote();
    }
}

void LusionSamplerVoice::pitchWheelMoved (int newValue)
{
    pitchWheelSemitones = (newValue - 8192) / 8192.0 * 2.0;
}

void LusionSamplerVoice::recalcAdsr (double sr)
{
    if (sr <= 0.0) return;
    auto tc = [&](float timeSec) -> float
    {
        const float t = juce::jmax (timeSec, 0.001f);
        return std::exp (-1.0f / (float)(sr * (double)t));
    };
    attackCoeff  = tc (paramAttack.load());
    decayCoeff   = tc (paramDecay.load());
    releaseCoeff = tc (paramRelease.load());
}

double LusionSamplerVoice::calcPitchRatio (double sr) const
{
    auto* sound = dynamic_cast<const LusionSamplerSound*> (getCurrentlyPlayingSound().get());
    if (sound == nullptr) return 1.0;

    const int rootOverride = paramRootOverride.load();
    const int detectedRoot = sound->detectedRoot.load();

    // ── Root note resolution ─────────────────────────────────────────────────
    // Priority: manual override > auto-detected root > fallback (play at unity)
    int rootNote = -1;
    if (rootOverride >= 0)
        rootNote = rootOverride;
    else if (detectedRoot >= 0)
        rootNote = detectedRoot;
    else
    {
        // No root known — play at unity pitch (source rate compensation only).
        // This preserves the original sample pitch regardless of target key.
        return sound->getSourceRate() / sr;
    }

    const double fineCents  = (double) paramFineTune.load() / 100.0;
    const double targetNote = currentMidiNote + pitchWheelSemitones + fineCents;
    const double semitones  = targetNote - (double) rootNote;

    return std::pow (2.0, semitones / 12.0) * sound->getSourceRate() / sr;
}

void LusionSamplerVoice::renderNextBlock (juce::AudioBuffer<float>& outputBuffer,
                                           int startSample, int numSamples)
{
    if (adsrState == AdsrState::Idle) return;

    auto* sound = dynamic_cast<LusionSamplerSound*> (getCurrentlyPlayingSound().get());
    if (sound == nullptr || !sound->isReady()) return;

    const juce::AudioBuffer<float>& srcBuf = sound->getBuffer();
    const int    srcLen = srcBuf.getNumSamples();
    const int    srcCh  = srcBuf.getNumChannels();
    const int    outCh  = outputBuffer.getNumChannels();
    const double sr     = getSampleRate();

    if (srcLen == 0 || srcCh == 0 || sr <= 0.0) return;

    if (scratchBuffer.getNumSamples() < numSamples)
        scratchBuffer.setSize (juce::jmax (outCh, 2), numSamples, false, false, true);

    recalcAdsr (sr);

    const float  glideTime = paramGlide.load();
    const double glideTc   = (glideTime > 0.005f)
                             ? std::exp (-1.0 / (sr * (double) glideTime))
                             : 0.0;

    const float drive   = paramDrive.load();
    const float gainLin = paramGain.load();
    const float sustain = paramSustain.load();

    const int scratchCh = juce::jmin (outCh, scratchBuffer.getNumChannels());
    for (int ch = 0; ch < scratchCh; ++ch)
        scratchBuffer.clear (ch, 0, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        // Glide
        if (glideTc > 0.0)
            currentMidiNote = targetMidiNote + glideTc * (currentMidiNote - targetMidiNote);
        else
            currentMidiNote = targetMidiNote;

        pitchRatio = calcPitchRatio (sr);

        const int   pos0 = (int) playPosition;
        const float frac = (float) (playPosition - (double) pos0);
        const int   pos1 = pos0 + 1;

        // ── ADSR envelope ────────────────────────────────────────────────────
        float env = 0.0f;
        switch (adsrState)
        {
            case AdsrState::Attack:
                adsrLevel = attackCoeff * adsrLevel + (1.0f - attackCoeff) * 1.2f;
                if (adsrLevel >= 1.0f) { adsrLevel = 1.0f; adsrState = AdsrState::Decay; }
                env = adsrLevel;
                break;
            case AdsrState::Decay:
                adsrLevel = decayCoeff * adsrLevel + (1.0f - decayCoeff) * sustain;
                env = adsrLevel;
                if (std::abs (adsrLevel - sustain) < 0.001f) adsrState = AdsrState::Sustain;
                break;
            case AdsrState::Sustain:
                env = sustain;
                break;
            case AdsrState::Release:
                adsrLevel = releaseCoeff * adsrLevel;
                env = adsrLevel;
                if (env < 1e-5f) { env = 0.0f; adsrState = AdsrState::Idle; }
                break;
            default:
                env = 0.0f;
                break;
        }

        // ── Sample interpolation (linear) ────────────────────────────────────
        for (int ch = 0; ch < scratchCh; ++ch)
        {
            const int    srcChIdx = (ch < srcCh) ? ch : 0;
            const float* srcData  = srcBuf.getReadPointer (srcChIdx);
            const float  s0       = (pos0 >= 0 && pos0 < srcLen) ? srcData[pos0] : 0.0f;
            const float  s1       = (pos1 >= 0 && pos1 < srcLen) ? srcData[pos1] : 0.0f;
            scratchBuffer.getWritePointer (ch)[i] = (s0 + frac * (s1 - s0)) * env * velocityGain;
        }

        playPosition += pitchRatio;

        // End of sample — go to release
        if (playPosition >= (double) srcLen && adsrState != AdsrState::Idle
                && adsrState != AdsrState::Release)
            adsrState = AdsrState::Release;

        if (adsrState == AdsrState::Idle)
        {
            // Zero remaining samples cleanly
            for (int ch = 0; ch < scratchCh; ++ch)
                for (int j = i + 1; j < numSamples; ++j)
                    scratchBuffer.getWritePointer (ch)[j] = 0.0f;
            break;
        }
    }

    // ── DSP chain (drive + gain + DC blocker) ─────────────────────────────────
    {
        juce::AudioBuffer<float> dspBuf (scratchBuffer.getArrayOfWritePointers(),
                                          scratchCh, numSamples);
        dspProc.process (dspBuf, drive, gainLin);
    }

    for (int ch = 0; ch < scratchCh; ++ch)
        outputBuffer.addFrom (ch, startSample, scratchBuffer.getReadPointer (ch), numSamples);

    if (adsrState == AdsrState::Idle)
        clearCurrentNote();
}