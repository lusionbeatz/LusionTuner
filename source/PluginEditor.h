#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "LusionLookAndFeel.h"

// ── Waveform display ──────────────────────────────────────────────────────────
class WaveDisplay : public juce::Component, public juce::Timer
{
public:
    WaveDisplay() { startTimerHz(60); }
    ~WaveDisplay() override { stopTimer(); }

    void loadBuffer(const juce::AudioBuffer<float>* buf)
    {
        peaks.clear();
        if (buf == nullptr || buf->getNumSamples() == 0) { repaint(); return; }
        const int n    = buf->getNumSamples();
        const int step = juce::jmax(1, n / 512);
        for (int i = 0; i < n; i += step)
        {
            float pk = 0.f;
            for (int j = i; j < juce::jmin(i + step, n); ++j)
                pk = juce::jmax(pk, std::abs(buf->getSample(0, j)));
            peaks.push_back(pk);
        }
        repaint();
    }

    void setPlaying(bool p) { playing = p; if (!p) playhead = 0.f; }

    void timerCallback() override
    {
        if (playing) { playhead += 0.003f; if (playhead > 1.f) playhead = 0.f; repaint(); }
    }

    void paint(juce::Graphics& g) override
    {
        const float W = (float)getWidth(), H = (float)getHeight(), hh = H * 0.5f;

        g.setColour(juce::Colour(0xFF0C0C10));
        g.fillRoundedRectangle(0, 0, W, H, 8.f);

        g.setColour(juce::Colours::white.withAlpha(0.03f));
        for (float x = 0; x < W; x += 40.f) g.drawVerticalLine((int)x, 0, H);
        for (float y = 0; y < H; y += 40.f) g.drawHorizontalLine((int)y, 0, W);

        g.setColour(juce::Colour(0xFFFF6B1A).withAlpha(0.06f));
        g.drawHorizontalLine((int)hh, 0, W);

        if (!peaks.empty())
        {
            const int n = (int)peaks.size();
            juce::Path fill;
            for (int i = 0; i < n; ++i)
            {
                float x = W * (float)i / n;
                float y = hh - peaks[i] * hh * 0.88f;
                if (i == 0) fill.startNewSubPath(x, y); else fill.lineTo(x, y);
            }
            for (int i = n - 1; i >= 0; --i)
                fill.lineTo(W * (float)i / n, hh + peaks[i] * hh * 0.88f);
            fill.closeSubPath();
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xFFFF6B1A).withAlpha(0.22f), 0, 0,
                juce::Colour(0xFFFF6B1A).withAlpha(0.04f), W, 0, false));
            g.fillPath(fill);

            juce::Path stroke;
            for (int i = 0; i < n; ++i)
            {
                float x = W * (float)i / n, y = hh - peaks[i] * hh * 0.88f;
                if (i == 0) stroke.startNewSubPath(x, y); else stroke.lineTo(x, y);
            }
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xFFFF6B1A).withAlpha(0.95f), 0, 0,
                juce::Colour(0xFFFF6B1A).withAlpha(0.15f), W, 0, false));
            g.strokePath(stroke, juce::PathStrokeType(1.6f));

            juce::Path stroke2;
            for (int i = 0; i < n; ++i)
            {
                float x = W * (float)i / n, y = hh + peaks[i] * hh * 0.88f;
                if (i == 0) stroke2.startNewSubPath(x, y); else stroke2.lineTo(x, y);
            }
            g.strokePath(stroke2, juce::PathStrokeType(1.6f));
        }
        else
        {
            g.setFont(juce::Font("Helvetica Neue", 13.f, juce::Font::plain));
            g.setColour(juce::Colour(0xFF3A3A50));
            g.drawText("Drop 808 / Kick sample here", 0, 0, (int)W, (int)H, juce::Justification::centred);
        }

        if (playing)
        {
            float px = W * playhead;
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xFFFF6B1A).withAlpha(0.4f), px, 0,
                juce::Colours::transparentBlack, px + 20.f, 0, false));
            g.fillRect(juce::Rectangle<float>(px - 6, 0, 26, H));
            g.setColour(juce::Colours::white.withAlpha(0.9f));
            g.drawVerticalLine((int)px, 0, H);
            juce::Path tri;
            tri.addTriangle(px - 5, 0, px + 5, 0, px, 8);
            g.setColour(juce::Colour(0xFFFF6B1A));
            g.fillPath(tri);
        }

        if (isDragTarget)
        {
            g.setColour(juce::Colour(0xFFFF6B1A).withAlpha(0.6f));
            g.drawRoundedRectangle(1.f, 1.f, W - 2.f, H - 2.f, 8.f, 2.f);
        }
        else
        {
            g.setColour(juce::Colour(0xFF2A2A35));
            g.drawRoundedRectangle(0.5f, 0.5f, W - 1, H - 1, 8.f, 1.f);
        }
    }

    bool isDragTarget = false;

