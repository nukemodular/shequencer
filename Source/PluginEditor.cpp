#include "PluginProcessor.h"
#include "PluginEditor.h"

ShequencerAudioProcessorEditor::ShequencerAudioProcessorEditor (ShequencerAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), masterTriggerComp(p)
{
    addAndMakeVisible(masterTriggerComp);
    
    noteLaneComp = std::make_unique<LaneComponent>(p.noteLane, "NOTE", Theme::noteColor, 0, 11);
    noteLaneComp->valueFormatter = [](int val) {
        const char* notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        if (val >= 0 && val < 12) return juce::String(notes[val]);
        return juce::String(val);
    };
    addAndMakeVisible(*noteLaneComp);
    
    octaveLaneComp = std::make_unique<LaneComponent>(p.octaveLane, "OCT", Theme::octaveColor, -2, 8);
    octaveLaneComp->valueFormatter = [](int val) { return juce::String(val); };
    addAndMakeVisible(*octaveLaneComp);
    
    velocityLaneComp = std::make_unique<LaneComponent>(p.velocityLane, "VEL", Theme::velocityColor, 0, 127);
    velocityLaneComp->valueFormatter = [](int val) { return juce::String(val); };
    addAndMakeVisible(*velocityLaneComp);
    
    lengthLaneComp = std::make_unique<LaneComponent>(p.lengthLane, "LEN", Theme::lengthColor, 0, 5);
    lengthLaneComp->valueFormatter = [](int val) -> juce::String {
        switch(val) {
            case 0: return "OFF";
            case 1: return "128n";
            case 2: return "64n";
            case 3: return "32n";
            case 4: return "16n";
            case 5: return "LEG";
            default: return juce::String(val);
        }
    };
    addAndMakeVisible(*lengthLaneComp);

    setSize (800, 600);
    startTimerHz(30); // Refresh UI at 30fps
}

ShequencerAudioProcessorEditor::~ShequencerAudioProcessorEditor()
{
    stopTimer();
}

void ShequencerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void ShequencerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);
    
    // Master Trigger Row (Increased height)
    masterTriggerComp.setBounds(area.removeFromTop(40));
    
    // Add some spacing
    area.removeFromTop(20);
    
    // Fixed height for lanes
    int gap = 20;
    int laneHeight = 110;
    
    noteLaneComp->setBounds(area.removeFromTop(laneHeight));
    area.removeFromTop(gap);
    
    octaveLaneComp->setBounds(area.removeFromTop(laneHeight));
    area.removeFromTop(gap);
    
    velocityLaneComp->setBounds(area.removeFromTop(laneHeight));
    area.removeFromTop(gap);
    
    lengthLaneComp->setBounds(area.removeFromTop(laneHeight));
}

void ShequencerAudioProcessorEditor::timerCallback()
{
    noteLaneComp->tick();
    octaveLaneComp->tick();
    velocityLaneComp->tick();
    lengthLaneComp->tick();

    masterTriggerComp.repaint();
    noteLaneComp->repaint();
    octaveLaneComp->repaint();
    velocityLaneComp->repaint();
    lengthLaneComp->repaint();
}
