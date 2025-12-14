#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class LaneComponent : public juce::Component
{
public:
    LaneComponent(SequencerLane& lane, juce::String name, juce::Colour color, int minVal, int maxVal)
        : laneData(lane), laneName(name), laneColor(color), minVal(minVal), maxVal(maxVal)
    {
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        
        // Draw Name
        g.setColour(juce::Colours::white);
        g.drawText(laneName, area.removeFromLeft(60), juce::Justification::centredLeft);
        
        // Draw Loop Length
        auto loopArea = area.removeFromRight(40);
        g.setColour(juce::Colours::lightgrey);
        g.drawText("L: " + juce::String(laneData.loopLength), loopArea, juce::Justification::centred);

        // Draw Steps
        float stepWidth = area.getWidth() / 16.0f;
        
        for (int i = 0; i < 16; ++i)
        {
            auto stepArea = area.removeFromLeft(stepWidth); // No gap
            
            // Dim if outside loop
            float alpha = (i < laneData.loopLength) ? 1.0f : 0.3f;
            
            // Draw Advance Trigger Button (Fixed 15px height at bottom)
            auto btnArea = stepArea.removeFromBottom(15);
            
            // Draw Value Bar (Remaining top part)
            auto barArea = stepArea;
            
            // Background
            g.setColour(juce::Colours::darkgrey.withAlpha(0.3f * alpha));
            g.fillRect(barArea);
            
            float normVal = (float)(laneData.values[i] - minVal) / (float)(maxVal - minVal);
            if (maxVal == minVal) normVal = 0.5f; // Avoid div by zero
            
            int barHeight = (int)(barArea.getHeight() * normVal);
            auto fillArea = barArea.removeFromBottom(barHeight);
            
            g.setColour(laneColor.withAlpha(alpha));
            g.fillRect(fillArea);
            
            // Highlight current step
            if (i == laneData.currentStep)
            {
                g.setColour(juce::Colours::white.withAlpha(0.3f));
                g.fillRect(barArea); // Fill the whole slot background
            }

            // Draw Advance Trigger Button
            g.setColour(juce::Colours::grey.withAlpha(alpha));
            g.drawRect(btnArea);
            
            if (laneData.advanceTriggers[i])
            {
                g.setColour(laneColor.brighter().withAlpha(alpha));
                g.fillRect(btnArea.reduced(2));
            }
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        auto area = getLocalBounds();
        area.removeFromLeft(60); // Name
        auto loopArea = area.removeFromRight(40); // Loop Len
        
        if (loopArea.contains(e.getPosition()))
        {
            // Change Loop Length
            // Simple drag logic: click top half -> increase, bottom -> decrease?
            // Or just cycle?
            // Let's do: if y < height/2 increment, else decrement
            if (e.position.y < loopArea.getHeight() / 2)
                laneData.loopLength = juce::jmin(16, laneData.loopLength + 1);
            else
                laneData.loopLength = juce::jmax(1, laneData.loopLength - 1);
                
            repaint();
            return;
        }
        
        float stepWidth = area.getWidth() / 16.0f;
        int stepIdx = (int)((e.x - 60) / stepWidth);
        
        if (stepIdx >= 0 && stepIdx < 16)
        {
            // Check if clicked on Bar or Button
            // Button is bottom 15px
            if (e.y < getHeight() - 15)
            {
                // Set Value
                float norm = 1.0f - (float)e.y / (float)(getHeight() - 15);
                int val = minVal + (int)(norm * (maxVal - minVal));
                laneData.values[stepIdx] = juce::jlimit(minVal, maxVal, val);
            }
            else
            {
                // Toggle Trigger
                laneData.advanceTriggers[stepIdx] = !laneData.advanceTriggers[stepIdx];
            }
            repaint();
        }
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        mouseDown(e);
    }

private:
    SequencerLane& laneData;
    juce::String laneName;
    juce::Colour laneColor;
    int minVal;
    int maxVal;
};

class MasterTriggerComponent : public juce::Component
{
public:
    MasterTriggerComponent(ShequencerAudioProcessor& p) : processor(p) {}
    
    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        area.removeFromLeft(60); // Margin
        area.removeFromRight(40); // Match LaneComponent layout
        
        float stepWidth = area.getWidth() / 16.0f;
        
        for (int i = 0; i < 16; ++i)
        {
            auto stepArea = area.removeFromLeft(stepWidth).reduced(2);
            
            g.setColour(juce::Colours::grey);
            g.drawRect(stepArea);
            
            if (processor.masterTriggers[i])
            {
                g.setColour(juce::Colour(0xFFE0E329));
                g.fillRect(stepArea.reduced(2));
            }
            
            // Highlight current master step
            if (i == processor.currentMasterStep)
            {
                g.setColour(juce::Colours::red.withAlpha(0.5f));
                g.drawRect(stepArea, 2.0f);
            }
        }
    }
    
    void mouseDown(const juce::MouseEvent& e) override
    {
        auto area = getLocalBounds();
        area.removeFromLeft(60);
        area.removeFromRight(40); // Match LaneComponent layout
        
        float stepWidth = area.getWidth() / 16.0f;
        int stepIdx = (int)((e.x - 60) / stepWidth);
        
        if (stepIdx >= 0 && stepIdx < 16)
        {
            processor.masterTriggers[stepIdx] = !processor.masterTriggers[stepIdx];
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShequencerAudioProcessorEditor)
};
