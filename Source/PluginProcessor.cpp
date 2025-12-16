#include "PluginProcessor.h"
#include "PluginEditor.h"

ShequencerAudioProcessor::ShequencerAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Initialize default values
    masterTriggers.fill(false);
    
    noteLane.values.fill(0); // C
    noteLane.triggers.fill(true);
    
    octaveLane.values.fill(3); // Octave 3
    octaveLane.triggers.fill(true);
    
    velocityLane.values.fill(100);
    velocityLane.triggers.fill(true);
    
    lengthLane.values.fill(7); // 16th
    lengthLane.triggers.fill(true);
}

ShequencerAudioProcessor::~ShequencerAudioProcessor()
{
}

const juce::String ShequencerAudioProcessor::getName() const
{
    return "toolBoy Sh-equencer v1";
}

bool ShequencerAudioProcessor::acceptsMidi() const
{
    return true;
}

bool ShequencerAudioProcessor::producesMidi() const
{
    return true;
}

bool ShequencerAudioProcessor::isMidiEffect() const
{
    return false;
}

double ShequencerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ShequencerAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ShequencerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ShequencerAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String ShequencerAudioProcessor::getProgramName (int index)
{
    return {};
}

void ShequencerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

void ShequencerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialization that you need..
    currentPositionInQuarterNotes = 0.0;
    lastPositionInQuarterNotes = 0.0;
    for (auto& note : activeNotes) note.isActive = false;
    
    noteLane.reset();
    octaveLane.reset();
    velocityLane.reset();
    lengthLane.reset();
}

void ShequencerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool ShequencerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // This is an Instrument, so we want an output, but no input.
    if (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::disabled())
        return false;

    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    return true;
}

void ShequencerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    auto* playHead = getPlayHead();
    if (playHead == nullptr) return;

    auto positionInfo = playHead->getPosition();
    if (!positionInfo.hasValue()) return;
    
    auto pos = *positionInfo;
    
    if (!pos.getIsPlaying())
    {
        isPlaying = false;
        
        // Update timing info even when stopped so UI sync works
        if (auto ppq = pos.getPpqPosition())
             lastPositionInQuarterNotes = *ppq;
        if (auto barStart = pos.getPpqPositionOfLastBarStart())
             lastBarStartPPQ = *barStart;
        if (auto ts = pos.getTimeSignature())
        {
            sigNumerator = ts->numerator;
            sigDenominator = ts->denominator;
        }
             
        for (auto& note : activeNotes) note.isActive = false;
        return;
    }

    if (!isPlaying)
    {
        isPlaying = true;
        if (auto ppq = pos.getPpqPosition())
            lastPositionInQuarterNotes = *ppq;
            
        // Reset offsets on start to ensure alignment with grid
        globalStepOffset = 0;
        noteLane.triggerStepOffset = 0;
        octaveLane.triggerStepOffset = 0;
        velocityLane.triggerStepOffset = 0;
        lengthLane.triggerStepOffset = 0;

        // Reset value sequences to step 1
        noteLane.reset();
        octaveLane.reset();
        velocityLane.reset();
        lengthLane.reset();
    }
    
    // Update Timing Info
    if (auto barStart = pos.getPpqPositionOfLastBarStart())
    {
        if (*barStart != lastBarStartPPQ)
        {
            auto checkReset = [](SequencerLane& lane) {
                if (lane.resetValuesAtNextBar) {
                    lane.currentValueStep = 0;
                    lane.activeValueStep = 0;
                    lane.resetValuesAtNextBar = false;
                }
            };
            checkReset(noteLane);
            checkReset(octaveLane);
            checkReset(velocityLane);
            checkReset(lengthLane);
        }
        lastBarStartPPQ = *barStart;
    }
    
    // Handle Automatic Resets (Intervals)
    // We need to track bar changes relative to the start of playback or some reference
    // But simpler: count bars since last reset?
    // Or just use absolute bar count?
    // pos.getPpqPosition() gives total quarters.
    // Bars = PPQ / 4 (assuming 4/4).
    // Let's use the bar index.
    
    long long currentBarIndex = 0;
    if (auto ppq = pos.getPpqPosition())
    {
        currentBarIndex = (long long)(*ppq / 4.0); // Approximation for 4/4
        // Better: use barStartPPQ if available to detect change
    }
    
    static long long lastProcessedBarIndex = -1;
    if (currentBarIndex != lastProcessedBarIndex)
    {
        auto processIntervalReset = [&](SequencerLane& lane) {
            // Value Reset
            if (lane.valueResetInterval > 0)
            {
                if (currentBarIndex % lane.valueResetInterval == 0)
                {
                    lane.currentValueStep = 0;
                    lane.activeValueStep = 0;
                }
            }
            // Trigger Reset (Step Progression)
            if (lane.triggerResetInterval > 0)
            {
                if (currentBarIndex % lane.triggerResetInterval == 0)
                {
                    // Sync to bar start
                    // We want (currentStep + offset) % loopLen == 0 at this moment?
                    // Or just reset the offset so it aligns with global?
                    // "reset/resync step 1 to bar"
                    // This implies aligning the lane's trigger sequence to the current bar start.
                    
                    // Calculate what the offset should be to hit step 0 NOW (at bar start)
                    // At bar start, global step count (relative to bar) is 0?
                    // Actually, we just want to reset the internal counter if it was independent,
                    // but here it's derived from global time + offset.
                    // So we adjust the offset.
                    
                    // We are at the start of a bar (approximately).
                    // We want localStep to be 0.
                    // localStep = (globalStep + offset) % loopLen
                    // So offset = -globalStep
                    
                    // Get current global step
                    double currentPPQ = *pos.getPpqPosition();
                    long long currentAbsStep = (long long)std::floor(currentPPQ / 0.25);
                    long long globalStep = currentAbsStep + globalStepOffset;
                    
                    lane.triggerStepOffset = -globalStep;
                }
            }
        };
        
        processIntervalReset(noteLane);
        processIntervalReset(octaveLane);
        processIntervalReset(velocityLane);
        processIntervalReset(lengthLane);
        
        lastProcessedBarIndex = currentBarIndex;
    }
        
    if (auto ts = pos.getTimeSignature())
    {
        sigNumerator = ts->numerator;
        sigDenominator = ts->denominator;
    }

    double currentPPQ = *pos.getPpqPosition();
    double bpm = *pos.getBpm();
    if (bpm <= 0) bpm = 120.0;
    
    double samplesPerQuarterNote = (getSampleRate() * 60.0) / bpm;
    int numSamples = buffer.getNumSamples();
    double endPPQ = currentPPQ + (numSamples / samplesPerQuarterNote);
    
    // Step Logic
    double stepDuration = 0.25; // 16th note
    double maxDelay = 0.125; // 32nd note
    double currentShuffleDelay = 0.0;
    if (shuffleAmount > 1)
        currentShuffleDelay = ((double)(shuffleAmount - 1) / 6.0) * maxDelay;
    
    double searchStart = currentPPQ;
    double searchEnd = endPPQ;
    
    // Find the first step boundary > searchStart
    // We iterate through potential step indices
    // We look back enough to catch delayed steps from previous grid points
    double lookBack = stepDuration + maxDelay + 0.05;
    long long startStepIdx = (long long)std::floor((searchStart - lookBack) / stepDuration);
    if (startStepIdx < 0) startStepIdx = 0;
    
    for (long long k = startStepIdx; ; ++k)
    {
        lastAbsStep = k;
        double baseTime = k * stepDuration;
        double time = baseTime;
        
        long long stepCount = k + globalStepOffset;
        int stepIdx = stepCount % masterLength;
        if (stepIdx < 0) stepIdx += masterLength;

        // Apply shuffle to odd steps (1, 3, 5...)
        // Note: k is 0-based index. 0=Straight, 1=Delayed.
        if (k % 2 != 0)
            time += currentShuffleDelay;
            
        if (time >= searchEnd) break; // Past the buffer
        
        if (time >= searchStart)
        {
            // Process this step
            // Calculate sample offset
            double offsetPPQ = time - currentPPQ;
            int sampleOffset = (int)(offsetPPQ * samplesPerQuarterNote);
            sampleOffset = juce::jlimit(0, numSamples - 1, sampleOffset);
            
            // Calculate actual step duration for length logic
            double nextStepBaseTime = (k + 1) * stepDuration;
            double nextStepTime = nextStepBaseTime;
            if ((k + 1) % 2 != 0)
                nextStepTime += currentShuffleDelay;
            
            int nextStepIdx = (stepCount + 1) % masterLength;
            if (nextStepIdx < 0) nextStepIdx += masterLength;

            double actualStepDuration = nextStepTime - time;
            if (actualStepDuration <= 0) actualStepDuration = 0.01;
        
            // --- CORE LOGIC ---
            currentMasterStep = stepIdx;
            
            // Update Active Step for UI and Playback
            noteLane.activeValueStep = noteLane.currentValueStep;
            octaveLane.activeValueStep = octaveLane.currentValueStep;
            velocityLane.activeValueStep = velocityLane.currentValueStep;
            lengthLane.activeValueStep = lengthLane.currentValueStep;

            int l = lengthLane.values[lengthLane.activeValueStep];
            bool isHoldOrLegato = (l == 8 || l == 9);

            // 2. Play Note (Only if Master Trigger is active OR Length overrides)
            if (masterTriggers[stepIdx] || isHoldOrLegato)
            {
                 int n = noteLane.values[noteLane.activeValueStep];
                 int o = octaveLane.values[octaveLane.activeValueStep];
                 int v = velocityLane.values[velocityLane.activeValueStep];
                 // int l is already defined above
                 
                 // Note: 0-11 (C to B)
                 // Octave: -2 to 8
                 // C-2 is MIDI 0.
                 // Formula: (Octave + 2) * 12 + Note
                 
                 int mNote = (o + 2) * 12 + n;
                 mNote = juce::jlimit(0, 127, mNote);
                 
                 double dur = 0.25;
                 bool play = true;
                 bool isHold = false;
                 
                 // Length Values: 
                 // 0:OFF, 1:128n, 2:128d, 3:64n, 4:64d, 5:32n, 6:32d, 7:16n, 8:LEG, 9:HOLD
                 bool isHoldStep = (l == 9);
                 bool shouldPlay = (l != 0);
                 
                 if (l == 0) play = false;
                 else if (l == 1) dur = 0.03125;
                 else if (l == 2) dur = 0.046875;
                 else if (l == 3) dur = 0.0625;
                 else if (l == 4) dur = 0.09375;
                 else if (l == 5) dur = 0.125;
                 else if (l == 6) dur = 0.1875;
                 else if (l == 7) dur = actualStepDuration * 0.96;
                 else if (l == 8) dur = actualStepDuration + 0.01; // Legato
                 else if (l == 9) {
                     play = true; // HOLD triggers a note
                     dur = actualStepDuration; // Default to fill step
                 }
                 
                 bool extended = false;
                 
                 // Check if we are continuing a hold chain
                 if (isHoldActive && lastTriggeredNoteIndex >= 0)
                 {
                     // Verify note is still active
                     bool noteFound = false;
                     for (auto& note : activeNotes) {
                         // We can't rely on index being stable if notes are removed, but activeNotes is a fixed array
                         // and we only clear isActive. So index is stable.
                         // But we should check if it's actually active.
                         if (&note == &activeNotes[lastTriggeredNoteIndex] && note.isActive) {
                             noteFound = true;
                             break;
                         }
                     }
                     
                     if (noteFound && shouldPlay)
                     {
                         // Extend the held note
                         auto& note = activeNotes[lastTriggeredNoteIndex];
                         note.noteOffPosition = time + dur;
                         extended = true;
                         
                         // Update State
                         if (!isHoldStep) isHoldActive = false; // End of chain
                     }
                     else
                     {
                         isHoldActive = false; // Note died or Rest, can't extend
                     }
                 }
                 
                 if (!extended && shouldPlay && v > 0)
                 {
                     // Handle overlapping notes of same pitch
                     for (auto& note : activeNotes)
                     {
                         if (!note.isActive) continue;

                         // Check if same pitch AND overlaps (ends at or after new note start)
                         if (note.noteNumber == mNote && note.midiChannel == 1 && note.noteOffPosition >= time - 0.0001)
                         {
                             midiMessages.addEvent(juce::MidiMessage::noteOff(1, mNote), sampleOffset);
                             note.isActive = false;
                         }
                     }

                     midiMessages.addEvent(juce::MidiMessage::noteOn(1, mNote, (juce::uint8)v), sampleOffset);
                     
                     // Find free slot
                     for (int i = 0; i < maxActiveNotes; ++i)
                     {
                         auto& note = activeNotes[i];
                         if (!note.isActive)
                         {
                             note.isActive = true;
                             note.noteNumber = mNote;
                             note.midiChannel = 1;
                             note.noteOffPosition = time + dur;
                             
                             lastTriggeredNoteIndex = i;
                             if (isHoldStep) isHoldActive = true;
                             else isHoldActive = false;
                             
                             break;
                         }
                     }
                 }
                 else if (!shouldPlay)
                 {
                     isHoldActive = false;
                 }
            }
            else
            {
                // Master Trigger OFF -> Break Hold
                isHoldActive = false;
            }

            // 1. Advance Lanes
            auto processLaneAdvancement = [&](SequencerLane& lane) {
                bool masterHit = lane.enableMasterSource && masterTriggers[stepIdx];
                int localStep = (stepCount + lane.triggerStepOffset) % lane.triggerLoopLength;
                if (localStep < 0) localStep += lane.triggerLoopLength;
                lane.currentTriggerStep = localStep;
                
                bool localHit = lane.enableLocalSource && lane.triggers[localStep];
                
                if (masterHit || localHit)
                    lane.advanceValue();
            };
            
            processLaneAdvancement(noteLane);
            processLaneAdvancement(octaveLane);
            processLaneAdvancement(velocityLane);
            processLaneAdvancement(lengthLane);
        }
    }
    
    // Process Note Offs (Moved to end to handle new notes ending in this block)
    for (auto& note : activeNotes)
    {
        if (!note.isActive) continue;

        if (currentPPQ >= note.noteOffPosition) // Already passed?
        {
             midiMessages.addEvent(juce::MidiMessage::noteOff(note.midiChannel, note.noteNumber), 0);
             note.isActive = false;
        }
        else if (note.noteOffPosition < endPPQ) // Will happen in this block
        {
            double offsetPPQ = note.noteOffPosition - currentPPQ;
            int sampleOffset = (int)(offsetPPQ * samplesPerQuarterNote);
            sampleOffset = juce::jlimit(0, numSamples - 1, sampleOffset);
            
            midiMessages.addEvent(juce::MidiMessage::noteOff(note.midiChannel, note.noteNumber), sampleOffset);
            note.isActive = false;
        }
    }
    
    lastPositionInQuarterNotes = endPPQ;
}

