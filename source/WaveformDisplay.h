#pragma once
#include <JuceHeader.h>

//==============================================================================
/**  WaveformDisplay — draws a decaying 808-style waveform.
     When playing, it animates the playhead scrolling.
     Feed it audio data via setSampleData() after loading.         */
class WaveformDisplay : public juce::Component,
                         public juce::Timer
{
public:
    WaveformDisplay()
    {
        generateDemoWave();
        startTimerHz (60);
    }

    ~WaveformDisplay() override { stopTimer(); }

    //──────────────────────────────────────────────────────────────────────────
    void setSampleData (const juce::AudioBuffer<float>& buf)
    {
        // Downsample to display resolution
        wavePoints.clear();
        int totalSamples = buf.getNumSamples();
        int step = juce::jmax (1, totalSamples / 1024);
        for (int i = 0; i < totalSamples; i += step)
            wavePoints.push_back (buf.getSample (0, i));
        repaint();
    }

    void setPlaying (bool playing)
    {
        isPlaying = playing;
        if (!playing) playheadPos = 0.f;
    }

    //──────────────────────────────────────────────────────────────────────────
    void timerCallback() override
    {
        if (isPlaying)
        {
            playheadPos += 0.004f;
            if (playheadPos > 1.f) playheadPos = 0.f;
            repaint();
        }
    }

    //──────────────────────────────────────────────────────────────────────────
    void paint (juce::Graphics& g) override
    {
        const auto W = (float) getWidth();
        const auto H = (float) getHeight();
        const auto hh = H * 0.5f;

        //── Background ────────────────────────────────────────────────────────
        g.setColour (juce::Colour (0xFF0F0F14));
        g.fillRoundedRectangle (0, 0, W, H, 8.f);

        //── Grid ──────────────────────────────────────────────────────────────
        g.setColour (juce::Colours::white.withAlpha (0.025f));
        for (float x = 0; x < W; x += 38.f)
            g.drawVerticalLine ((int)x, 0, H);
        for (float y = 0; y < H; y += 38.f)
            g.drawHorizontalLine ((int)y, 0, W);

        //── Centre line ───────────────────────────────────────────────────────
        g.setColour (juce::Colour (0xFFFF6B1A).withAlpha (0.08f));
        g.drawHorizontalLine ((int)hh, 0, W);

        if (wavePoints.empty()) return;

        //── Waveform fill ─────────────────────────────────────────────────────
        const int n = (int) wavePoints.size();
        juce::Path fill;
        fill.startNewSubPath (0, hh);

        for (int i = 0; i < n; ++i)
        {
            float x = W * ((float)i / n);
            float y = hh - wavePoints[i] * hh * 0.9f;
            i == 0 ? fill.startNewSubPath (x, y) : fill.lineTo (x, y);
        }
        fill.lineTo (W, hh);
        fill.closeSubPath();

        juce::ColourGradient fillGrad (juce::Colour (0xFFFF6B1A).withAlpha (0.18f), 0, 0,
                                        juce::Colour (0xFFFF6B1A).withAlpha (0.0f), W, 0, false);
        g.setGradientFill (fillGrad);
        g.fillPath (fill);

        //── Waveform stroke ───────────────────────────────────────────────────
        juce::Path stroke;
        for (int i = 0; i < n; ++i)
        {
            float x = W * ((float)i / n);
            float y = hh - wavePoints[i] * hh * 0.9f;
            i == 0 ? stroke.startNewSubPath (x, y) : stroke.lineTo (x, y);
        }

        juce::ColourGradient strokeGrad (juce::Colour (0xFFFF6B1A).withAlpha (0.9f), 0, 0,
                                          juce::Colour (0xFFFF6B1A).withAlpha (0.12f), W, 0, false);
        g.setGradientFill (strokeGrad);
        g.strokePath (stroke, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));

        //── Playhead ──────────────────────────────────────────────────────────
        if (isPlaying)
        {
            float px = W * playheadPos;

            // Glow
            juce::ColourGradient pg (juce::Colour (0xFFFF6B1A).withAlpha (0.35f), px, 0,
                                      juce::Colours::transparentBlack, px + 18.f, 0, false);
            g.setGradientFill (pg);
            g.fillRect (juce::Rectangle<float> (px - 8, 0, 28, H));

            // Playhead line
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawVerticalLine ((int)px, 0, H);

            // Top/bottom triangles
            juce::Path topTri;
            topTri.addTriangle (px - 5, 0, px + 5, 0, px, 8);
            juce::Path botTri;
            botTri.addTriangle (px - 5, H, px + 5, H, px, H - 8);
            g.setColour (juce::Colour (0xFFFF6B1A));
            g.fillPath (topTri);
            g.fillPath (botTri);
        }

        //── Border ────────────────────────────────────────────────────────────
        g.setColour (juce::Colour (0xFF2A2A35));
        g.drawRoundedRectangle (0.5f, 0.5f, W - 1, H - 1, 8.f, 1.f);
    }

private:
    std::vector<float> wavePoints;
    float playheadPos = 0.f;
    bool  isPlaying   = false;

    void generateDemoWave()
    {
        // 808-style decaying sine envelope
        const int n = 1024;
        wavePoints.resize (n);
        juce::Random rng;
        for (int i = 0; i < n; ++i)
        {
            float t        = (float)i / n;
            float envelope = std::pow (1.f - t, 1.5f);
            float freq     = 18.f + t * 6.f;          // slight pitch drop
            float noise    = (rng.nextFloat() - 0.5f) * 0.04f;
            wavePoints[i]  = std::sin ((float)i / freq) * envelope + noise;
        }
    }
};
