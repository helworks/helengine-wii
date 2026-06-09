#include "platform/wii/WiiApplication.hpp"

#include <cstdio>
#include <cstring>

#include <ogc/dvd.h>
#include <ogc/isfs.h>
#include <ogc/system.h>

static struct __argv NullSafeSystemArgv = {};

extern "C" void __CheckARGV() {
    __system_argv = &NullSafeSystemArgv;
}

namespace {
    /// <summary>
    /// Creates one host-readable per-title trace file path under the emulated Wii save-data tree.
    /// </summary>
    /// <param name="fileName">Trace file name to place under the title data directory.</param>
    /// <param name="pathBuffer">Destination buffer that receives the resolved ISFS path.</param>
    /// <param name="pathBufferLength">Capacity of <paramref name="pathBuffer"/> in bytes.</param>
    /// <returns><see langword="true"/> when the current disc id was available and the path fit in the supplied buffer.</returns>
    bool TryResolveTitleDataTracePath(const char* fileName, char* pathBuffer, std::size_t pathBufferLength) {
        dvddiskid* diskId = DVD_GetCurrentDiskID();
        if (diskId == nullptr) {
            return false;
        }

        int writtenCharacterCount = std::snprintf(
            pathBuffer,
            pathBufferLength,
            "/title/00010000/%02X%02X%02X%02X/data/%s",
            static_cast<unsigned char>(diskId->gamename[0]),
            static_cast<unsigned char>(diskId->gamename[1]),
            static_cast<unsigned char>(diskId->gamename[2]),
            static_cast<unsigned char>(diskId->gamename[3]),
            fileName);
        return writtenCharacterCount > 0 && static_cast<std::size_t>(writtenCharacterCount) < pathBufferLength;
    }

    /// <summary>
    /// Creates the per-title save-data directory used for packaged-disc host-readable trace files.
    /// </summary>
    /// <param name="directoryPathBuffer">Destination buffer that receives the resolved <c>data</c> directory path.</param>
    /// <param name="directoryPathBufferLength">Capacity of <paramref name="directoryPathBuffer"/> in bytes.</param>
    /// <returns><see langword="true"/> when the per-title data directory path was resolved and created or already existed.</returns>
    bool TryEnsureTitleDataTraceDirectory(char* directoryPathBuffer, std::size_t directoryPathBufferLength) {
        dvddiskid* diskId = DVD_GetCurrentDiskID();
        if (diskId == nullptr) {
            return false;
        }

        char titleDirectoryPath[96];
        int titleDirectoryCharacterCount = std::snprintf(
            titleDirectoryPath,
            sizeof(titleDirectoryPath),
            "/title/00010000/%02X%02X%02X%02X",
            static_cast<unsigned char>(diskId->gamename[0]),
            static_cast<unsigned char>(diskId->gamename[1]),
            static_cast<unsigned char>(diskId->gamename[2]),
            static_cast<unsigned char>(diskId->gamename[3]));
        if (titleDirectoryCharacterCount <= 0 || static_cast<std::size_t>(titleDirectoryCharacterCount) >= sizeof(titleDirectoryPath)) {
            return false;
        }

        int dataDirectoryCharacterCount = std::snprintf(
            directoryPathBuffer,
            directoryPathBufferLength,
            "%s/data",
            titleDirectoryPath);
        if (dataDirectoryCharacterCount <= 0 || static_cast<std::size_t>(dataDirectoryCharacterCount) >= directoryPathBufferLength) {
            return false;
        }

        ISFS_CreateDir(titleDirectoryPath, 0, 3, 3, 3);
        ISFS_CreateDir(directoryPathBuffer, 0, 3, 3, 3);
        return true;
    }
}

int main() {
    SYS_STDIO_Report(true);
    std::fprintf(stderr, "[Wii] stderr bridge armed.\n");
    std::fflush(stderr);
    SYS_Report("[Wii] main() entered.\n");
    SYS_Report("[Wii] main() title-trace pre.\n");

    if (ISFS_Initialize() == ISFS_OK) {
        constexpr const char* MainEntryTraceText = "[Wii] main() entered.\n";
        char traceDirectoryPath[112];
        char traceFilePath[144];
        if (TryEnsureTitleDataTraceDirectory(traceDirectoryPath, sizeof(traceDirectoryPath))
            && TryResolveTitleDataTracePath("main_entry_trace.txt", traceFilePath, sizeof(traceFilePath))) {
            s32 fileDescriptor = ISFS_Open(traceFilePath, ISFS_OPEN_RW);
            if (fileDescriptor < 0) {
                if (ISFS_CreateFile(traceFilePath, 0, 3, 3, 3) == ISFS_OK) {
                    fileDescriptor = ISFS_Open(traceFilePath, ISFS_OPEN_RW);
                }
            }

            if (fileDescriptor >= 0) {
                ISFS_Seek(fileDescriptor, 0, SEEK_END);
                ISFS_Write(fileDescriptor, MainEntryTraceText, static_cast<u32>(std::strlen(MainEntryTraceText)));
                ISFS_Close(fileDescriptor);
            }
        }
    }

    SYS_Report("[Wii] main() title-trace post.\n");
    SYS_Report("[Wii] main() before application run.\n");

    helengine::wii::WiiApplication application;
    return application.Run();
}
