#include "PluginProcessor.h"
#include "PluginEditor.h"

ShequencerAudioProcessor::ShequencerAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Initialize default values
    masterTriggers.fill(false);
    masterProbEnabled.fill(false);
    masterProbability = 50;
    
    noteLane.values.fill(0); // C
    noteLane.triggers.fill(true);
    noteLane.enableMasterSource = false;
    
    octaveLane.values.fill(3); // Octave 3
    octaveLane.triggers.fill(true);
    octaveLane.enableMasterSource = false;
    
    velocityLane.values.fill(100);
    velocityLane.triggers.fill(true);
    velocityLane.enableMasterSource = false;
    
    lengthLane.values.fill(5); // 32n
    lengthLane.triggers.fill(true);
    lengthLane.enableMasterSource = false;
    
    // Initialize CC Lanes
    ccLane1.values.fill(0);
    ccLane1.triggers.fill(true);
    ccLane1.midiCC = 0; // OFF by default

    ccLane2.values.fill(0);
    ccLane2.triggers.fill(true);
    ccLane2.midiCC = 0;

    ccLane3.values.fill(0);
    ccLane3.triggers.fill(true);
    ccLane3.midiCC = 0;

    ccLane4.values.fill(0);
    ccLane4.triggers.fill(true);
    ccLane4.midiCC = 0;

    activeShuffleAmount = shuffleAmount;
}

ShequencerAudioProcessor::~ShequencerAudioProcessor()
{
}

