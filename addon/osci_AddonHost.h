#include <juce_core/juce_core.h>
#include <juce_core/threads/juce_DynamicLibrary.h>
#include "osci_Addon.h"

namespace osci {

class AddOnHost {
public:
    void scanAndLoad(juce::File& folder) {
        folder.createDirectory();

        for (auto f : folder.findChildFiles (juce::File::findFiles, false, "*.dll;*.so;*.dylib")) {
            juce::DynamicLibrary lib (f.getFullPathName());
            CreateFunc createFunc = reinterpret_cast<CreateFunc>(lib.getFunction("createAddon"));
            if (createFunc != nullptr) {
                std::unique_ptr<Addon> module(createFunc());
                module->initialize();
                modules.add(std::move(module));
                libs.add(std::move(lib));  // keep alive
                DBG("Loaded addon: " << modules.getLast()->getName() << " v" << modules.getLast()->getVersion());
            }
        }
    }

private:
    juce::OwnedArray<Addon> modules;
    juce::Array<juce::DynamicLibrary> libs;
};

} // namespace osci