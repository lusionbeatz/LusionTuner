#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ParamID
{
    static const juce::String attack       = "attack";
    static const juce::String decay        = "decay";
    static const juce::String sustain      = "sustain";
    static const juce::String release      = "release";
    static const juce::String drive        = "drive";
    static const juce::String gain         = "gain";
    static const juce::String fineTune     = "finetune";
    static const juce::String rootOvrd     = "rootoverride";
    static const juce::String targetKey    = "targetkey";
    static const juce::String targetOctave = "targetoctave";
}

juce::AudioProcessorValueTreeState::ParameterLayout
LusionTunerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamID::attack,   "Attack",   juce::NormalisableRange<float> (0.001f, 4.f, 0.001f, 0.4f), 0.01f, "s"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamID::decay,    "Decay",    juce::NormalisableRange<float> (0.001f, 4.f, 0.001f, 0.4f), 0.1f,  "s"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamID::sustain,  "Sustain",  juce::NormalisableRange<float> (0.f, 1.f, 0.001f),           0.8f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamID::release,  "Release",  juce::NormalisableRange<float> (0.001f, 8.f, 0.001f, 0.4f), 0.5f,  "s"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamID::drive,    "Drive",    juce::NormalisableRange<float> (0.f, 1.f, 0.001f),           0.f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamID::gain,     "Gain",     juce::NormalisableRange<float> (-24.f, 12.f, 0.1f),          0.f, "dB"));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        ParamID::fineTune, "Fine",     juce::NormalisableRange<float> (-100.f, 100.f, 0.1f),        0.f, "ct"));
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        ParamID::rootOvrd,     "Root",         -1, 127, -1));
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        ParamID::targetKey,    "TargetKey",      0,  11,  9));
    params.push_back (std::make_unique<juce::AudioParameterInt> (
        ParamID::targetOctave, "TargetOctave",   0,   5,  1));
    return { params.begin(), params.end() };
}

LusionTunerAudioProcessor::LusionTunerAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "LusionTuner", createParameterLayout()),
      thumbnailCache (4),
      thumbnail (4096, formatManager, thumbnailCache)
{
    formatManager.registerBasicFormats();
    auto* snd = new LusionSamplerSound();
    soundPtr  = snd;
    synth.addSound (snd);
    for (int i = 0; i < kNumVoices; ++i)
        synth.addVoice (new LusionSamplerVoice());
    for (auto& id : { ParamID::attack, ParamID::decay, ParamID::sustain, ParamID::release,
                      ParamID::drive, ParamID::gain, ParamID::fineTune, ParamID::rootOvrd })
        apvts.addParameterListener (id, this);
}

LusionTunerAudioProcessor::~LusionTunerAudioProcessor()
{
    if (loadingThread != nullptr) loadingThread->stopThread (3000);
    for (auto& id : { ParamID::attack, ParamID::decay, ParamID::sustain, ParamID::release,
                      ParamID::drive, ParamID::gain, ParamID::fineTune, ParamID::rootOvrd })
        apvts.removeParameterListener (id, this);
}

bool LusionTunerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void LusionTunerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<LusionSamplerVoice*> (synth.getVoice (i)))
            v->prepareToPlay (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    pushParamsToVoices();
}

void LusionTunerAudioProcessor::releaseResources() {}

void LusionTunerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // ── Drain MIDI queue (noteOn/Off posted from message thread) ─────────────
    {
        juce::ScopedLock sl (midiQueueLock);
        if (!midiQueue.isEmpty())
        {
            midi.addEvents (midiQueue, 0, buffer.getNumSamples(), 0);
            midiQueue.clear();
        }
    }

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    // ── Consume load-finished flag (written atomically by loader thread) ──────
    if (loadFinishedFlag.exchange (false))
    {
        pitchDetectedFlag   = lastResult.pitch.valid;
        detectedNote        = lastResult.pitch.noteName;
        detectedHz          = lastResult.pitch.frequencyHz;
        detectedMidiNote    = lastResult.pitch.midiNote;
        detectedCentsOffset = lastResult.pitch.centsOffset;
        sampleLoadedFlag    = lastResult.sampleLoaded;

        // Queue target key/octave for the editor timer to apply on the message thread
        // (APVTS::setValueNotifyingHost must NOT be called from the audio thread)
        if (pitchDetectedFlag && detectedMidiNote >= 0)
        {
            pendingTargetKey.store    (detectedMidiNote % 12);
            pendingTargetOctave.store (juce::jlimit (0, 5, detectedMidiNote / 12 - 1));
        }
    }
}

// ── Editor queries ────────────────────────────────────────────────────────────
juce::String LusionTunerAudioProcessor::getDetectedNote()     const { return detectedNote; }
float        LusionTunerAudioProcessor::getDetectedHz()       const { return detectedHz; }
bool         LusionTunerAudioProcessor::isPitchDetected()     const { return pitchDetectedFlag; }
bool         LusionTunerAudioProcessor::isSampleLoaded()      const { return sampleLoadedFlag; }
bool         LusionTunerAudioProcessor::isLoadingInProgress() const { return loadingInProgress.load(); }

