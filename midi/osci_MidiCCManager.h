#pragma once

#include <JuceHeader.h>
#include "../effect/osci_EffectParameter.h"

namespace osci {

// Manages MIDI CC → parameter mappings.
// Owned by the processor. Audio thread only updates atomics; the message thread
// polls those atomics to manage assignments, undo, and UI notifications.
// Inherits ChangeBroadcaster so UI components can repaint when learning completes.
//
// Type-agnostic: stores juce::AudioProcessorParameterWithID* and works entirely
// in normalised [0,1] space.  Tree sync is handled automatically via the
// osci::TreeSyncableParam interface — callers never supply tree-sync lambdas,
// making it impossible to accidentally perform tree writes on the audio thread.
class MidiCCManager : public juce::ChangeBroadcaster, private juce::Timer {
public:
    using Param = juce::AudioProcessorParameterWithID;

    static constexpr int NUM_CC = 128;
    static constexpr int GESTURE_TIMEOUT_MS = 300;
    static constexpr int TIMER_INTERVAL_MS = 16;

    // Returned by the findParam callback during load so that MidiCCManager can
    // store the base pointer and optional EffectParameter* without knowing the
    // concrete parameter type.
    struct ParamBinding {
        Param* param = nullptr;
        EffectParameter* effectParam = nullptr;
    };

    MidiCCManager();
    ~MidiCCManager() override;

    // Wire undo support — call after construction from the processor.
    void setUndoManager(juce::UndoManager* um, bool* suppressedFlag, juce::ValueTree* tree);

    // --- Audio thread (realtime-safe) ---

    // Process all CC messages in a MIDI buffer. Call from processBlock before synth.
    void processMidiBuffer(const juce::MidiBuffer& midiMessages);

    // --- Message thread ---

    // Start learning: the next CC message will be assigned to this parameter.
    void startLearning(Param* param, EffectParameter* effectParam = nullptr);

    // Cancel learning without assigning.
    void stopLearning();

    // Returns true if any parameter is currently in learning mode.
    bool isLearning() const;

    // Returns true if this specific parameter is currently in learning mode.
    bool isLearning(const Param* param) const;

    // Remove the CC assignment for a given parameter (message thread only).
    void removeAssignment(Param* param);

    // Get the CC number assigned to a parameter, or -1 if none.
    int getAssignedCC(const Param* param) const;

    // Remove all CC assignments (message thread only).
    void clearAllAssignments();

    // --- Serialization (message thread) ---

    void save(juce::XmlElement* parent) const;

    void load(const juce::XmlElement* parent,
              const std::function<ParamBinding(const juce::String&)>& findParam);

    // --- Helpers for building ParamBindings ---

    static ParamBinding makeBinding(FloatParameter* p, EffectParameter* ep = nullptr) {
        return { p, ep };
    }

    static ParamBinding makeBinding(IntParameter* p) {
        return { p, nullptr };
    }

    static ParamBinding makeBinding(BooleanParameter* p) {
        return { p, nullptr };
    }

private:
    // Called from audio thread — must be realtime-safe.
    void processCC(int ccNumber, int ccValue);

    // Sync ValueTree for a CC slot. Must be called on the message thread.
    void syncTree(int cc);

    void clearAllAssignmentsNoLock();
    void updateTimerStateNoLock();
    void timerCallback() override;

    // CC number → parameter pointer (base type). Indexed by CC number (0-127).
    std::atomic<Param*> ccToParam[NUM_CC];

    // CC number → EffectParameter* for sub-range support.
    std::atomic<EffectParameter*> ccEffectParam[NUM_CC];

    // CC number → TreeSyncableParam* for message-thread tree sync.
    TreeSyncableParam* ccSyncable[NUM_CC];

    // Reverse map: parameter → CC number. Message thread only.
    std::unordered_map<Param*, int> paramToCC;

    // Parameter currently awaiting CC assignment.
    std::atomic<Param*> learningParam { nullptr };
    std::atomic<int> learnedCC { -1 };

    // Pending data for the learning parameter (message thread only).
    EffectParameter* pendingEffectParam = nullptr;

    // Gesture tracking — written from audio thread, consumed on message thread.
    std::atomic<bool> gestureStartPending[NUM_CC];
    std::atomic<bool> gestureActive[NUM_CC];
    std::atomic<int64_t> lastCCTime[NUM_CC];
    std::atomic<bool> ccTreeDirty[NUM_CC];
    int activeGestureCount = 0;
    juce::var gestureStartTreeValue[NUM_CC];

    // UndoableAction that captures the net value change for an entire CC gesture.
    struct CCGestureUndoAction : public juce::UndoableAction {
        CCGestureUndoAction(juce::ValueTree tree, juce::Identifier prop, juce::var old, juce::var cur)
            : tree(tree), property(prop), oldValue(old), newValue(cur) {}

        bool perform() override { tree.setProperty(property, newValue, nullptr); return true; }
        bool undo()    override { tree.setProperty(property, oldValue, nullptr); return true; }

        juce::ValueTree tree;
        juce::Identifier property;
        juce::var oldValue, newValue;
    };

    // Protects paramToCC for message-thread operations.
    mutable juce::SpinLock messageLock;

    // Undo support — set via setUndoManager().
    juce::UndoManager* undoManager = nullptr;
    bool* undoSuppressedFlag = nullptr;
    juce::ValueTree* stateTree = nullptr;
};

} // namespace osci
