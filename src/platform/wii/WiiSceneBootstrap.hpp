#pragma once

#include "runtime/native_string.hpp"

class RuntimeSceneCatalog;

namespace helengine::wii {
    /// Declares the authored startup scene and content-root helpers used by the direct-DOL Wii bootstrap flow.
    class WiiSceneBootstrap {
    public:
        /// Relative repo path that must contain the staged cooked scene bundle before Dolphin verification.
        static std::string BundledContentRootPath;

        /// Absolute Windows host path used when Dolphin does not launch with the repo root as its working directory.
        static std::string BundledContentRootWindowsHostPath;

        /// Absolute WSL path used for local validation in the shared workspace.
        static std::string BundledContentRootWslPath;

        /// Stable scene id expected by the generated runtime scene catalog.
        static std::string StartupSceneId;

        /// Returns the staged content root and fails if the bundle has not been prepared.
        static std::string GetValidatedContentRootPath();

        /// Creates the single-scene runtime catalog used by the current Wii authored-scene bootstrap milestone.
        static RuntimeSceneCatalog* CreateSceneCatalog();

        /// Returns the authored startup scene id used by the staged runtime scene catalog.
        static std::string GetStartupSceneId();

    private:
        /// Returns whether all required staged files exist under the candidate content root.
        static bool HasRequiredFiles(std::string rootPath);

        /// Verifies one required staged content file exists under the bundle root.
        static void ValidateRequiredFile(std::string rootPath, std::string relativePath);
    };
}
