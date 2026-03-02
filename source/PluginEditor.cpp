#include "PluginEditor.h"
#include "PitchDetector.h"
#include <BinaryData.h>

LusionTunerAudioProcessorEditor::LusionTunerAudioProcessorEditor(LusionTunerAudioProcessor& p)
    : AudioProcessorEditor(&p), proc(p),
    loadBtn("  LOAD SAMPLE"),
    exportBtn("  EXPORT TUNED"),
    playBtn("  PLAY")
{
    setLookAndFeel(&laf);

    // Logo
    logoImage = juce::PNGImageFormat::loadFrom(BinaryData::logo_png, (size_t)BinaryData::logo_pngSize);

    setWantsKeyboardFocus(false);

    // Waveform - no mouse intercept so drops pass through
    waveDisplay.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(waveDisplay);

    // ── Pitch display labels ──────────────────────────────────────────────────
    bigNoteLbl.setText("--", juce::dontSendNotification);
    bigNoteLbl.setFont(juce::Font("Helvetica Neue", 80.f, juce::Font::bold));
    bigNoteLbl.setColour(juce::Label::textColourId, juce::Colour(C_DIM));
    bigNoteLbl.setJustificationType(juce::Justification::centredLeft);
    bigNoteLbl.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(bigNoteLbl);

    bigOctLbl.setText("", juce::dontSendNotification);
    bigOctLbl.setFont(juce::Font("Helvetica Neue", 32.f, juce::Font::bold));
    bigOctLbl.setColour(juce::Label::textColourId, juce::Colour(C_DIM));
    bigOctLbl.setJustificationType(juce::Justification::bottomLeft);
    bigOctLbl.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(bigOctLbl);

    pitchBadge.setText("DROP SAMPLE TO DETECT", juce::dontSendNotification);
    pitchBadge.setFont(juce::Font("Helvetica Neue", 10.f, juce::Font::bold));
    pitchBadge.setColour(juce::Label::textColourId, juce::Colour(C_DIM));
    pitchBadge.setJustificationType(juce::Justification::centredLeft);
    pitchBadge.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(pitchBadge);

    detHzLbl.setText("--- Hz", juce::dontSendNotification);
    detHzLbl.setFont(juce::Font("Helvetica Neue", 13.f, juce::Font::plain));
    detHzLbl.setColour(juce::Label::textColourId, juce::Colour(C_DIM));
    detHzLbl.setJustificationType(juce::Justification::centredLeft);
    detHzLbl.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(detHzLbl);

    auto setupInfoLbl = [&](juce::Label& l, const juce::String& t, float sz, juce::Colour c)
    {
        l.setText(t, juce::dontSendNotification);
        l.setFont(juce::Font("Helvetica Neue", sz, juce::Font::bold));
        l.setColour(juce::Label::textColourId, c);
        l.setJustificationType(juce::Justification::centredLeft);
        l.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(l);
    };
    setupInfoLbl(tgtKeyLbl,  "A1",              22.f, juce::Colour(C_ORANGE));
    setupInfoLbl(tgtHzLbl,   "55.0 Hz",         18.f, juce::Colour(C_ORANGE));
    setupInfoLbl(semShiftLbl, "0 st",            22.f, juce::Colour(C_PURPLE));
    setupInfoLbl(inTuneLbl,   "Already in tune", 11.f, juce::Colour(C_GREEN));
    inTuneLbl.setVisible(false);

    setupInfoLbl(stDetLbl,   "--",      12.f, juce::Colour(C_TEXT));
    setupInfoLbl(stTgtLbl,   "A1",     12.f, juce::Colour(C_TEXT));
    setupInfoLbl(stShiftLbl, "0 st",   12.f, juce::Colour(C_PURPLE));
    setupInfoLbl(stHzLbl,    "55.0 Hz",12.f, juce::Colour(C_ORANGE));

    // ── Keyboard strip ────────────────────────────────────────────────────────
    keyStrip.setInterceptsMouseClicks(true, true);
    keyStrip.setSelectedKey(9);
    keyStrip.onKeySelected = [this](int k)
    {
        proc.setTargetKey(k);
        updateUI();
    };
    addAndMakeVisible(keyStrip);

    // ── Octave buttons ────────────────────────────────────────────────────────
    for (int i = 0; i < 6; ++i)
    {
        octBtn[i].setButtonText(juce::String(i));
        octBtn[i].setColour(juce::TextButton::buttonColourId, juce::Colour(C_SURF));
        octBtn[i].setColour(juce::TextButton::textColourOffId, juce::Colour(C_TEXT));
        octBtn[i].onClick = [this, i]()
        {
            currentOctave = i;
            proc.setTargetOctave(i);
            for (int j = 0; j < 6; ++j)
                octBtn[j].setToggleState(j == i, juce::dontSendNotification);
            updateUI();
        };
        addAndMakeVisible(octBtn[i]);
    }

    // Sync octave from processor on startup
    currentOctave = proc.getTargetOctave();
    for (int j = 0; j < 6; ++j)
        octBtn[j].setToggleState(j == currentOctave, juce::dontSendNotification);

    // ── Action buttons ────────────────────────────────────────────────────────
    loadBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(C_ORANGE));
    loadBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    loadBtn.onClick = [this]()
    {
        auto chooser = std::make_shared<juce::FileChooser>(
            "Load 808 / Kick Sample",
            juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
            "*.wav;*.aiff;*.aif;*.flac;*.mp3");
        chooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                const juce::File f = fc.getResult();
                if (f.existsAsFile())
                {
                    waveformLoaded = false;
                    editingRoot    = -1;
                    proc.loadSampleFromFile(f);
                }
            });
    };
    addAndMakeVisible(loadBtn);

    exportBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(C_GREEN));
    exportBtn.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    exportBtn.onClick = [this]() { proc.triggerExportTuned(); };
    addAndMakeVisible(exportBtn);

    playBtn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1E1E28));
    playBtn.setColour(juce::TextButton::textColourOffId, juce::Colour(C_ORANGE));
    playBtn.onClick = [this]()
    {
        isPlaying = !isPlaying;
        playBtn.setButtonText(isPlaying ? "  STOP" : "  PLAY");
        if (isPlaying) proc.startPlayback();
        else           proc.stopPlayback();
        waveDisplay.setPlaying(isPlaying);
    };
    addAndMakeVisible(playBtn);

    // ── Manual pitch correction controls ─────────────────────────────────────
    auto styleSmallBtn = [&](juce::TextButton& b, juce::uint32 col)
    {
        b.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xFF1E1E28));
        b.setColour(juce::TextButton::textColourOffId, juce::Colour(col));
        addAndMakeVisible(b);
    };
    styleSmallBtn(rootDownBtn,  C_ORANGE);
    styleSmallBtn(rootUpBtn,    C_ORANGE);
    styleSmallBtn(rootClearBtn, C_DIM);

    rootEditLbl.setFont(juce::Font("Helvetica Neue", 12.f, juce::Font::bold));
    rootEditLbl.setColour(juce::Label::textColourId, juce::Colour(C_ORANGE));
    rootEditLbl.setJustificationType(juce::Justification::centred);
    rootEditLbl.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(rootEditLbl);

    rootDownBtn.onClick = [this]()
    {
        // Nudge detected/manual root down by 1 semitone
        int cur = editingRoot >= 0 ? editingRoot
                : proc.getDetectedMidiNote();
        if (cur < 0) return;
        editingRoot = juce::jlimit(0, 127, cur - 1);
        proc.setManualRoot(editingRoot);
        updateUI();
    };

    rootUpBtn.onClick = [this]()
    {
        int cur = editingRoot >= 0 ? editingRoot
                : proc.getDetectedMidiNote();
        if (cur < 0) return;
        editingRoot = juce::jlimit(0, 127, cur + 1);
        proc.setManualRoot(editingRoot);
        updateUI();
    };

    rootClearBtn.onClick = [this]()
    {
        editingRoot = -1;
        proc.setManualRoot(-1);
        updateUI();
    };
    auto setupKnob = [&](LusionKnob& k)
    {
        k.slider.setLookAndFeel(&laf);
        addAndMakeVisible(k);
    };
    setupKnob(knobAttack);
    setupKnob(knobDecay);
    setupKnob(knobSustain);
    setupKnob(knobRelease);
    setupKnob(knobDrive);
    setupKnob(knobGain);
    setupKnob(knobFine);

    attAttack  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "attack",    knobAttack.slider);
    attDecay   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "decay",     knobDecay.slider);
    attSustain = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "sustain",   knobSustain.slider);
    attRelease = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "release",   knobRelease.slider);
    attDrive   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "drive",     knobDrive.slider);
    attGain    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "gain",      knobGain.slider);
    attFine    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "finetune",  knobFine.slider);

    knobAttack.updateValueLabel();
    knobDecay.updateValueLabel();
    knobSustain.updateValueLabel();
    knobRelease.updateValueLabel();
    knobDrive.updateValueLabel();
    knobGain.updateValueLabel();
    knobFine.updateValueLabel();

    setSize(880, 720);
    startTimerHz(15);
    updateUI();
}