private:
    std::vector<float> peaks;
    float playhead = 0.f;
    bool  playing  = false;
};

// ── Keyboard strip ────────────────────────────────────────────────────────────
class KeyStrip : public juce::Component
{
public:
    std::function<void(int)> onKeySelected;
    int selectedKey = 9;

    void setSelectedKey(int k) { selectedKey = k % 12; repaint(); }

    void paint(juce::Graphics& g) override
    {
        const float W = (float)getWidth(), H = (float)getHeight();
        const float kw = W / 7.f, bh = H * 0.56f, bw = kw * 0.6f;
        static const int wMidi[7]  = { 0,2,4,5,7,9,11 };
        static const int bAfter[5] = { 0,1,3,4,5 };
        static const int bMidi[5]  = { 1,3,6,8,10 };
        static const char* wName[7] = { "C","D","E","F","G","A","B" };

        for (int i = 0; i < 7; ++i)
        {
            bool sel = (wMidi[i] == selectedKey);
            float kx = i * kw;
            juce::Rectangle<float> r(kx + 1.f, 0.f, kw - 2.f, H);
            g.setColour(sel ? juce::Colour(0xFFFF6B1A) : juce::Colour(0xFF1A1A22));
            g.fillRoundedRectangle(r, 6.f);
            g.setColour(sel ? juce::Colour(0xFFFF8844) : juce::Colour(0xFF2A2A38));
            g.drawRoundedRectangle(r.reduced(0.5f), 6.f, 1.f);
            if (sel) { g.setColour(juce::Colour(0xFFFF6B1A).withAlpha(0.15f)); g.fillRoundedRectangle(r.expanded(3.f), 8.f); }
            g.setFont(juce::Font("Helvetica Neue", 10.f, juce::Font::bold));
            g.setColour(sel ? juce::Colours::white : juce::Colour(0xFF5A5A70));
            g.drawText(wName[i], (int)kx, (int)(H - 22), (int)kw, 14, juce::Justification::centred);
            g.setColour(sel ? juce::Colours::white : juce::Colour(0xFF2E2E3E));
            g.fillEllipse(kx + kw * 0.5f - 3.f, H - 10.f, 6.f, 6.f);
        }
        for (int b = 0; b < 5; ++b)
        {
            bool sel = (bMidi[b] == selectedKey);
            float kx = (bAfter[b] + 1) * kw - bw * 0.5f;
            juce::Rectangle<float> r(kx, 1.f, bw, bh);
            g.setColour(sel ? juce::Colour(0xFFFF6B1A) : juce::Colour(0xFF0E0E14));
            g.fillRoundedRectangle(r, 4.f);
            g.setColour(sel ? juce::Colour(0xFFFF8844) : juce::Colour(0xFF1A1A24));
            g.drawRoundedRectangle(r.reduced(0.5f), 4.f, 1.f);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const float W = (float)getWidth(), H = (float)getHeight();
        const float kw = W / 7.f, bh = H * 0.56f, bw = kw * 0.6f;
        static const int bAfter[5] = { 0,1,3,4,5 };
        static const int bMidi[5]  = { 1,3,6,8,10 };
        static const int wMidi[7]  = { 0,2,4,5,7,9,11 };
        float mx = (float)e.x, my = (float)e.y;
        // Check black keys first (they overlap white keys visually)
        for (int b = 0; b < 5; ++b)
        {
            float kx = (bAfter[b] + 1) * kw - bw * 0.5f;
            if (mx >= kx && mx < kx + bw && my < bh)
            {
                selectedKey = bMidi[b];
                if (onKeySelected) onKeySelected(selectedKey);
                repaint();
                return;
            }
        }
        int wi = juce::jlimit(0, 6, (int)(mx / kw));
        selectedKey = wMidi[wi];
        if (onKeySelected) onKeySelected(selectedKey);
        repaint();
    }
};

// ── Labelled rotary knob ──────────────────────────────────────────────────────
class LusionKnob : public juce::Component
{
public:
    juce::Slider slider;
    juce::Label  label;
    juce::Label  valueLabel;

