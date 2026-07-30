#pragma once

#include <cstddef>
#include <cstdint>

namespace helengine::wii {
    /// Owns the title-local IOS disc descriptor used for encrypted reads from the partition opened by the loader.
    class WiiDiscInterface {
    public:
        /// Opens the cIOS disc device without requiring AHBPROT and returns whether a usable descriptor was acquired.
        static bool Initialize();

        /// Submits one aligned encrypted read relative to the partition already opened by the loader and returns the raw IOS result.
        static int ReadEncryptedPartition(void* destination, std::size_t length, std::size_t partitionRelativeOffset);

    private:
        /// Title-local IOS descriptor for the loader-configured disc device.
        static int FileDescriptor;

        /// Aligned IOS device path retained for descriptor initialization.
        alignas(32) static char DevicePath[8];

        /// Aligned eight-word request buffer required by the `/dev/di` ioctl ABI.
        alignas(32) static uint32_t CommandBuffer[8];
    };
}
