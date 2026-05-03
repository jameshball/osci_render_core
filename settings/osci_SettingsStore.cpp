#include <map>

namespace
{
    juce::CriticalSection& settingsLockRegistryLock() {
        static auto* lock = new juce::CriticalSection();
        return *lock;
    }

    std::map<juce::String, std::unique_ptr<juce::CriticalSection>>& settingsLocksByPath()
    {
        static auto* locks = new std::map<juce::String, std::unique_ptr<juce::CriticalSection>>();
        return *locks;
    }

    juce::CriticalSection& inProcessFileLockFor (const juce::File& file) {
        const juce::ScopedLock registryLock (settingsLockRegistryLock());
        auto& locks = settingsLocksByPath();
        const auto path = file.getFullPathName();

        const auto existing = locks.find (path);
        if (existing != locks.end()) {
            return *existing->second;
        }

        auto [inserted, _] = locks.emplace (path, std::make_unique<juce::CriticalSection>());
        juce::ignoreUnused (_);
        return *inserted->second;
    }
}

namespace osci
{

SettingsStore::SettingsStore() = default;

SettingsStore::SettingsStore (juce::PropertiesFile::Options options)
{
    initialise (std::move (options));
}

SettingsStore::~SettingsStore()
{
    juce::ignoreUnused (save());
}

SettingsStore::SettingsStore (SettingsStore&& other) noexcept
{
    const juce::ScopedLock lockOther (other.lock);
    options = std::move (other.options);
    processLock = std::move (other.processLock);
    properties = std::move (other.properties);
    pendingSets = std::move (other.pendingSets);
    pendingRemoves = std::move (other.pendingRemoves);

    if (processLock != nullptr)
        options.processLock = processLock.get();
}

SettingsStore& SettingsStore::operator= (SettingsStore&& other) noexcept
{
    if (this != &other)
    {
        const juce::ScopedLock lockThis (lock);
        const juce::ScopedLock lockOther (other.lock);
        options = std::move (other.options);
        processLock = std::move (other.processLock);
        properties = std::move (other.properties);
        pendingSets = std::move (other.pendingSets);
        pendingRemoves = std::move (other.pendingRemoves);

        if (processLock != nullptr)
            options.processLock = processLock.get();
    }

    return *this;
}

juce::PropertiesFile::Options SettingsStore::optionsForProductGlobals (juce::StringRef productName)
{
    return baseOptions (juce::String (productName) + "_globals");
}

juce::PropertiesFile::Options SettingsStore::optionsForStandaloneApp (juce::StringRef productName)
{
    return baseOptions (productName);
}

juce::PropertiesFile::Options SettingsStore::optionsForSharedLicensing()
{
    return baseOptions ("osci-licensing");
}

SettingsStore SettingsStore::forProductGlobals (juce::StringRef productName)
{
    return SettingsStore (optionsForProductGlobals (productName));
}

SettingsStore SettingsStore::forStandaloneApp (juce::StringRef productName)
{
    return SettingsStore (optionsForStandaloneApp (productName));
}

SettingsStore SettingsStore::forSharedLicensing()
{
    return SettingsStore (optionsForSharedLicensing());
}

bool SettingsStore::isValid() const noexcept
{
    const juce::ScopedLock scopedLock (lock);
    return properties != nullptr;
}

bool SettingsStore::getBool (const juce::String& keyName, bool defaultValue) const
{
    const juce::ScopedLock scopedLock (lock);
    if (pendingRemoves.contains (keyName))
        return defaultValue;

    const auto pendingIndex = pendingSets.getAllKeys().indexOf (keyName, false);
    if (pendingIndex >= 0)
        return pendingSets.getAllValues()[pendingIndex].getIntValue() != 0;

    return properties != nullptr ? properties->getBoolValue (keyName, defaultValue) : defaultValue;
}

int SettingsStore::getInt (const juce::String& keyName, int defaultValue) const
{
    const juce::ScopedLock scopedLock (lock);
    if (pendingRemoves.contains (keyName))
        return defaultValue;

    const auto pendingIndex = pendingSets.getAllKeys().indexOf (keyName, false);
    if (pendingIndex >= 0)
        return pendingSets.getAllValues()[pendingIndex].getIntValue();

    return properties != nullptr ? properties->getIntValue (keyName, defaultValue) : defaultValue;
}

double SettingsStore::getDouble (const juce::String& keyName, double defaultValue) const
{
    const juce::ScopedLock scopedLock (lock);
    if (pendingRemoves.contains (keyName))
        return defaultValue;

    const auto pendingIndex = pendingSets.getAllKeys().indexOf (keyName, false);
    if (pendingIndex >= 0)
        return pendingSets.getAllValues()[pendingIndex].getDoubleValue();

    return properties != nullptr ? properties->getDoubleValue (keyName, defaultValue) : defaultValue;
}

juce::String SettingsStore::getString (const juce::String& keyName, const juce::String& defaultValue) const
{
    const juce::ScopedLock scopedLock (lock);
    if (pendingRemoves.contains (keyName))
        return defaultValue;

    const auto pendingIndex = pendingSets.getAllKeys().indexOf (keyName, false);
    if (pendingIndex >= 0)
        return pendingSets.getAllValues()[pendingIndex];

    return properties != nullptr ? properties->getValue (keyName, defaultValue) : defaultValue;
}

void SettingsStore::set (const juce::String& keyName, const juce::var& value)
{
    if (keyName.isEmpty())
        return;

    const juce::ScopedLock scopedLock (lock);
    pendingSets.set (keyName, value.toString());
    pendingRemoves.removeString (keyName);
}

void SettingsStore::remove (const juce::String& keyName)
{
    if (keyName.isEmpty())
        return;

    const juce::ScopedLock scopedLock (lock);
    pendingSets.remove (pendingSets.getAllKeys().indexOf (keyName, false));
    pendingRemoves.addIfNotAlreadyThere (keyName);
}

bool SettingsStore::save()
{
    const juce::ScopedLock scopedLock (lock);

    if (properties == nullptr || ! hasPendingChanges())
        return true;

    auto mergedOptions = options;
    mergedOptions.processLock = processLock.get();

    const juce::ScopedLock scopedInProcessFileLock (inProcessFileLockFor (options.getDefaultFile()));

    std::unique_ptr<juce::InterProcessLock::ScopedLockType> scopedProcessLock;
    if (processLock != nullptr)
    {
        scopedProcessLock = std::make_unique<juce::InterProcessLock::ScopedLockType> (*processLock);
        if (! scopedProcessLock->isLocked())
            return false;
    }

    auto merged = std::make_unique<juce::PropertiesFile> (mergedOptions);
    if (! merged->isValidFile())
        return false;

    for (const auto& key : pendingRemoves)
        merged->removeValue (key);

    for (int index = 0; index < pendingSets.size(); ++index)
        merged->setValue (pendingSets.getAllKeys()[index], pendingSets.getAllValues()[index]);

    if (! merged->save())
        return false;

    properties = std::move (merged);
    pendingSets.clear();
    pendingRemoves.clear();
    return true;
}

void SettingsStore::reload()
{
    const juce::ScopedLock scopedLock (lock);

    if (properties == nullptr)
        return;

    auto reloadedOptions = options;
    reloadedOptions.processLock = processLock.get();

    const juce::ScopedLock scopedInProcessFileLock (inProcessFileLockFor (options.getDefaultFile()));
    properties = std::make_unique<juce::PropertiesFile> (reloadedOptions);
    pendingSets.clear();
    pendingRemoves.clear();
}

juce::File SettingsStore::getFile() const
{
    const juce::ScopedLock scopedLock (lock);
    return properties != nullptr ? properties->getFile() : juce::File();
}

void SettingsStore::initialise (juce::PropertiesFile::Options optionsToUse)
{
    options = std::move (optionsToUse);
    processLock = std::make_unique<juce::InterProcessLock> (lockNameFor (options.getDefaultFile()));
    options.processLock = processLock.get();

    const juce::ScopedLock scopedInProcessFileLock (inProcessFileLockFor (options.getDefaultFile()));
    properties = std::make_unique<juce::PropertiesFile> (options);
}

bool SettingsStore::hasPendingChanges() const
{
    return pendingSets.size() > 0 || ! pendingRemoves.isEmpty();
}

juce::PropertiesFile::Options SettingsStore::baseOptions (juce::StringRef applicationName)
{
    juce::PropertiesFile::Options options;
    options.applicationName = juce::String (applicationName);
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";

#if JUCE_LINUX || JUCE_BSD
    options.folderName = "~/.config";
#else
    options.folderName = "";
#endif

    return options;
}

juce::String SettingsStore::lockNameFor (const juce::File& file)
{
    return "osci-settings-" + juce::String::toHexString (file.getFullPathName().hashCode64());
}

} // namespace osci