LusionTunerAudioProcessorEditor::~LusionTunerAudioProcessorEditor()
{
    stopTimer();
    attAttack.reset();
    attDecay.reset();
    attSustain.reset();
    attRelease.reset();
    attDrive.reset();
    attGain.reset();
    attFine.reset();
    knobAttack.slider.setLookAndFeel(nullptr);
    knobDecay.slider.setLookAndFeel(nullptr);
    knobSustain.slider.setLookAndFeel(nullptr);
    knobRelease.slider.setLookAndFeel(nullptr);
    knobDrive.slider.setLookAndFeel(nullptr);
    knobGain.slider.setLookAndFeel(nullptr);
    knobFine.slider.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

//==============================================================================
void LusionTunerAudioProcessorEditor::timerCallback()
{
    // ── Apply pending target key/octave set by the audio thread after detection ─
    // Must be called on the message thread.
    {
        int k = proc.pendingTargetKey.exchange(-1);
        int o = proc.pendingTargetOctave.exchange(-1);
        if (k >= 0) proc.setTargetKey(k);
        if (o >= 0) proc.setTargetOctave(o);
    }

    if (proc.isLoadingInProgress())
    {
        spinAngle += 0.15f;
        repaint();
        return;
    }

    // ── Load waveform only ONCE per sample ────────────────────────────────────
    if (proc.isSampleLoaded() && !waveformLoaded)
    {
        const auto* buf = proc.getLoadedBuffer();
        if (buf != nullptr)
        {
            waveDisplay.loadBuffer(buf);
            waveformLoaded = true;
        }
    }

    if (!proc.isSampleLoaded())
        waveformLoaded = false;

    updateUI();
}

void LusionTunerAudioProcessorEditor::updateUI()
{
    const bool         detected = proc.isPitchDetected();
    const bool         loaded   = proc.isSampleLoaded();
    const juce::String dn       = proc.getDetectedNote();
    const float        dhz      = proc.getDetectedHz();
    const juce::String tn       = proc.getTargetNote();
    const float        thz      = proc.getTargetHz();
    const int          shift    = proc.getSemitoneShift();
    const int          keyIdx   = proc.getTargetKeyIndex();
    const int          oct      = proc.getTargetOctave();

    // ── Big pitch display ─────────────────────────────────────────────────────
    if (detected && dn.isNotEmpty() && dn != "?")
    {
        // Split note name from octave number (e.g. "A#1" → name="A#", octave="1")
        // The octave part is everything after the last letter/# in the string.
        // noteName format: "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" + octave digits
        juce::String notePart = dn;
        juce::String octPart;
        // Find where digits (and minus) start from the end
        int splitPos = dn.length();
        while (splitPos > 0)
        {
            juce::juce_wchar c = dn[splitPos - 1];
            if (juce::CharacterFunctions::isDigit(c) || c == '-')
                --splitPos;
            else
                break;
        }
        notePart = dn.substring(0, splitPos);
        octPart  = dn.substring(splitPos);

        bigNoteLbl.setText(notePart, juce::dontSendNotification);
        bigNoteLbl.setColour(juce::Label::textColourId, juce::Colours::white);
        bigOctLbl.setText(octPart, juce::dontSendNotification);
        bigOctLbl.setColour(juce::Label::textColourId, juce::Colour(C_ORANGE));
    }
    else
    {
        bigNoteLbl.setText("--", juce::dontSendNotification);
        bigNoteLbl.setColour(juce::Label::textColourId, juce::Colour(C_DIM));
        bigOctLbl.setText("", juce::dontSendNotification);
    }

    // ── Status badge ──────────────────────────────────────────────────────────
    pitchBadge.setText(
        detected ? "  PITCH DETECTED"
                 : (proc.isLoadingInProgress() ? "  ANALYSING..."
                 : (loaded ? "  PITCH NOT FOUND" : "  DROP SAMPLE TO DETECT")),
        juce::dontSendNotification);
    pitchBadge.setColour(juce::Label::textColourId,
        detected ? juce::Colour(C_GREEN) : juce::Colour(C_DIM));

    detHzLbl.setText(detected ? juce::String(dhz, 1) + " Hz" : "--- Hz",
        juce::dontSendNotification);
    detHzLbl.setColour(juce::Label::textColourId,
        detected ? juce::Colour(C_TEXT) : juce::Colour(C_DIM));

    // ── Target info ───────────────────────────────────────────────────────────
    tgtKeyLbl.setText(tn, juce::dontSendNotification);
    tgtHzLbl.setText(juce::String(thz, 1) + " Hz", juce::dontSendNotification);
    semShiftLbl.setText(juce::String(shift) + " st", juce::dontSendNotification);
    semShiftLbl.setColour(juce::Label::textColourId,
        shift == 0 ? juce::Colour(C_GREEN) : juce::Colour(C_PURPLE));
    inTuneLbl.setVisible(shift == 0 && detected);

    // ── Status row ────────────────────────────────────────────────────────────
    stDetLbl.setText(detected ? dn : "--", juce::dontSendNotification);
    stTgtLbl.setText(tn, juce::dontSendNotification);
    stShiftLbl.setText(juce::String(shift) + " st", juce::dontSendNotification);
    stHzLbl.setText(juce::String(thz, 1) + " Hz", juce::dontSendNotification);

    // ── Manual root correction controls ──────────────────────────────────────
    const bool showRootEdit = loaded;
    rootDownBtn.setVisible(showRootEdit);
    rootUpBtn.setVisible(showRootEdit);
    rootEditLbl.setVisible(showRootEdit);
    rootClearBtn.setVisible(showRootEdit);

    if (showRootEdit)
    {
        // Sync editingRoot if it was reset externally (e.g. new sample loaded)
        const int manualRoot = proc.getManualRoot();
        if (manualRoot < 0) editingRoot = -1;

        const int displayRoot = (editingRoot >= 0) ? editingRoot
                              : proc.getDetectedMidiNote();
        if (displayRoot >= 0)
        {
            juce::String noteName = PitchDetector::midiNoteToName(displayRoot);
            rootEditLbl.setText(noteName, juce::dontSendNotification);
            rootEditLbl.setColour(juce::Label::textColourId,
                editingRoot >= 0 ? juce::Colour(C_ORANGE) : juce::Colour(C_GREEN));
        }
        else
        {
            rootEditLbl.setText("?", juce::dontSendNotification);
            rootEditLbl.setColour(juce::Label::textColourId, juce::Colour(C_DIM));
        }
        rootClearBtn.setEnabled(editingRoot >= 0 || manualRoot >= 0);
    }

    // ── Keyboard & octave sync ────────────────────────────────────────────────
    keyStrip.setSelectedKey(keyIdx);

    if (oct != currentOctave)
    {
        currentOctave = oct;
        for (int j = 0; j < 6; ++j)
            octBtn[j].setToggleState(j == oct, juce::dontSendNotification);
    }

    repaint();
}

//==============================================================================
bool LusionTunerAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (auto& f : files)
    {
        juce::String ext = juce::File(f).getFileExtension().toLowerCase();
        if (ext == ".wav" || ext == ".aiff" || ext == ".aif"
            || ext == ".flac" || ext == ".mp3" || ext == ".ogg")
            return true;
    }
    return false;
}

