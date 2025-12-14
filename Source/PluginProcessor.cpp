#include "PluginProcessor.h"
#include "PluginEditor.h"

ShequencerAudioProcessor::ShequencerAudioProcessor()
     : AudioProcessor (BusesProperties()
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                       )
{
    // Initialize default values
    masterTriggers.fill(false);
    
    noteLane.values.fill(0); // C
    noteLane.advanceTriggers.fill(true);
    
    octaveLane.values.fill(3); // Octave 3
    octaveLane.advanceTriggers.fill(true);
    
    velocityLane.values.fill(100);
    velocityLane.advanceTriggers.fill(true);
    
    lengthLane.values.fill(4); // 16th
    lengthLane.advanceTriggers.fill(true);
}

ShequencerAudioProcessor::~ShequencerAudioProcessor()
{
}

const juce::String ShequencerAudioProcessor::getName() const
{
    return "shequencer";
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
    return true;
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
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
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

    // Get PlayHead
    auto* playHead = getPlayHead();
    if (playHead == nullptr) return;

    auto positionInfo = playHead->getPosition();
    if (!positionInfo.hasValue()) return;
    
    auto pos = *positionInfo;
    
    if (!pos.getIsPlaying())
    {
        isPlaying = false;
        // Send All Notes Off if we just stopped?
        // For now, just clear active notes
        activeNotes.clear();
        return;
    }

    if (!isPlaying)
    {
        isPlaying = true;
        // Reset logic if needed, or sync to PPQ
    }

    double ppq = *pos.getPpqPosition();
    double bpm = *pos.getBpm();
    
    // Calculate time info
    // We want to trigger on 16th notes.
    // 16th note = 0.25 PPQ.
    
    double samplesPerQuarterNote = (getSampleRate() * 60.0) / bpm;
    
    // Check for Note Offs
    for (auto it = activeNotes.begin(); it != activeNotes.end();)
    {
        if (ppq >= it->noteOffPosition)
        {
            // Send Note Off
            // Calculate offset in samples
            double offsetPPQ = it->noteOffPosition - lastPositionInQuarterNotes;
            // If offset is negative (should have happened in past), send immediately at 0
            int sampleOffset = 0;
            if (offsetPPQ > 0)
                sampleOffset = (int)(offsetPPQ * samplesPerQuarterNote);
            
            if (sampleOffset < 0) sampleOffset = 0;
            if (sampleOffset >= buffer.getNumSamples()) sampleOffset = buffer.getNumSamples() - 1;

            midiMessages.addEvent(juce::MidiMessage::noteOff(it->midiChannel, it->noteNumber), sampleOffset);
            it = activeNotes.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Check for new Step
    // We check if the integer part of (ppq / 0.25) has changed
    // Or simpler: check if we crossed a 0.25 boundary
    
    double stepDuration = 0.25;
    double currentStepFloat = ppq / stepDuration;
    double lastStepFloat = lastPositionInQuarterNotes / stepDuration;
    
    if (std::floor(currentStepFloat) > std::floor(lastStepFloat))
    {
        // New 16th note step!
        // Calculate which step index (0-15)
        long long totalSteps = (long long)std::floor(currentStepFloat);
        int stepIndex = totalSteps % 16;
        currentMasterStep = stepIndex;

        // Check Master Trigger
        if (masterTriggers[stepIndex])
        {
            // Get Values from Lanes
            int noteVal = noteLane.getNextValueAndAdvance();
            int octVal = octaveLane.getNextValueAndAdvance();
            int velVal = velocityLane.getNextValueAndAdvance();
            int lenVal = lengthLane.getNextValueAndAdvance();
            
            // Process Values
            // Note: 0-11 (C to B)
            // Octave: -2 to 8
            // MIDI Note = (Octave + 2) * 12 + Note? 
            // Standard: C3 is 60. C-2 is 0.
            // Let's assume Octave 3 is middle C range.
            // (Octave + 1) * 12 + Note is a common formula where -1 is bottom.
            // Let's use: (octVal + 2) * 12 + noteVal.
            // If octVal is -2, noteVal 0 -> 0. Correct.
            
            int midiNote = (octVal + 2) * 12 + noteVal;
            midiNote = juce::jlimit(0, 127, midiNote);
            
            int velocity = velVal;
            
            // Length
            // 0: Off, 1: 128th, 2: 64th, 3: 32th, 4: 16th, 5: Legato
            double duration = 0.0;
            bool shouldPlay = true;
            
            switch (lenVal)
            {
                case 0: shouldPlay = false; break;
                case 1: duration = 0.03125; break; // 1/32
                case 2: duration = 0.0625; break;  // 1/16
                case 3: duration = 0.125; break;   // 1/8
                case 4: duration = 0.25; break;    // 1/4 (16th note step)
                case 5: duration = 0.25; break;    // Legato (full step)
                default: duration = 0.25; break;
            }
            
            if (shouldPlay && velocity > 0)
            {
                // Send Note On
                // Calculate sample offset relative to block start
                // The step started at floor(currentStepFloat) * 0.25
                double stepStartPPQ = std::floor(currentStepFloat) * stepDuration;
                double offsetPPQ = stepStartPPQ - lastPositionInQuarterNotes; 
                // Note: lastPositionInQuarterNotes is the start of this block? 
                // Wait, getPosition() returns the position at the START of the block.
                // So ppq is start of block.
                // Actually, we need to check if the step boundary is WITHIN this block.
                
                // Let's re-evaluate the timing logic.
                // We have start_ppq (ppq) and end_ppq (ppq + samples / samplesPerQuarterNote).
                // We want to find if a multiple of 0.25 falls in [start_ppq, end_ppq).
                
                double endPPQ = ppq + (buffer.getNumSamples() / samplesPerQuarterNote);
                
                double nextStepPPQ = std::ceil(ppq / stepDuration) * stepDuration;
                
                // If the next step boundary is within this block
                if (nextStepPPQ < endPPQ)
                {
                    // We have a trigger!
                    // But wait, if we just started playback, we might be exactly ON a step.
                    // std::ceil(4.0) is 4.0.
                    // If ppq is 4.0, nextStepPPQ is 4.0.
                    // If ppq is 4.0001, nextStepPPQ is 4.25.
                    
                    // Let's use a small epsilon or just check range.
                    // Actually, the previous logic `floor(current) > floor(last)` was based on state.
                    // But `processBlock` is stateless regarding the *previous* block's exact end unless we store it.
                    // `lastPositionInQuarterNotes` is stored.
                    
                    // Better approach for block processing:
                    // Iterate through all 16th note boundaries that occur in this block.
                    
                    double currentSearchPPQ = std::ceil(ppq / stepDuration) * stepDuration;
                    if (currentSearchPPQ == ppq) {
                         // If we are exactly on the beat, trigger now.
                         // But we need to make sure we don't double trigger if we processed this in previous block?
                         // Usually hosts handle this, but let's be safe.
                         // If we use `nextStepPPQ < endPPQ`, we catch it.
                    }
                    
                    // Let's loop in case buffer is huge (unlikely to span multiple steps but possible)
                    while (currentSearchPPQ < endPPQ)
                    {
                        // Calculate sample offset
                        double offsetFromStart = currentSearchPPQ - ppq;
                        int sampleOffset = (int)(offsetFromStart * samplesPerQuarterNote);
                        
                        // Trigger Logic
                        long long stepCount = (long long)std::round(currentSearchPPQ / stepDuration);
                        int stepIdx = stepCount % 16;
                        currentMasterStep = stepIdx; // Update UI state
                        
                        if (masterTriggers[stepIdx])
                        {
                             // ... (Get values logic repeated) ...
                             // I should refactor this into a helper or just put it here.
                             
                             int n = noteLane.getNextValueAndAdvance();
                             int o = octaveLane.getNextValueAndAdvance();
                             int v = velocityLane.getNextValueAndAdvance();
                             int l = lengthLane.getNextValueAndAdvance();
                             
                             int mNote = (o + 2) * 12 + n;
                             mNote = juce::jlimit(0, 127, mNote);
                             
                             double dur = 0.25;
                             bool play = true;
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
                                 an.noteOffPosition = currentSearchPPQ + dur;
                                 activeNotes.push_back(an);
                             }
                        }
                        
                        currentSearchPPQ += stepDuration;
                    }
                }
            }
        }
    }
    
    lastPositionInQuarterNotes = ppq;
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
    // You should store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void ShequencerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ShequencerAudioProcessor();
}
