#include "platform/wii/WiiSceneBootstrap.hpp"

#include <cstddef>
#include <cstring>
#include <cstdlib>

#include <malloc.h>

#include <di/di.h>
#include <ogc/dvd.h>
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
    namespace {
        constexpr std::size_t WiiPartitionTableCountOffset = 0x40000U;
        constexpr std::size_t WiiPartitionTableEntryOffsetOffset = 0x40004U;
        constexpr std::size_t WiiPartitionHeaderDataOffsetOffset = 0x2B8U;
        constexpr uint32_t MountPollIntervalIterations = 2000000U;
        constexpr uint32_t MountPollIterationLimit = 40000000U;
        volatile bool DvdReadCompleted = false;
        volatile s32 DvdReadResult = DVD_ERROR_FATAL;
        volatile bool DvdMountCompleted = false;
        volatile s32 DvdMountResult = DVD_ERROR_FATAL;

        void HandleMountCompleted(s32 result, dvdcmdblk*) {
            DvdMountResult = result;
            DvdMountCompleted = true;
        }

        void HandleAbsoluteReadCompleted(s32 result, dvdcmdblk*) {
            DvdReadResult = result;
            DvdReadCompleted = true;
        }

        uint32_t ReadBigEndianU32(const uint8_t* bytes) {
            return (static_cast<uint32_t>(bytes[0]) << 24)
                | (static_cast<uint32_t>(bytes[1]) << 16)
                | (static_cast<uint32_t>(bytes[2]) << 8)
                | static_cast<uint32_t>(bytes[3]);
        }

        std::size_t Align32(std::size_t value) {
            return (value + 31U) & ~static_cast<std::size_t>(31U);
        }

        bool ReadRawDiscBytes(void* destination, std::size_t length, std::size_t offset) {
            if (destination == nullptr) {
                return false;
            }

            const std::size_t alignedLength = Align32(length);
            uint8_t* alignedBuffer = static_cast<uint8_t*>(memalign(32, alignedLength));
            if (alignedBuffer == nullptr) {
                return false;
            }

            std::memset(alignedBuffer, 0, alignedLength);
            constexpr s32 Priority = 2;
            dvdcmdblk commandBlock {};
            DvdReadCompleted = false;
            DvdReadResult = DVD_ERROR_FATAL;
            const s32 readStarted = DVD_ReadAbsAsyncPrio(&commandBlock, alignedBuffer, static_cast<u32>(alignedLength), static_cast<s64>(offset), HandleAbsoluteReadCompleted, Priority);
            if (readStarted < 0) {
                free(alignedBuffer);
                return false;
            }

            while (!DvdReadCompleted) {
            }

            const bool readSucceeded = DvdReadResult >= 0;
            if (readSucceeded) {
                std::memcpy(destination, alignedBuffer, length);
            }

            free(alignedBuffer);
            return readSucceeded;
        }

        bool TryMountDisc(s32& mountResult) {
            mountResult = DVD_ERROR_FATAL;
            dvdcmdblk commandBlock {};
            DvdMountCompleted = false;
            DvdMountResult = DVD_ERROR_FATAL;
            const s32 mountStartedResult = DVD_MountAsync(&commandBlock, HandleMountCompleted);
            SYS_Report("[Wii] DVD_MountAsync start result: %ld\n", static_cast<long>(mountStartedResult));
            if (mountStartedResult < 0) {
                mountResult = mountStartedResult;
                return false;
            }

            for (uint32_t iteration = 0U; iteration < MountPollIterationLimit; iteration++) {
                if (DvdMountCompleted) {
                    mountResult = DvdMountResult;
                    return mountResult >= 0;
                }

                if (iteration != 0U && (iteration % MountPollIntervalIterations) == 0U) {
                    SYS_Report("[Wii] DVD mount wait status=%ld iteration=%lu\n", static_cast<long>(DVD_GetDriveStatus()), static_cast<unsigned long>(iteration));
                }
            }

            mountResult = DvdMountResult;
            SYS_Report("[Wii] DVD_MountAsync timed out with drive status=%ld.\n", static_cast<long>(DVD_GetDriveStatus()));
            return false;
        }

        bool TryResolvePartitionOffsets(uint32_t& partitionOffset, uint32_t& partitionDataOffset) {
            partitionOffset = 0U;
            partitionDataOffset = 0U;

            alignas(32) uint8_t partitionTableHeader[8];
            if (!ReadRawDiscBytes(partitionTableHeader, sizeof(partitionTableHeader), WiiPartitionTableCountOffset)) {
                SYS_Report("[Wii] Could not read the Wii partition table header.\n");
                return false;
            }

            const uint32_t partitionCount = ReadBigEndianU32(partitionTableHeader + 0);
            const uint32_t partitionTableEntryOffsetWords = ReadBigEndianU32(partitionTableHeader + 4);
            const uint32_t partitionTableEntryOffset = partitionTableEntryOffsetWords << 2U;
            if (partitionCount == 0U || partitionTableEntryOffset == 0U) {
                SYS_Report("[Wii] Wii partition table header did not contain a data partition entry.\n");
                return false;
            }

            alignas(32) uint8_t partitionEntry[8];
            if (!ReadRawDiscBytes(partitionEntry, sizeof(partitionEntry), partitionTableEntryOffset)) {
                SYS_Report("[Wii] Could not read the Wii data partition entry.\n");
                return false;
            }

            const uint32_t partitionOffsetWords = ReadBigEndianU32(partitionEntry + 0);
            partitionOffset = partitionOffsetWords << 2U;
            if (partitionOffset == 0U) {
                SYS_Report("[Wii] Wii data partition offset was zero.\n");
                return false;
            }

            alignas(32) uint8_t partitionHeader[4];
            if (!ReadRawDiscBytes(partitionHeader, sizeof(partitionHeader), partitionOffset + WiiPartitionHeaderDataOffsetOffset)) {
                SYS_Report("[Wii] Could not read the Wii partition data offset.\n");
                return false;
            }

            const uint32_t partitionDataOffsetWords = ReadBigEndianU32(partitionHeader + 0);
            const uint32_t partitionDataOffsetBytes = partitionOffset + (partitionDataOffsetWords << 2U);
            if (partitionDataOffsetBytes == 0U) {
                SYS_Report("[Wii] Wii partition data offset was zero.\n");
                return false;
            }

            partitionDataOffset = partitionDataOffsetBytes;
            SYS_Report("[Wii] Wii partition data offset: %08lX\n", static_cast<unsigned long>(partitionDataOffset));
            return true;
        }
    }

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

    /// Initializes the packaged Wii disc interface before scene loads begin.
    bool WiiSceneBootstrap::InitializePackagedStorage() {
        SYS_Report("[Wii] InitializePackagedStorage begin.\n");
        DVD_Init();
        SYS_Report("[Wii] DVD_Init completed.\n");
        SYS_Report("[Wii] Drive status before mount: %ld\n", static_cast<long>(DVD_GetDriveStatus()));
        SYS_Report("[Wii] Calling DVD_MountAsync.\n");
        s32 mountResult = DVD_ERROR_FATAL;
        if (!TryMountDisc(mountResult)) {
            SYS_Report("[Wii] DVD mount failed result=%ld status=%ld\n", static_cast<long>(mountResult), static_cast<long>(DVD_GetDriveStatus()));
            return false;
        }

        SYS_Report("[Wii] DVD mount completed result: %ld\n", static_cast<long>(mountResult));
        if (mountResult < 0) {
            return false;
        }

        const int diInitResult = DI_Init();
        SYS_Report("[Wii] DI_Init result: %d\n", diInitResult);
        if (diInitResult < 0) {
            return false;
        }

        uint32_t partitionOffset = 0U;
        uint32_t partitionDataOffset = 0U;
        SYS_Report("[Wii] Resolving partition data offset.\n");
        if (!TryResolvePartitionOffsets(partitionOffset, partitionDataOffset)) {
            return false;
        }

        const int openPartitionResult = DI_OpenPartition(partitionOffset >> 2U);
        SYS_Report("[Wii] DI_OpenPartition result: %d offset=%08lX\n", openPartitionResult, static_cast<unsigned long>(partitionOffset));
        if (openPartitionResult < 0) {
            return false;
        }

        SYS_Report("[Wii] Configuring packaged partition data offset.\n");
        WiiDiscFileSystem::ConfigurePartitionDataOffset(partitionDataOffset);
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
