#include "osci_MidiCCManager.h"

namespace osci {

MidiCCManager::MidiCCManager() {
    for (int i = 0; i < NUM_SLOTS; i++) {
        slotToParam[i].store(nullptr, std::memory_order_relaxed);
        slotEffectParam[i].store(nullptr, std::memory_order_relaxed);
        slotSyncable[i] = nullptr;
        gestureStartPending[i].store(false, std::memory_order_relaxed);
        gestureActive[i].store(false, std::memory_order_relaxed);
        lastCCTime[i].store(0, std::memory_order_relaxed);
        slotTreeDirty[i].store(false, std::memory_order_relaxed);
    }
    learningParam.store(nullptr, std::memory_order_relaxed);
}

MidiCCManager::~MidiCCManager() { stopTimer(); }

void MidiCCManager::setUndoManager(juce::UndoManager* um, bool* suppressedFlag, juce::ValueTree* tree) {
    jassert(um != nullptr && suppressedFlag != nullptr && tree != nullptr);
    jassert(juce::MessageManager::existsAndIsCurrentThread());
    undoManager = um;
    undoSuppressedFlag = suppressedFlag;
    stateTree = tree;
}

void MidiCCManager::processMidiBuffer(const juce::MidiBuffer& midiMessages) {
#if OSCI_PREMIUM
    for (const auto& meta : midiMessages) {
        auto msg = meta.getMessage();
        if (msg.isController()) {
            processCC(msg.getChannel(), msg.getControllerNumber(), msg.getControllerValue());
        }
    }
#else
    juce::ignoreUnused(midiMessages);
#endif
}

void MidiCCManager::startLearning(Param* param, EffectParameter* effectParam) {
#if !OSCI_PREMIUM
    juce::ignoreUnused(param, effectParam);
    return;
#else
    jassert(param != nullptr);
    learnedSlot.store(-1, std::memory_order_release);
    pendingEffectParam = effectParam;
    learningParam.store(param, std::memory_order_release);
    updateTimerStateNoLock();
    sendChangeMessage();
#endif
}

void MidiCCManager::stopLearning() {
    learnedSlot.store(-1, std::memory_order_release);
    learningParam.store(nullptr, std::memory_order_release);
    pendingEffectParam = nullptr;
    updateTimerStateNoLock();
    sendChangeMessage();
}

bool MidiCCManager::isLearning() const {
    return learningParam.load(std::memory_order_acquire) != nullptr;
}

bool MidiCCManager::isLearning(const Param* param) const {
    return learningParam.load(std::memory_order_acquire) == param;
}

void MidiCCManager::removeAssignment(Param* param) {
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

MidiCCManager::Assignment MidiCCManager::getAssignment(const Param* param) const {
    juce::SpinLock::ScopedLockType lock(messageLock);
    auto it = paramToSlot.find(const_cast<Param*>(param));
    if (it != paramToSlot.end()) {
        int slot = it->second;
        return Assignment{ channelForSlot(slot), ccForSlot(slot) };
    }
    return Assignment{};
}

int MidiCCManager::getAssignedCC(const Param* param) const {
    return getAssignment(param).cc;
}

void MidiCCManager::clearAllAssignments() {
    juce::SpinLock::ScopedLockType lock(messageLock);
    clearAllAssignmentsNoLock();
}

void MidiCCManager::save(juce::XmlElement* parent) const {
    juce::SpinLock::ScopedLockType lock(messageLock);
    if (paramToSlot.empty()) {
        juce::Logger::writeToLog("MidiCCManager::save: no assignments to save");
        return;
    }

    auto* midiCCXml = parent->createNewChildElement("midiCCAssignments");
    for (auto& [param, slot] : paramToSlot) {
        auto* assignXml = midiCCXml->createNewChildElement("assignment");
        assignXml->setAttribute("paramId", param->paramID);
        assignXml->setAttribute("cc", ccForSlot(slot));
        assignXml->setAttribute("channel", channelForSlot(slot));
    }
    juce::Logger::writeToLog("MidiCCManager::save: saved "
                             + juce::String((int)paramToSlot.size()) + " CC assignments");
}

void MidiCCManager::load(const juce::XmlElement* parent,
                          const std::function<ParamBinding(const juce::String&)>& findParam) {
    juce::SpinLock::ScopedLockType lock(messageLock);
#if !OSCI_PREMIUM
    juce::ignoreUnused(parent, findParam);
    clearAllAssignmentsNoLock();
    return;
#endif
    clearAllAssignmentsNoLock();

    auto* midiCCXml = parent->getChildByName("midiCCAssignments");
    if (midiCCXml == nullptr) {
        juce::Logger::writeToLog("MidiCCManager::load: no <midiCCAssignments> element in state");
        return;
    }

    int loaded = 0;
    int skipped = 0;
    for (auto* assignXml : midiCCXml->getChildIterator()) {
        auto paramId = assignXml->getStringAttribute("paramId");
        int ccNum = assignXml->getIntAttribute("cc", -1);
        // Back-compat: older saves had no channel attribute; default to channel 1.
        int channel = assignXml->getIntAttribute("channel", 1);
        if (ccNum < 0 || ccNum >= NUM_CC) { ++skipped; continue; }
        if (channel < 1 || channel > NUM_CHANNELS) { ++skipped; continue; }

        auto binding = findParam(paramId);
        if (binding.param == nullptr) {
            juce::Logger::writeToLog("MidiCCManager::load: could not find parameter '"
                                     + paramId + "' for CC " + juce::String(ccNum)
                                     + " ch " + juce::String(channel));
            ++skipped;
            continue;
        }

        int slot = slotFor(channel, ccNum);

        // If this param is already mapped to another slot, clear it.
        auto it = paramToSlot.find(binding.param);
        if (it != paramToSlot.end()) {
            slotToParam[it->second].store(nullptr, std::memory_order_release);
            slotEffectParam[it->second].store(nullptr, std::memory_order_release);
            slotSyncable[it->second] = nullptr;
        }

        // If this slot already holds a different param, evict that param's mapping.
        auto* existing = slotToParam[slot].load(std::memory_order_acquire);
        if (existing != nullptr) {
            paramToSlot.erase(existing);
        }

        slotToParam[slot].store(binding.param, std::memory_order_release);
        slotEffectParam[slot].store(binding.effectParam, std::memory_order_release);
        slotSyncable[slot] = dynamic_cast<TreeSyncableParam*>(binding.param);
        paramToSlot[binding.param] = slot;
        ++loaded;
    }

    juce::Logger::writeToLog("MidiCCManager::load: loaded " + juce::String(loaded)
                             + " CC assignments (" + juce::String(skipped) + " skipped)");

    updateTimerStateNoLock();
}

void MidiCCManager::processCC(int channel, int ccNumber, int ccValue) {
    if (channel < 1 || channel > NUM_CHANNELS) return;
    if (ccNumber < 0 || ccNumber >= NUM_CC) return;

    const int slot = slotFor(channel, ccNumber);

    auto* learning = learningParam.load(std::memory_order_acquire);
    if (learning != nullptr) {
        learnedSlot.store(slot, std::memory_order_release);
        return;
    }

    auto* param = slotToParam[slot].load(std::memory_order_acquire);
    if (param == nullptr) return;

    if (!gestureActive[slot].load(std::memory_order_acquire)
        && !gestureStartPending[slot].load(std::memory_order_acquire)) {
        gestureStartPending[slot].store(true, std::memory_order_release);
    }
    lastCCTime[slot].store((int64_t)juce::Time::getMillisecondCounterHiRes(), std::memory_order_release);

    float normCC = static_cast<float>(ccValue) / 127.0f;

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

void MidiCCManager::syncTree(int slot) {
    jassert(juce::MessageManager::existsAndIsCurrentThread());
    if (slotSyncable[slot] != nullptr) {
        if (undoSuppressedFlag != nullptr) *undoSuppressedFlag = true;
        slotSyncable[slot]->syncToTree();
        if (undoSuppressedFlag != nullptr) *undoSuppressedFlag = false;
    }
}

void MidiCCManager::clearAllAssignmentsNoLock() {
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
    }
    activeGestureCount = 0;
    paramToSlot.clear();
    gestureStartTreeValue.clear();
    updateTimerStateNoLock();
}

void MidiCCManager::updateTimerStateNoLock() {
    const bool shouldRun = learningParam.load(std::memory_order_acquire) != nullptr
                           || !paramToSlot.empty()
                           || activeGestureCount > 0;

    if (shouldRun) {
        if (!isTimerRunning())
            startTimer(TIMER_INTERVAL_MS);
    } else if (isTimerRunning()) {
        stopTimer();
    }
}

void MidiCCManager::timerCallback() {
    juce::SpinLock::ScopedLockType lock(messageLock);

    int pendingSlot = learnedSlot.exchange(-1, std::memory_order_acq_rel);
    if (pendingSlot >= 0 && pendingSlot < NUM_SLOTS) {
        auto* learning = learningParam.load(std::memory_order_acquire);
        if (learning != nullptr) {
            auto* existing = slotToParam[pendingSlot].load(std::memory_order_acquire);
            if (existing != nullptr)
                paramToSlot.erase(existing);

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
        }
    }

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
}

} // namespace osci
