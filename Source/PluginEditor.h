#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>
#include "PluginProcessor.h"

namespace Theme
{
    static const juce::Colour noteColor(0xFF3FA2FE);
    static const juce::Colour octaveColor(0xFFF8A43D);
    static const juce::Colour velocityColor(0xFFF72DA3);
    static const juce::Colour lengthColor(0xFFA228FF);
    static const juce::Colour masterColor(0xFFE0E329);
    static const juce::Colour slotsColor(0xFF40FF99);
    
    static juce::Font getValueFont() { return juce::Font("Arial", 35.0f, juce::Font::bold); }
}

class LaneComponent : public juce::Component
{
public:
    LaneComponent(SequencerLane& lane, juce::String name, juce::Colour color, int minVal, int maxVal)
        : laneData(lane), laneName(name), laneColor(color), minVal(minVal), maxVal(maxVal)
    {
    }
    
    std::function<juce::String(int)> valueFormatter;
    std::function<void(int, bool)> onStepShiftClicked;
    std::function<void(bool)> onResetClicked;
    std::function<void(bool)> onLabelClicked; // bool isShift
    
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
        auto area = getLocalBounds();
        int h = getHeight();
        int triggerHeight = 24;
        int barAreaHeight = h - triggerHeight;
        int reducedBarHeight = barAreaHeight;
        int barTopY = 0;
        
        // Left Controls (Toggles)
        auto leftArea = area.removeFromLeft(60);
        
        // Draw Toggles
        // L aligned with step sequencer buttons (bottom)
        // Width reduced to 2/3 (40px), centered in 60px area (x offset 10)
        juce::Rectangle<int> localToggle(10, h - triggerHeight, 40, triggerHeight);
        localToggle = localToggle.reduced(2);
        
        // M same size, aligned with Y of bar-sequencers (upper border)
        juce::Rectangle<int> masterToggle(10, barTopY, 40, triggerHeight);
        masterToggle = masterToggle.reduced(2);
        
        // Draw Name (Between M and L)
        int labelTop = barTopY + triggerHeight;
        int labelBottom = h - triggerHeight;
        juce::Rectangle<int> labelRect(0, labelTop, 60, labelBottom - labelTop);

        g.setColour(laneColor);
        g.setFont(juce::Font("Arial", 14.0f, juce::Font::bold));
        g.drawText(laneName, labelRect, juce::Justification::centred);
        
        // Master Toggle (Yellow)
        g.setColour(Theme::masterColor.withAlpha(laneData.enableMasterSource ? 1.0f : 0.33f));
        g.fillRect(masterToggle);
        g.setColour(Theme::masterColor);
        g.drawRect(masterToggle);
        
        // Local Toggle (Lane Color)
        g.setColour(laneColor.withAlpha(laneData.enableLocalSource ? 1.0f : 0.33f));
        g.fillRect(localToggle);
        g.setColour(laneColor);
        g.drawRect(localToggle);

        // Right Controls (Loop Lengths)
        // Align with Left Controls (M/L)
        // V aligned with M (top)
        // T aligned with L (bottom)
        // Width 40px, centered in right margin (assuming right margin is 40px, so x = width - 40)
        // Actually, let's center it in the 40px area.
        
        // New Layout: 120px margin
        // Col A (Width-120 to Width-80): Shift Left (20px, right aligned)
        // Col B (Width-80 to Width-40): Loop/Random (40px)
        // Col C (Width-40 to Width): Shift Right (20px, left aligned)
        
        int colB_X = getWidth() - 90; // Moved left by 10px to reduce gap
        
        juce::Rectangle<int> valLoopRect(colB_X, barTopY, 40, triggerHeight);
        valLoopRect = valLoopRect.reduced(2);
        
        juce::Rectangle<int> trigLoopRect(colB_X, h - triggerHeight, 40, triggerHeight);
        trigLoopRect = trigLoopRect.reduced(2);
        
        // Shift Buttons (Reduced by 2 to match loop button height)
        juce::Rectangle<int> valShiftL(colB_X - 20, barTopY, 20, triggerHeight);
        valShiftL = valShiftL.reduced(2);
        
        juce::Rectangle<int> valShiftR(colB_X + 40, barTopY, 20, triggerHeight);
        valShiftR = valShiftR.reduced(2);
        
