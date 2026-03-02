#pragma once
#include <JuceHeader.h>
#include "SamplerVoice.h"
#include "PitchDetector.h"

class LusionTunerAudioProcessor : public juce::AudioProcessor,
                                   public juce::AudioProcessorValueTreeState::Listener
{
public:
    LusionTunerAudioProcessor();
    ~LusionTunerAudioProcessor() override;

    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock   (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override   { return "LusionTuner"; }
    bool   acceptsMidi()  const override          { return true; }
    bool   producesMidi() const override          { return false; }
    bool   isMidiEffect() const override          { return false; }
    double getTailLengthSeconds() const override  { return 2.0; }
    int    getNumPrograms()    override           { return 1; }
    int    getCurrentProgram() override           { return 0; }
    void   setCurrentProgram (int) override       {}
    const juce::String getProgramName (int) override { return {}; }
    void   changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void parameterChanged (const juce::String& paramID, float newValue) override;

    // ── Editor queries ────────────────────────────────────────────────────────
    juce::String getDetectedNote()     const;
    float        getDetectedHz()       const;
    juce::String getTargetNote()       const;
    float        getTargetHz()         const;
    int          getSemitoneShift()    const;
    bool         isPitchDetected()     const;
    int          getTargetKeyIndex()   const;
    int          getTargetOctave()     const;
    int          getDetectedMidiNote() const { return detectedMidiNote; }
    float        getDetectedCents()    const { return detectedCentsOffset; }

    // ── Actions ───────────────────────────────────────────────────────────────
    void setTargetKey    (int noteIndex);
    void setTargetOctave (int octave);
    void startPlayback();
    void stopPlayback();
    void loadSampleFromFile (const juce::File& file);
    void triggerExportTuned();
    bool isSampleLoaded()      const;
    bool isLoadingInProgress() const;

    /** Override the detected root note. Pass -1 to revert to auto-detected. */
    void setManualRoot (int midiNote);
    /** Returns the manual root override from APVTS (-1 = none). */
    int  getManualRoot() const;

    const juce::AudioBuffer<float>* getLoadedBuffer() const;

    // ── Public data ───────────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioFormatManager           formatManager;
    juce::AudioThumbnailCache          thumbnailCache;
    juce::AudioThumbnail               thumbnail;

    /** Set by audio thread after detection; editor timer reads + applies on message thread. */
    std::atomic<int> pendingTargetKey    { -1 };
    std::atomic<int> pendingTargetOctave { -1 };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void pushParamsToVoices();
    void exportTunedSample (const juce::File& dest);

    juce::Synthesiser   synth;
    static const int    kNumVoices = 8;
    LusionSamplerSound* soundPtr   = nullptr;

    juce::String detectedNote;
    float        detectedHz          = 0.0f;
    float        detectedCentsOffset = 0.0f;
    int          detectedMidiNote    = -1;
    bool         pitchDetectedFlag   = false;
    bool         sampleLoadedFlag    = false;

    juce::AudioBuffer<float> displayBuffer;

    // Thread-safe MIDI queue: message thread → audio thread
    juce::MidiBuffer      midiQueue;
    juce::CriticalSection midiQueueLock;

    struct PitchInfo
    {
        float        frequencyHz = 0.0f;
        int          midiNote    = -1;
        juce::String noteName;
        float        centsOffset = 0.0f;
        bool         valid       = false;
    };

    struct LastResult
    {
        PitchInfo    pitch;
        juce::String fileName;
        bool         sampleLoaded = false;
    } lastResult;

    std::atomic<bool> loadFinishedFlag  { false };
    std::atomic<bool> loadErrorFlag     { false };
    std::atomic<bool> loadingInProgress { false };

    struct LoadingThread : public juce::Thread
    {
        LusionTunerAudioProcessor& proc;
        juce::File fileToLoad;

        LoadingThread (LusionTunerAudioProcessor& p, const juce::File& f)
            : juce::Thread ("LusionLoader"), proc (p), fileToLoad (f) {}

        void run() override
        {
            if (threadShouldExit()) return;
            juce::AudioFormatManager fmt;
            fmt.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (fmt.createReaderFor (fileToLoad));
            if (reader == nullptr || threadShouldExit())
            {
                proc.loadErrorFlag.store (true);
                proc.loadFinishedFlag.store (true);
                proc.loadingInProgress.store (false);
                return;
            }
            const int64_t maxSamp = 10 * (int64_t) reader->sampleRate;
            const int64_t numSamp = juce::jmin (reader->lengthInSamples, maxSamp);
            const int     numCh   = (int) juce::jmin ((uint32_t) reader->numChannels, (uint32_t) 2);
            juce::AudioBuffer<float> buf (numCh, (int) numSamp);
            reader->read (&buf, 0, (int) numSamp, 0, true, numCh > 1);
            if (threadShouldExit()) return;
            PitchDetector detector;
            auto result = detector.detectFundamental (buf, reader->sampleRate);
            if (threadShouldExit()) return;
            if (proc.soundPtr != nullptr)
            {
                proc.soundPtr->loadBuffer (buf, reader->sampleRate);
                proc.soundPtr->detectedRoot.store (result.valid ? result.midiNote : -1);
            }
            proc.thumbnail.reset (numCh, reader->sampleRate, numSamp);
            proc.thumbnail.addBlock (0, buf, 0, (int) numSamp);
            proc.displayBuffer = buf;
            proc.lastResult.pitch        = { result.frequencyHz, result.midiNote,
                                              result.noteName, result.centsOffset, result.valid };
            proc.lastResult.fileName     = fileToLoad.getFileNameWithoutExtension();
            proc.lastResult.sampleLoaded = true;
            proc.loadErrorFlag.store    (!result.valid);
            proc.loadFinishedFlag.store (true);
            proc.loadingInProgress.store (false);
        }
    };

    std::unique_ptr<LoadingThread>     loadingThread;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LusionTunerAudioProcessor)
};