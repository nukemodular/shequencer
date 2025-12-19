#include "PluginProcessor.h"
#include "PluginEditor.h"

ShequencerAudioProcessorEditor::ShequencerAudioProcessorEditor (ShequencerAudioProcessor& p)
    : AudioProcessorEditor (&p),
      vBlankAttachment(this, [this] {
          if (currentPage == 0) {
              if (noteLaneComp) noteLaneComp->tick();
              if (octaveLaneComp) octaveLaneComp->tick();
              if (velocityLaneComp) velocityLaneComp->tick();
              if (lengthLaneComp) lengthLaneComp->tick();

              if (noteLaneComp) noteLaneComp->repaint();
              if (octaveLaneComp) octaveLaneComp->repaint();
              if (velocityLaneComp) velocityLaneComp->repaint();
              if (lengthLaneComp) lengthLaneComp->repaint();
          } else {
              if (ccLane1Comp) ccLane1Comp->tick();
              if (ccLane2Comp) ccLane2Comp->tick();
              if (ccLane3Comp) ccLane3Comp->tick();
              if (ccLane4Comp) ccLane4Comp->tick();

              if (ccLane1Comp) ccLane1Comp->repaint();
              if (ccLane2Comp) ccLane2Comp->repaint();
              if (ccLane3Comp) ccLane3Comp->repaint();
              if (ccLane4Comp) ccLane4Comp->repaint();
          }

          masterTriggerComp.repaint();
          bankSelectorComp.repaint();
          patternSlotsComp.repaint();
          shuffleComp.repaint();
          pageSelectorComp.repaint();
      }),
      masterTriggerComp(p), bankSelectorComp(p), patternSlotsComp(p), shuffleComp(p), fileOpsComp(p)
{
    setWantsKeyboardFocus(true);
    addAndMakeVisible(mainContainer);
    mainContainer.addAndMakeVisible(masterTriggerComp);
    mainContainer.addAndMakeVisible(pageSelectorComp);
    
    noteLaneComp = std::make_unique<LaneComponent>(p.noteLane, "NOTE", Theme::noteColor, 0, 11, 6);
    noteLaneComp->valueFormatter = [](int val) {
        const char* notes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        if (val >= 0 && val < 12) return juce::String(notes[val]);
        return juce::String(val);
    };
    noteLaneComp->onStepShiftClicked = [&](int step, bool isTriggerRow) {
        if (isTriggerRow) p.setLaneTriggerIndex(p.noteLane, step);
        else p.setLaneValueIndex(p.noteLane, step);
    };
    noteLaneComp->onResetClicked = [&](bool resetAll) {
        if (resetAll) p.resetAllLanes();
        else p.resetLane(p.noteLane, 0); // C
    };
    noteLaneComp->onLabelClicked = [&](bool shift) {
        if (shift) p.resetLane(p.noteLane, 0);
        else p.syncLaneToBar(p.noteLane);
    };
    mainContainer.addAndMakeVisible(*noteLaneComp);
    
    octaveLaneComp = std::make_unique<LaneComponent>(p.octaveLane, "OCT", Theme::octaveColor, -2, 8, 5);
    octaveLaneComp->valueFormatter = [](int val) { return juce::String(val); };
    octaveLaneComp->onStepShiftClicked = [&](int step, bool isTriggerRow) {
        if (isTriggerRow) p.setLaneTriggerIndex(p.octaveLane, step);
        else p.setLaneValueIndex(p.octaveLane, step);
    };
    octaveLaneComp->onResetClicked = [&](bool resetAll) {
        if (resetAll) p.resetAllLanes();
        else p.resetLane(p.octaveLane, 3); // 3
    };
    octaveLaneComp->onLabelClicked = [&](bool shift) {
        if (shift) p.resetLane(p.octaveLane, 3);
        else p.syncLaneToBar(p.octaveLane);
    };
    mainContainer.addAndMakeVisible(*octaveLaneComp);
    
    velocityLaneComp = std::make_unique<LaneComponent>(p.velocityLane, "VEL", Theme::velocityColor, 0, 127, 63);
    velocityLaneComp->valueFormatter = [](int val) { return juce::String(val); };
    velocityLaneComp->onStepShiftClicked = [&](int step, bool isTriggerRow) {
        if (isTriggerRow) p.setLaneTriggerIndex(p.velocityLane, step);
        else p.setLaneValueIndex(p.velocityLane, step);
    };
    velocityLaneComp->onResetClicked = [&](bool resetAll) {
        if (resetAll) p.resetAllLanes();
        else p.resetLane(p.velocityLane, 64); // 64
    };
    velocityLaneComp->onLabelClicked = [&](bool shift) {
        if (shift) p.resetLane(p.velocityLane, 64);
        else p.syncLaneToBar(p.velocityLane);
    };
    mainContainer.addAndMakeVisible(*velocityLaneComp);
    
    lengthLaneComp = std::make_unique<LaneComponent>(p.lengthLane, "LEN", Theme::lengthColor, 0, 9, 5);
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
        else p.setLaneValueIndex(p.lengthLane, step);
    };
    lengthLaneComp->onResetClicked = [&](bool resetAll) {
        if (resetAll) p.resetAllLanes();
        else p.resetLane(p.lengthLane, 5); // 32n
    };
    lengthLaneComp->onLabelClicked = [&](bool shift) {
        if (shift) p.resetLane(p.lengthLane, 5);
        else p.syncLaneToBar(p.lengthLane);
    };
    mainContainer.addAndMakeVisible(*lengthLaneComp);
    
    // Initialize CC Lanes
    auto setupCCLane = [&](std::unique_ptr<LaneComponent>& comp, SequencerLane& lane, juce::String name) {
        comp = std::make_unique<LaneComponent>(lane, name, Theme::controllerColor, 0, 127, 63, true);
        comp->valueFormatter = [&lane](int val) -> juce::String {
            if (lane.midiCC == 130) {
                switch(val) {
                    case 0: return "OFF";
                    // 3-Note Chords (1-12)
                    case 1: return "Maj";
                    case 2: return "Min";
                    case 3: return "Dim";
                    case 4: return "Aug";
                    case 5: return "Sus2";
                    case 6: return "Sus4";
                    case 7: return "Pow";
                    case 8: return "Maj/1";
                    case 9: return "Min/1";
                    case 10: return "Maj/2";
                    case 11: return "Min/2";
                    case 12: return "Oct";
                    // 4-Note Chords (13-24)
                    case 13: return "Maj7";
                    case 14: return "Min7";
                    case 15: return "Dom7";
                    case 16: return "Dim7";
                    case 17: return "hDim7";
                    case 18: return "mM7";
                    case 19: return "Maj6";
                    case 20: return "Min6";
                    case 21: return "Maj9";
                    case 22: return "Min9";
                    case 23: return "7sus";
                    case 24: return "7#9";
                    default: return juce::String(val);
                }
            }
            return juce::String(val);
        };
        comp->onStepShiftClicked = [&](int step, bool isTriggerRow) {
            if (isTriggerRow) p.setLaneTriggerIndex(lane, step);
            else p.setLaneValueIndex(lane, step);
        };
        comp->onResetClicked = [&](bool resetAll) {
            if (resetAll) p.resetAllLanes();
            else p.resetLane(lane, 0);
        };
        comp->onLabelClicked = [&](bool shift) {
            if (shift) {
                p.resetLane(lane, 0);
            } else {
                // Show CC Menu
                juce::PopupMenu m;
                m.addItem(1, "OFF", true, lane.midiCC == 0);
                m.addItem(2, "PGM", true, lane.midiCC == 128);
                m.addItem(3, "PRESSURE", true, lane.midiCC == 129);
                m.addItem(4, "CHORDS", true, lane.midiCC == 130);
                for(int i=1; i<=127; ++i)
                    m.addItem(i+4, "CC " + juce::String(i), true, lane.midiCC == i);
                
                m.showMenuAsync(juce::PopupMenu::Options(), [&lane, &comp](int result) {
                    if (result == 1) lane.midiCC = 0;
                    else if (result == 2) lane.midiCC = 128;
                    else if (result == 3) lane.midiCC = 129;
                    else if (result == 4) lane.midiCC = 130;
                    else if (result > 4) lane.midiCC = result - 4;
                    
                    if (result > 0) {
                        juce::String newName;
                        if (lane.midiCC == 0) { newName = "OFF"; comp->setRange(0, 127); }
                        else if (lane.midiCC == 128) { newName = "PGM"; comp->setRange(0, 127); }
                        else if (lane.midiCC == 129) { newName = "PRESSURE"; comp->setRange(0, 127); }
                        else if (lane.midiCC == 130) { newName = "CHORD"; comp->setRange(0, 24); }
                        else { newName = "CC " + juce::String(lane.midiCC); comp->setRange(0, 127); }
                        
                        comp->setLaneName(newName);
                    }
                });
            }
        };
        
        // Set initial name
        juce::String initialName;
        if (lane.midiCC == 0) { initialName = "OFF"; comp->setRange(0, 127); }
        else if (lane.midiCC == 128) { initialName = "PGM"; comp->setRange(0, 127); }
        else if (lane.midiCC == 129) { initialName = "PRESSURE"; comp->setRange(0, 127); }
        else if (lane.midiCC == 130) { initialName = "CHORD"; comp->setRange(0, 24); }
        else { initialName = "CC " + juce::String(lane.midiCC); comp->setRange(0, 127); }
        
        comp->setLaneName(initialName);
        
        mainContainer.addAndMakeVisible(*comp);
    };
    
    setupCCLane(ccLane1Comp, p.ccLane1, "CC 1");
    setupCCLane(ccLane2Comp, p.ccLane2, "CC 2");
    setupCCLane(ccLane3Comp, p.ccLane3, "CC 3");
    setupCCLane(ccLane4Comp, p.ccLane4, "CC 4");
    
    pageSelectorComp.onPageChanged = [this] { 
        currentPage = pageSelectorComp.currentPage;
        updatePageVisibility(); 
    };
    mainContainer.addAndMakeVisible(pageSelectorComp);
    
    mainContainer.addAndMakeVisible(bankSelectorComp);
    mainContainer.addAndMakeVisible(patternSlotsComp);
    mainContainer.addAndMakeVisible(shuffleComp);
    mainContainer.addAndMakeVisible(fileOpsComp);
    mainContainer.addAndMakeVisible(buildNumberComp);
    
    updatePageVisibility();

    setResizable(true, true);
    setResizeLimits(382, 340, 1528, 1360);
    getConstrainer()->setFixedAspectRatio(764.0 / 680.0);

    setSize (764, 680);
}

