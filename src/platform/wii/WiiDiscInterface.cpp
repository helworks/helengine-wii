#include "platform/wii/WiiDiscInterface.hpp"

#include <cerrno>
#include <cstdint>
#include <cstring>

#include <ogc/ipc.h>

namespace helengine::wii {
    namespace {
        /// Identifies the IOS `/dev/di` command for an encrypted partition-relative read.
        static constexpr uint32_t IoctlDiRead = 0x71U;
    }

    int WiiDiscInterface::FileDescriptor = -1;
    alignas(32) char WiiDiscInterface::DevicePath[8] = "/dev/di";
    alignas(32) uint32_t WiiDiscInterface::CommandBuffer[8] = {};

    /// Opens the loader-configured cIOS disc device and returns whether a usable descriptor was acquired.
    bool WiiDiscInterface::Initialize() {
        if (FileDescriptor >= 0) {
            return true;
        }

        FileDescriptor = IOS_Open(DevicePath, 0);
        return FileDescriptor >= 0;
    }

    /// Submits one aligned encrypted read relative to the partition already opened by the loader and returns the raw IOS result.
    int WiiDiscInterface::ReadEncryptedPartition(void* destination, std::size_t length, std::size_t partitionRelativeOffset) {
        if (FileDescriptor < 0) {
            return -ENXIO;
        } else if (destination == nullptr
            || length == 0U
            || (reinterpret_cast<uintptr_t>(destination) & 0x1FU) != 0U
            || (length & 0x1FU) != 0U
            || (partitionRelativeOffset & 0x3U) != 0U) {
            return -EINVAL;
        }

        std::memset(CommandBuffer, 0, sizeof(CommandBuffer));
        CommandBuffer[0] = IoctlDiRead << 24U;
        CommandBuffer[1] = static_cast<uint32_t>(length);
        CommandBuffer[2] = static_cast<uint32_t>(partitionRelativeOffset >> 2U);
        return IOS_Ioctl(
            FileDescriptor,
            IoctlDiRead,
            CommandBuffer,
            sizeof(CommandBuffer),
            destination,
            static_cast<uint32_t>(length));
    }
}
