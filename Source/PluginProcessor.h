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
    int activeTriggerStep = 0;
    
    // Source Toggles
    bool enableMasterSource = false; // Yellow Toggle
    bool enableLocalSource = true;   // Colored Toggle
    
    int triggerStepOffset = 0;
    
    bool forceNextStep = false;
    bool resetValuesAtNextBar = false;
    
    // Reset Intervals (0 = OFF, 1, 2, 4, 8, 16, 32, 64, 128)
    int valueResetInterval = 0;
    int triggerResetInterval = 0;
    
    // Randomization Range (0 = Full Random, >0 = +/- Range)
    int randomRange = 0;
    
    enum class Direction { Forward, Backward, PingPong, Bounce, Random, RandomDirection };
    Direction valueDirection = Direction::Forward;
    Direction triggerDirection = Direction::Forward;
    
    bool valueMovingForward = true;
    bool triggerMovingForward = true;
    
    // MIDI CC (0 = Off, 1-127 = CC Number)
    int midiCC = 0;

    // Helper to advance value
    void advanceValue(juce::Random& r)
    {
        if (forceNextStep)
        {
            forceNextStep = false;
        }
        else
        {
            auto getNextStep = [&](int current, int len, Direction dir, bool& movingForward) -> int {
                if (len <= 1) return 0;
                
                switch (dir)
                {
                    case Direction::Forward:
                        return (current + 1) % len;
                        
                    case Direction::Backward:
                        return (current - 1 + len) % len;
                        
                    case Direction::PingPong: // 0, 1, 2, 2, 1, 0
                        if (movingForward) {
                            if (current >= len - 1) {
                                movingForward = false;
                                return current; // Repeat end
                            }
                            return current + 1;
                        } else {
                            if (current <= 0) {
                                movingForward = true;
                                return current; // Repeat start
                            }
                            return current - 1;
                        }
                        
                    case Direction::Bounce: // 0, 1, 2, 1, 0
                        if (movingForward) {
                            if (current >= len - 1) {
                                movingForward = false;
                                return len - 2;
                            }
                            return current + 1;
                        } else {
                            if (current <= 0) {
                                movingForward = true;
                                return 1;
                            }
                            return current - 1;
                        }
                        
                    case Direction::Random:
                        return r.nextInt(len);

                    case Direction::RandomDirection:
                        int stepDir = r.nextBool() ? 1 : -1;
                        return (current + stepDir + len) % len;
                }
                return 0;
            };
            
            currentValueStep = getNextStep(currentValueStep, valueLoopLength, valueDirection, valueMovingForward);
        }
    }
    
    void advanceTrigger(juce::Random& r)
    {
        auto getNextStep = [&](int current, int len, Direction dir, bool& movingForward) -> int {
            if (len <= 1) return 0;
            
            switch (dir)
            {
                case Direction::Forward:
                    return (current + 1) % len;
                    
                case Direction::Backward:
                    return (current - 1 + len) % len;
                    
                case Direction::PingPong:
                    if (movingForward) {
                        if (current >= len - 1) {
                            movingForward = false;
                            return current; // Repeat end
                        }
                        return current + 1;
                    } else {
                        if (current <= 0) {
                            movingForward = true;
                            return current; // Repeat start
                        }
                        return current - 1;
                    }
                    
                case Direction::Bounce:
                    if (movingForward) {
                        if (current >= len - 1) {
                            movingForward = false;
                            return len - 2;
                        }
                        return current + 1;
                    } else {
                        if (current <= 0) {
                            movingForward = true;
                            return 1;
                        }
                        return current - 1;
                    }
                    
                case Direction::Random:
                    return r.nextInt(len);

                case Direction::RandomDirection:
                    int stepDir = r.nextBool() ? 1 : -1;
                    return (current + stepDir + len) % len;
            }
            return 0;
        };
        
        currentTriggerStep = getNextStep(currentTriggerStep, triggerLoopLength, triggerDirection, triggerMovingForward);
    }

    void reset() { 
        currentValueStep = valueLoopLength - 1; 
        activeValueStep = 0; 
        currentTriggerStep = 0;
        activeTriggerStep = 0;
        forceNextStep = false; 
        valueMovingForward = true;
        triggerMovingForward = true;
    }
    
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
    std::array<bool, 16> masterProbEnabled; // Probability Step Toggle
    int masterLength = 16;
    int shuffleAmount = 1;
    int masterProbability = 100; // 0-100%
    
    // Lanes
    struct LaneData {
        std::array<int, 16> values;
        std::array<bool, 16> triggers;
        int valueLoopLength = 16;
        int triggerLoopLength = 16;
        int valueResetInterval = 0;
        int triggerResetInterval = 0;
        int randomRange = 0;
        bool enableMasterSource = false;
        bool enableLocalSource = true;
        int valueDirection = 0; // Stored as int
        int triggerDirection = 0;
        int midiCC = 0;
    };
    
    LaneData noteLane;
    LaneData octaveLane;
    LaneData velocityLane;
    LaneData lengthLane;
    
    LaneData ccLane1;
    LaneData ccLane2;
    LaneData ccLane3;
    LaneData ccLane4;
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
    std::array<bool, 16> masterProbEnabled;
    int masterLength = 16;
    int shuffleAmount = 1; // 1 (Straight) to 7 (Max Swing)
    int masterProbability = 100; // 0-100%
    int activeShuffleAmount = 1; // Used for audio processing to ensure safe updates
    bool isShuffleGlobal = true;
    
    juce::Random random; // Member random object for audio thread safety

    SequencerLane noteLane;
    SequencerLane octaveLane;
    SequencerLane velocityLane;
    SequencerLane lengthLane;
    
    SequencerLane ccLane1;
    SequencerLane ccLane2;
    SequencerLane ccLane3;
    SequencerLane ccLane4;
    
    // Pattern Management
    std::array<std::array<PatternData, 16>, 4> patternBanks; // 4 Banks of 16 Patterns
    int currentBank = 0;
    int loadedBank = -1;
    int loadedSlot = -1;
    
    juce::CriticalSection patternLock;
    std::atomic<int> pendingLoadBank{ -1 };
    std::atomic<int> pendingLoadSlot{ -1 };
    
    void savePattern(int bank, int slot);
    void loadPattern(int bank, int slot);
    void applyPendingPatternLoad();
    void clearPattern(int bank, int slot);
    
    void saveAllPatternsToJson(const juce::File& file);
    void loadAllPatternsFromJson(const juce::File& file);
    
    void shiftMasterTriggers(int delta);
    
    void setGlobalStepIndex(int targetIndex);
    void setLaneTriggerIndex(SequencerLane& lane, int targetIndex);
    void setLaneValueIndex(SequencerLane& lane, int targetIndex);
    
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