    LusionKnob(const juce::String& name, const juce::String& unit = "")
        : unitStr(unit)
    {
        slider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);

        label.setText(name, juce::dontSendNotification);
        label.setFont(juce::Font("Helvetica Neue", 9.f, juce::Font::bold));
        label.setColour(juce::Label::textColourId, juce::Colour(0xFF6A6A80));
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);

        valueLabel.setFont(juce::Font("Helvetica Neue", 9.f, juce::Font::plain));
        valueLabel.setColour(juce::Label::textColourId, juce::Colour(0xFF9A9AAA));
        valueLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(valueLabel);

        slider.onValueChange = [this]() { updateValueLabel(); };
    }

    void resized() override
    {
        const int W = getWidth(), H = getHeight();
        label.setBounds(0, 0, W, 14);
        slider.setBounds(4, 14, W - 8, W - 8);
        valueLabel.setBounds(0, 14 + (W - 8), W, 14);
    }

    void updateValueLabel()
    {
        double v = slider.getValue();
        juce::String text;
        if      (unitStr == "s")  text = juce::String(v, 2) + "s";
        else if (unitStr == "dB") text = juce::String(v, 1) + "dB";
        else if (unitStr == "ct") text = juce::String((int)v) + "ct";
        else                      text = juce::String(v, 2);
        valueLabel.setText(text, juce::dontSendNotification);
    }

private:
    juce::String unitStr;
};

// ── Main editor ───────────────────────────────────────────────────────────────
class LusionTunerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         public juce::Timer,
                                         public juce::FileDragAndDropTarget
{
public:
    explicit LusionTunerAudioProcessorEditor(LusionTunerAudioProcessor& p);
    ~LusionTunerAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray& files, int, int) override;

private:
    LusionTunerAudioProcessor& proc;
    LusionLookAndFeel laf;

    static const juce::uint32 C_BG     = 0xFF0A0A0C;
    static const juce::uint32 C_SURF   = 0xFF111114;
    static const juce::uint32 C_CARD   = 0xFF141418;
    static const juce::uint32 C_BORDER = 0xFF2A2A35;
    static const juce::uint32 C_ORANGE = 0xFFFF6B1A;
    static const juce::uint32 C_GREEN  = 0xFF00E5A0;
    static const juce::uint32 C_PURPLE = 0xFF9D6FFF;
    static const juce::uint32 C_TEXT   = 0xFFE8E8F0;
    static const juce::uint32 C_DIM    = 0xFF6A6A80;

    juce::Image logoImage;

    WaveDisplay waveDisplay;
    bool isDragOver    = false;
    bool waveformLoaded = false;

    juce::Label bigNoteLbl, bigOctLbl, pitchBadge, detHzLbl;
    juce::Label tgtKeyLbl, tgtHzLbl, semShiftLbl, inTuneLbl;
    juce::Label stDetLbl, stTgtLbl, stShiftLbl, stHzLbl;

    KeyStrip keyStrip;
    juce::TextButton octBtn[6];
    int currentOctave = 1;

    juce::TextButton loadBtn, exportBtn, playBtn;
    bool isPlaying = false;

    // ── Manual pitch correction ───────────────────────────────────────────────
    // Shows when a sample is loaded; lets the user nudge the detected root note
    // up/down by semitone and confirm it as the new root.
    juce::TextButton rootDownBtn { "-" };     // root note -1 semitone
    juce::TextButton rootUpBtn   { "+" };     // root note +1 semitone
    juce::TextButton rootClearBtn { "AUTO" }; // revert to auto-detected
    juce::Label      rootEditLbl;             // shows current editable root
    int              editingRoot = -1;        // -1 = not editing

    LusionKnob knobAttack  { "ATTACK",  "s"  };
    LusionKnob knobDecay   { "DECAY",   "s"  };
    LusionKnob knobSustain { "SUSTAIN", ""   };
    LusionKnob knobRelease { "RELEASE", "s"  };
    LusionKnob knobDrive   { "DRIVE",   ""   };
    LusionKnob knobGain    { "GAIN",    "dB" };
    LusionKnob knobFine    { "FINE",    "ct" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        attAttack, attDecay, attSustain, attRelease, attDrive, attGain, attFine;

    float spinAngle = 0.f;

    void paintCard(juce::Graphics& g, juce::Rectangle<int> b, float r = 8.f);
    void paintTitleBar(juce::Graphics& g);
    void updateUI();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LusionTunerAudioProcessorEditor)
};