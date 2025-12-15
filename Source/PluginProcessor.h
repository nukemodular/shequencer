#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

struct SequencerLane
{
    // Value Sequence (Bars)
    std::array<int, 16> values;
    int valueLoopLength = 16;
    int currentValueStep = 0;

    // Trigger Sequence (Buttons)
    std::array<bool, 16> triggers;
    int triggerLoopLength = 16;
    int currentTriggerStep = 0;
    
    // Source Toggles
    bool enableMasterSource = false; // Yellow Toggle
    bool enableLocalSource = true;   // Colored Toggle
    
    // Helper to advance value
    void advanceValue()
    {
        currentValueStep = (currentValueStep + 1) % valueLoopLength;
    }

    void reset() { currentValueStep = 0; }
};

class ShequencerAudioProcessor  : public juce::AudioProcessor
{
public:
    ShequencerAudioProcessor();
    ~ShequencerAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Sequencer Data
    std::array<bool, 16> masterTriggers;
    int masterLength = 16;
    
    SequencerLane noteLane;
    SequencerLane octaveLane;
    SequencerLane velocityLane;
    SequencerLane lengthLane;

    // Playback State
    int currentMasterStep = 0;
    double currentPositionInQuarterNotes = 0.0;
    double lastPositionInQuarterNotes = 0.0;
    bool isPlaying = false;

    // Note Off Management
    struct ActiveNote
    {
        int noteNumber;
        int midiChannel;
        double noteOffPosition; // In quarter notes
    };
    std::vector<ActiveNote> activeNotes;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShequencerAudioProcessor)
};
