#include "platform/wii/WiiSceneBootstrap.hpp"

#include "RuntimeSceneCatalog.hpp"
#include "RuntimeSceneCatalogEntry.hpp"
#include "runtime/array.hpp"
#include "runtime/native_exceptions.hpp"
#include "system/io/file.hpp"
#include "system/io/path.hpp"

namespace helengine::wii {
    std::string WiiSceneBootstrap::BundledContentRootPath = "tmp/city-demo-disc-main-menu-content";

    std::string WiiSceneBootstrap::BundledContentRootWindowsHostPath = "C:/dev/helworks/helengine-wii/tmp/city-demo-disc-main-menu-content";

    std::string WiiSceneBootstrap::BundledContentRootWslPath = "/mnt/c/dev/helworks/helengine-wii/tmp/city-demo-disc-main-menu-content";

    std::string WiiSceneBootstrap::StartupSceneId = "Scenes/DemoDiscMainMenu.helen";

    /// Returns the staged content root and fails if the bundle has not been prepared.
    std::string WiiSceneBootstrap::GetValidatedContentRootPath() {
        const std::string relativeRootPath = Path::GetFullPath(BundledContentRootPath);
        if (HasRequiredFiles(relativeRootPath)) {
            return relativeRootPath;
        }

        const std::string windowsHostRootPath = Path::GetFullPath(BundledContentRootWindowsHostPath);
        if (HasRequiredFiles(windowsHostRootPath)) {
            return windowsHostRootPath;
        }

        const std::string wslRootPath = Path::GetFullPath(BundledContentRootWslPath);
        if (HasRequiredFiles(wslRootPath)) {
            return wslRootPath;
        }

        ValidateRequiredFile(relativeRootPath, "cooked/scenes/DemoDiscMainMenu.hasset");
        return relativeRootPath;
    }

    /// Creates the single-scene runtime catalog used by the current Wii authored-scene bootstrap milestone.
    RuntimeSceneCatalog* WiiSceneBootstrap::CreateSceneCatalog() {
        Array<RuntimeSceneCatalogEntry*>* entries = new Array<RuntimeSceneCatalogEntry*>(1);
        (*entries)[0] = new RuntimeSceneCatalogEntry(StartupSceneId, "cooked/scenes/DemoDiscMainMenu.hasset");
        return new RuntimeSceneCatalog(entries);
    }

    /// Returns the authored startup scene id used by the staged runtime scene catalog.
    std::string WiiSceneBootstrap::GetStartupSceneId() {
        return StartupSceneId;
    }

    /// Returns whether all required staged files exist under the candidate content root.
    bool WiiSceneBootstrap::HasRequiredFiles(std::string rootPath) {
        if (String::IsNullOrWhiteSpace(rootPath)) {
            return false;
        }

        return File::Exists(Path::GetFullPath(Path::Combine(rootPath, "cooked/scenes/DemoDiscMainMenu.hasset")));
    }

    /// Verifies one required staged content file exists under the bundle root.
    void WiiSceneBootstrap::ValidateRequiredFile(std::string rootPath, std::string relativePath) {
        const std::string fullPath = Path::GetFullPath(Path::Combine(rootPath, relativePath));
        if (!File::Exists(fullPath)) {
            throw new InvalidOperationException(std::string("Required staged Wii content file is missing: ") + fullPath);
        }
    }
}