        juce::Rectangle<int> trigShiftL(colB_X - 20, h - triggerHeight, 20, triggerHeight);
        trigShiftL = trigShiftL.reduced(2);
        
        juce::Rectangle<int> trigShiftR(colB_X + 40, h - triggerHeight, 20, triggerHeight);
        trigShiftR = trigShiftR.reduced(2);
        
        // Reset Button (Small Circle)
        // Located after right shift buttons (new column)
        int midY = h / 2;
        int resetX = colB_X + 75;
        int resetY = midY;
        float resetRadius = 4.0f;
        
        g.setColour(laneColor);
        g.drawEllipse(resetX - resetRadius, resetY - resetRadius, resetRadius * 2, resetRadius * 2, 1.0f);

        g.setColour(laneColor);
        g.setFont(juce::Font("Arial", 12.0f, juce::Font::bold));
        
        // Draw Value Loop Control (Outline only)
        g.drawRect(valLoopRect);
        g.drawText(juce::String(laneData.valueLoopLength), valLoopRect, juce::Justification::centred);
        
        // Draw Trigger Loop Control (Outline only)
        g.drawRect(trigLoopRect);
        g.drawText(juce::String(laneData.triggerLoopLength), trigLoopRect, juce::Justification::centred);
        
        // Draw Shift Triangles
        auto drawTriangle = [&](juce::Rectangle<int> r, bool left) {
            juce::Path p;
            float w = (float)r.getWidth();
            float h = (float)r.getHeight();
            float x = (float)r.getX();
            float y = (float)r.getY();
            
            float cx = x + w * 0.5f;
            float cy = y + h * 0.5f;
            float s = w * 0.3f;
            
            if (left)
            {
                p.addTriangle(cx + s, cy - s, cx + s, cy + s, cx - s, cy);
            }
            else
            {
                p.addTriangle(cx - s, cy - s, cx - s, cy + s, cx + s, cy);
            }
            
            g.drawRect(r);
            g.fillPath(p);
        };
        
        drawTriangle(valShiftL, true);
        drawTriangle(valShiftR, false);
        drawTriangle(trigShiftL, true);
        drawTriangle(trigShiftR, false);

        // Draw Random Button
        juce::Rectangle<int> randomRect(colB_X, midY - 12, 40, 24);
        randomRect = randomRect.reduced(2);
        
        g.setColour(laneColor);
        g.drawRect(randomRect);
        
        // Draw Random Icon (4 bars)
        float barH[] = { 0.5f, 0.25f, 1.0f, 0.33f };
        int numBars = 4;
        int padding = 4;
        auto iconArea = randomRect.reduced(padding);
        float barWidth = iconArea.getWidth() / (float)numBars;
        
        for(int i=0; i<numBars; ++i)
        {
            float bh = iconArea.getHeight() * barH[i];
            float bx = iconArea.getX() + i * barWidth;
            float by = iconArea.getBottom() - bh;
            g.fillRect((float)bx + 1.0f, by, barWidth - 2.0f, bh);
        }

        // Draw Steps
        area.removeFromRight(120);
        float stepWidth = area.getWidth() / 16.0f;
        
        // Save area for overlay
        auto stepsArea = area;
        