bool ShequencerAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* ShequencerAudioProcessor::createEditor()
{
    return new ShequencerAudioProcessorEditor (*this);
}

void ShequencerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::XmlElement xml("SHEQUENCER_STATE");
    
    // Save Master Data
    xml.setAttribute("masterLength", masterLength);
    xml.setAttribute("shuffleAmount", shuffleAmount);
    xml.setAttribute("isShuffleGlobal", isShuffleGlobal);
    
    juce::String masterTrigStr;
    for (bool b : masterTriggers) masterTrigStr += (b ? "1" : "0");
    xml.setAttribute("masterTriggers", masterTrigStr);

    // Helper to save lane
    auto saveLane = [&](SequencerLane& lane, juce::String name) {
        auto* laneXml = xml.createNewChildElement(name);
        laneXml->setAttribute("valueLoopLength", lane.valueLoopLength);
        laneXml->setAttribute("triggerLoopLength", lane.triggerLoopLength);
        laneXml->setAttribute("valueResetInterval", lane.valueResetInterval);
        laneXml->setAttribute("triggerResetInterval", lane.triggerResetInterval);
        laneXml->setAttribute("enableMasterSource", lane.enableMasterSource);
        laneXml->setAttribute("enableLocalSource", lane.enableLocalSource);
        
        juce::String valStr;
        for (int v : lane.values) valStr += juce::String(v) + ",";
        laneXml->setAttribute("values", valStr);
        
        juce::String trigStr;
        for (bool b : lane.triggers) trigStr += (b ? "1" : "0");
        laneXml->setAttribute("triggers", trigStr);
    };
    
    saveLane(noteLane, "NOTE_LANE");
    saveLane(octaveLane, "OCTAVE_LANE");
    saveLane(velocityLane, "VELOCITY_LANE");
    saveLane(lengthLane, "LENGTH_LANE");
    
    // Save Banks
    auto* banksXml = xml.createNewChildElement("BANKS");
    for (int b = 0; b < 4; ++b)
    {
        auto* bankXml = banksXml->createNewChildElement("BANK");
        bankXml->setAttribute("index", b);
        
        for (int s = 0; s < 16; ++s)
        {
            const auto& pat = patternBanks[b][s];
            if (!pat.isEmpty)
            {
                auto* patXml = bankXml->createNewChildElement("PATTERN");
                patXml->setAttribute("slot", s);
                patXml->setAttribute("masterLength", pat.masterLength);
                patXml->setAttribute("shuffleAmount", pat.shuffleAmount);
                
                juce::String mTrig;
                for (bool v : pat.masterTriggers) mTrig += (v ? "1" : "0");
                patXml->setAttribute("masterTriggers", mTrig);

                auto savePatLane = [&](const PatternData::LaneData& ld, juce::String name) {
                    auto* lXml = patXml->createNewChildElement(name);
                    lXml->setAttribute("valueLoopLength", ld.valueLoopLength);
                    lXml->setAttribute("triggerLoopLength", ld.triggerLoopLength);
                    lXml->setAttribute("valueResetInterval", ld.valueResetInterval);
                    lXml->setAttribute("triggerResetInterval", ld.triggerResetInterval);
                    lXml->setAttribute("enableMasterSource", ld.enableMasterSource);
                    lXml->setAttribute("enableLocalSource", ld.enableLocalSource);
                    
                    juce::String vStr;
                    for (int v : ld.values) vStr += juce::String(v) + ",";
                    lXml->setAttribute("values", vStr);
                    
                    juce::String tStr;
                    for (bool v : ld.triggers) tStr += (v ? "1" : "0");
                    lXml->setAttribute("triggers", tStr);
                };
                
                savePatLane(pat.noteLane, "NOTE_LANE");
                savePatLane(pat.octaveLane, "OCTAVE_LANE");
                savePatLane(pat.velocityLane, "VELOCITY_LANE");
                savePatLane(pat.lengthLane, "LENGTH_LANE");
            }
        }
    }
    
    copyXmlToBinary(xml, destData);
}

void ShequencerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState != nullptr && xmlState->hasTagName("SHEQUENCER_STATE"))
    {
        masterLength = xmlState->getIntAttribute("masterLength", 16);
        shuffleAmount = xmlState->getIntAttribute("shuffleAmount", 1);
        isShuffleGlobal = xmlState->getBoolAttribute("isShuffleGlobal", true);

        juce::String masterTrigStr = xmlState->getStringAttribute("masterTriggers");
        for (int i = 0; i < 16 && i < masterTrigStr.length(); ++i)
            masterTriggers[i] = (masterTrigStr[i] == '1');
            
        auto loadLane = [&](SequencerLane& lane, juce::String name) {
            auto* laneXml = xmlState->getChildByName(name);
            if (laneXml)
            {
                lane.valueLoopLength = laneXml->getIntAttribute("valueLoopLength", 16);
                lane.triggerLoopLength = laneXml->getIntAttribute("triggerLoopLength", 16);
                lane.valueResetInterval = laneXml->getIntAttribute("valueResetInterval", 0);
                lane.triggerResetInterval = laneXml->getIntAttribute("triggerResetInterval", 0);
                lane.enableMasterSource = laneXml->getBoolAttribute("enableMasterSource", false);
                lane.enableLocalSource = laneXml->getBoolAttribute("enableLocalSource", true);
                
                juce::String valStr = laneXml->getStringAttribute("values");
                juce::StringArray tokens;
                tokens.addTokens(valStr, ",", "");
                for (int i = 0; i < 16 && i < tokens.size(); ++i)
                    lane.values[i] = tokens[i].getIntValue();
                    
                juce::String trigStr = laneXml->getStringAttribute("triggers");
                for (int i = 0; i < 16 && i < trigStr.length(); ++i)
                    lane.triggers[i] = (trigStr[i] == '1');
            }
        };
        
        loadLane(noteLane, "NOTE_LANE");
        loadLane(octaveLane, "OCTAVE_LANE");
        loadLane(velocityLane, "VELOCITY_LANE");
        loadLane(lengthLane, "LENGTH_LANE");
        
        // Load Banks
        auto* banksXml = xmlState->getChildByName("BANKS");
        if (banksXml)
        {
            for (auto* bankXml : banksXml->getChildIterator())
            {
                int b = bankXml->getIntAttribute("index");
                if (b >= 0 && b < 4)
                {
                    for (auto* patXml : bankXml->getChildIterator())
                    {
                        int s = patXml->getIntAttribute("slot");
                        if (s >= 0 && s < 16)
                        {
                            auto& pat = patternBanks[b][s];
                            pat.isEmpty = false;
                            pat.masterLength = patXml->getIntAttribute("masterLength", 16);

                            pat.shuffleAmount = patXml->getIntAttribute("shuffleAmount", 1);
                            
                            juce::String mTrig = patXml->getStringAttribute("masterTriggers");
                            for(int i=0; i<16 && i<mTrig.length(); ++i) pat.masterTriggers[i] = (mTrig[i] == '1');
                            
                            auto loadPatLane = [&](PatternData::LaneData& ld, juce::String name) {
                                auto* lXml = patXml->getChildByName(name);
                                if (lXml) {
                                    ld.valueLoopLength = lXml->getIntAttribute("valueLoopLength", 16);
                                    ld.triggerLoopLength = lXml->getIntAttribute("triggerLoopLength", 16);
                                    ld.valueResetInterval = lXml->getIntAttribute("valueResetInterval", 0);
                                    ld.triggerResetInterval = lXml->getIntAttribute("triggerResetInterval", 0);
                                    ld.enableMasterSource = lXml->getBoolAttribute("enableMasterSource", false);
                                    ld.enableLocalSource = lXml->getBoolAttribute("enableLocalSource", true);
                                    
                                    juce::String vStr = lXml->getStringAttribute("values");
                                    juce::StringArray toks; toks.addTokens(vStr, ",", "");
                                    for(int i=0; i<16 && i<toks.size(); ++i) ld.values[i] = toks[i].getIntValue();
                                    
                                    juce::String tStr = lXml->getStringAttribute("triggers");
                                    for(int i=0; i<16 && i<tStr.length(); ++i) ld.triggers[i] = (tStr[i] == '1');
                                }
                            };
                            
                            loadPatLane(pat.noteLane, "NOTE_LANE");
                            loadPatLane(pat.octaveLane, "OCTAVE_LANE");
                            loadPatLane(pat.velocityLane, "VELOCITY_LANE");
                            loadPatLane(pat.lengthLane, "LENGTH_LANE");
                        }
                    }
                }
            }
        }
    }
}

