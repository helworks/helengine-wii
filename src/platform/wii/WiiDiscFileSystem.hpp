#pragma once

#include <cstddef>
#include <string>

class FileStream;

namespace helengine::wii {
    /// Provides read-only packaged Wii disc access by indexing the apploader FST and reading files from the opened encrypted partition.
    class WiiDiscFileSystem {
    public:
        /// Initializes packaged reads for the encrypted partition already opened by the disc apploader or USB loader.
        static void InitializeOpenedPartition();

        /// Returns whether the supplied path should be resolved from the packaged Wii disc image.
        static bool CanHandlePath(const char* path);

        /// Returns whether the supplied packaged Wii disc path resolves to a file entry in the indexed disc FST.
        static bool Exists(const char* path);

        /// Opens one packaged Wii disc file as a read-only memory-backed stream loaded from the encrypted partition.
        static FileStream* OpenRead(const char* path);

    private:
        /// Ensures the packaged disc FST has been loaded into memory before path resolution occurs.
        static bool EnsureIndexLoaded();

        /// Reads the disc header and FST, then indexes all packaged file entries.
        static bool LoadIndex();

        /// Recursively indexes one FST directory entry and all of its children.
        static void IndexDirectory(std::size_t directoryEntryIndex, std::string directoryPath);

        /// Returns the UTF-8 entry name stored for the specified FST entry.
        static std::string ReadEntryName(std::size_t entryIndex);

        /// Normalizes one <c>dvd:/...</c> path into the slash form used by indexed entries.
        static std::string NormalizePath(const char* path);

        /// Resolves one packaged file path to its disc offset and byte length.
        static bool TryResolveFile(const char* path, std::size_t& discOffset, std::size_t& fileSize);
    };
}
