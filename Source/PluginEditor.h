#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "PluginProcessor.h"
#include "BuildVersion.h"

namespace Theme
{
    static const juce::Colour noteColor(0xFF3FA2FE);
    static const juce::Colour octaveColor(0xFFF8A43D);
    static const juce::Colour velocityColor(0xFFF72DA3);
    static const juce::Colour lengthColor(0xFFA228FF);
    static const juce::Colour masterColor(0xFFE0E329);
    static const juce::Colour slotsColor(0xFF40FF99);
    static const juce::Colour controllerColor(0xFFFF0050);
    
    static juce::Font getValueFont() { return juce::FontOptions("Arial", 50.0f, juce::Font::bold); }
}

class ColorPickerClient : public juce::Component, public juce::ChangeListener
{
public:
    ColorPickerClient(juce::Colour& target, juce::Colour initialColor, std::function<void()> callback)
        : targetColor(target), onUpdate(callback)
    {
        selector.setCurrentColour(initialColor);
        selector.addChangeListener(this);
        addAndMakeVisible(selector);
        setSize(200, 200);
    }
    
    void resized() override { selector.setBounds(getLocalBounds()); }
    
    void changeListenerCallback(juce::ChangeBroadcaster* /*source*/) override
    {
        targetColor = selector.getCurrentColour();
        if (onUpdate) onUpdate();
    }
    
    juce::ColourSelector selector { juce::ColourSelector::showColourspace };
    juce::Colour& targetColor;
    std::function<void()> onUpdate;
};

class LaneComponent : public juce::Component
{
public:
    LaneComponent(SequencerLane& lane, juce::String name, juce::Colour color, int minV, int maxV, int maxRR)
        : laneData(lane), laneName(name), laneColor(color), minVal(minV), maxVal(maxV), maxRandomRange(maxRR)
    {
        setOpaque(true);
    }
    
    std::function<juce::String(int)> valueFormatter;
    std::function<void(int, bool)> onStepShiftClicked;
    std::function<void(bool)> onResetClicked;
    std::function<void(bool)> onLabelClicked; // bool isShift
    
    void setLaneName(juce::String newName) { laneName = newName; repaint(); }
    
    juce::Colour getEffectiveColor() const {
        return laneData.customColor.isTransparent() ? laneColor : laneData.customColor;
    }
    
