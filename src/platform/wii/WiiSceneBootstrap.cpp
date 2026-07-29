#include "platform/wii/WiiSceneBootstrap.hpp"

#include <cstddef>

#include <di/di.h>
#include <ogc/system.h>

#include "RuntimeSceneCatalog.hpp"
#include "RuntimeSceneCatalogEntry.hpp"
#include "platform/wii/WiiDiscFileSystem.hpp"
#include "runtime/array.hpp"
#include "runtime/native_exceptions.hpp"
#include "system/io/file.hpp"
#include "system/io/path.hpp"

#if HELENGINE_WII_HAS_RUNTIME_SCENE_MANIFEST
#include "runtime/wii_runtime_scene_manifest.hpp"
#endif

namespace helengine::wii {
    std::string WiiSceneBootstrap::StartupSceneId = "Scenes/DemoDiscMainMenu.helen";

    /// Returns the staged direct-DOL content root and fails if the required authored content bundle has not been prepared.
    std::string WiiSceneBootstrap::GetValidatedContentRootPath() {
        const std::string relativeRootPath = Path::GetFullPath("tmp/city-demo-disc-main-menu-content");
        if (HasRequiredFiles(relativeRootPath)) {
            return relativeRootPath;
        }

        const std::string windowsHostRootPath = Path::GetFullPath("C:/dev/helworks/helengine-wii/tmp/city-demo-disc-main-menu-content");
        if (HasRequiredFiles(windowsHostRootPath)) {
            return windowsHostRootPath;
        }

        const std::string wslRootPath = Path::GetFullPath("/mnt/c/dev/helworks/helengine-wii/tmp/city-demo-disc-main-menu-content");
        if (HasRequiredFiles(wslRootPath)) {
            return wslRootPath;
        }

        ValidateRequiredFile(relativeRootPath, "cooked/scenes/DemoDiscMainMenu.hasset");
        return relativeRootPath;
    }

    /// Creates the single-scene runtime catalog used by the direct-DOL developer bootstrap.
    RuntimeSceneCatalog* WiiSceneBootstrap::CreateSceneCatalog() {
        Array<RuntimeSceneCatalogEntry*>* entries = new Array<RuntimeSceneCatalogEntry*>(1);
        (*entries)[0] = new RuntimeSceneCatalogEntry(StartupSceneId, "cooked/scenes/DemoDiscMainMenu.hasset");
        return new RuntimeSceneCatalog(entries);
    }

    /// Returns the authored startup scene id used by the direct-DOL developer bootstrap.
    std::string WiiSceneBootstrap::GetStartupSceneId() {
        return StartupSceneId;
    }

    /// Initializes access to the encrypted Wii partition already opened by the disc apploader or USB loader.
    bool WiiSceneBootstrap::InitializePackagedStorage() {
        SYS_Report("[Wii] InitializePackagedStorage begin.\n");
        const int diInitResult = DI_Init();
        SYS_Report("[Wii] DI_Init result: %d\n", diInitResult);
        if (diInitResult < 0) {
            return false;
        }

        WiiDiscFileSystem::InitializeOpenedPartition();
        return true;
    }

    /// Returns the packaged Wii content root used by disc-backed startup.
    std::string WiiSceneBootstrap::GetPackagedContentRootPath() {
        return "dvd:/";
    }

    /// Creates the runtime scene catalog emitted by the Wii builder.
    RuntimeSceneCatalog* WiiSceneBootstrap::CreatePackagedSceneCatalog() {
#if HELENGINE_WII_HAS_RUNTIME_SCENE_MANIFEST
        std::size_t entryCount = 0;
        const HEWiiRuntimeSceneEntry* entries = he_get_runtime_wii_scene_entries(&entryCount);
        SYS_Report("[Wii] Runtime manifest entry count: %u\n", static_cast<unsigned int>(entryCount));
        const std::string startupSceneId = GetPackagedStartupSceneId();
        const std::string startupSceneAliasId = "DemoDiscMainMenu";
        bool startupSceneAliasExists = false;
        bool startupSceneSourceExists = false;
        for (std::size_t index = 0; index < entryCount; index++) {
            if (startupSceneAliasId == entries[index].SceneId) {
                startupSceneAliasExists = true;
            }

            if (startupSceneId == entries[index].SceneId) {
                startupSceneSourceExists = true;
            }
        }

        const bool shouldAddStartupSceneAlias = !startupSceneAliasExists && startupSceneSourceExists;
        const std::size_t runtimeEntryCount = shouldAddStartupSceneAlias ? entryCount + 1U : entryCount;
        Array<RuntimeSceneCatalogEntry*>* runtimeEntries = new Array<RuntimeSceneCatalogEntry*>(static_cast<int32_t>(runtimeEntryCount));
        std::size_t runtimeEntryIndex = 0;
        for (std::size_t index = 0; index < entryCount; index++) {
            SYS_Report(
                "[Wii] Runtime manifest entry[%u] scene=%s path=%s\n",
                static_cast<unsigned int>(index),
                entries[index].SceneId,
                entries[index].CookedRelativePath);
            (*runtimeEntries)[static_cast<int32_t>(runtimeEntryIndex)] = new RuntimeSceneCatalogEntry(entries[index].SceneId, entries[index].CookedRelativePath);
            runtimeEntryIndex++;
            if (shouldAddStartupSceneAlias && startupSceneId == entries[index].SceneId) {
                (*runtimeEntries)[static_cast<int32_t>(runtimeEntryIndex)] = new RuntimeSceneCatalogEntry(startupSceneAliasId, entries[index].CookedRelativePath);
                runtimeEntryIndex++;
            }
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

    /// Returns whether all required staged authored-scene files exist under the candidate direct-DOL content root.
    bool WiiSceneBootstrap::HasRequiredFiles(std::string rootPath) {
        if (String::IsNullOrWhiteSpace(rootPath)) {
            return false;
        }

        return File::Exists(Path::GetFullPath(Path::Combine(rootPath, "cooked/scenes/DemoDiscMainMenu.hasset")));
    }

    /// Verifies that one required staged authored-scene file exists under the direct-DOL bundle root.
    void WiiSceneBootstrap::ValidateRequiredFile(std::string rootPath, std::string relativePath) {
        const std::string fullPath = Path::GetFullPath(Path::Combine(rootPath, relativePath));
        if (!File::Exists(fullPath)) {
            throw new InvalidOperationException(std::string("Required staged Wii content file is missing: ") + fullPath);
        }
    }
}
