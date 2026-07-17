#include "osci_MidiManager.h"

namespace osci {

MidiManager::MidiManager()
    : slotToParam(std::make_unique<std::atomic<Param*>[]>(NUM_SLOTS)),
      slotEffectParam(std::make_unique<std::atomic<EffectParameter*>[]>(NUM_SLOTS)),
      slotSyncable(std::make_unique<TreeSyncableParam*[]>(NUM_SLOTS)),
      slotHasCustom(std::make_unique<std::atomic<bool>[]>(NUM_SLOTS)),
      slotCustomPendingValue(std::make_unique<std::atomic<float>[]>(NUM_SLOTS)),
      slotCustomPendingDirty(std::make_unique<std::atomic<bool>[]>(NUM_SLOTS)),
      slotCustomId(std::make_unique<juce::String[]>(NUM_SLOTS)),
      slotCustomSetter(std::make_unique<CustomSetter[]>(NUM_SLOTS)),
      gestureStartPending(std::make_unique<std::atomic<bool>[]>(NUM_SLOTS)),
      gestureActive(std::make_unique<std::atomic<bool>[]>(NUM_SLOTS)),
      lastCCTime(std::make_unique<std::atomic<int64_t>[]>(NUM_SLOTS)),
      slotTreeDirty(std::make_unique<std::atomic<bool>[]>(NUM_SLOTS))
{
    for (int i = 0; i < NUM_SLOTS; i++) {
        slotToParam[i].store(nullptr, std::memory_order_relaxed);
        slotEffectParam[i].store(nullptr, std::memory_order_relaxed);
        slotSyncable[i] = nullptr;
        gestureStartPending[i].store(false, std::memory_order_relaxed);
        gestureActive[i].store(false, std::memory_order_relaxed);
        lastCCTime[i].store(0, std::memory_order_relaxed);
        slotTreeDirty[i].store(false, std::memory_order_relaxed);

        slotHasCustom[i].store(false, std::memory_order_relaxed);
        slotCustomPendingValue[i].store(0.0f, std::memory_order_relaxed);
        slotCustomPendingDirty[i].store(false, std::memory_order_relaxed);
    }
    learningParam.store(nullptr, std::memory_order_relaxed);
    customLearningActive.store(false, std::memory_order_relaxed);

#if OSCI_RENDER_CORE_ENABLE_MIDI_CC_LEARN
    setMessageHandler(MessageType::controlChange, [this](const juce::MidiMessage& message) {
        processCC(message.getChannel(), message.getControllerNumber(), message.getControllerValue());
    });
#endif
}

MidiManager::~MidiManager() { stopTimer(); }

void MidiManager::setUndoManager(juce::UndoManager* um, bool* suppressedFlag, juce::ValueTree* tree) {
    jassert(um != nullptr && suppressedFlag != nullptr && tree != nullptr);
    jassert(juce::MessageManager::existsAndIsCurrentThread());
    undoManager = um;
    undoSuppressedFlag = suppressedFlag;
    stateTree = tree;
}

void MidiManager::processMidiBuffer(const juce::MidiBuffer& midiMessages) {
    for (const auto meta : midiMessages) {
        const auto message = meta.getMessage();

        MessageType type;
        if (message.isController()) {
            type = MessageType::controlChange;
        } else if (message.isProgramChange()) {
            type = MessageType::programChange;
        } else {
            continue;
        }

        auto& handler = messageHandlers[handlerIndex(type)];
        if (handler != nullptr) {
            handler(message);
        }
    }
}

void MidiManager::setMessageHandler(MessageType type, MessageHandler handler) {
    jassert(type != MessageType::numMessageTypes);
    messageHandlers[handlerIndex(type)] = std::move(handler);
}

void MidiManager::startLearning(Param* param, EffectParameter* effectParam) {
#if !OSCI_RENDER_CORE_ENABLE_MIDI_CC_LEARN
    juce::ignoreUnused(param, effectParam);
    return;
#else
    jassert(param != nullptr);
    // Cancel any existing custom-target learn so only one learn is active.
    customLearningActive.store(false, std::memory_order_release);
    pendingCustomId = {};
    pendingCustomSetter = {};

    learnedSlot.store(-1, std::memory_order_release);
    pendingEffectParam = effectParam;
    learningParam.store(param, std::memory_order_release);
    updateTimerStateNoLock();
    sendChangeMessage();
#endif
}

void MidiManager::startLearningCustom(const juce::String& id, CustomSetter setter) {
#if !OSCI_RENDER_CORE_ENABLE_MIDI_CC_LEARN
    juce::ignoreUnused(id, setter);
    return;
#else
    jassert(id.isNotEmpty() && setter != nullptr);
    // Cancel any existing param learn.
    learningParam.store(nullptr, std::memory_order_release);
    pendingEffectParam = nullptr;

    learnedSlot.store(-1, std::memory_order_release);
    pendingCustomId = id;
    pendingCustomSetter = std::move(setter);
    customLearningActive.store(true, std::memory_order_release);
    updateTimerStateNoLock();
    sendChangeMessage();
#endif
}

void MidiManager::stopLearning() {
    learnedSlot.store(-1, std::memory_order_release);
    learningParam.store(nullptr, std::memory_order_release);
    pendingEffectParam = nullptr;
    customLearningActive.store(false, std::memory_order_release);
    pendingCustomId = {};
    pendingCustomSetter = {};
    updateTimerStateNoLock();
    sendChangeMessage();
}

bool MidiManager::isLearning() const {
    return learningParam.load(std::memory_order_acquire) != nullptr
        || customLearningActive.load(std::memory_order_acquire);
}

bool MidiManager::isLearning(const Param* param) const {
    return learningParam.load(std::memory_order_acquire) == param;
}

bool MidiManager::isLearningCustom(const juce::String& id) const {
    if (!customLearningActive.load(std::memory_order_acquire)) return false;
    juce::SpinLock::ScopedLockType lock(messageLock);
    return pendingCustomId == id;
}

void MidiManager::removeAssignment(Param* param) {
    juce::SpinLock::ScopedLockType lock(messageLock);
    auto it = paramToSlot.find(param);
    if (it != paramToSlot.end()) {
        int slot = it->second;
        if (gestureActive[slot].load(std::memory_order_acquire)) {
            if (param->getParameterIndex() >= 0)
                param->endChangeGesture();
            gestureActive[slot].store(false, std::memory_order_release);
            if (--activeGestureCount < 0) activeGestureCount = 0;
        }
        gestureStartPending[slot].store(false, std::memory_order_release);
        slotToParam[slot].store(nullptr, std::memory_order_release);
        slotEffectParam[slot].store(nullptr, std::memory_order_release);
        slotSyncable[slot] = nullptr;
        paramToSlot.erase(it);
        updateTimerStateNoLock();
    }
}

MidiManager::Assignment MidiManager::getAssignment(const Param* param) const {
    juce::SpinLock::ScopedLockType lock(messageLock);
    auto it = paramToSlot.find(const_cast<Param*>(param));
    if (it != paramToSlot.end()) {
        int slot = it->second;
        return Assignment{ channelForSlot(slot), ccForSlot(slot) };
    }
    return Assignment{};
}

MidiManager::Assignment MidiManager::getCustomAssignment(const juce::String& id) const {
    juce::SpinLock::ScopedLockType lock(messageLock);
    auto it = customIdToSlot.find(id);
    if (it == customIdToSlot.end()) return {};
    int slot = it->second;
    return Assignment{ channelForSlot(slot), ccForSlot(slot) };
}

int MidiManager::getAssignedCC(const Param* param) const {
    return getAssignment(param).cc;
}

void MidiManager::removeCustomAssignment(const juce::String& id) {
    juce::SpinLock::ScopedLockType lock(messageLock);
    auto it = customIdToSlot.find(id);
    if (it == customIdToSlot.end()) return;
    int slot = it->second;
    slotHasCustom[slot].store(false, std::memory_order_release);
    slotCustomPendingDirty[slot].store(false, std::memory_order_release);
    slotCustomId[slot].clear();
    slotCustomSetter[slot] = {};
    customIdToSlot.erase(it);
    updateTimerStateNoLock();
}

void MidiManager::rebindCustomSetter(const juce::String& id, CustomSetter setter) {
    juce::SpinLock::ScopedLockType lock(messageLock);
    auto it = customIdToSlot.find(id);
    if (it == customIdToSlot.end()) return;
    slotCustomSetter[it->second] = std::move(setter);
}

void MidiManager::clearAllAssignments() {
    juce::SpinLock::ScopedLockType lock(messageLock);
    clearAllAssignmentsNoLock();
}

void MidiManager::save(juce::XmlElement* parent) const {
    juce::SpinLock::ScopedLockType lock(messageLock);
    if (paramToSlot.empty() && customIdToSlot.empty()) {
        juce::Logger::writeToLog("MidiManager::save: no assignments to save");
        return;
    }

    auto* midiCCXml = parent->createNewChildElement("midiCCAssignments");
    for (auto& [param, slot] : paramToSlot) {
        auto* assignXml = midiCCXml->createNewChildElement("assignment");
        assignXml->setAttribute("paramId", param->paramID);
        assignXml->setAttribute("cc", ccForSlot(slot));
        assignXml->setAttribute("channel", channelForSlot(slot));
    }
    for (auto& [id, slot] : customIdToSlot) {
        auto* assignXml = midiCCXml->createNewChildElement("assignment");
        assignXml->setAttribute("customId", id);
        assignXml->setAttribute("cc", ccForSlot(slot));
        assignXml->setAttribute("channel", channelForSlot(slot));
    }
    juce::Logger::writeToLog("MidiManager::save: saved "
                             + juce::String((int)paramToSlot.size()) + " param CC assignments, "
                             + juce::String((int)customIdToSlot.size()) + " custom CC assignments");
}

void MidiManager::load(const juce::XmlElement* parent,
                          const std::function<ParamBinding(const juce::String&)>& findParam) {
    juce::SpinLock::ScopedLockType lock(messageLock);
#if !OSCI_RENDER_CORE_ENABLE_MIDI_CC_LEARN
    juce::ignoreUnused(parent, findParam);
    clearAllAssignmentsNoLock();
    return;
#endif
    clearAllAssignmentsNoLock();

    auto* midiCCXml = parent->getChildByName("midiCCAssignments");
    if (midiCCXml == nullptr) {
        juce::Logger::writeToLog("MidiManager::load: no <midiCCAssignments> element in state");
        return;
    }

    int loaded = 0;
    int skipped = 0;
    int customLoaded = 0;
    for (auto* assignXml : midiCCXml->getChildIterator()) {
        int ccNum = assignXml->getIntAttribute("cc", -1);
        // Back-compat: older saves had no channel attribute; default to channel 1.
        int channel = assignXml->getIntAttribute("channel", 1);
        if (ccNum < 0 || ccNum >= NUM_CC) { ++skipped; continue; }
        if (channel < 1 || channel > NUM_CHANNELS) { ++skipped; continue; }

        int slot = slotFor(channel, ccNum);

        // Custom-target entry: stored inert, awaiting rebindCustomSetter().
        auto customId = assignXml->getStringAttribute("customId");
        if (customId.isNotEmpty()) {
            // Evict any existing occupant of this slot.
            if (auto* existing = slotToParam[slot].load(std::memory_order_acquire)) {
                paramToSlot.erase(existing);
                slotToParam[slot].store(nullptr, std::memory_order_release);
                slotEffectParam[slot].store(nullptr, std::memory_order_release);
                slotSyncable[slot] = nullptr;
            }
            // If this id was already mapped to a different slot, clear it.
            auto it = customIdToSlot.find(customId);
            if (it != customIdToSlot.end() && it->second != slot) {
                slotHasCustom[it->second].store(false, std::memory_order_release);
                slotCustomId[it->second].clear();
                slotCustomSetter[it->second] = {};
            }
            slotCustomId[slot] = customId;
            slotCustomSetter[slot] = {};  // inert until rebindCustomSetter
            slotHasCustom[slot].store(true, std::memory_order_release);
            customIdToSlot[customId] = slot;
            ++customLoaded;
            continue;
        }

        auto paramId = assignXml->getStringAttribute("paramId");
        if (paramId.isEmpty()) { ++skipped; continue; }

        auto binding = findParam(paramId);
        if (binding.param == nullptr) {
            juce::Logger::writeToLog("MidiManager::load: could not find parameter '"
                                     + paramId + "' for CC " + juce::String(ccNum)
                                     + " ch " + juce::String(channel));
            ++skipped;
            continue;
        }

        // If this param is already mapped to another slot, clear it.
        auto it = paramToSlot.find(binding.param);
        if (it != paramToSlot.end()) {
            slotToParam[it->second].store(nullptr, std::memory_order_release);
            slotEffectParam[it->second].store(nullptr, std::memory_order_release);
            slotSyncable[it->second] = nullptr;
        }

        // If this slot already holds a different param or a custom target, evict it.
        auto* existing = slotToParam[slot].load(std::memory_order_acquire);
        if (existing != nullptr) {
            paramToSlot.erase(existing);
        }
        if (slotHasCustom[slot].load(std::memory_order_acquire)) {
            customIdToSlot.erase(slotCustomId[slot]);
            slotHasCustom[slot].store(false, std::memory_order_release);
            slotCustomId[slot].clear();
            slotCustomSetter[slot] = {};
        }

        slotToParam[slot].store(binding.param, std::memory_order_release);
        slotEffectParam[slot].store(binding.effectParam, std::memory_order_release);
        slotSyncable[slot] = dynamic_cast<TreeSyncableParam*>(binding.param);
        paramToSlot[binding.param] = slot;
        ++loaded;
    }

    juce::Logger::writeToLog("MidiManager::load: loaded " + juce::String(loaded)
                             + " param CC assignments, " + juce::String(customLoaded)
                             + " custom CC assignments (" + juce::String(skipped) + " skipped)");

    updateTimerStateNoLock();
}

void MidiManager::processCC(int channel, int ccNumber, int ccValue) {
    if (channel < 1 || channel > NUM_CHANNELS) return;
    if (ccNumber < 0 || ccNumber >= NUM_CC) return;

    const int slot = slotFor(channel, ccNumber);

    auto* learning = learningParam.load(std::memory_order_acquire);
    if (learning != nullptr || customLearningActive.load(std::memory_order_acquire)) {
        learnedSlot.store(slot, std::memory_order_release);
        return;
    }

    float normCC = static_cast<float>(ccValue) / 127.0f;

    // Custom-target path: stash value, message thread will deliver.
    if (slotHasCustom[slot].load(std::memory_order_acquire)) {
        slotCustomPendingValue[slot].store(normCC, std::memory_order_release);
        slotCustomPendingDirty[slot].store(true, std::memory_order_release);
        return;
    }

    auto* param = slotToParam[slot].load(std::memory_order_acquire);
    if (param == nullptr) return;

    if (!gestureActive[slot].load(std::memory_order_acquire)
        && !gestureStartPending[slot].load(std::memory_order_acquire)) {
        gestureStartPending[slot].store(true, std::memory_order_release);
    }
    lastCCTime[slot].store((int64_t)juce::Time::getMillisecondCounterHiRes(), std::memory_order_release);

    auto* effectParam = slotEffectParam[slot].load(std::memory_order_acquire);
    if (effectParam != nullptr
        && effectParam->lfoStartPercent != nullptr
        && effectParam->lfoEndPercent != nullptr) {
        float startNorm = juce::jlimit(0.0f, 1.0f,
            effectParam->lfoStartPercent->getValueUnnormalised() / 100.0f);
        float endNorm = juce::jlimit(0.0f, 1.0f,
            effectParam->lfoEndPercent->getValueUnnormalised() / 100.0f);
        normCC = startNorm + normCC * (endNorm - startNorm);
    }

    param->setValue(normCC);
    slotTreeDirty[slot].store(true, std::memory_order_release);
}

void MidiManager::syncTree(int slot) {
    jassert(juce::MessageManager::existsAndIsCurrentThread());
    if (slotSyncable[slot] != nullptr) {
        if (undoSuppressedFlag != nullptr) *undoSuppressedFlag = true;
        slotSyncable[slot]->syncToTree();
        if (undoSuppressedFlag != nullptr) *undoSuppressedFlag = false;
    }
}

void MidiManager::clearAllAssignmentsNoLock() {
    for (int i = 0; i < NUM_SLOTS; i++) {
        auto* param = slotToParam[i].load(std::memory_order_acquire);
        if (param != nullptr && gestureActive[i].load(std::memory_order_acquire)) {
            if (param->getParameterIndex() >= 0)
                param->endChangeGesture();
        }
        gestureActive[i].store(false, std::memory_order_relaxed);
        gestureStartPending[i].store(false, std::memory_order_relaxed);
        slotToParam[i].store(nullptr, std::memory_order_release);
        slotEffectParam[i].store(nullptr, std::memory_order_release);
        slotSyncable[i] = nullptr;

        slotHasCustom[i].store(false, std::memory_order_release);
        slotCustomPendingDirty[i].store(false, std::memory_order_release);
        slotCustomId[i].clear();
        slotCustomSetter[i] = {};
    }
    activeGestureCount = 0;
    paramToSlot.clear();
    customIdToSlot.clear();
    gestureStartTreeValue.clear();
    updateTimerStateNoLock();
}

void MidiManager::updateTimerStateNoLock() {
    const bool shouldRun = learningParam.load(std::memory_order_acquire) != nullptr
                           || customLearningActive.load(std::memory_order_acquire)
                           || !paramToSlot.empty()
                           || !customIdToSlot.empty()
                           || activeGestureCount > 0;

    if (shouldRun) {
        if (!isTimerRunning())
            startTimer(TIMER_INTERVAL_MS);
    } else if (isTimerRunning()) {
        stopTimer();
    }
}

void MidiManager::timerCallback() {
    // Collect pending custom-target CC values under the lock, then deliver
    // outside it so that setter callbacks (which may do allocations / undo)
    // don't extend the SpinLock hold time.
    static constexpr int maxPending = 16;
    std::pair<CustomSetter*, float> pendingCustomDeliveries[maxPending];
    int numPending = 0;

    {
        juce::SpinLock::ScopedLockType lock(messageLock);

        int pendingSlot = learnedSlot.exchange(-1, std::memory_order_acq_rel);
        if (pendingSlot >= 0 && pendingSlot < NUM_SLOTS) {
            auto* learning = learningParam.load(std::memory_order_acquire);
            if (learning != nullptr) {
                auto* existing = slotToParam[pendingSlot].load(std::memory_order_acquire);
                if (existing != nullptr)
                    paramToSlot.erase(existing);
                // Evict a custom target in the same slot.
                if (slotHasCustom[pendingSlot].load(std::memory_order_acquire)) {
                    customIdToSlot.erase(slotCustomId[pendingSlot]);
                    slotHasCustom[pendingSlot].store(false, std::memory_order_release);
                    slotCustomId[pendingSlot].clear();
                    slotCustomSetter[pendingSlot] = {};
                }

                auto oldIt = paramToSlot.find(learning);
                if (oldIt != paramToSlot.end()) {
                    int oldSlot = oldIt->second;
                    slotToParam[oldSlot].store(nullptr, std::memory_order_release);
                    slotEffectParam[oldSlot].store(nullptr, std::memory_order_release);
                    slotSyncable[oldSlot] = nullptr;
                    paramToSlot.erase(oldIt);
                }

                slotToParam[pendingSlot].store(learning, std::memory_order_release);
                slotEffectParam[pendingSlot].store(pendingEffectParam, std::memory_order_release);
                slotSyncable[pendingSlot] = dynamic_cast<TreeSyncableParam*>(learning);
                pendingEffectParam = nullptr;
                paramToSlot[learning] = pendingSlot;
                learningParam.store(nullptr, std::memory_order_release);
                sendChangeMessage();
            } else if (customLearningActive.load(std::memory_order_acquire)) {
                // Evict any param in this slot.
                if (auto* existing = slotToParam[pendingSlot].load(std::memory_order_acquire)) {
                    paramToSlot.erase(existing);
                    slotToParam[pendingSlot].store(nullptr, std::memory_order_release);
                    slotEffectParam[pendingSlot].store(nullptr, std::memory_order_release);
                    slotSyncable[pendingSlot] = nullptr;
                }
                // If this id was already mapped to a different slot, clear that slot.
                auto oldIt = customIdToSlot.find(pendingCustomId);
                if (oldIt != customIdToSlot.end() && oldIt->second != pendingSlot) {
                    int oldSlot = oldIt->second;
                    slotHasCustom[oldSlot].store(false, std::memory_order_release);
                    slotCustomId[oldSlot].clear();
                    slotCustomSetter[oldSlot] = {};
                }
                // Evict any other custom target in the target slot.
                if (slotHasCustom[pendingSlot].load(std::memory_order_acquire)
                    && slotCustomId[pendingSlot] != pendingCustomId) {
                    customIdToSlot.erase(slotCustomId[pendingSlot]);
                }
                slotCustomId[pendingSlot] = pendingCustomId;
                slotCustomSetter[pendingSlot] = std::move(pendingCustomSetter);
                slotHasCustom[pendingSlot].store(true, std::memory_order_release);
                customIdToSlot[pendingCustomId] = pendingSlot;

                pendingCustomId = {};
                pendingCustomSetter = {};
                customLearningActive.store(false, std::memory_order_release);
                sendChangeMessage();
            }
        }

        for (auto& [id, slot] : customIdToSlot) {
            if (numPending >= maxPending) break;
            if (slotCustomPendingDirty[slot].exchange(false, std::memory_order_acq_rel)) {
                float v = slotCustomPendingValue[slot].load(std::memory_order_acquire);
                if (slotCustomSetter[slot])
                    pendingCustomDeliveries[numPending++] = { &slotCustomSetter[slot], v };
            }
        }
    } // release messageLock before invoking setters

    for (int i = 0; i < numPending; ++i)
        (*pendingCustomDeliveries[i].first)(pendingCustomDeliveries[i].second);

    {
        juce::SpinLock::ScopedLockType lock(messageLock);

        int64_t now = (int64_t)juce::Time::getMillisecondCounterHiRes();
        for (auto& [param, slot] : paramToSlot) {
            if (slotTreeDirty[slot].exchange(false, std::memory_order_acq_rel)) {
                param->sendValueChangedMessageToListeners(param->getValue());
                syncTree(slot);
            }

            if (gestureStartPending[slot].exchange(false, std::memory_order_acq_rel)) {
                if (!gestureActive[slot].load(std::memory_order_acquire)) {
                    if (param->getParameterIndex() >= 0)
                        param->beginChangeGesture();
                    if (stateTree != nullptr)
                        gestureStartTreeValue[slot] = (*stateTree)[juce::Identifier(param->paramID)];
                    gestureActive[slot].store(true, std::memory_order_release);
                    ++activeGestureCount;
                }
            }
            if (!gestureActive[slot].load(std::memory_order_acquire)) continue;

            if (slotTreeDirty[slot].exchange(false, std::memory_order_acq_rel)) {
                param->sendValueChangedMessageToListeners(param->getValue());
                syncTree(slot);
            }

            int64_t last = lastCCTime[slot].load(std::memory_order_acquire);
            if (now - last > GESTURE_TIMEOUT_MS) {
                if (param->getParameterIndex() >= 0)
                    param->endChangeGesture();
                if (undoManager != nullptr && stateTree != nullptr) {
                    undoManager->beginNewTransaction("CC " + param->getName(100));
                    auto prop = juce::Identifier(param->paramID);
                    auto startIt = gestureStartTreeValue.find(slot);
                    juce::var startVal = (startIt != gestureStartTreeValue.end())
                                         ? startIt->second : (*stateTree)[prop];
                    undoManager->perform(
                        new CCGestureUndoAction(*stateTree, prop,
                            startVal,
                            (*stateTree)[prop]),
                        "CC " + param->getName(100));
                    gestureStartTreeValue.erase(slot);
                }
                gestureActive[slot].store(false, std::memory_order_release);
                if (--activeGestureCount < 0) activeGestureCount = 0;
            }
        }

        updateTimerStateNoLock();
    } // release messageLock
}

} // namespace osci