    void tick()
    {
        if (valueDisplayAlpha > 0.0f)
        {
            valueDisplayAlpha -= 0.05f;
            if (valueDisplayAlpha < 0.0f) valueDisplayAlpha = 0.0f;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);

        auto area = getLocalBounds();
        int h = getHeight();
        int triggerHeight = 24;
        int barAreaHeight = h - triggerHeight;
        int reducedBarHeight = barAreaHeight - 1;
        int barTopY = 0;
        
        // Left Controls (Toggles)
        area.removeFromLeft(60);
        
        // Draw Toggles
        // L aligned with step sequencer buttons (bottom)
        // Width reduced to 2/3 (40px), centered in 60px area (x offset 10)
        juce::Rectangle<int> localToggle(10, h - triggerHeight, 40, triggerHeight);
        localToggle = localToggle.reduced(2);
        
        // M same size, aligned with Y of bar-sequencers (upper border)
        juce::Rectangle<int> masterToggle(10, barTopY, 40, triggerHeight);
        masterToggle = masterToggle.reduced(2);
        
        // Draw Reset Button (Between M and L)
        int btnH = 70;
        int btnW = 36;
        int btnX = 12;
        int btnY = (h - btnH) / 2;
        
        juce::Rectangle<int> resetBtnRect(btnX, btnY, btnW, btnH);
        
        g.setColour(getEffectiveColor());
        g.fillRect(resetBtnRect);
        
        g.setColour(juce::Colours::black);
        g.setFont(juce::FontOptions("Arial", 16.0f, juce::Font::bold));
        
        juce::String labelText = laneName;
        if (laneName == "NOTE") labelText = "NO\nTE";
        else if (laneName == "OCT") labelText = "OC\nTA\nVE";
        else if (laneName == "VEL") labelText = "VE\nLO\nCI\nTY";
        else if (laneName == "LEN") labelText = "LE\nNG\nTH";
        else if (laneName == "PRESSURE") labelText = "PR\nES\nSU\nRE";
        else if (laneName.startsWith("CC ")) labelText = laneName.replace(" ", "\n");
        
        g.drawFittedText(labelText, resetBtnRect, juce::Justification::centred, 3);
        
        // Master Toggle (Yellow)
        g.setColour(Theme::masterColor);
        g.fillRect(masterToggle);
        g.setColour(juce::Colours::black);
        g.fillRect(masterToggle.reduced(1));
        
        g.setColour(Theme::masterColor.withAlpha(laneData.enableMasterSource ? 1.0f : 0.33f));
        g.fillRect(masterToggle.reduced(1));
        
        // Local Toggle (Lane Color)
        g.setColour(getEffectiveColor());
        g.fillRect(localToggle);
        g.setColour(juce::Colours::black);
        g.fillRect(localToggle.reduced(1));
        
        g.setColour(getEffectiveColor().withAlpha(laneData.enableLocalSource ? 1.0f : 0.33f));
        g.fillRect(localToggle.reduced(1));

        // Right Controls (Loop Lengths)
        // Align with Left Controls (M/L)
        // V aligned with M (top)
        // T aligned with L (bottom)
        // Width 40px, centered in right margin (assuming right margin is 40px, so x = width - 40)
        // Actually, let's center it in the 40px area.
        
        // New Layout: 120px margin
        // Col A (Width-120 to Width-80): Length (40px)
        // Col B (Width-80 to Width-40): Shift L (20px) + Shift R (20px)
        // Col C (Width-40 to Width): (Unused/Margin)
        
        int rightMarginX = getWidth() - 120;
        int col1_X = rightMarginX;
        int col2_X = rightMarginX + 40;
        
        // Vertical Layout:
        // Row 0: Value Length | Value Shift L | Value Shift R
        // Row 1: Value Reset  | Value Direction
        // Row 2: Random (Centered?)
        // Row 3: Trigger Direction | Trigger Reset
        // Row 4: Trigger Length | Trigger Shift L | Trigger Shift R
        
        // Wait, user said: "bar-reset below/above step-length....and direction-button below/above left/right shift"
        // "below for value seq"
        // Value Seq (Top):
        // Row 0: Length | Shift L | Shift R
        // Row 1: Reset  | Direction
        
        // "above for step-progression sequencer" (Trigger Seq - Bottom)
        // Row N-1: Reset | Direction
        // Row N:   Length | Shift L | Shift R
        
        int ctrlH = 24;
        int gap = 1;
        
        // Value Controls
        juce::Rectangle<int> valShiftL(col1_X, 0, 20, ctrlH);
        valShiftL = valShiftL.reduced(1);

        juce::Rectangle<int> valLoopRect(col1_X + 20, 0, 40, ctrlH);
        valLoopRect = valLoopRect.reduced(1);
        
        juce::Rectangle<int> valShiftR(col1_X + 60, 0, 20, ctrlH);
        valShiftR = valShiftR.reduced(1);
        
        juce::Rectangle<int> valResetRect(col1_X, ctrlH + gap, 40, ctrlH);
        valResetRect = valResetRect.reduced(1);

        juce::Rectangle<int> valDirRect(col2_X, ctrlH + gap, 40, ctrlH);
        valDirRect = valDirRect.reduced(1);
        
        // Random Button (Row 2, Col 1)
        juce::Rectangle<int> randomRect(col1_X, (ctrlH + gap) * 2 + 3, 40, ctrlH);
        randomRect = randomRect.reduced(1);
        
        // Random Range Slider (Row 2, Col 2)
        juce::Rectangle<int> randomRangeRect(col2_X, (ctrlH + gap) * 2 + 3, 40, ctrlH);
        randomRangeRect = randomRangeRect.reduced(1);

        // Trigger Controls (Bottom Up)
        int bottomY = getHeight();
        
        juce::Rectangle<int> trigShiftL(col1_X, bottomY - ctrlH, 20, ctrlH);
        trigShiftL = trigShiftL.reduced(1);

        juce::Rectangle<int> trigLoopRect(col1_X + 20, bottomY - ctrlH, 40, ctrlH);
        trigLoopRect = trigLoopRect.reduced(1);
        
        juce::Rectangle<int> trigShiftR(col1_X + 60, bottomY - ctrlH, 20, ctrlH);
        trigShiftR = trigShiftR.reduced(1);
        
        juce::Rectangle<int> trigResetRect(col1_X, bottomY - (ctrlH * 2) - gap, 40, ctrlH);
        trigResetRect = trigResetRect.reduced(1);
        
        juce::Rectangle<int> trigDirRect(col2_X, bottomY - (ctrlH * 2) - gap, 40, ctrlH);
        trigDirRect = trigDirRect.reduced(1);
        
        // Reset Button (Small Circle) - Removed
        /*
        int midY = h / 2;
        int resetX = col2_X + 55;
        int resetY = midY;
        float resetRadius = 4.0f;
        
        g.setColour(laneColor);
        g.drawEllipse(resetX - resetRadius, resetY - resetRadius, resetRadius * 2, resetRadius * 2, 1.0f);
        */

        g.setColour(getEffectiveColor());
        g.setFont(juce::FontOptions("Arial", 12.0f, juce::Font::bold));
        
        // Draw Value Loop Control (Outline only)
        g.fillRect(valLoopRect);
        g.setColour(juce::Colours::black);
        g.fillRect(valLoopRect.reduced(1));
        g.setColour(getEffectiveColor());
        g.drawText(juce::String(laneData.valueLoopLength), valLoopRect, juce::Justification::centred);
        
        // Draw Value Reset Control
        g.fillRect(valResetRect);
        g.setColour(juce::Colours::black);
        g.fillRect(valResetRect.reduced(1));
        g.setColour(getEffectiveColor());
        g.drawText(laneData.valueResetInterval == 0 ? "FREE" : juce::String(laneData.valueResetInterval), valResetRect, juce::Justification::centred);

        // Draw Value Direction Control
        g.fillRect(valDirRect);
        g.setColour(juce::Colours::black);
        g.fillRect(valDirRect.reduced(1));
        g.setColour(getEffectiveColor());
        g.drawText(getDirectionString(laneData.valueDirection), valDirRect, juce::Justification::centred);
        
        // Draw Trigger Reset Control
        g.fillRect(trigResetRect);
        g.setColour(juce::Colours::black);
        g.fillRect(trigResetRect.reduced(1));
        g.setColour(getEffectiveColor());
        g.drawText(laneData.triggerResetInterval == 0 ? "FREE" : juce::String(laneData.triggerResetInterval), trigResetRect, juce::Justification::centred);

        // Draw Trigger Direction Control
        g.fillRect(trigDirRect);
        g.setColour(juce::Colours::black);
        g.fillRect(trigDirRect.reduced(1));
        g.setColour(getEffectiveColor());
        g.drawText(getDirectionString(laneData.triggerDirection), trigDirRect, juce::Justification::centred);
        
        // Draw Trigger Loop Control (Outline only)
        g.fillRect(trigLoopRect);
        g.setColour(juce::Colours::black);
        g.fillRect(trigLoopRect.reduced(1));
        g.setColour(getEffectiveColor());
        g.drawText(juce::String(laneData.triggerLoopLength), trigLoopRect, juce::Justification::centred);
        
        // Draw Shift Triangles
        auto drawTriangle = [&](juce::Rectangle<int> r, bool left) {
            juce::Path p;
            float w = (float)r.getWidth();
            float th = (float)r.getHeight();
            float x = (float)r.getX();
            float y = (float)r.getY();
            
            float cx = x + w * 0.5f;
            float cy = y + th * 0.5f;
            float s = w * 0.3f;
            
            if (left)
            {
                p.addTriangle(cx + s, cy - s, cx + s, cy + s, cx - s, cy);
            }
            else
            {
                p.addTriangle(cx - s, cy - s, cx - s, cy + s, cx + s, cy);
            }
            
            g.setColour(juce::Colours::black);
            g.fillRect(r);
            
            g.setColour(getEffectiveColor());
            g.drawRect(r, 1);
            g.fillPath(p);
        };
        
        drawTriangle(valShiftL, true);
        drawTriangle(valShiftR, false);
        drawTriangle(trigShiftL, true);
        drawTriangle(trigShiftR, false);

        // Color Picker Button (Row 2, Col 3)
        int col3_X = rightMarginX + 80;
        int colorBtnSize = 16;
        int colorBtnX = col3_X + (40 - colorBtnSize) / 2;
        int colorBtnY = (ctrlH + gap) * 2 + 3 + (ctrlH - colorBtnSize) / 2;
        
        juce::Rectangle<int> colorBtnRect(colorBtnX, colorBtnY, colorBtnSize, colorBtnSize);
        
        g.setColour(getEffectiveColor());
        g.fillEllipse(colorBtnRect.toFloat());

        // Draw Random Button
        g.setColour(getEffectiveColor());
        g.fillRect(randomRect);
        g.setColour(juce::Colours::black);
        g.fillRect(randomRect.reduced(1));
        g.setColour(getEffectiveColor());
        
        bool showSquares = isHoveringRandom && juce::ModifierKeys::getCurrentModifiers().isShiftDown();
        
        if (showSquares)
        {
            // Draw Random Triggers Icon (Squares)
            int numSquares = 4;
            auto iconArea = randomRect.reduced(2);
            
            int sqWidth = iconArea.getWidth() / numSquares;
            int size = juce::jmin(sqWidth, iconArea.getHeight()) - 2;
            
            // Center the group horizontally
            int startX = iconArea.getX() + (iconArea.getWidth() - (numSquares * sqWidth)) / 2;
            // Center vertically
            int y = iconArea.getCentreY() - size / 2;
            
            for(int i=0; i<numSquares; ++i)
            {
                int x = startX + i * sqWidth + (sqWidth - size) / 2;
                
                if (i % 2 == 0)
                    g.fillRect(x, y, size, size);
                else
                {
                    g.fillRect(x, y, size, size);
                    g.setColour(juce::Colours::black);
                    g.fillRect(juce::Rectangle<int>(x, y, size, size).reduced(1));
                    g.setColour(getEffectiveColor());
                }
            }
        }
        else
        {
            // Draw Random Values Icon (Bars)
            float barH[] = { 0.5f, 0.25f, 1.0f, 0.33f };
            int numBars = 4;
            int padding = 3;
            auto iconArea = randomRect.reduced(padding);
            float barWidth = iconArea.getWidth() / (float)numBars;
            
            for(int i=0; i<numBars; ++i)
            {
                float bh = iconArea.getHeight() * barH[i];
                float bx = iconArea.getX() + i * barWidth;
                float by = iconArea.getBottom() - bh;
                g.fillRect((float)bx + 1.0f, by, barWidth - 2.0f, bh);
            }
        }
        
        // Draw Random Range Slider
        g.setColour(getEffectiveColor());
        g.fillRect(randomRangeRect);
        g.setColour(juce::Colours::black);
        g.fillRect(randomRangeRect.reduced(1));
        g.setColour(getEffectiveColor());
        g.setFont(juce::FontOptions("Arial", 12.0f, juce::Font::bold));
        juce::String rangeText = (laneData.randomRange == 0) ? "FULL" : ("+/-" + juce::String(laneData.randomRange));
        g.drawText(rangeText, randomRangeRect, juce::Justification::centred);

        // Draw Steps
        area.removeFromRight(130);
        float stepWidth = area.getWidth() / 16.0f;
        
        // Save area for overlay
        auto stepsArea = area;
        
        for (size_t i = 0; i < 16; ++i)
        {
            auto stepArea = area.removeFromLeft((int)stepWidth); // No gap
            
            // Draw Advance Trigger Button (Increased height to 24px)
            auto btnArea = stepArea.removeFromBottom(triggerHeight).reduced(2);
            
            // Draw Value Bar (Remaining top part)
            auto fullBarArea = stepArea;
            fullBarArea.removeFromBottom(1);
            
            // Use full height
            // int reducedBarHeight = fullBarArea.getHeight();
            auto effectiveBarArea = fullBarArea;
            
            // Dim if outside loop
            float valAlpha = (i < (size_t)laneData.valueLoopLength) ? 1.0f : 0.3f;
            float trigAlpha = (i < (size_t)laneData.triggerLoopLength) ? 1.0f : 0.3f;
            
            // Background for bar area
            g.setColour(getEffectiveColor().withAlpha(0.33f * valAlpha));
            g.fillRect(effectiveBarArea);
            
            float normVal = (float)(laneData.values[i] - minVal) / (float)(maxVal - minVal);
            if (maxVal == minVal) normVal = 0.5f; // Avoid div by zero
            
            int barHeight = (int)(effectiveBarArea.getHeight() * normVal);
            auto valueBarArea = effectiveBarArea;
            auto fillArea = valueBarArea.removeFromBottom(barHeight);
            
            g.setColour(getEffectiveColor().withAlpha(valAlpha));
            g.fillRect(fillArea);
            
            // Highlight current step
            if (i == (size_t)laneData.activeValueStep)
            {
                g.setColour(getEffectiveColor().darker(1.0f).withAlpha(0.5f));
                g.fillRect(effectiveBarArea);
            }

            // Draw Advance Trigger Button
            g.setColour(getEffectiveColor().withAlpha(trigAlpha));
            g.fillRect(btnArea);
            
            if (!laneData.triggers[i])
            {
                g.setColour(juce::Colours::black);
                g.fillRect(btnArea.reduced(1));
            }
            
            // Highlight current trigger step
            if (i == (size_t)laneData.activeTriggerStep)
            {
                g.setColour(getEffectiveColor().darker(1.0f).withAlpha(0.5f));
                g.fillRect(btnArea);
            }
        }
        
        // Draw Value Overlay (Inside Bar Boundaries)
        if (valueDisplayAlpha > 0.0f)
        {
            // Position inside the visual bar area
            // x = 60 (start of steps), y = barTopY, w = stepsArea.getWidth(), h = reducedBarHeight
            juce::Rectangle<int> overlayRect(60, barTopY, stepsArea.getWidth(), reducedBarHeight);
            
            g.setFont(Theme::getValueFont());
            
            // Shadow (Black, shifted +2, +2)
            g.setColour(juce::Colours::black.withAlpha(valueDisplayAlpha));
            g.drawText(lastEditedValue, overlayRect.translated(2, 2), juce::Justification::centred, false);
            
            // Main Text (Lane Color)
            g.setColour(getEffectiveColor().withAlpha(valueDisplayAlpha));
            g.drawText(lastEditedValue, overlayRect, juce::Justification::centred, false);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        auto area = getLocalBounds();
        area.removeFromLeft(60);
        area.removeFromRight(130);
        
        // Handle Left Toggles
        if (e.x < 60)
        {
            int triggerHeight = 24;
            int h = getHeight();
            int barTopY = 0;
            
            if (e.y >= h - triggerHeight)
            {
                laneData.enableLocalSource = !laneData.enableLocalSource;
                repaint();
            }
            else if (e.y >= barTopY && e.y < barTopY + triggerHeight)
            {
                laneData.enableMasterSource = !laneData.enableMasterSource;
                repaint();
            }
            else
            {
                // Reset Button Clicked (Between M and L)
                if (e.y >= triggerHeight && e.y < h - triggerHeight)
                {
                    // Check if clicking the label area
                    int btnH = 70;
                    int btnY = (h - btnH) / 2;
                    if (e.y >= btnY && e.y < btnY + btnH)
                    {
                        if (onLabelClicked) onLabelClicked(e.mods.isShiftDown());
                    }
                    else
                    {
                        if (onResetClicked) onResetClicked(e.mods.isAltDown());
                    }
                    repaint();
                }
            }
            return;
        }
        
        // Handle Right Controls
        if (e.x > getWidth() - 120)
        {
            // int triggerHeight = 24;
            // int h = getHeight();
            // int barTopY = 0;
            
            int rightMarginX = getWidth() - 120;
            int col1_X = rightMarginX;
            int col2_X = rightMarginX + 40;
            
            // Layout Constants
            int ctrlH = 24;
            int gap = 1;
            
            // Check Shift Buttons
            // Value Shift L
            if (e.x >= col1_X && e.x < col1_X + 20 && e.y >= 0 && e.y < ctrlH)
            {
                laneData.shiftValues(-1);
                repaint();
                return;
            }
            // Value Shift R
            if (e.x >= col1_X + 60 && e.x < col1_X + 80 && e.y >= 0 && e.y < ctrlH)
            {
                laneData.shiftValues(1);
                repaint();
                return;
            }
            
            int bottomY = getHeight();
            
            // Trigger Shift L
            if (e.x >= col1_X && e.x < col1_X + 20 && e.y >= bottomY - ctrlH)
            {
                laneData.shiftTriggers(-1);
                repaint();
                return;
            }
            // Trigger Shift R
            if (e.x >= col1_X + 60 && e.x < col1_X + 80 && e.y >= bottomY - ctrlH)
            {
                laneData.shiftTriggers(1);
                repaint();
                return;
            }
            
            // Value Loop (0)
            if (e.x >= col1_X + 20 && e.x < col1_X + 60 && e.y >= 0 && e.y < ctrlH)
            {
                isDraggingValueLoop = true;
                lastMouseY = e.y;
                lastMouseX = e.x;
                return;
            }
            
            // Value Reset (1)
            if (e.x >= col1_X && e.x < col1_X + 40 && e.y >= ctrlH + gap && e.y < ctrlH * 2 + gap)
            {
                isDraggingValueReset = true;
                lastMouseY = e.y;
                lastMouseX = e.x;
                return;
            }

            // Value Direction (2)
            if (e.x >= col2_X && e.x < col2_X + 40 && e.y >= ctrlH + gap && e.y < ctrlH * 2 + gap)
            {
                isDraggingValueDirection = true;
                lastMouseY = e.y;
                lastMouseX = e.x;
                return;
            }
            
            // Random Button (3 - Row 2 Col 1)
            if (e.x >= col1_X && e.x < col1_X + 40 && e.y >= (ctrlH + gap) * 2 + 3 && e.y < (ctrlH + gap) * 2 + ctrlH + 3)
            {
                if (e.mods.isShiftDown())
                    randomizeTriggers();
                else
                    randomizeValues();
                return;
            }
            
            // Random Range (Row 2 Col 2)
            if (e.x >= col2_X && e.x < col2_X + 40 && e.y >= (ctrlH + gap) * 2 + 3 && e.y < (ctrlH + gap) * 2 + ctrlH + 3)
            {
                isDraggingRandomRange = true;
                lastMouseY = e.y;
                lastMouseX = e.x;
                return;
            }

            // Color Picker (Row 2, Col 3)
            int col3_X = rightMarginX + 80;
            if (e.x >= col3_X && e.x < col3_X + 40 && e.y >= (ctrlH + gap) * 2 + 3 && e.y < (ctrlH + gap) * 2 + ctrlH + 3)
            {
                if (e.mods.isShiftDown())
                {
                    laneData.customColor = juce::Colours::transparentBlack;
                    repaint();
                }
                else
                {
                    auto* client = new ColorPickerClient(laneData.customColor, getEffectiveColor(), [this](){ repaint(); });
                    juce::CallOutBox::launchAsynchronously(std::unique_ptr<juce::Component>(client), getScreenBounds().translated(e.x, e.y), nullptr);
                }
                return;
            }

            // Trigger Direction (4)
            if (e.x >= col2_X && e.x < col2_X + 40 && e.y >= bottomY - (ctrlH * 2) - gap && e.y < bottomY - ctrlH - gap)
            {
                isDraggingTriggerDirection = true;
                lastMouseY = e.y;
                lastMouseX = e.x;
                return;
            }
            
            // Trigger Reset (5)
            if (e.x >= col1_X && e.x < col1_X + 40 && e.y >= bottomY - (ctrlH * 2) - gap && e.y < bottomY - ctrlH - gap)
            {
                isDraggingTriggerReset = true;
                lastMouseY = e.y;
                lastMouseX = e.x;
                return;
            }
            
            // Trigger Loop (6)
            if (e.x >= col1_X + 20 && e.x < col1_X + 60 && e.y >= bottomY - ctrlH)
            {
                isDraggingTriggerLoop = true;
                lastMouseY = e.y;
                lastMouseX = e.x;
                return;
            }
            
            // Reset Button - Removed
            /*
            int midY = h / 2;
            int resetX = col2_X + 55;
            int resetY = midY;
            if (std::abs(e.x - resetX) < 10 && std::abs(e.y - resetY) < 10)
            {
                if (onResetClicked) onResetClicked(e.mods.isAltDown());
                repaint();
            }
            */
            return;
        }
        
        float stepWidth = area.getWidth() / 16.0f;
        int stepIdx = (int)((e.x - 60) / stepWidth);
        
        if (stepIdx >= 0 && stepIdx < 16 && e.x <= getWidth() - 130)
        {
            int triggerHeight = 24;
            bool isTriggerRow = (e.y >= getHeight() - triggerHeight);

            if (e.mods.isShiftDown())
            {
                if (onStepShiftClicked) onStepShiftClicked(stepIdx, isTriggerRow);
                repaint();
                return;
            }

            lastEditedStep = stepIdx;
            
            // Check if clicked on Bar or Button
            if (!isTriggerRow)
            {
                isDraggingTrigger = false;
                
                int val = getValueFromY(e.y, getHeight());
                
                if (e.mods.isAltDown())
                {
                    // Relative: Shift all steps by the difference
                    int diff = val - laneData.values[(size_t)stepIdx];
                    for(size_t i=0; i<16; ++i)
                    {
                        laneData.values[i] = juce::jlimit(minVal, maxVal, laneData.values[i] + diff);
                    }
                }
                else
                {
                    laneData.values[(size_t)stepIdx] = val;
                }
                
                lastDragValue = val;
                
                if (valueFormatter)
                    lastEditedValue = valueFormatter(val);
                else
                    lastEditedValue = juce::String(val);
                
                valueDisplayAlpha = 2.0f;
            }
            else
            {
                isDraggingTrigger = true;
                targetTriggerState = !laneData.triggers[(size_t)stepIdx];
                laneData.triggers[(size_t)stepIdx] = targetTriggerState;
            }
            repaint();
        }
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        // Helper for Interval Steps
        auto getNextInterval = [](int current, int delta, bool isShift) -> int {
            if (isShift) {
                static const std::vector<int> steps = {0, 1, 2, 4, 8, 16, 32, 64, 128};
                // Find closest index
                int closestIdx = 0;
                int minDiff = 1000;
                for(int i=0; i<(int)steps.size(); ++i) {
                    int diff = std::abs(steps[(size_t)i] - current);
                    if(diff < minDiff) {
                        minDiff = diff;
                        closestIdx = i;
                    }
                }
                
                int nextIdx = closestIdx + (delta > 0 ? 1 : -1);
                nextIdx = juce::jlimit(0, (int)steps.size()-1, nextIdx);
                return steps[(size_t)nextIdx];
            } else {
                int next = current + (delta > 0 ? 1 : -1);
                return juce::jlimit(0, 128, next);
            }
        };

        // Handle Right Loop Lengths Drag
        if (isDraggingValueLoop)
        {
            int delta = (e.x - lastMouseX) - (e.y - lastMouseY); // Right/Up increases
            if (std::abs(delta) > 5) // Sensitivity threshold
            {
                if (delta > 0) laneData.valueLoopLength = juce::jmin(16, laneData.valueLoopLength + 1);
                else laneData.valueLoopLength = juce::jmax(1, laneData.valueLoopLength - 1);
                
                lastMouseX = e.x;
                lastMouseY = e.y;
                repaint();
            }
            return;
        }
        
        if (isDraggingValueReset)
        {
            int delta = (e.x - lastMouseX) - (e.y - lastMouseY);
            if (std::abs(delta) > 5) // Sensitivity
            {
                laneData.valueResetInterval = getNextInterval(laneData.valueResetInterval, delta, e.mods.isShiftDown());
                lastMouseX = e.x;
                lastMouseY = e.y;
                repaint();
            }
            return;
        }

        if (isDraggingValueDirection)
        {
            int delta = (e.x - lastMouseX) - (e.y - lastMouseY);
            if (std::abs(delta) > 10) // Lower sensitivity for enum
            {
                int dir = (int)laneData.valueDirection;
                if (delta > 0) dir = (dir + 1) % 6;
                else dir = (dir - 1 + 6) % 6;
                
                laneData.valueDirection = (SequencerLane::Direction)dir;
                lastMouseX = e.x;
                lastMouseY = e.y;
                repaint();
            }
            return;
        }
        
        if (isDraggingRandomRange)
        {
            int delta = (e.x - lastMouseX) - (e.y - lastMouseY);
            if (std::abs(delta) > 5)
            {
                if (delta > 0) laneData.randomRange = juce::jmin(maxRandomRange, laneData.randomRange + 1);
                else laneData.randomRange = juce::jmax(0, laneData.randomRange - 1);
                
                lastMouseX = e.x;
                lastMouseY = e.y;
                repaint();
            }
            return;
        }
        
        if (isDraggingTriggerReset)
        {
            int delta = (e.x - lastMouseX) - (e.y - lastMouseY);
            if (std::abs(delta) > 5)
            {
                laneData.triggerResetInterval = getNextInterval(laneData.triggerResetInterval, delta, e.mods.isShiftDown());
                lastMouseX = e.x;
                lastMouseY = e.y;
                repaint();
            }
            return;
        }

        if (isDraggingTriggerDirection)
        {
            int delta = (e.x - lastMouseX) - (e.y - lastMouseY);
            if (std::abs(delta) > 10)
            {
                int dir = (int)laneData.triggerDirection;
                if (delta > 0) dir = (dir + 1) % 6;
                else dir = (dir - 1 + 6) % 6;
                
                laneData.triggerDirection = (SequencerLane::Direction)dir;
                lastMouseX = e.x;
                lastMouseY = e.y;
                repaint();
            }
            return;
        }
        
        if (isDraggingTriggerLoop)
        {
            int delta = (e.x - lastMouseX) - (e.y - lastMouseY); // Right/Up increases
            if (std::abs(delta) > 5)
            {
                if (delta > 0) laneData.triggerLoopLength = juce::jmin(16, laneData.triggerLoopLength + 1);
                else laneData.triggerLoopLength = juce::jmax(1, laneData.triggerLoopLength - 1);
                
                lastMouseX = e.x;
                lastMouseY = e.y;
                repaint();
            }
            return;
        }

        if (e.mods.isShiftDown()) return;

        // Only drag values/triggers in the main area
        if (e.x >= 60 && e.x <= getWidth() - 130)
        {
            auto area = getLocalBounds();
            area.removeFromLeft(60);
            area.removeFromRight(130);
            float stepWidth = area.getWidth() / 16.0f;
            int stepIdx = (int)((e.x - 60) / stepWidth);
            
            if (isDraggingTrigger)
            {
                if (stepIdx >= 0 && stepIdx < 16 && stepIdx != lastEditedStep)
                {
                    lastEditedStep = stepIdx;
                    laneData.triggers[(size_t)stepIdx] = targetTriggerState;
                    repaint();
                }
            }
            else
            {
                int val = getValueFromY(e.y, getHeight());
                
                if (e.mods.isAltDown())
                {
                    // Relative shift based on mouse movement delta
                    int diff = val - lastDragValue;
                    if (diff != 0)
                    {
                        for(size_t i=0; i<16; ++i)
                        {
                            laneData.values[i] = juce::jlimit(minVal, maxVal, laneData.values[i] + diff);
                        }
                        lastDragValue = val;
                        repaint();
                    }
                }
                else
                {
                    // Normal paint
                    if (stepIdx >= 0 && stepIdx < 16)
                    {
                        laneData.values[(size_t)stepIdx] = val;
                        lastDragValue = val;
                        lastEditedStep = stepIdx;
                        repaint();
                    }
                }
                
                if (valueFormatter)
                    lastEditedValue = valueFormatter(lastDragValue);
                else
                    lastEditedValue = juce::String(lastDragValue);
                
                valueDisplayAlpha = 2.0f;
            }
        }
    }
    
    int getValueFromY(int y, int h)
    {
        int triggerHeight = 24;
        int barAreaHeight = h - triggerHeight;
        int reducedBarHeight = barAreaHeight - 1;
        int barTop = 0;
        
        int relativeY = y - barTop;
        
        float norm = 1.0f - (float)relativeY / (float)reducedBarHeight;
        norm = juce::jlimit(0.0f, 1.0f, norm);
        
        int val = minVal + (int)(norm * (maxVal - minVal));
        return juce::jlimit(minVal, maxVal, val);
    }

    void updateValue(int stepIdx, int y, int h)
    {
        int val = getValueFromY(y, h);
        laneData.values[(size_t)stepIdx] = val;
        
        if (valueFormatter)
            lastEditedValue = valueFormatter(val);
        else
            lastEditedValue = juce::String(val);
        
        valueDisplayAlpha = 2.0f;
    }
    
    void randomizeValues()
    {
        juce::Random r;
        for (size_t i = 0; i < 16; ++i)
        {
            if (laneData.randomRange == 0)
            {
                // Full Random
                laneData.values[i] = r.nextInt(maxVal - minVal + 1) + minVal;
            }
            else
            {
                // Jitter Random (+/- Range)
                int jitter = r.nextInt(laneData.randomRange * 2 + 1) - laneData.randomRange;
                laneData.values[i] = juce::jlimit(minVal, maxVal, laneData.values[i] + jitter);
            }
        }
        repaint();
    }

    void randomizeTriggers()
    {
        juce::Random r;
        for (size_t i = 0; i < 16; ++i)
        {
            laneData.triggers[i] = r.nextBool();
        }
        repaint();
    }
    
    void mouseUp(const juce::MouseEvent&) override
    {
        isDraggingValueLoop = false;
        isDraggingTriggerLoop = false;
        isDraggingValueReset = false;
        isDraggingTriggerReset = false;
        isDraggingValueDirection = false;
        isDraggingTriggerDirection = false;
        isDraggingRandomRange = false;
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        int col1_X = getWidth() - 120;
        int ctrlH = 24;
        int gap = 1;
        juce::Rectangle<int> randomRect(col1_X, (ctrlH + gap) * 2 + 3, 40, ctrlH);
        randomRect = randomRect.reduced(1);
        
        bool nowHovering = randomRect.contains(e.getPosition());
        if (nowHovering != isHoveringRandom)
        {
            isHoveringRandom = nowHovering;
            repaint(randomRect);
        }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (isHoveringRandom)
        {
            isHoveringRandom = false;
            int col1_X = getWidth() - 120;
            int ctrlH = 24;
            int gap = 1;
            juce::Rectangle<int> randomRect(col1_X, (ctrlH + gap) * 2 + 3, 40, ctrlH);
            randomRect = randomRect.reduced(1);
            repaint(randomRect);
        }
    }
    
    void modifierKeysChanged(const juce::ModifierKeys&) override
    {
        if (isHoveringRandom)
        {
            int col1_X = getWidth() - 120;
            int ctrlH = 24;
            int gap = 1;
            juce::Rectangle<int> randomRect(col1_X, (ctrlH + gap) * 2 + 3, 40, ctrlH);
            randomRect = randomRect.reduced(1);
            repaint(randomRect);
        }
    }

private:
    SequencerLane& laneData;
    juce::String laneName;
    juce::Colour laneColor;
    int minVal;
    int maxVal;
    int maxRandomRange;
    
    juce::String lastEditedValue;
    float valueDisplayAlpha = 0.0f;
    
    int lastEditedStep = -1;
    bool isDraggingTrigger = false;
    bool targetTriggerState = false;
    
    bool isDraggingValueLoop = false;
    bool isDraggingTriggerLoop = false;
    bool isDraggingValueReset = false;
    bool isDraggingTriggerReset = false;
    bool isDraggingValueDirection = false;
    bool isDraggingTriggerDirection = false;
    bool isDraggingRandomRange = false;
    int lastMouseX = 0;
    int lastMouseY = 0;
    int lastDragValue = 0;
    
    bool isHoveringRandom = false;
    
    juce::String getDirectionString(SequencerLane::Direction dir)
    {
        switch (dir)
        {
            case SequencerLane::Direction::Forward: return "FWD";
            case SequencerLane::Direction::Backward: return "BWD";
            case SequencerLane::Direction::PingPong: return "PING";
            case SequencerLane::Direction::Bounce: return "BNCE";
            case SequencerLane::Direction::Random: return "RAND";
            case SequencerLane::Direction::RandomDirection: return "RDIR";
            default: return "FWD";
        }
    }
};

class ShuffleComponent : public juce::Component
{
public:
    ShuffleComponent(ShequencerAudioProcessor& p) : processor(p) {}
    
    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().reduced(2);
        