void LusionTunerAudioProcessorEditor::fileDragEnter(const juce::StringArray&, int, int)
{
    isDragOver = true;
    waveDisplay.isDragTarget = true;
    repaint();
}

void LusionTunerAudioProcessorEditor::fileDragExit(const juce::StringArray&)
{
    isDragOver = false;
    waveDisplay.isDragTarget = false;
    repaint();
}

void LusionTunerAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    isDragOver = false;
    waveDisplay.isDragTarget = false;
    waveformLoaded = false;
    editingRoot    = -1;
    if (files.size() > 0)
    {
        const juce::File f(files[0]);
        if (f.existsAsFile())
            proc.loadSampleFromFile(f);
    }
    repaint();
}

//==============================================================================
void LusionTunerAudioProcessorEditor::resized()
{
    const int W = getWidth();
    int y = 0;
    y += 46;

    const int pad   = 12;
    const int split = 420;
    const int topH  = 288;
    const int topY  = y + pad;

    pitchBadge.setBounds(pad + 14, topY + 14, 260, 18);
    bigNoteLbl.setBounds(pad + 14, topY + 30, 160, 96);
    bigOctLbl.setBounds(pad + 14 + 150, topY + 80, 60, 46);
    detHzLbl.setBounds(pad + 14, topY + 128, 200, 18);

    const int icY = topY + 155;
    const int icW = (split - pad * 2 - 28 - 8) / 2;
    tgtKeyLbl.setBounds(pad + 18, icY + 18, icW, 28);
    tgtHzLbl.setBounds(pad + 18 + icW + 8, icY + 18, icW, 22);
    semShiftLbl.setBounds(pad + 18, icY + 52, icW, 28);
    inTuneLbl.setBounds(pad + 18 + icW + 8, icY + 52, icW + 20, 18);

    waveDisplay.setBounds(split, topY, W - split - pad, topH - pad);

    y += topH + pad;

    const int tuneH  = 160;
    const int obW = 36, obH = 24, obGap = 4;
    int obX = W - pad - 6 * (obW + obGap);
    for (int i = 0; i < 6; ++i)
        octBtn[i].setBounds(obX + i * (obW + obGap), y + 8, obW, obH);

    keyStrip.setBounds(pad, y + 40, W - pad * 2, tuneH - 60);

    const int rowY  = y + tuneH - 38;
    const int colW  = 72;
    int rx = pad + 8;
    stDetLbl.setBounds(rx, rowY, colW, 18);      rx += colW + 10;
    stTgtLbl.setBounds(rx, rowY, colW, 18);      rx += colW + 10;
    stShiftLbl.setBounds(rx, rowY, colW, 18);    rx += colW + 10;
    stHzLbl.setBounds(rx, rowY, colW + 20, 18);

    y += tuneH;

    const int botBarH = 68;
    const int btnH = 44, btnW = 160;
    loadBtn.setBounds(pad, y + 12, btnW, btnH);
    exportBtn.setBounds(pad + btnW + 8, y + 12, btnW, btnH);
    playBtn.setBounds(pad + (btnW + 8) * 2, y + 12, 110, btnH);

    // Manual root controls — right side of button bar
    const int rcX = pad + (btnW + 8) * 2 + 110 + 16;
    rootDownBtn.setBounds(rcX,          y + 18, 28, 32);
    rootEditLbl.setBounds(rcX + 32,     y + 18, 72, 32);
    rootUpBtn.setBounds(rcX + 108,      y + 18, 28, 32);
    rootClearBtn.setBounds(rcX + 140,   y + 18, 46, 32);

    y += botBarH;

    const int knobRowH = getHeight() - y - pad;
    const int knobSize = juce::jmin(80, knobRowH - 28);
    const int knobW    = knobSize + 4;
    const int totalKnobsW = 7 * (knobW + 8) + 12;
    int kx = (W - totalKnobsW) / 2;
    const int ky = y + (knobRowH - (knobSize + 28)) / 2;

    auto placeKnob = [&](LusionKnob& k)
    {
        k.setBounds(kx, ky, knobW, knobSize + 28);
        kx += knobW + 8;
    };
    placeKnob(knobAttack);
    placeKnob(knobDecay);
    placeKnob(knobSustain);
    placeKnob(knobRelease);
    kx += 12;
    placeKnob(knobDrive);
    placeKnob(knobGain);
    placeKnob(knobFine);
}