void ShequencerAudioProcessor::savePattern(int bank, int slot)
{
    if (bank < 0 || bank >= 4 || slot < 0 || slot >= 16) return;
    
    loadedBank = bank;
    loadedSlot = slot;
    
    auto& pat = patternBanks[bank][slot];
    pat.isEmpty = false;
    
    pat.masterLength = masterLength;
    pat.shuffleAmount = shuffleAmount;
    pat.masterTriggers = masterTriggers;
    
    auto copyLane = [](const SequencerLane& src, PatternData::LaneData& dst) {
        dst.values = src.values;
        dst.triggers = src.triggers;
        dst.valueLoopLength = src.valueLoopLength;
        dst.triggerLoopLength = src.triggerLoopLength;
        dst.valueResetInterval = src.valueResetInterval;
        dst.triggerResetInterval = src.triggerResetInterval;
        dst.enableMasterSource = src.enableMasterSource;
        dst.enableLocalSource = src.enableLocalSource;
    };
    
    copyLane(noteLane, pat.noteLane);
    copyLane(octaveLane, pat.octaveLane);
    copyLane(velocityLane, pat.velocityLane);
    copyLane(lengthLane, pat.lengthLane);
}

void ShequencerAudioProcessor::loadPattern(int bank, int slot)
{
    if (bank < 0 || bank >= 4 || slot < 0 || slot >= 16) return;
    
    const auto& pat = patternBanks[bank][slot];
    if (pat.isEmpty) return;
    
    loadedBank = bank;
    loadedSlot = slot;
    
    masterLength = pat.masterLength;
    if (!isShuffleGlobal) shuffleAmount = pat.shuffleAmount;
    masterTriggers = pat.masterTriggers;
    
    auto loadLane = [](SequencerLane& dst, const PatternData::LaneData& src) {
        dst.values = src.values;
        dst.triggers = src.triggers;
        dst.valueLoopLength = src.valueLoopLength;
        dst.triggerLoopLength = src.triggerLoopLength;
        dst.valueResetInterval = src.valueResetInterval;
        dst.triggerResetInterval = src.triggerResetInterval;
        dst.enableMasterSource = src.enableMasterSource;
        dst.enableLocalSource = src.enableLocalSource;
    };
    
    loadLane(noteLane, pat.noteLane);
    loadLane(octaveLane, pat.octaveLane);
    loadLane(velocityLane, pat.velocityLane);
    loadLane(lengthLane, pat.lengthLane);
}