        if (processor.isShuffleGlobal)
        {
            g.setColour(Theme::slotsColor);
            g.fillRect(area);
            g.setColour(juce::Colours::black);
        }
        else
        {
            g.setColour(Theme::slotsColor);
            g.fillRect(area);
            g.setColour(juce::Colours::black);
            g.fillRect(area.reduced(1));
            g.setColour(Theme::slotsColor);
        }
        
        g.setFont(juce::FontOptions("Arial", 12.0f, juce::Font::bold));
        g.drawText("S:" + juce::String(processor.shuffleAmount), area, juce::Justification::centred);
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isShiftDown())
        {
            processor.isShuffleGlobal = !processor.isShuffleGlobal;
            repaint();
            return;
        }
        lastMouseX = e.x;
        lastMouseY = e.y;
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        int delta = (e.x - lastMouseX) - (e.y - lastMouseY);
        if (std::abs(delta) > 5)
        {
            if (delta > 0) processor.shuffleAmount = juce::jmin(7, processor.shuffleAmount + 1);
            else processor.shuffleAmount = juce::jmax(1, processor.shuffleAmount - 1);
            
            lastMouseX = e.x;
            lastMouseY = e.y;
            repaint();
        }
    }

private:
    ShequencerAudioProcessor& processor;
    int lastMouseX = 0;
    int lastMouseY = 0;
};

