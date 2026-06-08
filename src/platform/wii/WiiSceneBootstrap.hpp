#pragma once

#include "runtime/native_string.hpp"

class RuntimeSceneCatalog;

namespace helengine::wii {
    /// Declares the direct-DOL developer boot helpers and packaged-disc runtime helpers used by Wii startup.
    class WiiSceneBootstrap {
    public:
        /// Stable authored scene id expected by the runtime scene catalog.
        static std::string StartupSceneId;

        /// Returns the staged direct-DOL content root and fails if the required authored content bundle has not been prepared.
        static std::string GetValidatedContentRootPath();

        /// Creates the single-scene runtime catalog used by the direct-DOL developer bootstrap.
        static RuntimeSceneCatalog* CreateSceneCatalog();

        /// Returns the authored startup scene id used by the direct-DOL developer bootstrap.
        static std::string GetStartupSceneId();

        /// Initializes the packaged Wii disc interface before any packaged content file access occurs.
        static bool InitializePackagedStorage();

        /// Returns the packaged Wii content root used by disc-backed startup.
        static std::string GetPackagedContentRootPath();

        /// Creates the runtime scene catalog emitted by the Wii builder.
        static RuntimeSceneCatalog* CreatePackagedSceneCatalog();

        /// Returns the startup scene id emitted by the Wii builder.
        static std::string GetPackagedStartupSceneId();

    private:
        /// Returns whether all required staged authored-scene files exist under the candidate direct-DOL content root.
        static bool HasRequiredFiles(std::string rootPath);

        /// Verifies that one required staged authored-scene file exists under the direct-DOL bundle root.
        static void ValidateRequiredFile(std::string rootPath, std::string relativePath);
    };
}