//==============================================================================
void LusionTunerAudioProcessorEditor::paint(juce::Graphics& g)
{
    const int W = getWidth(), H = getHeight();
    g.fillAll(juce::Colour(C_BG));

    paintTitleBar(g);
    g.setColour(juce::Colour(C_BORDER));
    g.drawHorizontalLine(46, 0.f, (float)W);

    const int pad   = 12;
    const int split = 420;
    const int topY  = 46 + pad;
    const int topH  = 288;

    // ── Left card (detected pitch) ────────────────────────────────────────────
    paintCard(g, { pad, topY, split - pad * 2, topH - pad });

    // ── Right card (waveform) — highlight on drag ─────────────────────────────
    if (isDragOver)
    {
        g.setColour(juce::Colour(C_ORANGE).withAlpha(0.12f));
        g.fillRoundedRectangle((float)split, (float)topY,
            (float)(W - split - pad), (float)(topH - pad), 8.f);
        g.setColour(juce::Colour(C_ORANGE));
        g.drawRoundedRectangle((float)split + 0.5f, (float)topY + 0.5f,
            (float)(W - split - pad) - 1.f, (float)(topH - pad) - 1.f, 8.f, 2.f);
    }

    g.setFont(juce::Font("Helvetica Neue", 8.f, juce::Font::plain));
    g.setColour(juce::Colour(C_DIM));
    g.drawText("WAVEFORM", split + 8, topY + 6, 80, 12, juce::Justification::centredLeft);

    // ── Sub-cards (target key / hz / semitone shift) ──────────────────────────
    const int icY = topY + 155;
    const int icW = (split - pad * 2 - 28 - 8) / 2;
    paintCard(g, { pad + 14, icY, icW,       88 }, 6.f);
    paintCard(g, { pad + 14 + icW + 8, icY, icW + 8, 88 }, 6.f);

    g.setFont(juce::Font("Helvetica Neue", 8.f, juce::Font::plain));
    g.setColour(juce::Colour(C_DIM));
    g.drawText("TARGET KEY",    pad + 18, icY + 6, icW, 12, juce::Justification::centredLeft);
    g.drawText("TARGET HZ",     pad + 18 + icW + 8, icY + 6, icW, 12, juce::Justification::centredLeft);
    g.drawText("SEMITONE SHIFT", pad + 18, icY + 46, icW, 12, juce::Justification::centredLeft);

    // ── Semitone offset bar ───────────────────────────────────────────────────
    {
        const int   bx    = pad + 14, by = topY + 150, bw = split - pad * 2 - 28, bh = 5;
        const int   shift = proc.getSemitoneShift();
        const float offset = juce::jlimit(0.f, 1.f, 0.5f + (float)shift / 12.f);
        g.setColour(juce::Colour(0xFF1A1A22));
        g.fillRoundedRectangle((float)bx, (float)by, (float)bw, (float)bh, 2.f);
        const float cx = bx + bw * 0.5f, px = bx + bw * offset;
        if (std::abs(px - cx) > 0.5f)
        {
            g.setColour(juce::Colour(C_ORANGE));
            g.fillRoundedRectangle(juce::jmin(cx, px), (float)by, std::abs(px - cx), (float)bh, 2.f);
        }
        g.setColour(juce::Colours::white);
        g.fillEllipse(px - 5.f, (float)(by - 3), 11.f, 11.f);
        g.setColour(juce::Colour(C_DIM));
        g.fillRect((int)cx - 1, by - 2, 2, bh + 4);
    }

    // ── Tune To Key card ─────────────────────────────────────────────────────
    const int tuneY = topY + topH + pad;
    const int tuneH = 160;
    paintCard(g, { pad, tuneY, W - pad * 2, tuneH });

    g.setColour(juce::Colour(C_ORANGE));
    g.fillEllipse((float)(pad + 12), (float)(tuneY + 11), 8.f, 8.f);
    g.setFont(juce::Font("Helvetica Neue", 10.f, juce::Font::bold));
    g.setColour(juce::Colour(C_TEXT));
    g.drawText("TUNE TO KEY", pad + 26, tuneY + 7, 120, 16, juce::Justification::centredLeft);
    g.setFont(juce::Font("Helvetica Neue", 9.f, juce::Font::plain));
    g.setColour(juce::Colour(C_DIM));
    g.drawText("OCTAVE", W - pad - 6 * 40 - 52, tuneY + 11, 48, 14, juce::Justification::centredRight);

    // ── Status row ────────────────────────────────────────────────────────────
    const int rowY = tuneY + tuneH - 38;
    g.setColour(juce::Colour(0xFF0C0C10));
    g.fillRoundedRectangle((float)(pad + 2), (float)(rowY - 4), (float)(W - pad * 2 - 4), 30.f, 5.f);

    const int colW = 72;
    int rx = pad + 8;
    static const char* sLabels[] = { "DETECTED","TARGET","SHIFT","TARGET HZ" };
    for (int i = 0; i < 4; ++i)
    {
        g.setFont(juce::Font("Helvetica Neue", 7.5f, juce::Font::plain));
        g.setColour(juce::Colour(C_DIM));
        g.drawText(sLabels[i], rx, rowY - 14, colW + (i == 3 ? 20 : 0), 12, juce::Justification::centredLeft);
        rx += colW + (i == 2 ? 20 : 0) + 10;
    }

    // ── Button bar ────────────────────────────────────────────────────────────
    const int botY    = tuneY + tuneH + pad;
    const int botBarH = 68;
    g.setColour(juce::Colour(C_SURF));
    g.fillRect(0, botY, W, botBarH);
    g.setColour(juce::Colour(C_BORDER));
    g.drawHorizontalLine(botY, 0.f, (float)W);
    g.setFont(juce::Font("Helvetica Neue", 9.f, juce::Font::plain));
    g.setColour(juce::Colour(C_DIM));
    g.drawText("WAV  -  AIFF  -  FLAC  -  MP3", W - 200, botY + 12, 188, 14, juce::Justification::centredRight);

    // Root note correction label
    if (proc.isSampleLoaded())
    {
        const int rcX = pad + (160 + 8) * 2 + 110 + 16;
        g.setFont(juce::Font("Helvetica Neue", 7.5f, juce::Font::bold));
        g.setColour(juce::Colour(C_DIM));
        g.drawText("ROOT NOTE", rcX, botY + 8, 200, 11, juce::Justification::centredLeft);
    }

    // ── DSP knob area ─────────────────────────────────────────────────────────
    const int dspY = botY + botBarH;
    const int dspH = H - dspY;
    g.setColour(juce::Colour(C_BG));
    g.fillRect(0, dspY, W, dspH);
    g.setColour(juce::Colour(C_BORDER));
    g.drawHorizontalLine(dspY, 0.f, (float)W);

    g.setColour(juce::Colour(C_CARD));
    g.fillRoundedRectangle((float)pad, (float)(dspY + 4), (float)(W - pad * 2), (float)(dspH - 8), 8.f);
    g.setColour(juce::Colour(C_BORDER));
    g.drawRoundedRectangle((float)pad + 0.5f, (float)(dspY + 4) + 0.5f,
        (float)(W - pad * 2) - 1.f, (float)(dspH - 8) - 1.f, 8.f, 1.f);

    const int sepX = W / 2 - 4;
    g.setColour(juce::Colour(C_BORDER));
    g.drawVerticalLine(sepX, (float)(dspY + 8), (float)(H - 8));

    g.setFont(juce::Font("Helvetica Neue", 8.f, juce::Font::bold));
    g.setColour(juce::Colour(C_DIM));
    g.drawText("ENVELOPE",     pad + 14,    dspY + 10, 80, 11, juce::Justification::centredLeft);
    g.drawText("SOUND SHAPING", sepX + 14,  dspY + 10, 120, 11, juce::Justification::centredLeft);

    // ── Loading spinner ───────────────────────────────────────────────────────
    if (proc.isLoadingInProgress())
    {
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRoundedRectangle((float)split, (float)topY,
            (float)(W - split - pad), (float)(topH - pad), 8.f);
        const float cx = split + (W - split - pad) * 0.5f;
        const float cy = topY + (topH - pad) * 0.5f;
        for (int i = 0; i < 8; ++i)
        {
            const float angle = spinAngle + i * juce::MathConstants<float>::twoPi / 8.f;
            g.setColour(juce::Colour(C_ORANGE).withAlpha((float)(i + 1) / 8.f));
            g.drawLine(cx + 18.f * std::cos(angle), cy + 18.f * std::sin(angle),
                cx + 28.f * std::cos(angle), cy + 28.f * std::sin(angle), 2.5f);
        }
        g.setFont(juce::Font("Helvetica Neue", 11.f, juce::Font::plain));
        g.setColour(juce::Colour(C_DIM));
        g.drawText("Analysing pitch...", (int)(cx - 80), (int)(cy + 36), 160, 16, juce::Justification::centred);
    }
}