class BuildNumberComponent : public juce::Component
{
public:
    BuildNumberComponent() {}
    
    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().reduced(2);
        
        // Color same as shuffle (Theme::slotsColor) but alpha 0.33f
        g.setColour(Theme::slotsColor.withAlpha(0.33f));
        
        g.setFont(juce::FontOptions("Arial", 10.0f, juce::Font::bold));
        
        juce::String buildStr = juce::String::formatted("b.%03d", BUILD_NUMBER);
        g.drawText(buildStr, area, juce::Justification::centred);
    }
};

class FileOpsComponent : public juce::Component
{
public:
    FileOpsComponent(ShequencerAudioProcessor& p) : processor(p) {}
    
    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        int w = area.getWidth() / 2;
        
        juce::Rectangle<int> loadRect(0, 0, w, area.getHeight());
        loadRect = loadRect.reduced(1);
        
        juce::Rectangle<int> saveRect(w, 0, w, area.getHeight());
        saveRect = saveRect.reduced(1);
        
        g.setColour(Theme::slotsColor);
        g.fillRect(loadRect);
        g.setColour(juce::Colours::black);
        g.fillRect(loadRect.reduced(1));
        
        g.setColour(Theme::slotsColor);
        g.fillRect(saveRect);
        g.setColour(juce::Colours::black);
        g.fillRect(saveRect.reduced(1));
        