        for (int i = 0; i < 16; ++i)
        {
            auto stepArea = area.removeFromLeft(stepWidth); // No gap
            
            // Draw Advance Trigger Button (Increased height to 24px)
            auto btnArea = stepArea.removeFromBottom(triggerHeight);
            
            // Draw Value Bar (Remaining top part)
            auto fullBarArea = stepArea;
            
            // Use full height
            int reducedBarHeight = fullBarArea.getHeight();
            auto effectiveBarArea = fullBarArea;
            
            // Dim if outside loop
            float valAlpha = (i < laneData.valueLoopLength) ? 1.0f : 0.3f;
            float trigAlpha = (i < laneData.triggerLoopLength) ? 1.0f : 0.3f;
            
            // Background for bar area
            g.setColour(laneColor.withAlpha(0.33f * valAlpha));
            g.fillRect(effectiveBarArea);
            
            float normVal = (float)(laneData.values[i] - minVal) / (float)(maxVal - minVal);
            if (maxVal == minVal) normVal = 0.5f; // Avoid div by zero
            
            int barHeight = (int)(effectiveBarArea.getHeight() * normVal);
            auto valueBarArea = effectiveBarArea;
            auto fillArea = valueBarArea.removeFromBottom(barHeight);
            
            g.setColour(laneColor.withAlpha(valAlpha));
            g.fillRect(fillArea);
            
            // Highlight current step
            if (i == laneData.activeValueStep)
            {
                g.setColour(laneColor.brighter(2.0f).withAlpha(0.5f));
                g.fillRect(effectiveBarArea);
            }

            // Draw Advance Trigger Button
            g.setColour(laneColor.withAlpha(0.33f * trigAlpha));
            g.drawRect(btnArea);
            
            if (laneData.triggers[i])
            {
                g.setColour(laneColor.withAlpha(trigAlpha));
                g.fillRect(btnArea.reduced(2));
            }
            
            // Highlight current trigger step
            if (i == laneData.currentTriggerStep)
            {
                g.setColour(laneColor.brighter(2.0f).withAlpha(0.5f));
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
            g.setColour(laneColor.withAlpha(valueDisplayAlpha));
            g.drawText(lastEditedValue, overlayRect, juce::Justification::centred, false);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        auto area = getLocalBounds();
        auto leftArea = area.removeFromLeft(60);
        auto rightArea = area.removeFromRight(120);
        
        // Handle Left Toggles
        if (e.x < 60)
        {
            int triggerHeight = 24;
            int h = getHeight();
            int barAreaHeight = h - triggerHeight;
            int reducedBarHeight = barAreaHeight;
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
                // Label Clicked
                if (onLabelClicked) onLabelClicked(e.mods.isShiftDown());
            }
            return;
        }
        
        // Handle Right Controls
        if (e.x > getWidth() - 120)
        {
            int triggerHeight = 24;
            int h = getHeight();
            int barTopY = 0;
            
            int colB_X = getWidth() - 90;
            
            // Check Shift Buttons
            // Value Shift L
            if (e.x >= colB_X - 20 && e.x < colB_X && e.y >= barTopY && e.y < barTopY + triggerHeight)
            {
                laneData.shiftValues(-1);
                repaint();
                return;
            }
            // Value Shift R
            if (e.x >= colB_X + 40 && e.x < colB_X + 60 && e.y >= barTopY && e.y < barTopY + triggerHeight)
            {
                laneData.shiftValues(1);
                repaint();
                return;
            }
            // Trigger Shift L
            if (e.x >= colB_X - 20 && e.x < colB_X && e.y >= h - triggerHeight)
            {
                laneData.shiftTriggers(-1);
                repaint();
                return;
            }
            // Trigger Shift R
            if (e.x >= colB_X + 40 && e.x < colB_X + 60 && e.y >= h - triggerHeight)
            {
                laneData.shiftTriggers(1);
                repaint();
                return;
            }
            
            // Value Loop (Top)
            if (e.x >= colB_X && e.x < colB_X + 40 && e.y >= barTopY && e.y < barTopY + triggerHeight)
            {
                isDraggingValueLoop = true;
                lastMouseY = e.y;
                lastMouseX = e.x;
            }
            // Trigger Loop (Bottom)
            else if (e.x >= colB_X && e.x < colB_X + 40 && e.y >= h - triggerHeight)
            {
                isDraggingTriggerLoop = true;
                lastMouseY = e.y;
                lastMouseX = e.x;
            }
            // Random Button (Middle)
            else
            {
                int midY = h / 2;
                if (e.x >= colB_X && e.x < colB_X + 40 && e.y >= midY - 12 && e.y < midY + 12)
                {
                    if (e.mods.isShiftDown())
                        randomizeTriggers();
                    else
                        randomizeValues();
                }
                
                // Reset Button
                int resetX = colB_X + 75;
                int resetY = midY;
                if (std::abs(e.x - resetX) < 10 && std::abs(e.y - resetY) < 10)
                {
                    if (onResetClicked) onResetClicked(e.mods.isAltDown());
                    repaint();
                }
            }
            return;
        }
        
        float stepWidth = area.getWidth() / 16.0f;
        int stepIdx = (int)((e.x - 60) / stepWidth);
        
        if (stepIdx >= 0 && stepIdx < 16 && e.x <= getWidth() - 120)
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
                
                int oldVal = laneData.values[stepIdx];
                updateValue(stepIdx, e.y, getHeight());
                int newVal = laneData.values[stepIdx];
                lastDragValue = newVal;
                
                if (e.mods.isAltDown())
                {
                    int delta = newVal - oldVal;
                    if (delta != 0)
                    {
                        for(int i=0; i<16; ++i)
                        {
                            if (i == stepIdx) continue;
                            laneData.values[i] = juce::jlimit(minVal, maxVal, laneData.values[i] + delta);
                        }
                    }
                }
            }
            else
            {
                isDraggingTrigger = true;
                targetTriggerState = !laneData.triggers[stepIdx];
                laneData.triggers[stepIdx] = targetTriggerState;
            }
            repaint();
        }
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
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
        if (e.x >= 60 && e.x <= getWidth() - 120)
        {
            auto area = getLocalBounds();
            area.removeFromLeft(60);
            area.removeFromRight(120);
            float stepWidth = area.getWidth() / 16.0f;
            int stepIdx = (int)((e.x - 60) / stepWidth);
            
            if (stepIdx >= 0 && stepIdx < 16 && stepIdx != lastEditedStep)
            {
                lastEditedStep = stepIdx;
                
                if (isDraggingTrigger)
                {
                    // Apply the same state we started with
                    laneData.triggers[stepIdx] = targetTriggerState;
                }
                else
                {
                    if (e.mods.isAltDown())
                    {
                        int oldVal = laneData.values[stepIdx];
                        updateValue(stepIdx, e.y, getHeight());
                        int newVal = laneData.values[stepIdx];
                        lastDragValue = newVal;
                        
                        int delta = newVal - oldVal;
                        if (delta != 0)
                        {
                            for(int i=0; i<16; ++i)
                            {
                                if (i == stepIdx) continue;
                                laneData.values[i] = juce::jlimit(minVal, maxVal, laneData.values[i] + delta);
                            }
                        }
                    }
                    else
                    {
                        updateValue(stepIdx, e.y, getHeight());
                    }
                }
                repaint();
            }
            else if (stepIdx == lastEditedStep && !isDraggingTrigger)
            {
                // Allow continuous value adjustment on the same step
                if (e.mods.isAltDown())
                {
                    int targetVal = getValueFromY(e.y, getHeight());
                    int delta = targetVal - lastDragValue;
                    
                    if (delta != 0)
                    {
                        for(int i=0; i<16; ++i)
                        {
                            laneData.values[i] = juce::jlimit(minVal, maxVal, laneData.values[i] + delta);
                        }
                        lastDragValue = laneData.values[stepIdx];
                        
                        if (valueFormatter)
                            lastEditedValue = valueFormatter(lastDragValue);
                        else
                            lastEditedValue = juce::String(lastDragValue);
                        valueDisplayAlpha = 2.0f;
                    }
                }
                else
                {
                    updateValue(stepIdx, e.y, getHeight());
                }
                repaint();
            }
        }
    }
    
    int getValueFromY(int y, int h)
    {
        int triggerHeight = 24;
        int barAreaHeight = h - triggerHeight;
        int reducedBarHeight = barAreaHeight;
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
        laneData.values[stepIdx] = val;
        
        if (valueFormatter)
            lastEditedValue = valueFormatter(val);
        else
            lastEditedValue = juce::String(val);
        
        valueDisplayAlpha = 2.0f;
    }
    
    void randomizeValues()
    {
        juce::Random r;
        for (int i = 0; i < 16; ++i)
        {
            laneData.values[i] = r.nextInt(maxVal - minVal + 1) + minVal;
        }
        repaint();
    }

    void randomizeTriggers()
    {
        juce::Random r;
        for (int i = 0; i < 16; ++i)
        {
            laneData.triggers[i] = r.nextBool();
        }
        repaint();
    }
    
    void mouseUp(const juce::MouseEvent&) override
    {
        isDraggingValueLoop = false;
        isDraggingTriggerLoop = false;
    }

