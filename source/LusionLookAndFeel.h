#pragma once
#include <JuceHeader.h>

//==============================================================================
/**  LusionLookAndFeel — dark premium styling for all JUCE widgets.  */
class LusionLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static constexpr juce::uint32
        C_BG     = 0xFF0A0A0C,
        C_SURF   = 0xFF111114,
        C_BORDER = 0xFF2A2A35,
        C_ORANGE = 0xFFFF6B1A,
        C_GREEN  = 0xFF00E5A0,
        C_PURPLE = 0xFF9D6FFF,
        C_TEXT   = 0xFFE8E8F0,
        C_DIM    = 0xFF6A6A80;

    LusionLookAndFeel()
    {
        // Global colour overrides
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (C_BG));
        setColour (juce::TextButton::buttonColourId,          juce::Colour (C_SURF));
        setColour (juce::TextButton::textColourOffId,         juce::Colour (C_TEXT));
        setColour (juce::ComboBox::backgroundColourId,        juce::Colour (C_SURF));
        setColour (juce::ComboBox::textColourId,              juce::Colour (C_TEXT));
        setColour (juce::ComboBox::outlineColourId,           juce::Colour (C_BORDER));
        setColour (juce::Label::textColourId,                 juce::Colour (C_TEXT));
        setColour (juce::PopupMenu::backgroundColourId,       juce::Colour (C_SURF));
        setColour (juce::PopupMenu::textColourId,             juce::Colour (C_TEXT));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (C_ORANGE).withAlpha(0.2f));
    }

    //──────────────────────────────────────────────────────────────────────────
    void drawButtonBackground (juce::Graphics& g,
                                juce::Button& btn,
                                const juce::Colour& bgCol,
                                bool highlighted,
                                bool down) override
    {
        auto bounds  = btn.getLocalBounds().toFloat().reduced (0.5f);
        auto baseCol = btn.findColour (juce::TextButton::buttonColourId);
        float corner = 7.f;

        if (baseCol == juce::Colour (C_ORANGE))
        {
            // Primary orange button — gradient
            juce::ColourGradient grad (juce::Colour (0xFFFF7F38), 0, 0,
                                       juce::Colour (0xFFD45510), 0, (float)btn.getHeight(), false);
            if (highlighted) grad.multiplyOpacity (1.15f);
            if (down)        grad.multiplyOpacity (0.8f);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bounds, corner);

            // Highlight sheen
            g.setColour (juce::Colours::white.withAlpha (0.1f));
            g.fillRoundedRectangle (bounds.withHeight (bounds.getHeight() * 0.5f), corner);
            // Glow border
            g.setColour (juce::Colour (C_ORANGE).withAlpha (0.6f));
            g.drawRoundedRectangle (bounds, corner, 1.f);
        }
        else if (baseCol == juce::Colour (C_GREEN))
        {
            // Export button — green
            juce::ColourGradient grad (juce::Colour (0xFF00FFB3), 0, 0,
                                       juce::Colour (0xFF00B37A), 0, (float)btn.getHeight(), false);
            if (highlighted) grad.multiplyOpacity (1.1f);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bounds, corner);
            g.setColour (juce::Colour (C_GREEN).withAlpha (0.5f));
            g.drawRoundedRectangle (bounds, corner, 1.f);
        }
        else if (baseCol == juce::Colour (0xFF1E1E28))
        {
            // Play button — dark with orange border
            g.setColour (highlighted ? juce::Colour (0xFF252530) : juce::Colour (0xFF1A1A22));
            g.fillRoundedRectangle (bounds, corner);
            g.setColour (highlighted ? juce::Colour (C_ORANGE) : juce::Colour (C_BORDER));
            g.drawRoundedRectangle (bounds, corner, 1.5f);
        }
        else
        {
            // Tab / octave / other button — subtle
            bool isActive = btn.getToggleState();
            g.setColour (isActive ? juce::Colour (C_ORANGE).withAlpha (0.1f)
                                  : (highlighted ? juce::Colour (0xFF1E1E28) : juce::Colours::transparentBlack));
            g.fillRoundedRectangle (bounds, 5.f);

            if (isActive)
            {
                // Underline for tabs
                g.setColour (juce::Colour (C_ORANGE));
                g.fillRect (bounds.getX(), bounds.getBottom() - 2.f, bounds.getWidth(), 2.f);
            }
        }
    }

    //──────────────────────────────────────────────────────────────────────────
    void drawButtonText (juce::Graphics& g,
                          juce::TextButton& btn,
                          bool highlighted,
                          bool down) override
    {
        auto col = btn.findColour (juce::TextButton::textColourOffId);

        // Tab buttons dim when inactive
        auto bgCol = btn.findColour (juce::TextButton::buttonColourId);
        bool isTabStyle = (bgCol == juce::Colour (C_SURF));
        if (isTabStyle && !btn.getToggleState())
            col = juce::Colour (C_DIM);

        g.setColour (col);
        g.setFont (juce::Font ("Helvetica Neue", 11.f, juce::Font::bold));
        g.drawText (btn.getButtonText(), btn.getLocalBounds(),
                    juce::Justification::centred, false);
    }

    //──────────────────────────────────────────────────────────────────────────
    void drawComboBox (juce::Graphics& g, int w, int h, bool,
                        int, int, int, int, juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float> (0, 0, (float)w, (float)h);
        g.setColour (juce::Colour (0xFF1A1A22));
        g.fillRoundedRectangle (bounds, 6.f);
        g.setColour (juce::Colour (C_BORDER));
        g.drawRoundedRectangle (bounds.reduced(0.5f), 6.f, 1.f);

        // Arrow
        float ax = w - 16.f, ay = h * 0.5f;
        juce::Path arrow;
        arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (juce::Colour (C_ORANGE));
        g.fillPath (arrow);
    }

    //──────────────────────────────────────────────────────────────────────────
    // Rotary knob (Exo-style premium)
    void drawRotarySlider (juce::Graphics& g,
                            int x, int y, int w, int h,
                            float sliderPos,
                            float startAngle, float endAngle,
                            juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float> ((float)x, (float)y, (float)w, (float)h)
                        .reduced (4.f);
        float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto  centre = bounds.getCentre();

        // Outer ring / track
        g.setColour (juce::Colour (0xFF1A1A22));
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius - 2, radius - 2,
                              0.f, startAngle, endAngle, true);
        g.strokePath (track, juce::PathStrokeType (3.f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));

        // Filled arc
        float angle = startAngle + sliderPos * (endAngle - startAngle);
        auto  arcCol = slider.isMouseOverOrDragging() ? juce::Colour (C_ORANGE)
                                                       : juce::Colour (C_ORANGE).withAlpha (0.85f);
        g.setColour (arcCol);
        juce::Path filled;
        filled.addCentredArc (centre.x, centre.y, radius - 2, radius - 2,
                               0.f, startAngle, angle, true);
        g.strokePath (filled, juce::PathStrokeType (3.f, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));

        // Knob body — radial gradient
        juce::ColourGradient body (juce::Colour (0xFF2E2E3A), centre.x - radius * 0.3f, centre.y - radius * 0.3f,
                                    juce::Colour (0xFF141418), centre.x + radius * 0.3f, centre.y + radius * 0.3f, true);
        g.setGradientFill (body);
        g.fillEllipse (centre.x - radius + 6, centre.y - radius + 6,
                       (radius - 6) * 2.f, (radius - 6) * 2.f);

        // Knob border
        g.setColour (juce::Colour (0xFF3A3A48));
        g.drawEllipse (centre.x - radius + 6, centre.y - radius + 6,
                       (radius - 6) * 2.f, (radius - 6) * 2.f, 1.f);

        // Specular highlight
        juce::ColourGradient shine (juce::Colours::white.withAlpha (0.12f), centre.x - radius * 0.2f, centre.y - radius * 0.4f,
                                     juce::Colours::transparentBlack, centre.x, centre.y, true);
        g.setGradientFill (shine);
        g.fillEllipse (centre.x - radius + 6, centre.y - radius + 6,
                       (radius - 6) * 2.f, (radius - 6) * 2.f);

        // Indicator line + dot
        float indicLen = radius - 10;
        auto  tip = centre.getPointOnCircumference (indicLen, angle);
        g.setColour (juce::Colour (C_ORANGE));
        g.drawLine (centre.x, centre.y, tip.x, tip.y, 2.f);
        g.fillEllipse (tip.x - 3, tip.y - 3, 6.f, 6.f);

        // Glow on hover
        if (slider.isMouseOverOrDragging())
        {
            g.setColour (juce::Colour (C_ORANGE).withAlpha (0.12f));
            g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.f, radius * 2.f);
        }
    }
};