void LusionTunerAudioProcessorEditor::paintCard(juce::Graphics& g, juce::Rectangle<int> b, float r)
{
    g.setColour(juce::Colour(C_CARD));
    g.fillRoundedRectangle(b.toFloat(), r);
    g.setColour(juce::Colour(C_BORDER));
    g.drawRoundedRectangle(b.toFloat().reduced(0.5f), r, 1.f);
}

void LusionTunerAudioProcessorEditor::paintTitleBar(juce::Graphics& g)
{
    const int W = getWidth();
    g.setColour(juce::Colour(C_SURF));
    g.fillRect(0, 0, W, 46);

    const int logoX = 8, logoY = 8, logoSz = 30;
    if (logoImage.isValid())
    {
        g.drawImageWithin(logoImage, logoX, logoY, logoSz, logoSz,
            juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }
    else
    {
        const juce::Colour cyTop(0xFF00EEFF), cyBot(0xFF0099CC);
        const float barW = 3.f, gap = 1.5f;
        const float totalW = 7 * (barW + gap) - gap;
        const float startX = logoX + (logoSz - totalW) * 0.5f;
        static const float heights[] = { 0.35f, 0.55f, 0.75f, 1.0f, 0.75f, 0.55f, 0.35f };
        const float maxH = logoSz * 0.85f, baseY = logoY + logoSz - 2.f;
        for (int i = 0; i < 7; ++i)
        {
            float bh = maxH * heights[i];
            float bx = startX + i * (barW + gap);
            float by = baseY - bh;
            g.setGradientFill(juce::ColourGradient(cyTop, bx, by, cyBot, bx, baseY, false));
            juce::Rectangle<float> bar(bx, by, barW, bh);
            g.fillRoundedRectangle(bar, barW * 0.5f);
            g.setColour(cyTop.withAlpha(0.6f));
            g.drawRoundedRectangle(bar.reduced(0.25f), barW * 0.5f, 0.5f);
        }
    }

    g.setFont(juce::Font("Helvetica Neue", 14.f, juce::Font::bold));
    g.setColour(juce::Colours::white);
    g.drawText("LUSION", 44, 0, 60, 46, juce::Justification::centredLeft);
    g.setColour(juce::Colour(C_ORANGE));
    g.drawText("TUNER", 98, 0, 56, 46, juce::Justification::centredLeft);
    g.setFont(juce::Font("Helvetica Neue", 9.f, juce::Font::plain));
    g.setColour(juce::Colour(C_DIM));
    g.drawText("v1.0", 152, 0, 36, 46, juce::Justification::centredLeft);
}