        g.setColour(Theme::slotsColor);
        g.setFont(juce::FontOptions("Arial", 12.0f, juce::Font::bold));
        g.drawText("L", loadRect, juce::Justification::centred);
        g.drawText("S", saveRect, juce::Justification::centred);
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        int w = getWidth() / 2;
        if (e.x < w)
        {
            // Load
            fileChooser = std::make_unique<juce::FileChooser>("Load Pattern Bank",
                                                              juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                              "*.json");
            auto folderChooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
            
            fileChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file.existsAsFile())
                {
                    processor.loadAllPatternsFromJson(file);
                    // Trigger repaint of slots
                    if (auto* parent = getParentComponent()) parent->repaint();
                }
            });
        }
        else
        {
            // Save
            fileChooser = std::make_unique<juce::FileChooser>("Save Pattern Bank",
                                                              juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
                                                              "*.json");
            auto folderChooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;
            
            fileChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file != juce::File())
                {
                    if (!file.getFileName().startsWith("SHseq_"))
                        file = file.getParentDirectory().getChildFile("SHseq_" + file.getFileName());
                        
                    if (!file.hasFileExtension("json"))
                        file = file.withFileExtension("json");
                        
                    processor.saveAllPatternsToJson(file);
                }
            });
        }
    }