private:
    SequencerLane& laneData;
    juce::String laneName;
    juce::Colour laneColor;
    int minVal;
    int maxVal;
    
    juce::String lastEditedValue;
    float valueDisplayAlpha = 0.0f;
    
    int lastEditedStep = -1;
    bool isDraggingTrigger = false;
    bool targetTriggerState = false;
    
    bool isDraggingValueLoop = false;
    bool isDraggingTriggerLoop = false;
    int lastMouseX = 0;
    int lastMouseY = 0;
    int lastDragValue = 0;
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
            g.drawRect(area);
        }
        
        g.setFont(juce::Font("Arial", 12.0f, juce::Font::bold));
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

class MasterTriggerComponent : public juce::Component
{
public:
    MasterTriggerComponent(ShequencerAudioProcessor& p) : processor(p) {}
    
    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        auto leftArea = area.removeFromLeft(60); // Margin
        auto rightArea = area.removeFromRight(120); // Match LaneComponent layout
        
        // Draw Name
        g.setColour(Theme::masterColor);
        g.setFont(juce::Font("Arial", 14.0f, juce::Font::bold));
        g.drawText("GATE", leftArea, juce::Justification::centred);
        
        // Draw Length Control (Col B - Aligned with Lane Loop Lengths)
        int colB_X = getWidth() - 90;
        
