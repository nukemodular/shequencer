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
    int activeValueStep = 0;

    // Trigger Sequence (Buttons)
    std::array<bool, 16> triggers;
    int triggerLoopLength = 16;
    int currentTriggerStep = 0;
    
    // Source Toggles
    bool enableMasterSource = false; // Yellow Toggle
    bool enableLocalSource = true;   // Colored Toggle
    
    int triggerStepOffset = 0;
    
    bool forceNextStep = false;
    bool resetValuesAtNextBar = false;
    
    // Reset Intervals (0 = OFF, 1, 2, 4, 8, 16, 32, 64, 128)
    int valueResetInterval = 0;
    int triggerResetInterval = 0;

    // Helper to advance value
    void advanceValue()
    {
        if (forceNextStep)
        {
            forceNextStep = false;
        }
        else
        {
            currentValueStep = (currentValueStep + 1) % valueLoopLength;
        }
    }

    void reset() { currentValueStep = 0; activeValueStep = 0; forceNextStep = false; }
    
    void shiftValues(int delta)
    {
        if (valueLoopLength < 2) return;
        int len = valueLoopLength;
        // Normalize delta
        delta = delta % len;
        if (delta < 0) delta += len;
        
        // Rotate right by delta
        auto start = values.begin();
        auto end = values.begin() + len;
        std::rotate(start, end - delta, end);
    }
    
    void shiftTriggers(int delta)
    {
        if (triggerLoopLength < 2) return;
        int len = triggerLoopLength;
        delta = delta % len;
        if (delta < 0) delta += len;
        
        auto start = triggers.begin();
        auto end = triggers.begin() + len;
        std::rotate(start, end - delta, end);
    }
};

struct PatternData
{
    bool isEmpty = true;
    
    // Master
    std::array<bool, 16> masterTriggers;
    int masterLength = 16;
    int shuffleAmount = 1;
    
    // Lanes
    struct LaneData {
        std::array<int, 16> values;
        std::array<bool, 16> triggers;
        int valueLoopLength = 16;
        int triggerLoopLength = 16;
        int valueResetInterval = 0;
        int triggerResetInterval = 0;
        bool enableMasterSource = false;
        bool enableLocalSource = true;
    };
    
    LaneData noteLane;
    LaneData octaveLane;
    LaneData velocityLane;
    LaneData lengthLane;
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
    int shuffleAmount = 1; // 1 (Straight) to 7 (Max Swing)
    bool isShuffleGlobal = true;
    
    SequencerLane noteLane;
    SequencerLane octaveLane;
    SequencerLane velocityLane;
    SequencerLane lengthLane;
    
    // Pattern Management
    std::array<std::array<PatternData, 16>, 4> patternBanks; // 4 Banks of 16 Patterns
    int currentBank = 0;
    int loadedBank = -1;
    int loadedSlot = -1;
    
    void savePattern(int bank, int slot);
    void loadPattern(int bank, int slot);
    void clearPattern(int bank, int slot);
    
    void shiftMasterTriggers(int delta);
    
    void setGlobalStepIndex(int targetIndex);
    void setLaneTriggerIndex(SequencerLane& lane, int targetIndex);
    
    void resetLane(SequencerLane& lane, int defaultValue);
    void resetAllLanes();
    
    // Sync Logic
    void syncLaneToBar(SequencerLane& lane);
    void syncAllToBar();

    // Playback State
    int currentMasterStep = 0;
    long long globalStepOffset = 0;
    long long lastAbsStep = 0;
    double currentPositionInQuarterNotes = 0.0;
    double lastPositionInQuarterNotes = 0.0;
    
    // Hold Logic State
    bool isHoldActive = false;
    int lastTriggeredNoteIndex = -1;
    
    // Timing Info for Sync
    double lastBarStartPPQ = 0.0;
    int sigNumerator = 4;
    int sigDenominator = 4;
    
    bool isPlaying = false;

    // Note Off Management
    struct ActiveNote
    {
        bool isActive = false;
        int noteNumber = 0;
        int midiChannel = 1;
        double noteOffPosition = 0.0; // In quarter notes
    };
    static constexpr int maxActiveNotes = 64;
    std::array<ActiveNote, maxActiveNotes> activeNotes;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShequencerAudioProcessor)
};