const juce::String ShequencerAudioProcessor::getName() const
{
    return "toolBoy SH-equencer v1";
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

void ShequencerAudioProcessor::setCurrentProgram (int)
{
}

const juce::String ShequencerAudioProcessor::getProgramName (int)
{
    return {};
}

void ShequencerAudioProcessor::changeProgramName (int, const juce::String&)
{
}

void ShequencerAudioProcessor::prepareToPlay (double, int)
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
    
    ccLane1.reset();
    ccLane2.reset();
    ccLane3.reset();
    ccLane4.reset();
    
    activeShuffleAmount = shuffleAmount;
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

    // Handle MIDI Pattern Switching
    juce::MidiBuffer processedMidi;
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        bool isControlMessage = false;
        
        if (msg.isNoteOn())
        {
            int channel = msg.getChannel();
            int note = msg.getNoteNumber();
            
            if (channel == 2)
            {
                // Map MIDI notes 0-63 to Patterns (Bank 0-3, Slot 0-15)
                if (note >= 0 && note < 64)
                {
                    int bank = note / 16;
                    int slot = note % 16;
                    loadPattern(bank, slot);
                    isControlMessage = true;
                }
            }
            else if (channel == 1)
            {
                // Transposition (Center at 60)
                transposeOffset = note - 60;
                
                // If NOT in MIDI Gate Mode, consume the message.
                // If IN MIDI Gate Mode, let it pass through to be used as a gate trigger.
                if (!isMidiGateMode) {
                    isControlMessage = true;
                }
            }
        }
        
        if (!isControlMessage)
            processedMidi.addEvent(msg, metadata.samplePosition);
    }
    midiMessages.swapWith(processedMidi);

    // Apply any pending pattern load (from UI or MIDI)
    applyPendingPatternLoad();

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

    // Handle MIDI Input for Gate Mode
    // We need to process MIDI events in time order relative to the grid steps
    // So we collect them here, but process them inside the grid loop
    struct MidiEvent {
        int sampleOffset;
        bool isNoteOn;
        int noteNumber;
    };
    std::vector<MidiEvent> midiEvents;

    if (isMidiGateMode)
    {
        for (const auto metadata : midiMessages)
        {
            auto msg = metadata.getMessage();
            if (msg.isNoteOn())
            {
                midiEvents.push_back({metadata.samplePosition, true, msg.getNoteNumber()});
            }
            else if (msg.isNoteOff())
            {
                midiEvents.push_back({metadata.samplePosition, false, msg.getNoteNumber()});
            }
        }
        
        // Filter out Note On/Off from passing through, but keep CCs
        // IMPORTANT: We must NOT clear the buffer if we want other plugins to receive it?
        // But this is an instrument/sequencer, so we usually replace the output.
        // We are generating our own notes, so we should filter out the input notes
        // to avoid double triggering or passing through the raw gate notes.
        juce::MidiBuffer processedMidi;
        for (const auto metadata : midiMessages)
        {
            auto msg = metadata.getMessage();
            if (!msg.isNoteOn() && !msg.isNoteOff())
                processedMidi.addEvent(msg, metadata.samplePosition);
        }
        midiMessages.swapWith(processedMidi);
    }

    if (!isPlaying)
    {
        isPlaying = true;
        if (auto ppq = pos.getPpqPosition())
            lastPositionInQuarterNotes = *ppq;
            
        // Check if we are starting mid-bar
        if (auto barStart = pos.getPpqPositionOfLastBarStart())
        {
            double currentPPQ = *pos.getPpqPosition();
            if (currentPPQ > *barStart + 0.05) // Tolerance
            {
                waitingForBarSync = true;
            }
            else
            {
                waitingForBarSync = false;
            }
            lastBarStartPPQ = *barStart;
        }
            
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
        
        ccLane1.reset();
        ccLane2.reset();
        ccLane3.reset();
        ccLane4.reset();
    }
    
    // Update Timing Info
    if (auto barStart = pos.getPpqPositionOfLastBarStart())
    {
        if (std::abs(*barStart - lastBarStartPPQ) > 0.0001)
        {
            // New Bar Detected
            if (waitingForBarSync)
            {
                waitingForBarSync = false;
                syncAllToBar();
                resetAllLanes();
            }
            
            auto checkReset = [](SequencerLane& lane) {
                if (lane.resetValuesAtNextBar) {
                    lane.currentValueStep = lane.valueLoopLength - 1;
                    lane.activeValueStep = 0;
                    lane.resetValuesAtNextBar = false;
                }
            };
            checkReset(noteLane);
            checkReset(octaveLane);
            checkReset(velocityLane);
            checkReset(lengthLane);
            
            checkReset(ccLane1);
            checkReset(ccLane2);
            checkReset(ccLane3);
            checkReset(ccLane4);
        }
        lastBarStartPPQ = *barStart;
    }
    
    if (waitingForBarSync) return; // Wait for next bar
    
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
                    // Reset trigger sequence to start
                    lane.currentTriggerStep = 0;
                    lane.triggerMovingForward = true;
                }
            }
        };
        
        processIntervalReset(ccLane1);
        processIntervalReset(ccLane2);
        processIntervalReset(ccLane3);
        processIntervalReset(ccLane4);
        
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
    
    int currentSamplePos = 0;
    
    auto sendCC = [&](SequencerLane& lane, int offset, int val) {
        if (lane.midiCC == 128) // PGM
            midiMessages.addEvent(juce::MidiMessage::programChange(1, val), offset);
        else if (lane.midiCC == 129) // A.TOUCH
            midiMessages.addEvent(juce::MidiMessage::channelPressureChange(1, val), offset);
        else if (lane.midiCC >= 1 && lane.midiCC <= 127)
            midiMessages.addEvent(juce::MidiMessage::controllerEvent(1, lane.midiCC, val), offset);
    };
    
    auto processCCRamps = [&](int startSample, int count) {
        for (auto* lane : {&ccLane1, &ccLane2, &ccLane3, &ccLane4}) {
            if (lane->midiCC == 0) continue;
            if (!lane->isRamping) continue;
            
            for (int i = 0; i < count; ++i) {
                if (lane->rampSamplesRemaining <= 0) {
                    lane->isRamping = false;
                    lane->currentSmoothedValue = (float)lane->targetCCValue;
                    
                    if (lane->lastSentCCValue != lane->targetCCValue) {
                        sendCC(*lane, startSample + i, lane->targetCCValue);
                        lane->lastSentCCValue = lane->targetCCValue;
                    }
                    break;
                }
                
                lane->currentSmoothedValue += lane->rampIncrement;
                lane->rampSamplesRemaining--;
                
                // Rate limit: every 128 samples (~2.9ms at 44.1k)
                if (i % 128 == 0) {
                    int val = (int)lane->currentSmoothedValue;
                    if (val != lane->lastSentCCValue) {
                        sendCC(*lane, startSample + i, val);
                        lane->lastSentCCValue = val;
                    }
                }
            }
        }
    };
    
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
        // Safe Shuffle Update: Only update on even (unshuffled) steps
        if (k % 2 == 0)
            activeShuffleAmount = shuffleAmount;

        lastAbsStep = k;
        double baseTime = k * stepDuration;
        double time = baseTime;
        
        long long stepCount = k + globalStepOffset;
        int stepIdx = stepCount % masterLength;
        if (stepIdx < 0) stepIdx += masterLength;

        // Calculate delay based on active shuffle amount
        double currentShuffleDelay = 0.0;
        if (activeShuffleAmount > 1)
            currentShuffleDelay = ((double)(activeShuffleAmount - 1) / 6.0) * maxDelay;

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
            
            // Process Ramps up to here
            int samplesToProcess = sampleOffset - currentSamplePos;
            if (samplesToProcess > 0) processCCRamps(currentSamplePos, samplesToProcess);
            currentSamplePos = sampleOffset;
            
            // Update MIDI State up to this sample offset
            if (isMidiGateMode)
            {
                // Process events that happened before or at this step
                auto it = midiEvents.begin();
                while (it != midiEvents.end())
                {
                    if (it->sampleOffset <= sampleOffset)
                    {
                        if (it->isNoteOn) {
                            heldMidiNotes.insert(it->noteNumber);
                            pendingMidiTrigger = true;
                        }
                        else {
                            heldMidiNotes.erase(it->noteNumber);
                            
                            // Kill specific sustained notes linked to this MIDI note
                            for (auto& note : activeNotes) {
                                if (note.isActive && note.isMidiSustain && note.sourceMidiNote == it->noteNumber) {
                                    midiMessages.addEvent(juce::MidiMessage::noteOff(note.midiChannel, note.noteNumber), it->sampleOffset);
                                    note.isActive = false;
                                }
                            }
                        }
                        it = midiEvents.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
            
            // Calculate actual step duration for length logic
            double nextStepBaseTime = (k + 1) * stepDuration;
            double nextStepTime = nextStepBaseTime;
            if ((k + 1) % 2 != 0)
                nextStepTime += currentShuffleDelay;
            
            // int nextStepIdx = (stepCount + 1) % masterLength;
            // if (nextStepIdx < 0) nextStepIdx += masterLength;

            double actualStepDuration = nextStepTime - time;
            if (actualStepDuration <= 0) actualStepDuration = 0.01;
        
            // --- CORE LOGIC ---
            currentMasterStep = stepIdx;
            
            // Check Probability (Needed for advancement logic)
            bool probCheck = true;
            if (masterProbEnabled[(size_t)stepIdx])
            {
                int roll = random.nextInt(100);
                if (roll >= masterProbability) probCheck = false;
            }
            
            bool isGateOpen = !heldMidiNotes.empty();

            // 1. Advance Values (Advance Before Play)
            auto processValueAdvancement = [&](SequencerLane& lane) {
                bool masterHit = false;
                if (isMidiGateMode) {
                    // In MIDI Gate Mode, advance only on new MIDI trigger (Step Advance)
                    masterHit = lane.enableMasterSource && pendingMidiTrigger && probCheck;
                } else {
                    masterHit = lane.enableMasterSource && masterTriggers[(size_t)stepIdx] && probCheck;
                }
                
                // Use current trigger step state
                int localStep = lane.currentTriggerStep;
                bool localHit = lane.enableLocalSource && lane.triggers[(size_t)localStep];
                
                if (masterHit || localHit)
                    lane.advanceValue(random);
            };
            
            processValueAdvancement(noteLane);
            processValueAdvancement(octaveLane);
            processValueAdvancement(velocityLane);
            processValueAdvancement(lengthLane);
            
            processValueAdvancement(ccLane1);
            processValueAdvancement(ccLane2);
            processValueAdvancement(ccLane3);
            processValueAdvancement(ccLane4);
            
            // Update Active Step for UI and Playback
            // Note: activeValueStep is updated every step to show current sequencer position
            
            noteLane.activeTriggerStep = noteLane.currentTriggerStep;
            octaveLane.activeTriggerStep = octaveLane.currentTriggerStep;
            velocityLane.activeTriggerStep = velocityLane.currentTriggerStep;
            lengthLane.activeTriggerStep = lengthLane.currentTriggerStep;

            noteLane.activeValueStep = noteLane.currentValueStep;
            octaveLane.activeValueStep = octaveLane.currentValueStep;
            velocityLane.activeValueStep = velocityLane.currentValueStep;
            lengthLane.activeValueStep = lengthLane.currentValueStep;

            ccLane1.activeTriggerStep = ccLane1.currentTriggerStep;
            ccLane2.activeTriggerStep = ccLane2.currentTriggerStep;
            ccLane3.activeTriggerStep = ccLane3.currentTriggerStep;
            ccLane4.activeTriggerStep = ccLane4.currentTriggerStep;
            
            ccLane1.activeValueStep = ccLane1.currentValueStep;
            ccLane2.activeValueStep = ccLane2.currentValueStep;
            ccLane3.activeValueStep = ccLane3.currentValueStep;
            ccLane4.activeValueStep = ccLane4.currentValueStep;

            // Use currentValueStep for logic (the value waiting to be played)
            int l = lengthLane.values[(size_t)lengthLane.currentValueStep];
            bool isHold = (l == 9);

            // Define CC Processing Helper
            auto processCCLane = [&](SequencerLane& lane, bool onlyPGM) {
                if (lane.midiCC == 0) return; // OFF
                if (lane.midiCC == 130) return; // CHORD Mode (Handled in Note Logic)
                
                bool isPGM = (lane.midiCC == 128);
                if (onlyPGM && !isPGM) return;
                if (!onlyPGM && isPGM) return;

                bool masterHit = lane.enableMasterSource && masterTriggers[(size_t)stepIdx] && probCheck;
                bool localHit = lane.enableLocalSource && lane.triggers[(size_t)lane.activeTriggerStep];
                
                if (masterHit || localHit)
                {
                    int val = lane.values[(size_t)lane.currentValueStep];
                    val = juce::jlimit(0, 127, val);
                    
                    lane.targetCCValue = val;
                    
                    if (isPGM) // PGM is never smoothed
                    {
                         lane.isRamping = false;
                         lane.currentSmoothedValue = (float)val;
                         
                         if (val != lane.lastSentCCValue) {
                             sendCC(lane, sampleOffset, val);
                             lane.lastSentCCValue = val;
                         }
                    }
                    else 
                    {
                        if (lane.smoothing == 0)
                        {
                            lane.isRamping = false;
                            lane.currentSmoothedValue = (float)val;
                            
                            if (val != lane.lastSentCCValue) {
                                sendCC(lane, sampleOffset, val);
                                lane.lastSentCCValue = val;
                            }
                        }
                        else
                        {
                            // Setup Ramp
                            // Max duration (100) = 1/32 note
                            // 1/32 note = samplesPerQuarterNote / 8.0
                            double maxDur = samplesPerQuarterNote / 8.0;
                            double dur = (lane.smoothing / 100.0) * maxDur;
                            
                            if (dur < 1.0) {
                                 lane.isRamping = false;
                                 lane.currentSmoothedValue = (float)val;
                                 
                                 if (val != lane.lastSentCCValue) {
                                     sendCC(lane, sampleOffset, val);
                                     lane.lastSentCCValue = val;
                                 }
                            } else {
                                 // Only start ramp if target is different from current output
                                 if (val != lane.lastSentCCValue) {
                                     lane.isRamping = true;
                                     lane.rampSamplesRemaining = (int)dur;
                                     lane.rampIncrement = (val - lane.currentSmoothedValue) / dur;
                                 }
                            }
                        }
                    }
                }
            };

            // Priority 1: Process PGM Changes (Before Notes)
            processCCLane(ccLane1, true);
            processCCLane(ccLane2, true);
            processCCLane(ccLane3, true);
            processCCLane(ccLane4, true);

            // 2. Play Note (Only if Master Trigger is active OR Hold is active)
            bool shouldTrigger = false;
            if (isMidiGateMode) {
                // In MIDI Mode, trigger only on new MIDI trigger (Step Advance)
                shouldTrigger = pendingMidiTrigger && probCheck;
            } else {
                shouldTrigger = (masterTriggers[(size_t)stepIdx] && probCheck);
            }
            if (shouldTrigger || (isHold && isHoldActive))
            {
                 int n = noteLane.values[(size_t)noteLane.currentValueStep];
                 int o = octaveLane.values[(size_t)octaveLane.currentValueStep];
                 int v = velocityLane.values[(size_t)velocityLane.currentValueStep];
                 // int l is already defined above
                 
                 // Note: 0-11 (C to B)
                 // Octave: -2 to 8
                 // C-2 is MIDI 0.
                 // Formula: (Octave + 2) * 12 + Note
                 
                 int mNote = (o + 2) * 12 + n + transposeOffset;
                 mNote = juce::jlimit(0, 127, mNote);

                 // CHORD LOGIC
                 std::vector<int> chordOffsets;
                 chordOffsets.push_back(0); // Root
                 
                 int chordType = 0;
                 auto checkChord = [&](SequencerLane& lane) {
                     if (lane.midiCC == 130) {
                         // Always read the current value for CHORD mode, regardless of trigger state
                         int val = lane.values[(size_t)lane.currentValueStep];
                         if (val > 0) chordType = val;
                     }
                 };
                 checkChord(ccLane1); checkChord(ccLane2); checkChord(ccLane3); checkChord(ccLane4);
                 
                 if (chordType > 0) {
                     switch(chordType) {
                         // 3-Note Chords (1-12)
                         case 1: chordOffsets = {0, 4, 7}; break; // Maj
                         case 2: chordOffsets = {0, 3, 7}; break; // Min
                         case 3: chordOffsets = {0, 3, 6}; break; // Dim
                         case 4: chordOffsets = {0, 4, 8}; break; // Aug
                         case 5: chordOffsets = {0, 2, 7}; break; // Sus2
                         case 6: chordOffsets = {0, 5, 7}; break; // Sus4
                         case 7: chordOffsets = {0, 7, 12}; break; // Power (Root+5+8)
                         case 8: chordOffsets = {0, 4, 12}; break; // Maj (Open/Inv)
                         case 9: chordOffsets = {0, 3, 12}; break; // Min (Open/Inv)
                         case 10: chordOffsets = {0, 7, 16}; break; // Maj (Spread)
                         case 11: chordOffsets = {0, 7, 15}; break; // Min (Spread)
                         case 12: chordOffsets = {0, 12, 24}; break; // Octaves
                         
                         // 4-Note Chords (13-24)
                         case 13: chordOffsets = {0, 4, 7, 11}; break; // Maj7
                         case 14: chordOffsets = {0, 3, 7, 10}; break; // Min7
                         case 15: chordOffsets = {0, 4, 7, 10}; break; // Dom7
                         case 16: chordOffsets = {0, 3, 6, 9}; break; // Dim7
                         case 17: chordOffsets = {0, 3, 6, 10}; break; // HalfDim7
                         case 18: chordOffsets = {0, 3, 7, 11}; break; // MinMaj7
                         case 19: chordOffsets = {0, 4, 7, 9}; break; // Maj6
                         case 20: chordOffsets = {0, 3, 7, 9}; break; // Min6
                         case 21: chordOffsets = {0, 4, 11, 14}; break; // Maj9 (No 5)
                         case 22: chordOffsets = {0, 3, 10, 14}; break; // Min9 (No 5)
                         case 23: chordOffsets = {0, 5, 7, 10}; break; // 7sus4
                         case 24: chordOffsets = {0, 4, 10, 15}; break; // 7#9
                         
                         default: chordOffsets = {0, 4, 7}; break; // Default to Maj
                     }
                 }
                 
                 double dur = 0.25;
                 // bool play = true;
                 
                 // Length Values: 
                 // 0:OFF, 1:128n, 2:128d, 3:64n, 4:64d, 5:32n, 6:32d, 7:16n, 8:LEG, 9:HOLD
                 bool isHoldStep = (l == 9);
                 bool shouldPlay = (l != 0);
                 
                 if (isMidiGateMode && l == 0) shouldPlay = true; // Treat 0 as Sustain in MIDI Mode

                 if (l == 0) { /* play = false; */ }
                 else if (l == 1) dur = 0.03125;
                 else if (l == 2) dur = 0.046875;
                 else if (l == 3) dur = 0.0625;
                 else if (l == 4) dur = 0.09375;
                 else if (l == 5) dur = 0.125;
                 else if (l == 6) dur = 0.1875;
                 else if (l == 7) dur = actualStepDuration * 0.96;
                 else if (l == 8) dur = actualStepDuration + 0.01; // Legato
                 else if (l == 9) {
                     // play = true; // HOLD triggers a note
                     dur = actualStepDuration; // Default to fill step
                 }
                 
                 bool extended = false;
                 
                 // Check if we are continuing a hold chain
                 // Only extend if there is NO new trigger (Gate OFF) OR if it is an explicit HOLD step
                 if (isHoldActive && lastTriggeredGroupID >= 0 && (!masterTriggers[(size_t)stepIdx] || isHoldStep))
                 {
                     // Verify at least one note in group is still active
                     bool groupFound = false;
                     for (auto& note : activeNotes) {
                         if (note.isActive && note.groupID == lastTriggeredGroupID) {
                             groupFound = true;
                             break;
                         }
                     }
                     
                     if (groupFound && shouldPlay)
                     {
                         // Extend ALL notes in the group
                         for (auto& note : activeNotes) {
                             if (note.isActive && note.groupID == lastTriggeredGroupID) {
                                 note.noteOffPosition = time + dur;
                             }
                         }
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
                     currentGroupID++; // New group for this trigger
                     lastTriggeredGroupID = currentGroupID;
                     
                     // Determine Source MIDI Note for Sustain
                     int sourceMidiNote = -1;
                     bool isSustain = false;
                     if (isMidiGateMode && l == 0) {
                         if (!heldMidiNotes.empty()) {
                             isSustain = true;
                             sourceMidiNote = *heldMidiNotes.rbegin();
                         } else {
                             // Gate closed before step triggered (staccato tap)
                             // Play short note instead of sustaining
                             isSustain = false;
                             dur = 0.125; 
                         }
                     }

                     for (int offset : chordOffsets) {
                         int currentNote = juce::jlimit(0, 127, mNote + offset);
                         
                         // Handle overlapping notes of same pitch
                         for (auto& note : activeNotes)
                         {
                             if (!note.isActive) continue;
                             if (note.noteNumber == currentNote && note.midiChannel == 1 && note.noteOffPosition >= time - 0.0001)
                             {
                                 midiMessages.addEvent(juce::MidiMessage::noteOff(1, currentNote), sampleOffset);
                                 note.isActive = false;
                             }
                         }

                         midiMessages.addEvent(juce::MidiMessage::noteOn(1, currentNote, (juce::uint8)v), sampleOffset);
                         
                         // Find free slot
                         for (int i = 0; i < maxActiveNotes; ++i)
                         {
                             auto& note = activeNotes[(size_t)i];
                             if (!note.isActive)
                             {
                                 note.isActive = true;
                                 note.noteNumber = currentNote;
                                 note.midiChannel = 1;
                                 note.groupID = currentGroupID;
                                 
                                 if (isSustain) {
                                     note.isMidiSustain = true;
                                     note.sourceMidiNote = sourceMidiNote;
                                     note.noteOffPosition = time + 10000.0; // Infinite
                                 } else {
                                     note.isMidiSustain = false;
                                     note.sourceMidiNote = -1;
                                     note.noteOffPosition = time + dur;
                                 }
                                 
                                 isHoldActive = true; 
                                 
                                 break;
                             }
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

            // 3. Process CC Lanes (Priority 3: Deferred/Smoothed)
            processCCLane(ccLane1, false);
            processCCLane(ccLane2, false);
            processCCLane(ccLane3, false);
            processCCLane(ccLane4, false);
        
            // 4. Advance Triggers (Post-Processing)
            auto processTriggerAdvancement = [&](SequencerLane& lane) {
                // Advance trigger step for next time
                lane.advanceTrigger(random);
            };
            
            processTriggerAdvancement(noteLane);
            processTriggerAdvancement(octaveLane);
            processTriggerAdvancement(velocityLane);
            processTriggerAdvancement(lengthLane);
            
            processTriggerAdvancement(ccLane1);
            processTriggerAdvancement(ccLane2);
            processTriggerAdvancement(ccLane3);
            processTriggerAdvancement(ccLane4);
            
            // Reset Pending Trigger after processing step
            if (isMidiGateMode) pendingMidiTrigger = false;
        }
    }
    
    // Process remaining ramps
    int remaining = numSamples - currentSamplePos;
    if (remaining > 0) processCCRamps(currentSamplePos, remaining);
    
    // Process any remaining MIDI events after the last step
    if (isMidiGateMode)
    {
        auto it = midiEvents.begin();
        while (it != midiEvents.end())
        {
            if (it->isNoteOn) {
                heldMidiNotes.insert(it->noteNumber);
                pendingMidiTrigger = true; // Persist to next block
            }
            else {
                heldMidiNotes.erase(it->noteNumber);
                
                // Kill specific sustained notes linked to this MIDI note
                for (auto& note : activeNotes) {
                    if (note.isActive && note.isMidiSustain && note.sourceMidiNote == it->noteNumber) {
                        midiMessages.addEvent(juce::MidiMessage::noteOff(note.midiChannel, note.noteNumber), it->sampleOffset);
                        note.isActive = false;
                    }
                }
            }
            it = midiEvents.erase(it);
        }
    }
    // Process Note Offs (Time-based Expiry)
    for (auto& note : activeNotes)
    {
        if (!note.isActive) continue;
        
        // Skip if sustained by MIDI
        if (note.isMidiSustain) continue;

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
    xml.setAttribute("masterColor", (int)masterColor.getARGB());
    
    // Save Selection State
    xml.setAttribute("currentBank", currentBank);
    xml.setAttribute("loadedBank", loadedBank);
    xml.setAttribute("loadedSlot", loadedSlot);
    
    juce::String masterTrigStr;
    for (bool b : masterTriggers) masterTrigStr += (b ? "1" : "0");
    xml.setAttribute("masterTriggers", masterTrigStr);

    // Helper to save lane
    auto saveLane = [&](SequencerLane& lane, juce::String name) {
        auto* laneXml = xml.createNewChildElement(name);
        laneXml->setAttribute("midiCC", lane.midiCC);
        laneXml->setAttribute("valueLoopLength", lane.valueLoopLength);
        laneXml->setAttribute("triggerLoopLength", lane.triggerLoopLength);
        laneXml->setAttribute("valueResetInterval", lane.valueResetInterval);
        laneXml->setAttribute("triggerResetInterval", lane.triggerResetInterval);
        laneXml->setAttribute("randomRange", lane.randomRange);
        laneXml->setAttribute("enableMasterSource", lane.enableMasterSource);
        laneXml->setAttribute("enableLocalSource", lane.enableLocalSource);
        laneXml->setAttribute("valueDirection", (int)lane.valueDirection);
        laneXml->setAttribute("triggerDirection", (int)lane.triggerDirection);
        laneXml->setAttribute("customColor", (int)lane.customColor.getARGB());
        
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
    
    saveLane(ccLane1, "CC_LANE_1");
    saveLane(ccLane2, "CC_LANE_2");
    saveLane(ccLane3, "CC_LANE_3");
    saveLane(ccLane4, "CC_LANE_4");
    
    // Save Banks
    auto* banksXml = xml.createNewChildElement("BANKS");
    for (int b = 0; b < 4; ++b)
    {
        auto* bankXml = banksXml->createNewChildElement("BANK");
        bankXml->setAttribute("index", b);
        
        for (int s = 0; s < 16; ++s)
        {
            const auto& pat = patternBanks[(size_t)b][(size_t)s];
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
                    lXml->setAttribute("midiCC", ld.midiCC);
                    lXml->setAttribute("valueLoopLength", ld.valueLoopLength);
                    lXml->setAttribute("triggerLoopLength", ld.triggerLoopLength);
                    lXml->setAttribute("valueResetInterval", ld.valueResetInterval);
                    lXml->setAttribute("triggerResetInterval", ld.triggerResetInterval);
                    lXml->setAttribute("randomRange", ld.randomRange);
                    lXml->setAttribute("enableMasterSource", ld.enableMasterSource);
                    lXml->setAttribute("enableLocalSource", ld.enableLocalSource);
                    lXml->setAttribute("valueDirection", ld.valueDirection);
                    lXml->setAttribute("triggerDirection", ld.triggerDirection);
                    lXml->setAttribute("customColor", (int)ld.customColor);
                    
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
                
                savePatLane(pat.ccLane1, "CC_LANE_1");
                savePatLane(pat.ccLane2, "CC_LANE_2");
                savePatLane(pat.ccLane3, "CC_LANE_3");
                savePatLane(pat.ccLane4, "CC_LANE_4");
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
        masterColor = juce::Colour((juce::uint32)xmlState->getIntAttribute("masterColor", 0));

        currentBank = xmlState->getIntAttribute("currentBank", 0);
        loadedBank = xmlState->getIntAttribute("loadedBank", -1);
        loadedSlot = xmlState->getIntAttribute("loadedSlot", -1);

        juce::String masterTrigStr = xmlState->getStringAttribute("masterTriggers");
        for (int i = 0; i < 16 && i < masterTrigStr.length(); ++i)
            masterTriggers[(size_t)i] = (masterTrigStr[i] == '1');
            
        auto loadLane = [&](SequencerLane& lane, juce::String name) {
            auto* laneXml = xmlState->getChildByName(name);
            if (laneXml)
            {
                lane.midiCC = laneXml->getIntAttribute("midiCC", 0);
                lane.valueLoopLength = laneXml->getIntAttribute("valueLoopLength", 16);
                lane.triggerLoopLength = laneXml->getIntAttribute("triggerLoopLength", 16);
                lane.valueResetInterval = laneXml->getIntAttribute("valueResetInterval", 0);
                lane.triggerResetInterval = laneXml->getIntAttribute("triggerResetInterval", 0);
                lane.randomRange = laneXml->getIntAttribute("randomRange", 0);
                lane.enableMasterSource = laneXml->getBoolAttribute("enableMasterSource", false);
                lane.enableLocalSource = laneXml->getBoolAttribute("enableLocalSource", true);
                lane.valueDirection = (SequencerLane::Direction)laneXml->getIntAttribute("valueDirection", 0);
                lane.triggerDirection = (SequencerLane::Direction)laneXml->getIntAttribute("triggerDirection", 0);
                lane.customColor = juce::Colour((juce::uint32)laneXml->getIntAttribute("customColor", 0));
                
                juce::String valStr = laneXml->getStringAttribute("values");
                juce::StringArray tokens;
                tokens.addTokens(valStr, ",", "");
                for (int i = 0; i < 16 && i < tokens.size(); ++i)
                    lane.values[(size_t)i] = tokens[i].getIntValue();
                    
                juce::String trigStr = laneXml->getStringAttribute("triggers");
                for (int i = 0; i < 16 && i < trigStr.length(); ++i)
                    lane.triggers[(size_t)i] = (trigStr[i] == '1');
            }
        };
        
        loadLane(noteLane, "NOTE_LANE");
        loadLane(octaveLane, "OCTAVE_LANE");
        loadLane(velocityLane, "VELOCITY_LANE");
        loadLane(lengthLane, "LENGTH_LANE");
        
        loadLane(ccLane1, "CC_LANE_1");
        loadLane(ccLane2, "CC_LANE_2");
        loadLane(ccLane3, "CC_LANE_3");
        loadLane(ccLane4, "CC_LANE_4");
        
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
                            auto& pat = patternBanks[(size_t)b][(size_t)s];
                            pat.isEmpty = false;
                            pat.masterLength = patXml->getIntAttribute("masterLength", 16);

                            pat.shuffleAmount = patXml->getIntAttribute("shuffleAmount", 1);
                            
                            juce::String mTrig = patXml->getStringAttribute("masterTriggers");
                            for(int i=0; i<16 && i<mTrig.length(); ++i) pat.masterTriggers[(size_t)i] = (mTrig[i] == '1');
                            
                            auto loadPatLane = [&](PatternData::LaneData& ld, juce::String name) {
                                auto* lXml = patXml->getChildByName(name);
                                if (lXml) {
                                    ld.midiCC = lXml->getIntAttribute("midiCC", 0);
                                    ld.valueLoopLength = lXml->getIntAttribute("valueLoopLength", 16);
                                    ld.triggerLoopLength = lXml->getIntAttribute("triggerLoopLength", 16);
                                    ld.valueResetInterval = lXml->getIntAttribute("valueResetInterval", 0);
                                    ld.triggerResetInterval = lXml->getIntAttribute("triggerResetInterval", 0);
                                    ld.randomRange = lXml->getIntAttribute("randomRange", 0);
                                    ld.enableMasterSource = lXml->getBoolAttribute("enableMasterSource", false);
                                    ld.enableLocalSource = lXml->getBoolAttribute("enableLocalSource", true);
                                    ld.valueDirection = lXml->getIntAttribute("valueDirection", 0);
                                    ld.triggerDirection = lXml->getIntAttribute("triggerDirection", 0);
                                    ld.customColor = (juce::uint32)lXml->getIntAttribute("customColor", 0);
                                    
                                    juce::String vStr = lXml->getStringAttribute("values");
                                    juce::StringArray toks; toks.addTokens(vStr, ",", "");
                                    for(int i=0; i<16 && i<toks.size(); ++i) ld.values[(size_t)i] = toks[i].getIntValue();
                                    
                                    juce::String tStr = lXml->getStringAttribute("triggers");
                                    for(int i=0; i<16 && i<tStr.length(); ++i) ld.triggers[(size_t)i] = (tStr[i] == '1');
                                }
                            };
                            
                            loadPatLane(pat.noteLane, "NOTE_LANE");
                            loadPatLane(pat.octaveLane, "OCTAVE_LANE");
                            loadPatLane(pat.velocityLane, "VELOCITY_LANE");
                            loadPatLane(pat.lengthLane, "LENGTH_LANE");
                            
                            loadPatLane(pat.ccLane1, "CC_LANE_1");
                            loadPatLane(pat.ccLane2, "CC_LANE_2");
                            loadPatLane(pat.ccLane3, "CC_LANE_3");
                            loadPatLane(pat.ccLane4, "CC_LANE_4");
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
    
    const juce::ScopedLock sl(patternLock);
    
    loadedBank = bank;
    loadedSlot = slot;
    
    auto& pat = patternBanks[(size_t)bank][(size_t)slot];
    pat.isEmpty = false;
    
    pat.masterLength = masterLength;
    pat.shuffleAmount = shuffleAmount;
    pat.masterProbability = masterProbability;
    pat.masterTriggers = masterTriggers;
    pat.masterProbEnabled = masterProbEnabled;
    pat.masterColor = masterColor.getARGB();
    
    auto copyLane = [](const SequencerLane& src, PatternData::LaneData& dst) {
        dst.midiCC = src.midiCC;
        dst.values = src.values;
        dst.triggers = src.triggers;
        dst.valueLoopLength = src.valueLoopLength;
        dst.triggerLoopLength = src.triggerLoopLength;
        dst.valueResetInterval = src.valueResetInterval;
        dst.triggerResetInterval = src.triggerResetInterval;
        dst.randomRange = src.randomRange;
        dst.enableMasterSource = src.enableMasterSource;
        dst.enableLocalSource = src.enableLocalSource;
        dst.valueDirection = (int)src.valueDirection;
        dst.triggerDirection = (int)src.triggerDirection;
        dst.customColor = src.customColor.getARGB();
        dst.smoothing = src.smoothing;
    };
    
    copyLane(noteLane, pat.noteLane);
    copyLane(octaveLane, pat.octaveLane);
    copyLane(velocityLane, pat.velocityLane);
    copyLane(lengthLane, pat.lengthLane);
    
    copyLane(ccLane1, pat.ccLane1);
    copyLane(ccLane2, pat.ccLane2);
    copyLane(ccLane3, pat.ccLane3);
    copyLane(ccLane4, pat.ccLane4);
}

void ShequencerAudioProcessor::loadPattern(int bank, int slot)
{
    if (bank < 0 || bank >= 4 || slot < 0 || slot >= 16) return;
    pendingLoadBank = bank;
    pendingLoadSlot = slot;
}

void ShequencerAudioProcessor::applyPendingPatternLoad()
{
    int slot = pendingLoadSlot.load();
    int bank = pendingLoadBank.load();
    
    if (slot == -1 || bank == -1) return;
    
    // Try to get lock - if failed (e.g. UI saving), skip this block
    if (patternLock.tryEnter())
    {
        if (bank >= 0 && bank < 4 && slot >= 0 && slot < 16)
        {
            const auto& pat = patternBanks[(size_t)bank][(size_t)slot];
            if (!pat.isEmpty)
            {
                loadedBank = bank;
                loadedSlot = slot;
                
                masterLength = pat.masterLength;
                if (!isShuffleGlobal) shuffleAmount = pat.shuffleAmount;
                masterProbability = pat.masterProbability;
                masterTriggers = pat.masterTriggers;
                masterProbEnabled = pat.masterProbEnabled;
                masterColor = juce::Colour(pat.masterColor);
                
                auto loadLane = [](SequencerLane& dst, const PatternData::LaneData& src) {
                    dst.midiCC = src.midiCC;
                    dst.values = src.values;
                    dst.triggers = src.triggers;
                    dst.valueLoopLength = src.valueLoopLength;
                    dst.triggerLoopLength = src.triggerLoopLength;
                    dst.valueResetInterval = src.valueResetInterval;
                    dst.triggerResetInterval = src.triggerResetInterval;
                    dst.randomRange = src.randomRange;
                    dst.enableMasterSource = src.enableMasterSource;
                    dst.enableLocalSource = src.enableLocalSource;
                    dst.valueDirection = (SequencerLane::Direction)src.valueDirection;
                    dst.triggerDirection = (SequencerLane::Direction)src.triggerDirection;
                    dst.customColor = juce::Colour(src.customColor);
                    dst.smoothing = src.smoothing;
                };
                
                loadLane(noteLane, pat.noteLane);
                loadLane(octaveLane, pat.octaveLane);
                loadLane(velocityLane, pat.velocityLane);
                loadLane(lengthLane, pat.lengthLane);
                loadLane(ccLane1, pat.ccLane1);
                loadLane(ccLane2, pat.ccLane2);
                loadLane(ccLane3, pat.ccLane3);
                loadLane(ccLane4, pat.ccLane4);
                
                // Reset Playheads on Pattern Load
                noteLane.reset();
                octaveLane.reset();
                velocityLane.reset();
                lengthLane.reset();
                
                ccLane1.reset();
                ccLane2.reset();
                ccLane3.reset();
                ccLane4.reset();
                lengthLane.reset();
            }
        }
        
        pendingLoadSlot = -1;
        pendingLoadBank = -1;
        
        patternLock.exit();
    }
}

void ShequencerAudioProcessor::clearPattern(int bank, int slot)
{
    if (bank < 0 || bank >= 4 || slot < 0 || slot >= 16) return;
    
    const juce::ScopedLock sl(patternLock);
    
    patternBanks[(size_t)bank][(size_t)slot].isEmpty = true;
    patternBanks[(size_t)bank][(size_t)slot].masterProbEnabled.fill(false);
    patternBanks[(size_t)bank][(size_t)slot].masterProbability = 100;
}

void ShequencerAudioProcessor::saveAllPatternsToJson(const juce::File& file)
{
    juce::var root(new juce::DynamicObject());
    juce::Array<juce::var> banks;
    
    {
        const juce::ScopedLock sl(patternLock);
        
        for (int b = 0; b < 4; ++b)
        {
            juce::var bankObj(new juce::DynamicObject());
            bankObj.getDynamicObject()->setProperty("index", b);
            
            juce::Array<juce::var> patterns;
            
            for (int s = 0; s < 16; ++s)
            {
                const auto& pat = patternBanks[(size_t)b][(size_t)s];
                if (!pat.isEmpty)
                {
                    juce::var patObj(new juce::DynamicObject());
                    patObj.getDynamicObject()->setProperty("slot", s);
                    patObj.getDynamicObject()->setProperty("masterLength", pat.masterLength);
                    patObj.getDynamicObject()->setProperty("shuffleAmount", pat.shuffleAmount);
                    patObj.getDynamicObject()->setProperty("masterProbability", pat.masterProbability);
                    patObj.getDynamicObject()->setProperty("masterColor", (int)pat.masterColor);
                    
                    juce::String mTrig;
                    for (bool v : pat.masterTriggers) mTrig += (v ? "1" : "0");
                    patObj.getDynamicObject()->setProperty("masterTriggers", mTrig);

                    juce::String mProb;
                    for (bool v : pat.masterProbEnabled) mProb += (v ? "1" : "0");
                    patObj.getDynamicObject()->setProperty("masterProbEnabled", mProb);
                    
                    auto savePatLane = [&](const PatternData::LaneData& ld, juce::String name) {
                        juce::var lObj(new juce::DynamicObject());
                        lObj.getDynamicObject()->setProperty("midiCC", ld.midiCC);
                        lObj.getDynamicObject()->setProperty("valueLoopLength", ld.valueLoopLength);
                        lObj.getDynamicObject()->setProperty("triggerLoopLength", ld.triggerLoopLength);
                        lObj.getDynamicObject()->setProperty("valueResetInterval", ld.valueResetInterval);
                        lObj.getDynamicObject()->setProperty("triggerResetInterval", ld.triggerResetInterval);
                        lObj.getDynamicObject()->setProperty("randomRange", ld.randomRange);
                        lObj.getDynamicObject()->setProperty("enableMasterSource", ld.enableMasterSource);
                        lObj.getDynamicObject()->setProperty("enableLocalSource", ld.enableLocalSource);
                        lObj.getDynamicObject()->setProperty("valueDirection", ld.valueDirection);
                        lObj.getDynamicObject()->setProperty("triggerDirection", ld.triggerDirection);
                        lObj.getDynamicObject()->setProperty("customColor", (int)ld.customColor);
                        lObj.getDynamicObject()->setProperty("smoothing", ld.smoothing);
                        
                        juce::String vStr;
                        for (int v : ld.values) vStr += juce::String(v) + ",";
                        lObj.getDynamicObject()->setProperty("values", vStr);
                        
                        juce::String tStr;
                        for (bool v : ld.triggers) tStr += (v ? "1" : "0");
                        lObj.getDynamicObject()->setProperty("triggers", tStr);
                        
                        patObj.getDynamicObject()->setProperty(name, lObj);
                    };
                    
                    savePatLane(pat.noteLane, "NOTE_LANE");
                    savePatLane(pat.octaveLane, "OCTAVE_LANE");
                    savePatLane(pat.velocityLane, "VELOCITY_LANE");
                    savePatLane(pat.lengthLane, "LENGTH_LANE");
                    
                    savePatLane(pat.ccLane1, "CC_LANE_1");
                    savePatLane(pat.ccLane2, "CC_LANE_2");
                    savePatLane(pat.ccLane3, "CC_LANE_3");
                    savePatLane(pat.ccLane4, "CC_LANE_4");
                    
                    patterns.add(patObj);
                }
            }
            bankObj.getDynamicObject()->setProperty("patterns", patterns);
            banks.add(bankObj);
        }
    }
    
    root.getDynamicObject()->setProperty("banks", banks);
    
    juce::String jsonString = juce::JSON::toString(root);
    file.replaceWithText(jsonString);
}

void ShequencerAudioProcessor::loadAllPatternsFromJson(const juce::File& file)
{
    juce::var root = juce::JSON::parse(file);
    if (!root.isObject()) return;
    
    const juce::ScopedLock sl(patternLock);
    
    // Clear existing patterns
    for(auto& bank : patternBanks)
        for(auto& pat : bank)
            pat.isEmpty = true;
            
    auto banks = root.getProperty("banks", juce::var());
    if (banks.isArray())
    {
        for (int i = 0; i < banks.size(); ++i)
        {
            auto bankObj = banks[i];
            int b = bankObj.getProperty("index", -1);
            if (b >= 0 && b < 4)
            {
                auto patterns = bankObj.getProperty("patterns", juce::var());
                if (patterns.isArray())
                {
                    for (int j = 0; j < patterns.size(); ++j)
                    {
                        auto patObj = patterns[j];
                        int s = patObj.getProperty("slot", -1);
                        if (s >= 0 && s < 16)
                        {
                            auto& pat = patternBanks[(size_t)b][(size_t)s];
                            pat.isEmpty = false;
                            pat.masterLength = patObj.getProperty("masterLength", 16);
                            pat.shuffleAmount = patObj.getProperty("shuffleAmount", 1);
                            pat.masterProbability = patObj.getProperty("masterProbability", 100);
                            pat.masterColor = (juce::uint32)(int)patObj.getProperty("masterColor", 0);
                            
                            juce::String mTrig = patObj.getProperty("masterTriggers", "").toString();
                            for(int k=0; k<16 && k<mTrig.length(); ++k) pat.masterTriggers[(size_t)k] = (mTrig[k] == '1');

                            juce::String mProb = patObj.getProperty("masterProbEnabled", "").toString();
                            for(int k=0; k<16 && k<mProb.length(); ++k) pat.masterProbEnabled[(size_t)k] = (mProb[k] == '1');
                            
                            auto loadPatLane = [&](PatternData::LaneData& ld, juce::String name) {
                                auto lObj = patObj.getProperty(name, juce::var());
                                if (lObj.isObject()) {
                                    ld.midiCC = lObj.getProperty("midiCC", 0);
                                    ld.valueLoopLength = lObj.getProperty("valueLoopLength", 16);
                                    ld.triggerLoopLength = lObj.getProperty("triggerLoopLength", 16);
                                    ld.valueResetInterval = lObj.getProperty("valueResetInterval", 0);
                                    ld.triggerResetInterval = lObj.getProperty("triggerResetInterval", 0);
                                    ld.randomRange = lObj.getProperty("randomRange", 0);
                                    ld.enableMasterSource = lObj.getProperty("enableMasterSource", false);
                                    ld.enableLocalSource = lObj.getProperty("enableLocalSource", true);
                                    ld.valueDirection = lObj.getProperty("valueDirection", 0);
                                    ld.triggerDirection = lObj.getProperty("triggerDirection", 0);
                                    ld.customColor = (juce::uint32)(int)lObj.getProperty("customColor", 0);
                                    ld.smoothing = lObj.getProperty("smoothing", 0);
                                    
                                    juce::String vStr = lObj.getProperty("values", "").toString();
                                    juce::StringArray toks; toks.addTokens(vStr, ",", "");
                                    for(int k=0; k<16 && k<toks.size(); ++k) ld.values[(size_t)k] = toks[k].getIntValue();
                                    
                                    juce::String tStr = lObj.getProperty("triggers", "").toString();
                                    for(int k=0; k<16 && k<tStr.length(); ++k) ld.triggers[(size_t)k] = (tStr[k] == '1');
                                }
                            };
                            
                            loadPatLane(pat.noteLane, "NOTE_LANE");
                            loadPatLane(pat.octaveLane, "OCTAVE_LANE");
                            loadPatLane(pat.velocityLane, "VELOCITY_LANE");
                            
                            loadPatLane(pat.ccLane1, "CC_LANE_1");
                            loadPatLane(pat.ccLane2, "CC_LANE_2");
                            loadPatLane(pat.ccLane3, "CC_LANE_3");
                            loadPatLane(pat.ccLane4, "CC_LANE_4");
                            loadPatLane(pat.lengthLane, "LENGTH_LANE");
                        }
                    }
                }
            }
        }
    }
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
    
    auto startProb = masterProbEnabled.begin();
    auto endProb = masterProbEnabled.begin() + len;
    std::rotate(startProb, endProb - delta, endProb);
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
    lane.currentTriggerStep = targetIndex;
    lane.activeTriggerStep = targetIndex;
    lane.triggerMovingForward = true; // Reset direction state on manual set
}

void ShequencerAudioProcessor::setLaneValueIndex(SequencerLane& lane, int targetIndex)
{
    lane.currentValueStep = targetIndex;
    lane.activeValueStep = targetIndex;
    lane.valueMovingForward = true;
    lane.forceNextStep = true;
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
    lane.currentValueStep = lane.valueLoopLength - 1;
    lane.activeValueStep = 0;
    lane.currentTriggerStep = 0;
    lane.activeTriggerStep = 0;
    lane.valueDirection = SequencerLane::Direction::Forward;
    lane.triggerDirection = SequencerLane::Direction::Forward;
    lane.valueMovingForward = true;
    lane.triggerMovingForward = true;
}

void ShequencerAudioProcessor::resetAllLanes()
{
    resetLane(ccLane1, 0);
    ccLane1.midiCC = 0;
    resetLane(ccLane2, 0);
    ccLane2.midiCC = 0;
    resetLane(ccLane3, 0);
    ccLane3.midiCC = 0;
    resetLane(ccLane4, 0);
    ccLane4.midiCC = 0;
    
    resetLane(noteLane, 0);      // C
    resetLane(octaveLane, 3);    // 3
    resetLane(velocityLane, 64); // 64
    resetLane(lengthLane, 5);    // 32n
    
    masterTriggers.fill(false);
    masterLength = 16;
}

void ShequencerAudioProcessor::syncLaneToBar(SequencerLane& lane)
{
    // Reset to start of sequence
    auto syncLane = [&](SequencerLane& l) {
        l.currentTriggerStep = 0;
        l.triggerMovingForward = true;
        l.resetValuesAtNextBar = true;
    };
    
    syncLane(ccLane1);
    syncLane(ccLane2);
    syncLane(ccLane3);
    syncLane(ccLane4);
    
    lane.currentTriggerStep = 0;
    lane.triggerMovingForward = true;
    
    lane.resetValuesAtNextBar = true;
    
    // Force update for UI feedback is implicit as we set currentTriggerStep
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
    
    // Reset all lane triggers to 0
    noteLane.currentTriggerStep = 0;
    octaveLane.currentTriggerStep = 0;
    velocityLane.currentTriggerStep = 0;
    lengthLane.currentTriggerStep = 0;
    
    noteLane.triggerMovingForward = true;
    octaveLane.triggerMovingForward = true;
    velocityLane.triggerMovingForward = true;
    lengthLane.triggerMovingForward = true;
    
    noteLane.resetValuesAtNextBar = true;
    octaveLane.resetValuesAtNextBar = true;
    velocityLane.resetValuesAtNextBar = true;
    lengthLane.resetValuesAtNextBar = true;
    
    // Update Master Step
    long long currentAbsStep = (long long)std::floor(lastPositionInQuarterNotes / 0.25);
    long long stepCount = currentAbsStep + globalStepOffset;
    
    int mStep = stepCount % masterLength;
    if (mStep < 0) mStep += masterLength;
    currentMasterStep = mStep;
}

// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ShequencerAudioProcessor();
}
