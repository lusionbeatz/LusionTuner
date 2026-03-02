#pragma once
#include <JuceHeader.h>

//==============================================================================
/**  KeyboardDisplay — chromatic key strip (C through B) with selected key
     highlighted in orange, sharps/flats shown as small badges on white keys.

     selectedKey: 0=C, 1=C#, 2=D, 3=D#, 4=E, 5=F, 6=F#, 7=G, 8=G#, 9=A, 10=A#, 11=B
*/
class KeyboardDisplay : public juce::Component
{
public:
    KeyboardDisplay() = default;

    void setSelectedKey (int midiNoteInOctave)   // 0-11
    {
        selectedKey = midiNoteInOctave % 12;
        repaint();
    }

    void setOctave (int oct) { octave = oct; repaint(); }

    //──────────────────────────────────────────────────────────────────────────
    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (1.f);
        const float W = bounds.getWidth();
        const float H = bounds.getHeight();
        const float keyW = W / 7.f;
        const float blackH = H * 0.58f;
        const float blackW = keyW * 0.62f;

        // Note names for each white key (indices 0-6 → C D E F G A B)
        static const int whiteToMidi[7] = { 0, 2, 4, 5, 7, 9, 11 };
        static const char* noteNames[7] = { "C","D","E","F","G","A","B" };
        // Black key positions (after which white key, offset factor)
        // C# after C(0), D# after D(1), F# after F(3), G# after G(4), A# after A(5)
        static const int blackAfter[5] = { 0, 1, 3, 4, 5 };
        static const int blackMidi[5]  = { 1, 3, 6, 8, 10 };

        // ── White keys ───────────────────────────────────────────────────────
        for (int i = 0; i < 7; ++i)
        {
            int midi  = whiteToMidi[i];
            bool sel  = (midi == selectedKey);
            float kx  = bounds.getX() + i * keyW;

            // Card shape
            juce::Rectangle<float> key (kx + 1.f, bounds.getY(), keyW - 2.f, H);

            // Background
            juce::Colour fill = sel ? juce::Colour (0xFFFF6B1A)
                                    : juce::Colour (0xFF1E1E28);
            g.setColour (fill);
            g.fillRoundedRectangle (key, 6.f);

            // Border
            g.setColour (juce::Colour (sel ? 0xFFFF6B1A : 0xFF2A2A35));
            g.drawRoundedRectangle (key.reduced(0.5f), 6.f, 1.f);

            // Sharp badge (top right of white key, if this white key has a black key after it)
            for (int b = 0; b < 5; ++b)
            {
                if (blackAfter[b] == i)
                {
                    // Draw the sharp label on the white key's top-right corner
                    float bx = kx + keyW * 0.55f;
                    float by = bounds.getY() + 4.f;
                    juce::Rectangle<float> badge (bx, by, keyW * 0.4f, 13.f);
                    // Sharp badge uses note name like "A#"
                    juce::String sharpName = juce::String (noteNames[i]) + "#";
                    g.setColour (juce::Colour (0xFF3A3A4A));
                    g.fillRoundedRectangle (badge, 3.f);
                    g.setFont (juce::Font ("Helvetica Neue", 7.f, juce::Font::plain));
                    g.setColour (juce::Colour (0xFF6A6A80));
                    g.drawText (sharpName, badge.toNearestInt(), juce::Justification::centred);
                    break;
                }
            }

            // Centre dot indicator
            float dotY = bounds.getBottom() - 12.f;
            g.setColour (sel ? juce::Colours::white : juce::Colour (0xFF3A3A4A));
            g.fillEllipse (kx + keyW * 0.5f - 3.f, dotY, 6.f, 6.f);

            // Note name label
            g.setFont (juce::Font ("Helvetica Neue", 10.f, juce::Font::bold));
            g.setColour (sel ? juce::Colours::white : juce::Colour (0xFF6A6A80));
            g.drawText (noteNames[i], (int)(kx), (int)(bounds.getBottom() - 26.f),
                        (int)keyW, 14, juce::Justification::centred);

            // Glow on selected
            if (sel)
            {
                g.setColour (juce::Colour (0xFFFF6B1A).withAlpha (0.18f));
                g.fillRoundedRectangle (key.expanded(4.f), 8.f);
            }
        }

        // ── Black keys ───────────────────────────────────────────────────────
        for (int b = 0; b < 5; ++b)
        {
            bool sel  = (blackMidi[b] == selectedKey);
            float kx  = bounds.getX() + (blackAfter[b] + 1) * keyW - blackW * 0.5f;

            juce::Rectangle<float> key (kx, bounds.getY() + 1.f, blackW, blackH);

            juce::Colour fill = sel ? juce::Colour (0xFFFF6B1A)
                                    : juce::Colour (0xFF111116);
            g.setColour (fill);
            g.fillRoundedRectangle (key, 4.f);
            g.setColour (juce::Colour (sel ? 0xFFFF8844 : 0xFF1A1A24));
            g.drawRoundedRectangle (key.reduced(0.5f), 4.f, 1.f);

            if (sel)
            {
                g.setColour (juce::Colour (0xFFFF6B1A).withAlpha (0.2f));
                g.fillRoundedRectangle (key.expanded(4.f), 6.f);
            }
        }
    }

private:
    int selectedKey = 9;  // A
    int octave      = 1;
};