void ShequencerAudioProcessor::clearPattern(int bank, int slot)
{
    if (bank < 0 || bank >= 4 || slot < 0 || slot >= 16) return;
    patternBanks[bank][slot].isEmpty = true;
}

void ShequencerAudioProcessor::shiftMasterTriggers(int delta)
{
    if (masterLength < 2) return;
    int len = masterLength;
    delta = delta % len;
    if (delta < 0) delta += len;
    
    auto start = masterTriggers.begin();
    auto end = masterTriggers.begin() + len;

    std::rotate(start, end - delta, end);
}

void ShequencerAudioProcessor::setGlobalStepIndex(int targetIndex)
{
    // We want the NEXT step (lastAbsStep + 1) to map to targetIndex
    // (lastAbsStep + 1 + globalStepOffset) % masterLength == targetIndex
    // globalStepOffset = targetIndex - (lastAbsStep + 1)
    
    long long nextAbs = lastAbsStep + 1;
    globalStepOffset = (long long)targetIndex - nextAbs;
    
    // Normalize offset to avoid huge negative numbers, though not strictly necessary for correctness if modulo is handled
    // But keeping it clean:
    // globalStepOffset %= masterLength; 
    // Actually, we don't want to modulo the offset itself because stepCount grows indefinitely.
    // Just setting it is fine.
}

