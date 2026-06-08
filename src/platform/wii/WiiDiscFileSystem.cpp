#include "platform/wii/WiiDiscFileSystem.hpp"

#include <cctype>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <malloc.h>
#include <stdexcept>
#include <string>
#include <vector>

#include <di/di.h>
#include <ogc/dvd.h>
#include <ogc/system.h>

#include "system/io/file-stream.hpp"

namespace helengine::wii {
    namespace {
        constexpr const char* BuildStamp = __DATE__ " " __TIME__;
        constexpr std::size_t FstEntrySize = 12;
        static constexpr std::size_t MaxDiReadLength = 0x800U;
        static constexpr uint32_t FstAddressLowMemoryAddress = 0x80000038u;
        static constexpr uint32_t FstSizeLowMemoryAddress = 0x8000003Cu;

        struct WiiDiscFileEntry {
            std::string Path;
            uint32_t DiscOffset;
            uint32_t FileSize;
        };

        std::vector<uint8_t> FstBytes;
        std::vector<WiiDiscFileEntry> FileEntries;
        bool IndexLoaded = false;
        uint32_t PartitionDataOffset = 0U;
        bool PartitionDataOffsetConfigured = false;
        uint32_t DiscReadLogCount = 0U;

        std::size_t Align32(std::size_t value) {
            return (value + 31U) & ~static_cast<std::size_t>(31U);
        }

        uint32_t ReadBigEndianU32(const uint8_t* bytes) {
            return (static_cast<uint32_t>(bytes[0]) << 24)
                | (static_cast<uint32_t>(bytes[1]) << 16)
                | (static_cast<uint32_t>(bytes[2]) << 8)
                | static_cast<uint32_t>(bytes[3]);
        }

        /// Reads an arbitrary byte range from the opened Wii partition using the synchronous decrypted libogc DI API.
        bool ReadDiscRange(void* destination, std::size_t offset, std::size_t length) {
            if (destination == nullptr) {
                return false;
            } else if (!PartitionDataOffsetConfigured) {
                SYS_Report("[Wii] Partition data offset was not configured before DVD reads.\n");
                return false;
            }

            uint8_t* destinationBytes = static_cast<uint8_t*>(destination);
            std::size_t currentOffset = offset;
            std::size_t remainingLength = length;
            while (remainingLength > 0U) {
                const std::size_t chunkLength = std::min(remainingLength, MaxDiReadLength);
                const std::size_t wordOffset = currentOffset >> 2U;
                const std::size_t absoluteOffset = static_cast<std::size_t>(PartitionDataOffset) + currentOffset;
                const std::size_t alignedLength = Align32(chunkLength);
                uint8_t* alignedBuffer = static_cast<uint8_t*>(memalign(32, alignedLength));
                if (alignedBuffer == nullptr) {
                    SYS_Report("[Wii] Could not allocate an aligned DI read buffer.\n");
                    return false;
                }

                std::memset(alignedBuffer, 0, alignedLength);
                if (DiscReadLogCount < 8U) {
                    SYS_Report("[Wii] DI_Read begin relative=%lu absolute=%lu wordOffset=%lu copyLength=%lu alignedLength=%lu\n",
                        static_cast<unsigned long>(currentOffset),
                        static_cast<unsigned long>(absoluteOffset),
                        static_cast<unsigned long>(wordOffset),
                        static_cast<unsigned long>(chunkLength),
                        static_cast<unsigned long>(alignedLength));
                }

                const int readResult = DI_Read(alignedBuffer, static_cast<u32>(alignedLength), static_cast<u32>(wordOffset));
                if (readResult < 0) {
                    SYS_Report("[Wii] DI_Read failed base=%08lX offset=%lu absolute=%lu wordOffset=%lu copyLength=%lu alignedLength=%lu result=%d\n",
                        static_cast<unsigned long>(PartitionDataOffset),
                        static_cast<unsigned long>(currentOffset),
                        static_cast<unsigned long>(absoluteOffset),
                        static_cast<unsigned long>(wordOffset),
                        static_cast<unsigned long>(chunkLength),
                        static_cast<unsigned long>(alignedLength),
                        readResult);
                    free(alignedBuffer);
                    return false;
                }

                std::memcpy(destinationBytes, alignedBuffer, chunkLength);
                free(alignedBuffer);
                if (DiscReadLogCount < 8U) {
                    SYS_Report("[Wii] DI_Read completed relative=%lu wordOffset=%lu copyLength=%lu alignedLength=%lu result=%d\n",
                        static_cast<unsigned long>(currentOffset),
                        static_cast<unsigned long>(wordOffset),
                        static_cast<unsigned long>(chunkLength),
                        static_cast<unsigned long>(alignedLength),
                        readResult);
                }

                destinationBytes += chunkLength;
                currentOffset += chunkLength;
                remainingLength -= chunkLength;
                DiscReadLogCount++;
            }

            return true;
        }
    }