private:
    ShequencerAudioProcessor& processor;
    std::unique_ptr<juce::FileChooser> fileChooser;
};

class MasterTriggerComponent : public juce::Component
{
public:
    MasterTriggerComponent(ShequencerAudioProcessor& p) : processor(p) { setOpaque(true); }
    
    juce::Colour getEffectiveColor() const {
        return processor.masterColor.isTransparent() ? Theme::masterColor : processor.masterColor;
    }
    
    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black);

        auto area = getLocalBounds();
        area.removeFromLeft(60); // Margin
        area.removeFromRight(130); // Match LaneComponent layout
        
        // Draw GATE Reset Button
        int btnH = 60;
        int btnW = 36;
        int btnX = 12;
        int btnY = (getHeight() - btnH) / 2;
        
        // Ensure it fits if component is small
        if (btnY < 0) { btnY = 0; btnH = getHeight(); }
        
        juce::Rectangle<int> resetBtnRect(btnX, btnY, btnW, btnH);
        
        g.setColour(getEffectiveColor());
        g.fillRect(resetBtnRect);
        
        g.setColour(juce::Colours::black);
        g.setFont(juce::FontOptions("Arial", 16.0f, juce::Font::bold));
        g.drawFittedText("GA\nTE", resetBtnRect, juce::Justification::centred, 2);
        
        // Draw Length Control (Col A)
        int rightMarginX = getWidth() - 120;
        int col1_X = rightMarginX;
        
        // Top Row: Shift L | Length | Shift R
        int topRowH = 24;
        int topRowY = 5; // Shifted down by 5px
        
        juce::Rectangle<int> shiftL(col1_X, topRowY, 20, topRowH);
        shiftL = shiftL.reduced(1);

        juce::Rectangle<int> lenRect(col1_X + 20, topRowY, 40, topRowH);
        lenRect = lenRect.reduced(1);

        juce::Rectangle<int> shiftR(col1_X + 60, topRowY, 20, topRowH);
        shiftR = shiftR.reduced(1);

        g.setColour(getEffectiveColor());
        g.fillRect(lenRect);
        g.setColour(juce::Colours::black);
        g.fillRect(lenRect.reduced(1));
        g.setColour(getEffectiveColor());
        g.setFont(juce::FontOptions("Arial", 12.0f, juce::Font::bold));
        g.drawText(juce::String(processor.masterLength), lenRect, juce::Justification::centred);
        
        auto drawTriangle = [&](juce::Rectangle<int> r, bool left) {
            juce::Path p;
            float w = (float)r.getWidth();
            float th = (float)r.getHeight();
            float x = (float)r.getX();
            float y = (float)r.getY();
            
            float cx = x + w * 0.5f;
            float cy = y + th * 0.5f;
            float s = w * 0.3f;
            
            if (left)
            {
                p.addTriangle(cx + s, cy - s, cx + s, cy + s, cx - s, cy);
            }
            else
            {
                p.addTriangle(cx - s, cy - s, cx - s, cy + s, cx + s, cy);
            }
            
            g.setColour(juce::Colours::black);
            g.fillRect(r);
            
            g.setColour(getEffectiveColor());
            g.drawRect(r, 1);
            g.fillPath(p);
        };
        
        g.setColour(getEffectiveColor());
        drawTriangle(shiftL, true);
        drawTriangle(shiftR, false);
        
        // Color Picker Button (Col 3)
        int col3_X = rightMarginX + 80;
        int colorBtnSize = 16;
        int colorBtnX = col3_X + (40 - colorBtnSize) / 2;
        int colorBtnY = (getHeight() - colorBtnSize) / 2;
        
        juce::Rectangle<int> colorBtnRect(colorBtnX, colorBtnY, colorBtnSize, colorBtnSize);
        
        g.setColour(getEffectiveColor());
        g.fillEllipse(colorBtnRect.toFloat());
        
        // Probability Slider (Below Top Row)
        // Full width of the 3 buttons (20+40+20 = 80)
        // Height 14px
        int sliderY = topRowY + topRowH + 1; // 5 + 24 + 1 = 30
        int sliderH = 12; // Reduced slightly to ensure it fits
        juce::Rectangle<int> probRect(col1_X, sliderY, 80, sliderH);
        probRect = probRect.reduced(1);
        
        g.setColour(getEffectiveColor().withAlpha(0.2f));
        g.fillRect(probRect); // Background track
        
        g.setColour(getEffectiveColor());
        g.fillRect(probRect);
        g.setColour(juce::Colours::black);
        g.fillRect(probRect.reduced(1));
        g.setColour(getEffectiveColor());
        
        if (processor.masterProbability > 0)
        {
            float fillW = (float)probRect.getWidth() * ((float)processor.masterProbability / 100.0f);
            g.fillRect((float)probRect.getX(), (float)probRect.getY(), fillW, (float)probRect.getHeight());
        }
        
        // Steps
        float stepWidth = area.getWidth() / 16.0f;
        
        for (size_t i = 0; i < 16; ++i)
        {
            auto stepArea = area.removeFromLeft((int)stepWidth);
            
            // Make Square and Center Vertically
            int size = stepArea.getWidth();
            int yOffset = (stepArea.getHeight() - size) / 2;
            auto squareArea = stepArea.withY(stepArea.getY() + yOffset).withHeight(size).reduced(2);
            
            // Dim if outside loop
            float alpha = (i < (size_t)processor.masterLength) ? 1.0f : 0.3f;
            
            g.setColour(getEffectiveColor().withAlpha(alpha));
            g.fillRect(squareArea);
            
            if (!processor.masterTriggers[i])
            {
                g.setColour(juce::Colours::black);
                g.fillRect(squareArea.reduced(1));
            }
            else
            {
                // Draw PROB indicator (Black Square - Hole)
                if (processor.masterProbEnabled[i])
                {
                    g.setColour(juce::Colours::black);
                    g.fillRect(squareArea.withSizeKeepingCentre(10, 10));
                }
            }
            
            // Highlight current master step
            if (i == (size_t)processor.currentMasterStep)
            {
                g.setColour(getEffectiveColor().darker(1.0f).withAlpha(0.5f));
                g.fillRect(squareArea);
            }
        }
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        auto area = getLocalBounds();
        area.removeFromLeft(60);
        
        // Handle GATE Reset Button (Left Area)
        if (e.x < 60)
        {
            if (e.mods.isAltDown())
            {
                processor.resetAllLanes();
            }
            else
            {
                processor.masterTriggers.fill(false);
                processor.masterLength = 16;
            }
            repaint();
            return;
        }
        
        int rightMarginX = getWidth() - 120;
        int col1_X = rightMarginX;
        
        int topRowH = 24;
        int topRowY = 5;
        
        // Handle Length (Col A)
        if (e.x >= col1_X + 20 && e.x < col1_X + 60 && e.y >= topRowY && e.y < topRowY + topRowH)
        {
            isDraggingLength = true;
            lastMouseX = e.x;
            lastMouseY = e.y;
            return;
        }
        
        // Handle Shift L
        if (e.x >= col1_X && e.x < col1_X + 20 && e.y >= topRowY && e.y < topRowY + topRowH)
        {
            processor.shiftMasterTriggers(-1);
            repaint();
            return;
        }
        
        // Handle Shift R
        if (e.x >= col1_X + 60 && e.x < col1_X + 80 && e.y >= topRowY && e.y < topRowY + topRowH)
        {
            processor.shiftMasterTriggers(1);
            repaint();
            return;
        }
        
        // Handle Color Picker (Col 3)
        int col3_X = rightMarginX + 80;
        // Hit area larger
        if (e.x >= col3_X && e.x < col3_X + 40 && e.y >= 0 && e.y < getHeight())
        {
            if (e.mods.isShiftDown())
            {
                processor.masterColor = juce::Colours::transparentBlack;
                repaint();
            }
            else
            {
                auto* client = new ColorPickerClient(processor.masterColor, getEffectiveColor(), [this](){ repaint(); });
                juce::CallOutBox::launchAsynchronously(std::unique_ptr<juce::Component>(client), getScreenBounds().translated(e.x, e.y), nullptr);
            }
            return;
        }
        
        // Handle Probability Slider
        int sliderY = topRowY + topRowH + 1; // 30
        int sliderH = 14; // Hit area slightly larger than visual 12
        if (e.x >= col1_X && e.x < col1_X + 80 && e.y >= sliderY && e.y < sliderY + sliderH)
        {
            isDraggingProbability = true;
            updateProbability(e.x, col1_X, 80);
            return;
        }
            
        area.removeFromRight(130); // Match LaneComponent layout
        
        float stepWidth = area.getWidth() / 16.0f;
        int stepIdx = (int)((e.x - 60) / stepWidth);
        
        if (stepIdx >= 0 && stepIdx < 16)
        {
            if (e.mods.isShiftDown())
            {
                // Toggle Probability Step
                targetProbState = !processor.masterProbEnabled[(size_t)stepIdx];
                processor.masterProbEnabled[(size_t)stepIdx] = targetProbState;
                
                // If enabling probability on an empty step, turn the step ON
                if (targetProbState && !processor.masterTriggers[(size_t)stepIdx])
                {
                    processor.masterTriggers[(size_t)stepIdx] = true;
                }
                
                lastEditedStep = stepIdx;
                repaint();
                return;
            }

            lastEditedStep = stepIdx;
            targetTriggerState = !processor.masterTriggers[(size_t)stepIdx];
            processor.masterTriggers[(size_t)stepIdx] = targetTriggerState;
            
            // If turning OFF gate, also disable probability to avoid "small yellow square" state
            if (!targetTriggerState)
                processor.masterProbEnabled[(size_t)stepIdx] = false;
                
            repaint();
        }
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (isDraggingLength)
        {
            int delta = (e.x - lastMouseX) - (e.y - lastMouseY);
            if (std::abs(delta) > 5)
            {
                if (delta > 0) processor.masterLength = juce::jmin(16, processor.masterLength + 1);
                else processor.masterLength = juce::jmax(1, processor.masterLength - 1);
                
                lastMouseX = e.x;
                lastMouseY = e.y;
                repaint();
            }
            return;
        }
        
        if (isDraggingProbability)
        {
            int rightMarginX = getWidth() - 120;
            int col1_X = rightMarginX;
            updateProbability(e.x, col1_X, 80);
            return;
        }

        // if (e.mods.isShiftDown()) return; // Allow shift-drag

        if (e.x >= 60 && e.x <= getWidth() - 130)
        {
            auto area = getLocalBounds();
            area.removeFromLeft(60);
            area.removeFromRight(130);
            float stepWidth = area.getWidth() / 16.0f;
            int stepIdx = (int)((e.x - 60) / stepWidth);
            
            if (stepIdx >= 0 && stepIdx < 16 && stepIdx != lastEditedStep)
            {
                lastEditedStep = stepIdx;
                
                if (e.mods.isShiftDown())
                {
                    // Dragging Probability
                    processor.masterProbEnabled[(size_t)stepIdx] = targetProbState;
                    if (targetProbState && !processor.masterTriggers[(size_t)stepIdx])
                        processor.masterTriggers[(size_t)stepIdx] = true;
                }
                else
                {
                    // Dragging Gate
                    processor.masterTriggers[(size_t)stepIdx] = targetTriggerState;
                    if (!targetTriggerState)
                        processor.masterProbEnabled[(size_t)stepIdx] = false;
                }
                repaint();
            }
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        isDraggingLength = false;
        isDraggingProbability = false;
    }