int LusionTunerAudioProcessor::getTargetKeyIndex() const
{
    return (int) apvts.getRawParameterValue (ParamID::targetKey)->load();
}

int LusionTunerAudioProcessor::getTargetOctave() const
{
    return (int) apvts.getRawParameterValue (ParamID::targetOctave)->load();
}

juce::String LusionTunerAudioProcessor::getTargetNote() const
{
    static const char* n[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    return juce::String (n[juce::jlimit (0, 11, getTargetKeyIndex())]) + juce::String (getTargetOctave());
}

float LusionTunerAudioProcessor::getTargetHz() const
{
    const int midi = (getTargetOctave() + 1) * 12 + getTargetKeyIndex();
    return 440.0f * std::pow (2.0f, (float) (midi - 69) / 12.0f);
}

int LusionTunerAudioProcessor::getSemitoneShift() const
{
    if (!pitchDetectedFlag || detectedMidiNote < 0) return 0;
    const int target = (getTargetOctave() + 1) * 12 + getTargetKeyIndex();
    return target - detectedMidiNote;
}

const juce::AudioBuffer<float>* LusionTunerAudioProcessor::getLoadedBuffer() const
{
    return displayBuffer.getNumSamples() > 0 ? &displayBuffer : nullptr;
}

// ── User actions ──────────────────────────────────────────────────────────────
void LusionTunerAudioProcessor::setTargetKey (int noteIndex)
{
    if (auto* p = apvts.getParameter (ParamID::targetKey))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (0, 11, noteIndex)));
}

void LusionTunerAudioProcessor::setTargetOctave (int octave)
{
    if (auto* p = apvts.getParameter (ParamID::targetOctave))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (0, 5, octave)));
}

void LusionTunerAudioProcessor::startPlayback()
{
    const int note = juce::jlimit (0, 127, (getTargetOctave() + 1) * 12 + getTargetKeyIndex());
    juce::ScopedLock sl (midiQueueLock);
    midiQueue.clear();
    midiQueue.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    midiQueue.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 1);
}

void LusionTunerAudioProcessor::stopPlayback()
{
    juce::ScopedLock sl (midiQueueLock);
    midiQueue.clear();
    midiQueue.addEvent (juce::MidiMessage::allNotesOff (1), 0);
}

void LusionTunerAudioProcessor::loadSampleFromFile (const juce::File& file)
{
    if (loadingThread != nullptr && loadingThread->isThreadRunning())
        loadingThread->stopThread (1000);
    loadFinishedFlag.store (false);
    loadErrorFlag.store (false);
    loadingInProgress.store (true);
    pitchDetectedFlag = false;
    sampleLoadedFlag  = false;
    pendingTargetKey.store (-1);
    pendingTargetOctave.store (-1);
    loadingThread = std::make_unique<LoadingThread> (*this, file);
    loadingThread->startThread (juce::Thread::Priority::low);
}

void LusionTunerAudioProcessor::setManualRoot (int midiNote)
{
    const int clamped = (midiNote < 0) ? -1 : juce::jlimit (0, 127, midiNote);

    // Write to APVTS — must be called from the message thread (editor callback does this)
    if (auto* p = apvts.getParameter (ParamID::rootOvrd))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) clamped));

    if (clamped >= 0)
    {
        detectedMidiNote    = clamped;
        detectedNote        = PitchDetector::midiNoteToName (clamped);
        detectedHz          = PitchDetector::midiNoteToFrequency (clamped);
        detectedCentsOffset = 0.f;
        pitchDetectedFlag   = true;
        sampleLoadedFlag    = true;

        if (soundPtr != nullptr)
            soundPtr->detectedRoot.store (clamped);

        // Auto-snap target key/octave to the manually chosen root
        pendingTargetKey.store    (clamped % 12);
        pendingTargetOctave.store (juce::jlimit (0, 5, clamped / 12 - 1));
    }
    else
    {
        // Revert to auto-detected root
        if (soundPtr != nullptr)
        {
            const int detected = soundPtr->detectedRoot.load();
            if (detected >= 0)
            {
                detectedMidiNote    = detected;
                detectedNote        = PitchDetector::midiNoteToName (detected);
                detectedHz          = PitchDetector::midiNoteToFrequency (detected);
                detectedCentsOffset = 0.f;
                pitchDetectedFlag   = true;
            }
        }
    }
}

int LusionTunerAudioProcessor::getManualRoot() const
{
    // getRawParameterValue for AudioParameterInt returns the actual integer value
    // cast to float (not normalized), so this round-trip is correct.
    return (int) apvts.getRawParameterValue (ParamID::rootOvrd)->load();
}