        juce::Rectangle<int> lenRect(colB_X, 0, 40, getHeight());
        lenRect = lenRect.withSizeKeepingCentre(40, 24).reduced(2);

        g.setColour(Theme::masterColor);
        g.drawRect(lenRect);
        g.setFont(juce::Font("Arial", 12.0f, juce::Font::bold));
        g.drawText(juce::String(processor.masterLength), lenRect, juce::Justification::centred);
        
        // Shift Buttons (Reduced by 2 to match loop button height)
        juce::Rectangle<int> shiftL(colB_X - 20, 0, 20, getHeight());
        shiftL = shiftL.withSizeKeepingCentre(20, 24).reduced(2);
        
        juce::Rectangle<int> shiftR(colB_X + 40, 0, 20, getHeight());
        shiftR = shiftR.withSizeKeepingCentre(20, 24).reduced(2);
        
        // Reset Button
        int resetX = colB_X + 75;
        int resetY = getHeight() / 2;
        float resetRadius = 4.0f;
        
        g.setColour(Theme::masterColor);
        g.drawEllipse(resetX - resetRadius, resetY - resetRadius, resetRadius * 2, resetRadius * 2, 1.0f);
        
        auto drawTriangle = [&](juce::Rectangle<int> r, bool left) {
            juce::Path p;
            float w = (float)r.getWidth();
            float h = (float)r.getHeight();
            float x = (float)r.getX();
            float y = (float)r.getY();
            
            float cx = x + w * 0.5f;
            float cy = y + h * 0.5f;
            float s = w * 0.3f;
            
            if (left)
            {
                p.addTriangle(cx + s, cy - s, cx + s, cy + s, cx - s, cy);
            }
            else
            {
                p.addTriangle(cx - s, cy - s, cx - s, cy + s, cx + s, cy);
            }
            
            g.drawRect(r);
            g.fillPath(p);
        };
        
        g.setColour(Theme::masterColor);
        drawTriangle(shiftL, true);
        drawTriangle(shiftR, false);
        
        float stepWidth = area.getWidth() / 16.0f;
        
        for (int i = 0; i < 16; ++i)
        {
            auto stepArea = area.removeFromLeft(stepWidth).reduced(2);
            
            // Dim if outside loop
            float alpha = (i < processor.masterLength) ? 1.0f : 0.3f;
            
            g.setColour(Theme::masterColor.withAlpha(0.33f * alpha));
            g.drawRect(stepArea);
            // 
            if (processor.masterTriggers[i])
            {
                g.setColour(Theme::masterColor.withAlpha(alpha));
                g.fillRect(stepArea.reduced(2));
            }
            
            // Highlight current master step
            if (i == processor.currentMasterStep)
            {
                g.setColour(Theme::masterColor.brighter(2.0f).withAlpha(0.5f));
                g.fillRect(stepArea);
            }
        }
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        auto area = getLocalBounds();
        auto leftArea = area.removeFromLeft(60);
        
        // Handle Label Click (Left Area)
        if (e.x < 60)
        {
            if (e.mods.isShiftDown())
            {
                processor.syncAllToBar();
            }
            else
            {
                // Clicking GATE without shift also syncs all (Global Transport)
                processor.syncAllToBar();
            }
            repaint();
            return;
        }
        