private:
    ShequencerAudioProcessor& processor;
    int lastEditedStep = -1;
    bool targetTriggerState = false;
    bool targetProbState = false;
    
    bool isDraggingLength = false;
    bool isDraggingProbability = false;
    int lastMouseX = 0;
    int lastMouseY = 0;
    
    void updateProbability(int mouseX, int x, int w)
    {
        float norm = (float)(mouseX - x) / (float)w;
        norm = juce::jlimit(0.0f, 1.0f, norm);
        processor.masterProbability = (int)(norm * 100.0f);
        repaint();
    }
};

class BankSelectorComponent : public juce::Component
{
public:
    BankSelectorComponent(ShequencerAudioProcessor& p) : processor(p) {}
    
    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().reduced(2);
        int w = area.getWidth() / 2;
        int h = area.getHeight() / 2;
        
        const char* labels[] = { "A", "B", "C", "D" };
        
        for (int i = 0; i < 4; ++i)
        {
            int r = i / 2;
            int c = i % 2;
            
            juce::Rectangle<int> btnRect(area.getX() + c * w, area.getY() + r * h, w, h);
            btnRect = btnRect.reduced(1);
            
            bool isSelected = (processor.currentBank == i);
            
            g.setColour(Theme::slotsColor.withAlpha(isSelected ? 1.0f : 0.2f));
            if (isSelected) g.fillRect(btnRect);
            else
            {
                g.setColour(Theme::slotsColor.withAlpha(0.2f));
                g.fillRect(btnRect);
                g.setColour(juce::Colours::black);
                g.fillRect(btnRect.reduced(1));
            }
            
            g.setColour(isSelected ? juce::Colours::black : Theme::slotsColor);
            g.setFont(juce::FontOptions("Arial", 12.0f, juce::Font::bold));
            g.drawText(labels[i], btnRect, juce::Justification::centred);
        }
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        auto area = getLocalBounds().reduced(2);
        int w = area.getWidth() / 2;
        int h = area.getHeight() / 2;
        