void LusionTunerAudioProcessor::triggerExportTuned()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Export Tuned Sample",
        juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
        "*.wav");
    fileChooser->launchAsync (
        juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            juce::File dest = fc.getResult();
            if (dest != juce::File{})
            {
                if (dest.getFileExtension().isEmpty())
                    dest = dest.withFileExtension (".wav");
                exportTunedSample (dest);
            }
        });
}

void LusionTunerAudioProcessor::exportTunedSample (const juce::File& dest)
{
    if (soundPtr == nullptr || !soundPtr->isReady()) return;

    // Use manual override if set, else auto-detected root
    const int rootOv   = (int) apvts.getRawParameterValue (ParamID::rootOvrd)->load();
    const int detRoot  = soundPtr->detectedRoot.load();
    const int rootNote = (rootOv >= 0) ? rootOv : detRoot;
    if (rootNote < 0) return;

    const int    target = (getTargetOctave() + 1) * 12 + getTargetKeyIndex();
    const double ratio  = std::pow (2.0, (double) (target - rootNote) / 12.0);

    const juce::AudioBuffer<float>& srcBuf  = soundPtr->getBuffer();
    const double                    srcRate = soundPtr->getSourceRate();
    const int                       srcLen  = srcBuf.getNumSamples();
    const int                       outLen  = juce::jmax (1, (int) ((double) srcLen / ratio));
    const int                       numCh   = srcBuf.getNumChannels();

    juce::AudioBuffer<float> outBuf (numCh, outLen);
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* src = srcBuf.getReadPointer (ch);
        float*       dst = outBuf.getWritePointer (ch);
        for (int i = 0; i < outLen; ++i)
        {
            const double pos = (double) i * ratio;
            const int    p0  = (int) pos;
            const float  f   = (float) (pos - (double) p0);
            const int    p1  = p0 + 1;
            dst[i] = ((p0 < srcLen) ? src[p0] : 0.f)
                   + f * ((p1 < srcLen) ? src[p1] - src[p0] : 0.f);
        }
    }

    dest.deleteFile();
    if (auto stream = std::unique_ptr<juce::FileOutputStream> (dest.createOutputStream()))
    {
        juce::WavAudioFormat wav;
        if (auto* w = wav.createWriterFor (stream.get(), srcRate, (uint32_t) numCh, 24, {}, 0))
        {
            stream.release();  // writer owns the stream now
            w->writeFromAudioSampleBuffer (outBuf, 0, outLen);
            delete w;
        }
    }
}

void LusionTunerAudioProcessor::parameterChanged (const juce::String& paramID, float newValue)
{
    auto dBtoLin = [] (float dB) { return std::pow (10.f, dB / 20.f); };

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* v = dynamic_cast<LusionSamplerVoice*> (synth.getVoice (i)))
        {
            if      (paramID == ParamID::attack)   v->paramAttack.store (newValue);
            else if (paramID == ParamID::decay)    v->paramDecay.store (newValue);
            else if (paramID == ParamID::sustain)  v->paramSustain.store (newValue);
            else if (paramID == ParamID::release)  v->paramRelease.store (newValue);
            else if (paramID == ParamID::drive)    v->paramDrive.store (newValue);
            else if (paramID == ParamID::gain)     v->paramGain.store (dBtoLin (newValue));
            else if (paramID == ParamID::fineTune) v->paramFineTune.store (newValue);
            else if (paramID == ParamID::rootOvrd)
            {
                // IMPORTANT: JUCE APVTS sends the NORMALIZED (0..1) value in parameterChanged,
                // even for AudioParameterInt. We must denormalize back to the actual int value.
                // AudioParameterInt(-1,127): normalized 0.0 → actual -1, 1.0 → actual 127.
                if (auto* p = apvts.getParameter (ParamID::rootOvrd))
                {
                    const int actual = (int) std::round (p->convertFrom0to1 (newValue));
                    v->paramRootOverride.store (actual);
                }
            }
        }
    }
}

void LusionTunerAudioProcessor::pushParamsToVoices()
{
    // Float params: getRawParameterValue returns the actual float value directly
    for (auto& id : { ParamID::attack, ParamID::decay, ParamID::sustain, ParamID::release,
                      ParamID::drive, ParamID::gain, ParamID::fineTune })
        if (auto* p = apvts.getRawParameterValue (id))
            parameterChanged (id, p->load());

    // Int param: getRawParameterValue also returns actual int value cast to float
    // But we route through parameterChanged which does convertFrom0to1, so we must
    // pass the NORMALIZED value. Get it via getParameter()->getValue().
    if (auto* p = apvts.getParameter (ParamID::rootOvrd))
        parameterChanged (ParamID::rootOvrd, p->getValue());  // getValue() returns 0..1 normalized
}

void LusionTunerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void LusionTunerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* LusionTunerAudioProcessor::createEditor()
{
    return new LusionTunerAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LusionTunerAudioProcessor();
}