    /// Configures the opened Wii partition data offset used to validate packaged reads and log their absolute disc positions.
    void WiiDiscFileSystem::ConfigurePartitionDataOffset(uint32_t partitionDataOffset) {
        PartitionDataOffset = partitionDataOffset;
        PartitionDataOffsetConfigured = true;
        IndexLoaded = false;
        DiscReadLogCount = 0U;
        FileEntries.clear();
        FstBytes.clear();
        SYS_Report("[Wii] WiiDiscFileSystem partition data offset configured: data=%08lX\n",
            static_cast<unsigned long>(PartitionDataOffset));
    }

    /// Returns whether the supplied path should be resolved from the packaged Wii disc image.
    bool WiiDiscFileSystem::CanHandlePath(const char* path) {
        return path != nullptr && std::strncmp(path, "dvd:/", 5) == 0;
    }

    /// Returns whether the supplied packaged Wii disc path resolves to a file entry in the indexed disc FST.
    bool WiiDiscFileSystem::Exists(const char* path) {
        std::size_t discOffset = 0;
        std::size_t fileSize = 0;
        const bool exists = TryResolveFile(path, discOffset, fileSize);
        SYS_Report("[Wii] WiiDiscFileSystem::Exists path=%s result=%d offset=%lu size=%lu\n", path != nullptr ? path : "<null>", exists ? 1 : 0, static_cast<unsigned long>(discOffset), static_cast<unsigned long>(fileSize));
        return exists;
    }

    /// Opens one packaged Wii disc file as a read-only memory-backed stream loaded from DVD sectors.
    FileStream* WiiDiscFileSystem::OpenRead(const char* path) {
        std::size_t discOffset = 0;
        std::size_t fileSize = 0;
        if (!TryResolveFile(path, discOffset, fileSize)) {
            throw std::runtime_error(std::string("Packaged Wii disc path was not found: ") + (path != nullptr ? path : "<null>"));
        }

        if (fileSize > static_cast<std::size_t>(std::numeric_limits<uint32_t>::max())) {
            throw std::runtime_error("Packaged Wii disc file exceeds the supported read size.");
        }

        const std::size_t alignedSize = Align32(fileSize);
        uint8_t* buffer = static_cast<uint8_t*>(memalign(32, alignedSize));
        if (buffer == nullptr) {
            throw std::runtime_error("Could not allocate a packaged Wii disc read buffer.");
        }

        std::memset(buffer, 0, alignedSize);
        SYS_Report("[Wii] Build stamp before dvd read: %s\n", BuildStamp);
        SYS_Report("[Wii] WiiDiscFileSystem reading indexed path=%s offset=%lu size=%lu\n", path != nullptr ? path : "<null>", static_cast<unsigned long>(discOffset), static_cast<unsigned long>(fileSize));
        if (!ReadDiscRange(buffer, discOffset, fileSize)) {
            free(buffer);
            throw std::runtime_error("Wii DVD sector read failed for a packaged content file.");
        }

        FileStream* stream = new FileStream(buffer, fileSize);
        free(buffer);
        return stream;
    }

    /// Ensures the packaged disc FST has been loaded into memory before path resolution occurs.
    bool WiiDiscFileSystem::EnsureIndexLoaded() {
        if (IndexLoaded) {
            return true;
        }

        return LoadIndex();
    }