        int c = (e.x - area.getX()) / w;
        int r = (e.y - area.getY()) / h;
        
        if (c >= 0 && c < 2 && r >= 0 && r < 2)
        {
            int bankIdx = r * 2 + c;
            processor.currentBank = bankIdx;
            repaint();
            // Notify parent to repaint slots? Parent handles it via timer/repaint
        }
    }

private:
    ShequencerAudioProcessor& processor;
};

class PatternSlotsComponent : public juce::Component
{
public:
    PatternSlotsComponent(ShequencerAudioProcessor& p) : processor(p) {}
    
    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        float stepWidth = area.getWidth() / 16.0f;
        
        for (size_t i = 0; i < 16; ++i)
        {
            auto slotRect = area.removeFromLeft((int)stepWidth).reduced(2);
            
            bool hasPattern = !processor.patternBanks[(size_t)processor.currentBank][i].isEmpty;
            
            g.setColour(Theme::slotsColor);
            g.fillRect(slotRect);
            
            if (!hasPattern)
            {
                g.setColour(juce::Colours::black);
                g.fillRect(slotRect.reduced(1));
            }
            
            // Draw Loaded Indicator
            if (processor.currentBank == processor.loadedBank && i == (size_t)processor.loadedSlot)
            {
                int globalSlotNum = (processor.currentBank * 16) + (int)i + 1;
                
                g.setColour(juce::Colours::black);
                g.setFont(juce::FontOptions("Arial", (float)slotRect.getHeight() * 0.8f, juce::Font::bold));
                g.drawText(juce::String(globalSlotNum), slotRect, juce::Justification::centred);
            }
        }
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        float stepWidth = getWidth() / 16.0f;
        int slotIdx = (int)(e.x / stepWidth);
        
        if (slotIdx >= 0 && slotIdx < 16)
        {
            if (e.mods.isShiftDown())
            {
                // Save
                processor.savePattern(processor.currentBank, slotIdx);
            }
            else if (e.mods.isAltDown())
            {
                // Clear
                processor.clearPattern(processor.currentBank, slotIdx);
                
                // If we are clearing the currently loaded pattern, reset the live state too
                if (processor.currentBank == processor.loadedBank && slotIdx == processor.loadedSlot)
                {
                    processor.resetAllLanes();
                }
            }
            else
            {
                // Load
                processor.loadPattern(processor.currentBank, slotIdx);
            }
            repaint();
        }
    }

private:
    ShequencerAudioProcessor& processor;
};

class PageSelectorComponent : public juce::Component
{
public:
    std::function<void()> onPageChanged;
    int currentPage = 0;
    
    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().reduced(2);
        g.setColour(Theme::slotsColor);
        g.fillRect(area);
        
        g.setColour(juce::Colours::black);
        g.setFont(juce::FontOptions("Arial", 20.0f, juce::Font::bold));
        g.drawText(currentPage == 0 ? "I" : "II", area, juce::Justification::centred);
    }
    
    void mouseDown(const juce::MouseEvent&) override
    {
        currentPage = !currentPage;
        if (onPageChanged) onPageChanged();
        repaint();
    }
};

class ShequencerAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    ShequencerAudioProcessorEditor (ShequencerAudioProcessor&);
    ~ShequencerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    
    void updatePageVisibility();
    int currentPage = 0;

private:
    juce::VBlankAttachment vBlankAttachment;
    juce::Component mainContainer;
    
    MasterTriggerComponent masterTriggerComp;
    std::unique_ptr<LaneComponent> noteLaneComp;
    std::unique_ptr<LaneComponent> octaveLaneComp;
    std::unique_ptr<LaneComponent> velocityLaneComp;
    std::unique_ptr<LaneComponent> lengthLaneComp;
    
    std::unique_ptr<LaneComponent> ccLane1Comp;
    std::unique_ptr<LaneComponent> ccLane2Comp;
    std::unique_ptr<LaneComponent> ccLane3Comp;
    std::unique_ptr<LaneComponent> ccLane4Comp;
    
    BankSelectorComponent bankSelectorComp;
    PatternSlotsComponent patternSlotsComp;
    ShuffleComponent shuffleComp;
    FileOpsComponent fileOpsComp;
    BuildNumberComponent buildNumberComp;
    PageSelectorComponent pageSelectorComp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShequencerAudioProcessorEditor)
};
