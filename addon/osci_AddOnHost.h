#include <juce_core/juce_core.h>
#include "osci_AddOn.h"

namespace osci {

class AddOnHost {
public:
    void scanAndLoad(juce::File folder) {
        folder.createDirectory();

        for (auto f : folder.findChildFiles (juce::File::findFiles, false, "*.dll;*.so;*.dylib")) {
            juce::DynamicLibrary lib (f.getFullPathName());
            CreateFunc createFunc = reinterpret_cast<CreateFunc>(lib.getFunction("createAddOn"));
            if (createFunc != nullptr) {
                std::unique_ptr<AddOn> module(createFunc());
                module->initialize();

                auto addonEffects = module->getEffects();
                for (auto& effect : addonEffects) {
                    // Add the effect to the list of effects
                    effects.push_back(effect);
                }

                modules.add(std::move(module));
                libs.add(std::move(lib));  // keep alive
            }
        }
    }

    void unload() {
        for (auto& module : modules) {
            module->shutdown();
        }
        modules.clear();
        libs.clear();
    }
    
    std::vector<std::shared_ptr<Effect>> getEffects() const {
        return effects;
    }

private:
    juce::OwnedArray<AddOn> modules;
    juce::Array<juce::DynamicLibrary> libs;
    std::vector<std::shared_ptr<Effect>> effects;
};

} // namespace osci
