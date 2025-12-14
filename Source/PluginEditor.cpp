#include "PluginProcessor.h"
#include "PluginEditor.h"

ShequencerAudioProcessorEditor::ShequencerAudioProcessorEditor (ShequencerAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), masterTriggerComp(p)
{
    addAndMakeVisible(masterTriggerComp);
    
    noteLaneComp = std::make_unique<LaneComponent>(p.noteLane, "NOTE", juce::Colour(0xFF3FA2FE), 0, 11);
    addAndMakeVisible(*noteLaneComp);
    
    octaveLaneComp = std::make_unique<LaneComponent>(p.octaveLane, "OCTAVE", juce::Colour(0xFFF8A43D), -2, 8);
    addAndMakeVisible(*octaveLaneComp);
    
    velocityLaneComp = std::make_unique<LaneComponent>(p.velocityLane, "VEL", juce::Colour(0xFFF72DA3), 0, 127);
    addAndMakeVisible(*velocityLaneComp);
    
    lengthLaneComp = std::make_unique<LaneComponent>(p.lengthLane, "LENGTH", juce::Colour(0xFFA228FF), 0, 5);
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
    
    // Master Trigger Row is fixed 15px
    masterTriggerComp.setBounds(area.removeFromTop(15));
    
    // Add some spacing
    area.removeFromTop(5);
    
    // Remaining area for 4 lanes
    int laneHeight = area.getHeight() / 4;
    
    noteLaneComp->setBounds(area.removeFromTop(laneHeight));
    octaveLaneComp->setBounds(area.removeFromTop(laneHeight));
    velocityLaneComp->setBounds(area.removeFromTop(laneHeight));
    lengthLaneComp->setBounds(area.removeFromTop(laneHeight));
}

void ShequencerAudioProcessorEditor::timerCallback()
{
    masterTriggerComp.repaint();
    noteLaneComp->repaint();
    octaveLaneComp->repaint();
    velocityLaneComp->repaint();
    lengthLaneComp->repaint();
}