    /// Copies the apploader-loaded packaged-disc FST from Wii low memory, then indexes all packaged file entries.
    bool WiiDiscFileSystem::LoadIndex() {
        const uint32_t fstAddress = *reinterpret_cast<volatile uint32_t*>(FstAddressLowMemoryAddress);
        const uint32_t fstSize = *reinterpret_cast<volatile uint32_t*>(FstSizeLowMemoryAddress);
        SYS_Report("[Wii] FST low memory address=%08lX size=%08lX\n",
            static_cast<unsigned long>(fstAddress),
            static_cast<unsigned long>(fstSize));
        if (fstAddress == 0U || fstSize < FstEntrySize) {
            SYS_Report("[Wii] Packaged Wii disc FST low-memory handoff was invalid.\n");
            return false;
        }

        const uint8_t* fstBytes = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(fstAddress));
        FstBytes.assign(fstBytes, fstBytes + fstSize);
        FileEntries.clear();
        IndexDirectory(0U, "dvd:/");
        IndexLoaded = true;
        SYS_Report("[Wii] Indexed %lu packaged Wii disc files.\n", static_cast<unsigned long>(FileEntries.size()));
        return true;
    }

    /// Recursively indexes one FST directory entry and all of its children.
    void WiiDiscFileSystem::IndexDirectory(std::size_t directoryEntryIndex, std::string directoryPath) {
        const std::size_t directoryOffset = directoryEntryIndex * FstEntrySize;
        const std::size_t directoryEndIndex = ReadBigEndianU32(FstBytes.data() + directoryOffset + 8);
        for (std::size_t entryIndex = directoryEntryIndex + 1; entryIndex < directoryEndIndex; entryIndex++) {
            const std::size_t entryOffset = entryIndex * FstEntrySize;
            const bool isDirectory = FstBytes[entryOffset] != 0;
            const std::string entryName = ReadEntryName(entryIndex);
            const std::string entryPath = directoryPath == "dvd:/"
                ? directoryPath + entryName
                : directoryPath + "/" + entryName;

            if (isDirectory) {
                const uint32_t parentDirectoryIndex = ReadBigEndianU32(FstBytes.data() + entryOffset + 4);
                if (parentDirectoryIndex == directoryEntryIndex) {
                    IndexDirectory(entryIndex, NormalizePath(entryPath.c_str()));
                    entryIndex = ReadBigEndianU32(FstBytes.data() + entryOffset + 8) - 1U;
                }

                continue;
            }

            const uint32_t fileOffsetWords = ReadBigEndianU32(FstBytes.data() + entryOffset + 4);
            const uint32_t fileOffset = fileOffsetWords << 2U;
            FileEntries.push_back(WiiDiscFileEntry {
                NormalizePath(entryPath.c_str()),
                fileOffset,
                ReadBigEndianU32(FstBytes.data() + entryOffset + 8)
            });
        }
    }

    /// Returns the UTF-8 entry name stored for the specified FST entry.
    std::string WiiDiscFileSystem::ReadEntryName(std::size_t entryIndex) {
        const std::size_t entryCount = ReadBigEndianU32(FstBytes.data() + 8);
        const std::size_t stringTableOffset = entryCount * FstEntrySize;
        const std::size_t entryOffset = entryIndex * FstEntrySize;
        const uint32_t nameOffset = (static_cast<uint32_t>(FstBytes[entryOffset + 1]) << 16)
            | (static_cast<uint32_t>(FstBytes[entryOffset + 2]) << 8)
            | static_cast<uint32_t>(FstBytes[entryOffset + 3]);
        return std::string(reinterpret_cast<const char*>(FstBytes.data() + stringTableOffset + nameOffset));
    }

    /// Normalizes one <c>dvd:/...</c> path into the slash form used by indexed entries.
    std::string WiiDiscFileSystem::NormalizePath(const char* path) {
        std::string normalizedPath = path != nullptr ? path : "";
        std::replace(normalizedPath.begin(), normalizedPath.end(), '\\', '/');
        std::transform(
            normalizedPath.begin(),
            normalizedPath.end(),
            normalizedPath.begin(),
            [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
        while (normalizedPath.size() > 5U && normalizedPath.back() == '/') {
            normalizedPath.pop_back();
        }

        return normalizedPath;
    }

    /// Resolves one packaged file path to its disc offset and byte length.
    bool WiiDiscFileSystem::TryResolveFile(const char* path, std::size_t& discOffset, std::size_t& fileSize) {
        discOffset = 0;
        fileSize = 0;
        if (!CanHandlePath(path) || !EnsureIndexLoaded()) {
            return false;
        }

        const std::string normalizedPath = NormalizePath(path);
        for (std::size_t entryIndex = 0; entryIndex < FileEntries.size(); entryIndex++) {
            if (FileEntries[entryIndex].Path != normalizedPath) {
                continue;
            }

            discOffset = FileEntries[entryIndex].DiscOffset;
            fileSize = FileEntries[entryIndex].FileSize;
            return true;
        }

        return false;
    }
}