void ShequencerAudioProcessor::setLaneTriggerIndex(SequencerLane& lane, int targetIndex)
{
    // We want the NEXT step to map to targetIndex for this lane
    // (stepCount + lane.triggerStepOffset) % laneLen == targetIndex
    // stepCount = lastAbsStep + 1 + globalStepOffset
    
    long long nextAbs = lastAbsStep + 1;
    long long nextGlobal = nextAbs + globalStepOffset;
    
    lane.triggerStepOffset = (long long)targetIndex - nextGlobal;
}

void ShequencerAudioProcessor::resetLane(SequencerLane& lane, int defaultValue)
{
    lane.values.fill(defaultValue);
    lane.triggers.fill(true);
    lane.valueLoopLength = 16;
    lane.triggerLoopLength = 16;
    lane.valueResetInterval = 0;
    lane.triggerResetInterval = 0;
    lane.triggerStepOffset = 0;
    lane.currentValueStep = 0;
    lane.currentTriggerStep = 0;
}

void ShequencerAudioProcessor::resetAllLanes()
{
    resetLane(noteLane, 0);      // C
    resetLane(octaveLane, 0);    // 0
    resetLane(velocityLane, 64); // 64
    resetLane(lengthLane, 7);    // 16n
    
    masterTriggers.fill(true);
    masterLength = 16;
}

