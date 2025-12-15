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
    
    lengthLane.values.fill(4); // 16th
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
    activeNotes.clear();
    
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
        activeNotes.clear();
        return;
    }

    if (!isPlaying)
    {
        isPlaying = true;
        lastPositionInQuarterNotes = *pos.getPpqPosition();
    }

    double currentPPQ = *pos.getPpqPosition();
    double bpm = *pos.getBpm();
    if (bpm <= 0) bpm = 120.0;
    
    double samplesPerQuarterNote = (getSampleRate() * 60.0) / bpm;
    int numSamples = buffer.getNumSamples();
    double endPPQ = currentPPQ + (numSamples / samplesPerQuarterNote);
    
    // Process Note Offs first
    for (auto it = activeNotes.begin(); it != activeNotes.end();)
    {
        if (currentPPQ >= it->noteOffPosition) // Already passed?
        {
             midiMessages.addEvent(juce::MidiMessage::noteOff(it->midiChannel, it->noteNumber), 0);
             it = activeNotes.erase(it);
        }
        else if (it->noteOffPosition < endPPQ) // Will happen in this block
        {
            double offsetPPQ = it->noteOffPosition - currentPPQ;
            int sampleOffset = (int)(offsetPPQ * samplesPerQuarterNote);
            sampleOffset = juce::jlimit(0, numSamples - 1, sampleOffset);
            
            midiMessages.addEvent(juce::MidiMessage::noteOff(it->midiChannel, it->noteNumber), sampleOffset);
            it = activeNotes.erase(it);
        }
        else
        {
            ++it;
        }
    }
    
    // Step Logic
    double stepDuration = 0.25; // 16th note
    
    double searchStart = lastPositionInQuarterNotes;
    double searchEnd = endPPQ;
    
    // Find the first step boundary > searchStart
    double nextStepBoundary = std::floor(searchStart / stepDuration) * stepDuration + stepDuration;
    
    while (nextStepBoundary <= searchEnd)
    {
        // We hit a step at nextStepBoundary!
        
        // Calculate sample offset
        double offsetPPQ = nextStepBoundary - currentPPQ;
        int sampleOffset = (int)(offsetPPQ * samplesPerQuarterNote);
        if (sampleOffset < 0) sampleOffset = 0;
        if (sampleOffset >= numSamples) sampleOffset = numSamples - 1;
        
        // --- CORE LOGIC ---
        long long stepCount = (long long)std::round(nextStepBoundary / stepDuration);
        int stepIdx = stepCount % masterLength;
        currentMasterStep = stepIdx;
        
        // 1. Advance Lanes
        auto processLaneAdvancement = [&](SequencerLane& lane) {
            bool masterHit = lane.enableMasterSource && masterTriggers[stepIdx];
            int localStep = stepCount % lane.triggerLoopLength;
            lane.currentTriggerStep = localStep;
            
            bool localHit = lane.enableLocalSource && lane.triggers[localStep];
            
            if (masterHit || localHit)
                lane.advanceValue();
        };
        
        processLaneAdvancement(noteLane);
        processLaneAdvancement(octaveLane);
        processLaneAdvancement(velocityLane);
        processLaneAdvancement(lengthLane);
        
        // 2. Play Note (Only if Master Trigger is active)
        if (masterTriggers[stepIdx])
        {
             int n = noteLane.values[noteLane.currentValueStep];
             int o = octaveLane.values[octaveLane.currentValueStep];
             int v = velocityLane.values[velocityLane.currentValueStep];
             int l = lengthLane.values[lengthLane.currentValueStep];
             
             // Note: 0-11 (C to B)
             // Octave: -2 to 8
             // C-2 is MIDI 0.
             // Formula: (Octave + 2) * 12 + Note
             
             int mNote = (o + 2) * 12 + n;
             mNote = juce::jlimit(0, 127, mNote);
             
             double dur = 0.25;
             bool play = true;
             
             // Length Values: 0:OFF, 1:128n, 2:64n, 3:32n, 4:16n, 5:LEG
             if (l == 0) play = false;
             else if (l == 1) dur = 0.03125;
             else if (l == 2) dur = 0.0625;
             else if (l == 3) dur = 0.125;
             else if (l == 4) dur = 0.25;
             else if (l == 5) dur = 0.25; // Legato
             
             if (play && v > 0)
             {
                 midiMessages.addEvent(juce::MidiMessage::noteOn(1, mNote, (juce::uint8)v), sampleOffset);
                 
                 ActiveNote an;
                 an.noteNumber = mNote;
                 an.midiChannel = 1;
                 an.noteOffPosition = nextStepBoundary + dur;
                 activeNotes.push_back(an);
             }
        }
        
        nextStepBoundary += stepDuration;
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
    
    juce::String masterTrigStr;
    for (bool b : masterTriggers) masterTrigStr += (b ? "1" : "0");
    xml.setAttribute("masterTriggers", masterTrigStr);
    
    // Helper to save lane
    auto saveLane = [&](SequencerLane& lane, juce::String name) {
        auto* laneXml = xml.createNewChildElement(name);
        laneXml->setAttribute("valueLoopLength", lane.valueLoopLength);
        laneXml->setAttribute("triggerLoopLength", lane.triggerLoopLength);
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
    
    copyXmlToBinary(xml, destData);
}

void ShequencerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState != nullptr && xmlState->hasTagName("SHEQUENCER_STATE"))
    {
        masterLength = xmlState->getIntAttribute("masterLength", 16);
        
        juce::String masterTrigStr = xmlState->getStringAttribute("masterTriggers");
        for (int i = 0; i < 16 && i < masterTrigStr.length(); ++i)
            masterTriggers[i] = (masterTrigStr[i] == '1');
            
        auto loadLane = [&](SequencerLane& lane, juce::String name) {
            auto* laneXml = xmlState->getChildByName(name);
            if (laneXml)
            {
                lane.valueLoopLength = laneXml->getIntAttribute("valueLoopLength", 16);
                lane.triggerLoopLength = laneXml->getIntAttribute("triggerLoopLength", 16);
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
    }
}

// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ShequencerAudioProcessor();
}
