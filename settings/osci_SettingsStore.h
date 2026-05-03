#pragma once

namespace osci
{

class SettingsStore final
{
public:
    SettingsStore();
    explicit SettingsStore (juce::PropertiesFile::Options options);
    ~SettingsStore();

    SettingsStore (SettingsStore&&) noexcept;
    SettingsStore& operator= (SettingsStore&&) noexcept;

    SettingsStore (const SettingsStore&) = delete;
    SettingsStore& operator= (const SettingsStore&) = delete;

    static juce::PropertiesFile::Options optionsForProductGlobals (juce::StringRef productName);
    static juce::PropertiesFile::Options optionsForStandaloneApp (juce::StringRef productName);
    static juce::PropertiesFile::Options optionsForSharedLicensing();

    static SettingsStore forProductGlobals (juce::StringRef productName);
    static SettingsStore forStandaloneApp (juce::StringRef productName);
    static SettingsStore forSharedLicensing();

    bool isValid() const noexcept;

    bool getBool (const juce::String& keyName, bool defaultValue = false) const;
    int getInt (const juce::String& keyName, int defaultValue = 0) const;
    double getDouble (const juce::String& keyName, double defaultValue = 0.0) const;
    juce::String getString (const juce::String& keyName, const juce::String& defaultValue = {}) const;

    void set (const juce::String& keyName, const juce::var& value);
    void remove (const juce::String& keyName);
    bool save();
    void reload();

    juce::File getFile() const;

private:
    juce::PropertiesFile::Options options;
    std::unique_ptr<juce::InterProcessLock> processLock;
    std::unique_ptr<juce::PropertiesFile> properties;
    juce::StringPairArray pendingSets;
    juce::StringArray pendingRemoves;
    mutable juce::CriticalSection lock;

    void initialise (juce::PropertiesFile::Options optionsToUse);
    bool hasPendingChanges() const;
    static juce::PropertiesFile::Options baseOptions (juce::StringRef applicationName);
    static juce::String lockNameFor (const juce::File& file);
};

} // namespace osci
