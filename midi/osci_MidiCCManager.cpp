#include "osci_MidiCCManager.h"

namespace osci {

MidiCCManager::MidiCCManager() {
    for (int i = 0; i < NUM_CC; i++) {
        ccToParam[i].store(nullptr, std::memory_order_relaxed);
        ccEffectParam[i].store(nullptr, std::memory_order_relaxed);
        ccSyncable[i] = nullptr;
        gestureStartPending[i].store(false, std::memory_order_relaxed);
        gestureActive[i].store(false, std::memory_order_relaxed);
        lastCCTime[i].store(0, std::memory_order_relaxed);
        ccTreeDirty[i].store(false, std::memory_order_relaxed);
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
            processCC(msg.getControllerNumber(), msg.getControllerValue());
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
    learnedCC.store(-1, std::memory_order_release);
    pendingEffectParam = effectParam;
    learningParam.store(param, std::memory_order_release);
    updateTimerStateNoLock();
    sendChangeMessage();
#endif
}

void MidiCCManager::stopLearning() {
    learnedCC.store(-1, std::memory_order_release);
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
    auto it = paramToCC.find(param);
    if (it != paramToCC.end()) {
        int cc = it->second;
        if (gestureActive[cc].load(std::memory_order_acquire)) {
            if (param->getParameterIndex() >= 0)
                param->endChangeGesture();
            gestureActive[cc].store(false, std::memory_order_release);
            if (--activeGestureCount < 0) activeGestureCount = 0;
        }
        gestureStartPending[cc].store(false, std::memory_order_release);
        ccToParam[cc].store(nullptr, std::memory_order_release);
        ccEffectParam[cc].store(nullptr, std::memory_order_release);
        ccSyncable[cc] = nullptr;
        paramToCC.erase(it);
        updateTimerStateNoLock();
    }
}

int MidiCCManager::getAssignedCC(const Param* param) const {
    juce::SpinLock::ScopedLockType lock(messageLock);
    auto it = paramToCC.find(const_cast<Param*>(param));
    if (it != paramToCC.end())
        return it->second;
    return -1;
}

void MidiCCManager::clearAllAssignments() {
    juce::SpinLock::ScopedLockType lock(messageLock);
    for (int i = 0; i < NUM_CC; i++)
        ccToParam[i].store(nullptr, std::memory_order_release);
    paramToCC.clear();
    updateTimerStateNoLock();
}

void MidiCCManager::save(juce::XmlElement* parent) const {
    juce::SpinLock::ScopedLockType lock(messageLock);
    if (paramToCC.empty()) return;

    auto* midiCCXml = parent->createNewChildElement("midiCCAssignments");
    for (auto& [param, ccNum] : paramToCC) {
        auto* assignXml = midiCCXml->createNewChildElement("assignment");
        assignXml->setAttribute("paramId", param->paramID);
        assignXml->setAttribute("cc", ccNum);
    }
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
    if (midiCCXml == nullptr) return;

    for (auto* assignXml : midiCCXml->getChildIterator()) {
        auto paramId = assignXml->getStringAttribute("paramId");
        int ccNum = assignXml->getIntAttribute("cc", -1);
        if (ccNum < 0 || ccNum >= NUM_CC) continue;

        auto binding = findParam(paramId);
        if (binding.param == nullptr) continue;

        auto it = paramToCC.find(binding.param);
        if (it != paramToCC.end()) {
            ccToParam[it->second].store(nullptr, std::memory_order_release);
            ccEffectParam[it->second].store(nullptr, std::memory_order_release);
            ccSyncable[it->second] = nullptr;
        }

        auto* existing = ccToParam[ccNum].load(std::memory_order_acquire);
        if (existing != nullptr) {
            paramToCC.erase(existing);
        }

        ccToParam[ccNum].store(binding.param, std::memory_order_release);
        ccEffectParam[ccNum].store(binding.effectParam, std::memory_order_release);
        ccSyncable[ccNum] = dynamic_cast<TreeSyncableParam*>(binding.param);
        paramToCC[binding.param] = ccNum;
    }

    updateTimerStateNoLock();
}

void MidiCCManager::processCC(int ccNumber, int ccValue) {
    if (ccNumber < 0 || ccNumber >= NUM_CC) return;

    auto* learning = learningParam.load(std::memory_order_acquire);
    if (learning != nullptr) {
        learnedCC.store(ccNumber, std::memory_order_release);
        return;
    }

    auto* param = ccToParam[ccNumber].load(std::memory_order_acquire);
    if (param == nullptr) return;

    if (!gestureActive[ccNumber].load(std::memory_order_acquire)
        && !gestureStartPending[ccNumber].load(std::memory_order_acquire)) {
        gestureStartPending[ccNumber].store(true, std::memory_order_release);
    }
    lastCCTime[ccNumber].store((int64_t)juce::Time::getMillisecondCounterHiRes(), std::memory_order_release);

    float normCC = static_cast<float>(ccValue) / 127.0f;

    auto* effectParam = ccEffectParam[ccNumber].load(std::memory_order_acquire);
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
    ccTreeDirty[ccNumber].store(true, std::memory_order_release);
}

void MidiCCManager::syncTree(int cc) {
    jassert(juce::MessageManager::existsAndIsCurrentThread());
    if (ccSyncable[cc] != nullptr) {
        if (undoSuppressedFlag != nullptr) *undoSuppressedFlag = true;
        ccSyncable[cc]->syncToTree();
        if (undoSuppressedFlag != nullptr) *undoSuppressedFlag = false;
    }
}

void MidiCCManager::clearAllAssignmentsNoLock() {
    for (int i = 0; i < NUM_CC; i++) {
        auto* param = ccToParam[i].load(std::memory_order_acquire);
        if (param != nullptr && gestureActive[i].load(std::memory_order_acquire)) {
            if (param->getParameterIndex() >= 0)
                param->endChangeGesture();
        }
        gestureActive[i].store(false, std::memory_order_relaxed);
        gestureStartPending[i].store(false, std::memory_order_relaxed);
        ccToParam[i].store(nullptr, std::memory_order_release);
        ccEffectParam[i].store(nullptr, std::memory_order_release);
        ccSyncable[i] = nullptr;
    }
    activeGestureCount = 0;
    paramToCC.clear();
    updateTimerStateNoLock();
}

void MidiCCManager::updateTimerStateNoLock() {
    const bool shouldRun = learningParam.load(std::memory_order_acquire) != nullptr
                           || !paramToCC.empty()
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

    int pendingCC = learnedCC.exchange(-1, std::memory_order_acq_rel);
    if (pendingCC >= 0 && pendingCC < NUM_CC) {
        auto* learning = learningParam.load(std::memory_order_acquire);
        if (learning != nullptr) {
            auto* existing = ccToParam[pendingCC].load(std::memory_order_acquire);
            if (existing != nullptr)
                paramToCC.erase(existing);

            auto oldIt = paramToCC.find(learning);
            if (oldIt != paramToCC.end()) {
                int oldCC = oldIt->second;
                ccToParam[oldCC].store(nullptr, std::memory_order_release);
                ccEffectParam[oldCC].store(nullptr, std::memory_order_release);
                ccSyncable[oldCC] = nullptr;
                paramToCC.erase(oldIt);
            }

            ccToParam[pendingCC].store(learning, std::memory_order_release);
            ccEffectParam[pendingCC].store(pendingEffectParam, std::memory_order_release);
            ccSyncable[pendingCC] = dynamic_cast<TreeSyncableParam*>(learning);
            pendingEffectParam = nullptr;
            paramToCC[learning] = pendingCC;
            learningParam.store(nullptr, std::memory_order_release);
            sendChangeMessage();
        }
    }

    int64_t now = (int64_t)juce::Time::getMillisecondCounterHiRes();
    for (auto& [param, i] : paramToCC) {
        if (ccTreeDirty[i].exchange(false, std::memory_order_acq_rel)) {
            param->sendValueChangedMessageToListeners(param->getValue());
            syncTree(i);
        }

        if (gestureStartPending[i].exchange(false, std::memory_order_acq_rel)) {
            if (!gestureActive[i].load(std::memory_order_acquire)) {
                if (param->getParameterIndex() >= 0)
                    param->beginChangeGesture();
                if (stateTree != nullptr)
                    gestureStartTreeValue[i] = (*stateTree)[juce::Identifier(param->paramID)];
                gestureActive[i].store(true, std::memory_order_release);
                ++activeGestureCount;
            }
        }
        if (!gestureActive[i].load(std::memory_order_acquire)) continue;

        if (ccTreeDirty[i].exchange(false, std::memory_order_acq_rel)) {
            param->sendValueChangedMessageToListeners(param->getValue());
            syncTree(i);
        }

        int64_t last = lastCCTime[i].load(std::memory_order_acquire);
        if (now - last > GESTURE_TIMEOUT_MS) {
            if (param->getParameterIndex() >= 0)
                param->endChangeGesture();
            if (undoManager != nullptr && stateTree != nullptr) {
                undoManager->beginNewTransaction("CC " + param->getName(100));
                auto prop = juce::Identifier(param->paramID);
                undoManager->perform(
                    new CCGestureUndoAction(*stateTree, prop,
                        gestureStartTreeValue[i],
                        (*stateTree)[prop]),
                    "CC " + param->getName(100));
            }
            gestureActive[i].store(false, std::memory_order_release);
            if (--activeGestureCount < 0) activeGestureCount = 0;
        }
    }

    updateTimerStateNoLock();
}

} // namespace osci