        int colB_X = getWidth() - 90;
        
        // Handle Length (Col B)
        if (e.x >= colB_X && e.x < colB_X + 40)
        {
            isDraggingLength = true;
            lastMouseX = e.x;
            lastMouseY = e.y;
            return;
        }
        
        // Handle Shift L
        if (e.x >= colB_X - 20 && e.x < colB_X)
        {
            processor.shiftMasterTriggers(-1);
            repaint();
            return;
        }
        
        // Handle Shift R
        if (e.x >= colB_X + 40 && e.x < colB_X + 60)
        {
            processor.shiftMasterTriggers(1);
            repaint();
            return;
        }
        
        // Handle Reset
        int resetX = colB_X + 75;
        int resetY = getHeight() / 2;
        if (std::abs(e.x - resetX) < 10 && std::abs(e.y - resetY) < 10)
        {
            if (e.mods.isAltDown())
            {
                processor.resetAllLanes();
            }
            else
            {
                processor.masterTriggers.fill(true);
                processor.masterLength = 16;
            }
            repaint();
            return;
        }
            
        area.removeFromRight(120); // Match LaneComponent layout
        
        float stepWidth = area.getWidth() / 16.0f;
        int stepIdx = (int)((e.x - 60) / stepWidth);
        
        if (stepIdx >= 0 && stepIdx < 16)
        {
            if (e.mods.isShiftDown())
            {
                processor.setGlobalStepIndex(stepIdx);
                repaint();
                return;
            }

            lastEditedStep = stepIdx;
            targetTriggerState = !processor.masterTriggers[stepIdx];
            processor.masterTriggers[stepIdx] = targetTriggerState;
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

        if (e.mods.isShiftDown()) return;

        if (e.x >= 60 && e.x <= getWidth() - 120)
        {
            auto area = getLocalBounds();
            area.removeFromLeft(60);
            area.removeFromRight(120);
            float stepWidth = area.getWidth() / 16.0f;
            int stepIdx = (int)((e.x - 60) / stepWidth);
            
            if (stepIdx >= 0 && stepIdx < 16 && stepIdx != lastEditedStep)
            {
                lastEditedStep = stepIdx;
                processor.masterTriggers[stepIdx] = targetTriggerState;
                repaint();
            }
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        isDraggingLength = false;
    }

private:
    ShequencerAudioProcessor& processor;
    int lastEditedStep = -1;
    bool targetTriggerState = false;
    
    bool isDraggingLength = false;
    int lastMouseX = 0;
    int lastMouseY = 0;
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
            else g.drawRect(btnRect);
            
            g.setColour(isSelected ? juce::Colours::black : Theme::slotsColor);
            g.setFont(juce::Font("Arial", 12.0f, juce::Font::bold));
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
        
        for (int i = 0; i < 16; ++i)
        {
            auto slotRect = area.removeFromLeft(stepWidth).reduced(2);
            
            bool hasPattern = !processor.patternBanks[processor.currentBank][i].isEmpty;
            
            g.setColour(Theme::slotsColor.withAlpha(hasPattern ? 0.8f : 0.2f));
            if (hasPattern) g.fillRect(slotRect);
            else g.drawRect(slotRect);
            
            // Draw Loaded Indicator
            if (processor.currentBank == processor.loadedBank && i == processor.loadedSlot)
            {
                g.setColour(juce::Colours::black);
                g.fillRect(slotRect.withSizeKeepingCentre(10, 10));
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

class ShequencerAudioProcessorEditor  : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    ShequencerAudioProcessorEditor (ShequencerAudioProcessor&);
    ~ShequencerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    ShequencerAudioProcessor& audioProcessor;
    
    MasterTriggerComponent masterTriggerComp;
    std::unique_ptr<LaneComponent> noteLaneComp;
    std::unique_ptr<LaneComponent> octaveLaneComp;
    std::unique_ptr<LaneComponent> velocityLaneComp;
    std::unique_ptr<LaneComponent> lengthLaneComp;
    
    BankSelectorComponent bankSelectorComp;
    PatternSlotsComponent patternSlotsComp;
    ShuffleComponent shuffleComp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShequencerAudioProcessorEditor)
};
