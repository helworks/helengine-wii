#include "platform/wii/WiiSceneBootstrap.hpp"

#include "RuntimeSceneCatalog.hpp"
#include "RuntimeSceneCatalogEntry.hpp"
#include "runtime/array.hpp"

#if HELENGINE_WII_HAS_RUNTIME_SCENE_MANIFEST
#include <ogc/system.h>

#include "runtime/wii_runtime_scene_manifest.hpp"
#endif

namespace helengine::wii {
    /// Creates the runtime scene catalog emitted by the Wii builder.
    RuntimeSceneCatalog* WiiSceneBootstrap::CreatePackagedSceneCatalog() {
#if HELENGINE_WII_HAS_RUNTIME_SCENE_MANIFEST
        std::size_t entryCount = 0;
        const HEWiiRuntimeSceneEntry* entries = he_get_runtime_wii_scene_entries(&entryCount);
        SYS_Report("[Wii] Runtime manifest entry count: %u\n", static_cast<unsigned int>(entryCount));
        Array<RuntimeSceneCatalogEntry*>* runtimeEntries = new Array<RuntimeSceneCatalogEntry*>(static_cast<int32_t>(entryCount));
        for (std::size_t index = 0; index < entryCount; index++) {
            SYS_Report(
                "[Wii] Runtime manifest entry[%u] scene=%s path=%s\n",
                static_cast<unsigned int>(index),
                entries[index].SceneId,
                entries[index].CookedRelativePath);
            (*runtimeEntries)[static_cast<int32_t>(index)] = new RuntimeSceneCatalogEntry(entries[index].SceneId, entries[index].CookedRelativePath);
        }

        return new RuntimeSceneCatalog(runtimeEntries);
#else
        return nullptr;
#endif
    }

    /// Returns the startup scene id emitted by the Wii builder.
    std::string WiiSceneBootstrap::GetPackagedStartupSceneId() {
#if HELENGINE_WII_HAS_RUNTIME_SCENE_MANIFEST
        return he_get_runtime_wii_startup_scene_id();
#else
        return std::string();
#endif
    }
}