void ShequencerAudioProcessor::syncLaneToBar(SequencerLane& lane)
{
    double barLen = (double)sigNumerator * 4.0 / (double)sigDenominator;
    double nextBarPPQ = lastBarStartPPQ + barLen;
    
    // Ensure nextBar is in future
    while (nextBarPPQ <= lastPositionInQuarterNotes) nextBarPPQ += barLen;
    
    // Calculate absolute step index at next bar start (assuming 16th notes)
    long long targetAbsStep = (long long)std::round(nextBarPPQ / 0.25);
    
    // We want (targetAbsStep + globalStepOffset + laneOffset) % loopLen == 0
    // laneOffset = - (targetAbsStep + globalStepOffset)
    long long globalPos = targetAbsStep + globalStepOffset;
    lane.triggerStepOffset = -globalPos;
    
    lane.resetValuesAtNextBar = true;
    
    // Force update for UI feedback
    // Calculate current absolute step based on current PPQ
    long long currentAbsStep = (long long)std::floor(lastPositionInQuarterNotes / 0.25);
    long long stepCount = currentAbsStep + globalStepOffset;
    
    int localStep = (stepCount + lane.triggerStepOffset) % lane.triggerLoopLength;
    if (localStep < 0) localStep += lane.triggerLoopLength;
    lane.currentTriggerStep = localStep;
}

void ShequencerAudioProcessor::syncAllToBar()
{
    double barLen = (double)sigNumerator * 4.0 / (double)sigDenominator;
    double nextBarPPQ = lastBarStartPPQ + barLen;
    
    while (nextBarPPQ <= lastPositionInQuarterNotes) nextBarPPQ += barLen;
    
    long long targetAbsStep = (long long)std::round(nextBarPPQ / 0.25);
    
    // Align Master to Bar
    // (targetAbsStep + globalOffset) % masterLength == 0
    globalStepOffset = -targetAbsStep;
    
    // Reset all lane offsets so they align with Master (and thus Bar)
    noteLane.triggerStepOffset = 0;
    octaveLane.triggerStepOffset = 0;
    velocityLane.triggerStepOffset = 0;
    lengthLane.triggerStepOffset = 0;
    
    noteLane.resetValuesAtNextBar = true;
    octaveLane.resetValuesAtNextBar = true;
    velocityLane.resetValuesAtNextBar = true;
    lengthLane.resetValuesAtNextBar = true;
    
    // Force update for UI feedback
    long long currentAbsStep = (long long)std::floor(lastPositionInQuarterNotes / 0.25);
    long long stepCount = currentAbsStep + globalStepOffset;
    
    auto updateLane = [&](SequencerLane& lane) {
        int localStep = (stepCount + lane.triggerStepOffset) % lane.triggerLoopLength;
        if (localStep < 0) localStep += lane.triggerLoopLength;
        lane.currentTriggerStep = localStep;
    };
    
    updateLane(noteLane);
    updateLane(octaveLane);
    updateLane(velocityLane);
    updateLane(lengthLane);
    
    // Update Master Step
    int mStep = stepCount % masterLength;
    if (mStep < 0) mStep += masterLength;
    currentMasterStep = mStep;
}

// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ShequencerAudioProcessor();
}
