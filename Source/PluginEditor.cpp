#include "PluginProcessor.h"
#include "PluginEditor.h"

ShequencerAudioProcessorEditor::ShequencerAudioProcessorEditor (ShequencerAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
      vBlankAttachment(this, [this] {
          if (noteLaneComp) noteLaneComp->tick();
          if (octaveLaneComp) octaveLaneComp->tick();
          if (velocityLaneComp) velocityLaneComp->tick();
          if (lengthLaneComp) lengthLaneComp->tick();

          masterTriggerComp.repaint();
          if (noteLaneComp) noteLaneComp->repaint();
          if (octaveLaneComp) octaveLaneComp->repaint();
          if (velocityLaneComp) velocityLaneComp->repaint();
          if (lengthLaneComp) lengthLaneComp->repaint();
          bankSelectorComp.repaint();
          patternSlotsComp.repaint();
          shuffleComp.repaint();
      }),
      masterTriggerComp(p), bankSelectorComp(p), patternSlotsComp(p), shuffleComp(p)
{
    addAndMakeVisible(masterTriggerComp);
    
    noteLaneComp = std::make_unique<LaneComponent>(p.noteLane, "NOTE", Theme::noteColor, 0, 11);
    noteLaneComp->valueFormatter = [](int val) {
        const char* notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        if (val >= 0 && val < 12) return juce::String(notes[val]);
        return juce::String(val);
    };
    noteLaneComp->onStepShiftClicked = [&](int step, bool isTriggerRow) {
        if (isTriggerRow) p.setLaneTriggerIndex(p.noteLane, step);
        else {
            p.noteLane.currentValueStep = step;
            p.noteLane.forceNextStep = true;
        }
    };
    noteLaneComp->onResetClicked = [&](bool resetAll) {
        if (resetAll) p.resetAllLanes();
        else p.resetLane(p.noteLane, 0); // C
    };
    noteLaneComp->onLabelClicked = [&](bool shift) {
        if (shift) p.syncAllToBar();
        else p.syncLaneToBar(p.noteLane);
    };
    addAndMakeVisible(*noteLaneComp);
    
    octaveLaneComp = std::make_unique<LaneComponent>(p.octaveLane, "OCT", Theme::octaveColor, -2, 8);
    octaveLaneComp->valueFormatter = [](int val) { return juce::String(val); };
    octaveLaneComp->onStepShiftClicked = [&](int step, bool isTriggerRow) {
        if (isTriggerRow) p.setLaneTriggerIndex(p.octaveLane, step);
        else {
            p.octaveLane.currentValueStep = step;
            p.octaveLane.forceNextStep = true;
        }
    };
    octaveLaneComp->onResetClicked = [&](bool resetAll) {
        if (resetAll) p.resetAllLanes();
        else p.resetLane(p.octaveLane, 0); // 0
    };
    octaveLaneComp->onLabelClicked = [&](bool shift) {
        if (shift) p.syncAllToBar();
        else p.syncLaneToBar(p.octaveLane);
    };
    addAndMakeVisible(*octaveLaneComp);
    
    velocityLaneComp = std::make_unique<LaneComponent>(p.velocityLane, "VEL", Theme::velocityColor, 0, 127);
    velocityLaneComp->valueFormatter = [](int val) { return juce::String(val); };
    velocityLaneComp->onStepShiftClicked = [&](int step, bool isTriggerRow) {
        if (isTriggerRow) p.setLaneTriggerIndex(p.velocityLane, step);
        else {
            p.velocityLane.currentValueStep = step;
            p.velocityLane.forceNextStep = true;
        }
    };
    velocityLaneComp->onResetClicked = [&](bool resetAll) {
        if (resetAll) p.resetAllLanes();
        else p.resetLane(p.velocityLane, 64); // 64
    };
    velocityLaneComp->onLabelClicked = [&](bool shift) {
        if (shift) p.syncAllToBar();
        else p.syncLaneToBar(p.velocityLane);
    };
    addAndMakeVisible(*velocityLaneComp);
    
    lengthLaneComp = std::make_unique<LaneComponent>(p.lengthLane, "LEN", Theme::lengthColor, 0, 9);
    lengthLaneComp->valueFormatter = [](int val) -> juce::String {
        switch(val) {
            case 0: return "OFF";
            case 1: return "128n";
            case 2: return "128d";
            case 3: return "64n";
            case 4: return "64d";
            case 5: return "32n";
            case 6: return "32d";
            case 7: return "16n";
            case 8: return "LEG";
            case 9: return "HOLD";
            default: return juce::String(val);
        }
    };
    lengthLaneComp->onStepShiftClicked = [&](int step, bool isTriggerRow) {
        if (isTriggerRow) p.setLaneTriggerIndex(p.lengthLane, step);
        else {
            p.lengthLane.currentValueStep = step;
            p.lengthLane.forceNextStep = true;
        }
    };
    lengthLaneComp->onResetClicked = [&](bool resetAll) {
        if (resetAll) p.resetAllLanes();
        else p.resetLane(p.lengthLane, 7); // 16n
    };
    lengthLaneComp->onLabelClicked = [&](bool shift) {
        if (shift) p.syncAllToBar();
        else p.syncLaneToBar(p.lengthLane);
    };
    addAndMakeVisible(*lengthLaneComp);
    
    addAndMakeVisible(bankSelectorComp);
    addAndMakeVisible(patternSlotsComp);
    addAndMakeVisible(shuffleComp);

    setSize (840, 660);
}

ShequencerAudioProcessorEditor::~ShequencerAudioProcessorEditor()
{
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
    
    area.removeFromTop(gap);
    
    auto patternRow = area.removeFromTop(40);
    
    // Left: Bank Selector (40px) + Gap (20px)
    auto leftArea = patternRow.removeFromLeft(60);
    bankSelectorComp.setBounds(leftArea.withSizeKeepingCentre(40, 40));
    
    // Right: Shuffle (Aligned with Loop Lengths)
    // Loop Lengths are at width-90 (width 40).
    // patternRow right margin is 120.
    auto rightArea = patternRow.removeFromRight(120);
    // Shuffle should be at x=30 inside rightArea (which corresponds to width-90)
    shuffleComp.setBounds(rightArea.getX() + 30, rightArea.getY() + 8, 40, 24); // Centered vertically (40-24)/2 = 8
    
    // Middle: Slots
    patternSlotsComp.setBounds(patternRow);
}