ShequencerAudioProcessorEditor::~ShequencerAudioProcessorEditor()
{
}

void ShequencerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
}

void ShequencerAudioProcessorEditor::updatePageVisibility()
{
    bool showPage1 = (currentPage == 0);
    
    if (noteLaneComp) noteLaneComp->setVisible(showPage1);
    if (octaveLaneComp) octaveLaneComp->setVisible(showPage1);
    if (velocityLaneComp) velocityLaneComp->setVisible(showPage1);
    if (lengthLaneComp) lengthLaneComp->setVisible(showPage1);
    
    if (ccLane1Comp) ccLane1Comp->setVisible(!showPage1);
    if (ccLane2Comp) ccLane2Comp->setVisible(!showPage1);
    if (ccLane3Comp) ccLane3Comp->setVisible(!showPage1);
    if (ccLane4Comp) ccLane4Comp->setVisible(!showPage1);
    
    resized();
}

void ShequencerAudioProcessorEditor::resized()
{
    float scale = (float)getWidth() / 764.0f;
    mainContainer.setTransform(juce::AffineTransform::scale(scale));
    mainContainer.setBounds(0, 0, 764, 680);

    auto area = mainContainer.getLocalBounds().reduced(10);
    
    // Master Trigger Row (Increased height)
    auto topRow = area.removeFromTop(48);
    
    // Page Selector Position (Centered in 5th Column)
    // 5th Column is the last 30px of the area width
    int col5_Width = 30;
    int pageBtnSize = 24; // Reduced slightly to fit 30px
    
    // Vertical Center:
    // Top of lanes = 10 + 48 + 12 = 70
    // Total height of 4 lanes + 3 gaps = 4 * 130 + 3 * 10 = 550
    // Center Y = 70 + 550 / 2 = 345
    int pageBtnY = 345 - (pageBtnSize / 2);
    
    // Horizontal Center in last 30px
    int pageBtnX = area.getRight() - col5_Width + (col5_Width - pageBtnSize) / 2;
    
    pageSelectorComp.setBounds(pageBtnX, pageBtnY, pageBtnSize, pageBtnSize);
    pageSelectorComp.toFront(false);
    
    masterTriggerComp.setBounds(topRow);
    
    // Add some spacing
    area.removeFromTop(12);
    
    // Fixed height for lanes
    int gap = 10;
    int laneHeight = 130;
    
    if (currentPage == 0)
    {
        if (noteLaneComp) noteLaneComp->setBounds(area.removeFromTop(laneHeight));
        area.removeFromTop(gap);
        
        if (octaveLaneComp) octaveLaneComp->setBounds(area.removeFromTop(laneHeight));
        area.removeFromTop(gap);
        
        if (velocityLaneComp) velocityLaneComp->setBounds(area.removeFromTop(laneHeight));
        area.removeFromTop(gap);
        
        if (lengthLaneComp) lengthLaneComp->setBounds(area.removeFromTop(laneHeight));
    }
    else
    {
        if (ccLane1Comp) ccLane1Comp->setBounds(area.removeFromTop(laneHeight));
        area.removeFromTop(gap);
        
        if (ccLane2Comp) ccLane2Comp->setBounds(area.removeFromTop(laneHeight));
        area.removeFromTop(gap);
        
        if (ccLane3Comp) ccLane3Comp->setBounds(area.removeFromTop(laneHeight));
        area.removeFromTop(gap);
        
        if (ccLane4Comp) ccLane4Comp->setBounds(area.removeFromTop(laneHeight));
    }
    
    area.removeFromTop(gap);
    
    auto patternRow = area.removeFromTop(40);
    
    // Left Margin: 70px (20px Col 1 + 50px Col 2)
    auto leftMargin = patternRow.removeFromLeft(70);
    // Col 1 (0-20): Empty
    leftMargin.removeFromLeft(20);
    // Col 2 (20-70): Bank Selector
    // Center 40px wide component in 50px space
    bankSelectorComp.setBounds(leftMargin.getX() + 5, leftMargin.getY(), 40, 40);
    
    // Right Margin: 130px (100px Col 4 + 30px Col 5)
    auto rightMargin = patternRow.removeFromRight(130);
    // Col 5 (Last 30px)
    auto col5 = rightMargin.removeFromRight(30);
    
    // Col 4 (Remaining 100px): Controls
    // Controls width is 80px total. Center in 100px (offset 10px).
    int controlsX = rightMargin.getX() + 10;
    
    // FileOps (40px)
    fileOpsComp.setBounds(controlsX, rightMargin.getY() + 8, 40, 24);
    
    // Shuffle (40px)
    shuffleComp.setBounds(controlsX + 40, rightMargin.getY() + 8, 40, 24);
    
    // Build Number (25px) - Centered in Col 5
    buildNumberComp.setBounds(col5.getX() + 2, rightMargin.getY() + 8, 25, 24);
    
    // Middle: Slots
    patternSlotsComp.setBounds(patternRow);
}

bool ShequencerAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::tabKey)
    {
        currentPage = !currentPage;
        pageSelectorComp.currentPage = currentPage;
        updatePageVisibility();
        pageSelectorComp.repaint();
        return true;
    }
    return false;
}
