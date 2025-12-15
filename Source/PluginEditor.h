#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace Theme
{
    static const juce::Colour noteColor(0xFF3FA2FE);
    static const juce::Colour octaveColor(0xFFF8A43D);
    static const juce::Colour velocityColor(0xFFF72DA3);
    static const juce::Colour lengthColor(0xFFA228FF);
    static const juce::Colour masterColor(0xFFE0E329);
    
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
        
        int rightMarginX = area.getWidth(); // Since we removed right 40, area is now the middle part?
        // Wait, area was getLocalBounds().
        // auto rightArea = area.removeFromRight(40);
        // So 'area' is now the middle part.
        // We need to draw in the original right area.
        
        int rightX = getWidth() - 40;
        
        juce::Rectangle<int> valLoopRect(rightX, barTopY, 40, triggerHeight);
        valLoopRect = valLoopRect.reduced(2);
        
        juce::Rectangle<int> trigLoopRect(rightX, h - triggerHeight, 40, triggerHeight);
        trigLoopRect = trigLoopRect.reduced(2);
        
        g.setColour(laneColor);
        g.setFont(juce::Font("Arial", 12.0f, juce::Font::bold));
        
        // Draw Value Loop Control (Outline only)
        g.drawRect(valLoopRect);
        g.drawText(juce::String(laneData.valueLoopLength), valLoopRect, juce::Justification::centred);
        
        // Draw Trigger Loop Control (Outline only)
        g.drawRect(trigLoopRect);
        g.drawText(juce::String(laneData.triggerLoopLength), trigLoopRect, juce::Justification::centred);

        // Draw Random Button
        int midY = h / 2;
        juce::Rectangle<int> randomRect(rightX, midY - 12, 40, 24);
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
        area.removeFromRight(40);
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
            if (i == laneData.currentValueStep)
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
        auto rightArea = area.removeFromRight(40);
        
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
            return;
        }
        
        // Handle Right Loop Lengths
        if (e.x > getWidth() - 40)
        {
            int triggerHeight = 24;
            int h = getHeight();
            int barAreaHeight = h - triggerHeight;
            int reducedBarHeight = barAreaHeight;
            int barTopY = 0;
            
            // Value Loop (Top)
            if (e.y >= barTopY && e.y < barTopY + triggerHeight)
            {
                isDraggingValueLoop = true;
                lastMouseY = e.y;
                lastMouseX = e.x;
            }
            // Trigger Loop (Bottom)
            else if (e.y >= h - triggerHeight)
            {
                isDraggingTriggerLoop = true;
                lastMouseY = e.y;
                lastMouseX = e.x;
            }
            // Random Button (Middle)
            else
            {
                int midY = h / 2;
                if (e.y >= midY - 12 && e.y < midY + 12)
                {
                    randomizeValues();
                }
            }
            return;
        }
        
        float stepWidth = area.getWidth() / 16.0f;
        int stepIdx = (int)((e.x - 60) / stepWidth);
        
        if (stepIdx >= 0 && stepIdx < 16)
        {
            lastEditedStep = stepIdx;
            
            int triggerHeight = 24;
            
            // Check if clicked on Bar or Button
            if (e.y < getHeight() - triggerHeight)
            {
                isDraggingTrigger = false;
                updateValue(stepIdx, e.y, getHeight());
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

        // Only drag values/triggers in the main area
        if (e.x >= 60 && e.x <= getWidth() - 40)
        {
            auto area = getLocalBounds();
            area.removeFromLeft(60);
            area.removeFromRight(40);
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
                    updateValue(stepIdx, e.y, getHeight());
                }
                repaint();
            }
            else if (stepIdx == lastEditedStep && !isDraggingTrigger)
            {
                // Allow continuous value adjustment on the same step
                updateValue(stepIdx, e.y, getHeight());
                repaint();
            }
        }
    }
    
    void updateValue(int stepIdx, int y, int h)
    {
        int triggerHeight = 24;
        int barAreaHeight = h - triggerHeight;
        int reducedBarHeight = barAreaHeight;
        int barTop = 0;
        
        // Map Y to the reduced bar area
        // If Y is above the bar area (in the empty space), treat as max value
        int relativeY = y - barTop;
        
        float norm = 1.0f - (float)relativeY / (float)reducedBarHeight;
        norm = juce::jlimit(0.0f, 1.0f, norm);
        
        int val = minVal + (int)(norm * (maxVal - minVal));
        val = juce::jlimit(minVal, maxVal, val);
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
};

class MasterTriggerComponent : public juce::Component
{
public:
    MasterTriggerComponent(ShequencerAudioProcessor& p) : processor(p) {}
    
    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        auto leftArea = area.removeFromLeft(60); // Margin
        auto rightArea = area.removeFromRight(40); // Match LaneComponent layout
        
        // Draw Name
        g.setColour(Theme::masterColor);
        g.setFont(juce::Font("Arial", 14.0f, juce::Font::bold));
        g.drawText("MAIN", leftArea, juce::Justification::centred);
        
        // Draw Length Control
        juce::Rectangle<int> lenRect(getWidth() - 40, 0, 40, getHeight());
        lenRect = lenRect.withSizeKeepingCentre(40, 24).reduced(2);

        g.setColour(Theme::masterColor);
        g.drawRect(lenRect);
        g.setFont(juce::Font("Arial", 12.0f, juce::Font::bold));
        g.drawText(juce::String(processor.masterLength), lenRect, juce::Justification::centred);
        
        float stepWidth = area.getWidth() / 16.0f;
        
        for (int i = 0; i < 16; ++i)
        {
            auto stepArea = area.removeFromLeft(stepWidth).reduced(2);
            
            // Dim if outside loop
            float alpha = (i < processor.masterLength) ? 1.0f : 0.3f;
            
            g.setColour(Theme::masterColor.withAlpha(0.33f * alpha));
            g.drawRect(stepArea);
            
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
        area.removeFromLeft(60);
        
        // Handle Right Loop Length
        if (e.x > getWidth() - 40)
        {
            isDraggingLength = true;
            lastMouseX = e.x;
            lastMouseY = e.y;
            return;
        }
        
        area.removeFromRight(40); // Match LaneComponent layout
        
        float stepWidth = area.getWidth() / 16.0f;
        int stepIdx = (int)((e.x - 60) / stepWidth);
        
        if (stepIdx >= 0 && stepIdx < 16)
        {
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

        if (e.x >= 60 && e.x <= getWidth() - 40)
        {
            auto area = getLocalBounds();
            area.removeFromLeft(60);
            area.removeFromRight(40);
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShequencerAudioProcessorEditor)
};
