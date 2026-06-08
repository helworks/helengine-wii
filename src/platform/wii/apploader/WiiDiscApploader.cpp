#include <stdint.h>

extern "C" {
        static constexpr uint32_t ArenaHighLowMemoryAddress = 0x80000034u;
        static constexpr uint32_t FstAddressLowMemoryAddress = 0x80000038u;
        static constexpr uint32_t FstSizeLowMemoryAddress = 0x8000003Cu;
        static constexpr uint32_t BootInfoVersionLowMemoryAddress = 0x80000024u;
        static constexpr uint32_t PhysicalMemorySizeLowMemoryAddress = 0x80000028u;
        static constexpr uint32_t SimulatedMemorySizeLowMemoryAddress = 0x800000F0u;
        static constexpr uint32_t Bi2LowMemoryAddress = 0x800000F4u;
        static constexpr uint32_t DiscLayerStateLowMemoryAddress = 0x8000319Cu;
        static constexpr uint32_t Bi2HeaderDiscOffsetWords = 0x110u;
        static constexpr bool EnableApploaderReportTracing = true;

    struct ApploaderLoadRequest {
        uint32_t DestinationAddress;
        uint32_t Length;
        uint32_t DiscOffsetWords;
    };

    struct ApploaderConfig {
        uint32_t Magic0;
        uint32_t Magic1;
        uint32_t Magic2;
        uint32_t Magic3;
        uint32_t RequestCount;
        uint32_t EntryPoint;
        uint32_t BssAddress;
        uint32_t BssSize;
        uint32_t CurrentRequestIndex;
        uint32_t FstLoadAddress;
        uint32_t FstSize;
        uint32_t FstDiscOffsetWords;
        ApploaderLoadRequest Requests[32];
    };

    typedef void (*ApploaderReportFunction)(const char* format, ...);
    typedef void (*ApploaderInitFunction)(void*);
    typedef int (*ApploaderMainFunction)(uint32_t*, uint32_t*, uint32_t*);
    typedef uint32_t (*ApploaderCloseFunction)();

    static void ApploaderInit(void* reportFunction);
    static int ApploaderMain(uint32_t* destinationAddress, uint32_t* length, uint32_t* discOffsetWords);
    static uint32_t ApploaderClose();

    __attribute__((section(".data.apploader_config"), used))
    static volatile ApploaderConfig Config = {
        0x48454C41u,
        0x50504C44u,
        0x30303131u,
        0xA55AA55Au,
        0u,
        0u,
        0u,
        0u,
        0u,
        0u,
        0u,
        0u,
        {}
    };

    __attribute__((section(".data.apploader_state"), used))
    static ApploaderReportFunction ReportFunction = nullptr;

    __attribute__((aligned(32), section(".data.apploader_state"), used))
    static volatile uint32_t Bi2HeaderWords[8] = {};

    __attribute__((section(".data.apploader_state"), used))
    static volatile uint32_t Bi2HeaderLoaded = 0u;

    static void PublishBi2BootInfo() {
        const uint32_t simulatedMemorySize = Bi2HeaderWords[1] != 0u
            ? Bi2HeaderWords[1]
            : *reinterpret_cast<volatile uint32_t*>(PhysicalMemorySizeLowMemoryAddress);
        *reinterpret_cast<volatile uint32_t*>(BootInfoVersionLowMemoryAddress) = 1u;
        *reinterpret_cast<volatile uint32_t*>(SimulatedMemorySizeLowMemoryAddress) = simulatedMemorySize;
        *reinterpret_cast<volatile uint32_t*>(Bi2LowMemoryAddress) = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&Bi2HeaderWords[0]));
        *reinterpret_cast<volatile uint8_t*>(DiscLayerStateLowMemoryAddress) = 0x80u;
    }

    static void ZeroBssRange() {
        volatile uint8_t* destination = reinterpret_cast<volatile uint8_t*>(Config.BssAddress);
        for (uint32_t index = 0; index < Config.BssSize; index++) {
            destination[index] = 0u;
        }
    }

    __attribute__((section(".text.apploader_entry"), used))
    void ApploaderEntry(
        ApploaderInitFunction* initFunction,
        ApploaderMainFunction* mainFunction,
        ApploaderCloseFunction* closeFunction) {
        *initFunction = &ApploaderInit;
        *mainFunction = &ApploaderMain;
        *closeFunction = &ApploaderClose;
    }

    static void ApploaderInit(void* reportFunction) {
        ReportFunction = reinterpret_cast<ApploaderReportFunction>(reportFunction);
        (void)reportFunction;
        Config.CurrentRequestIndex = 0u;
        Bi2HeaderLoaded = 0u;
        if (EnableApploaderReportTracing && ReportFunction != nullptr) {
            ReportFunction(
                "[HA] init requests=%lu entry=%08lX fst=%08lX size=%08lX disc=%08lX\n",
                static_cast<unsigned long>(Config.RequestCount),
                static_cast<unsigned long>(Config.EntryPoint),
                static_cast<unsigned long>(Config.FstLoadAddress),
                static_cast<unsigned long>(Config.FstSize),
                static_cast<unsigned long>(Config.FstDiscOffsetWords << 2u));
        }
    }

    static int ApploaderMain(uint32_t* destinationAddress, uint32_t* length, uint32_t* discOffsetWords) {
        if (Bi2HeaderLoaded == 0u) {
            const uint32_t bi2DestinationAddress = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&Bi2HeaderWords[0]));
            const uint32_t bi2Length = sizeof(Bi2HeaderWords);
            const uint32_t bi2DiscOffsetWords = Bi2HeaderDiscOffsetWords;
            Bi2HeaderLoaded = 1u;
            if (EnableApploaderReportTracing && ReportFunction != nullptr) {
                ReportFunction(
                    "[HA] bi2 dst=%08lX len=%08lX disc=%08lX\n",
                    static_cast<unsigned long>(bi2DestinationAddress),
                    static_cast<unsigned long>(bi2Length),
                    static_cast<unsigned long>(bi2DiscOffsetWords << 2u));
            }
            *destinationAddress = bi2DestinationAddress;
            *length = bi2Length;
            *discOffsetWords = bi2DiscOffsetWords;
            return 1;
        }

        if (Bi2HeaderLoaded == 1u) {
            PublishBi2BootInfo();
            Bi2HeaderLoaded = 2u;
        }

        if (Config.CurrentRequestIndex < Config.RequestCount) {
            const uint32_t requestIndex = Config.CurrentRequestIndex;
            const uint32_t requestDestinationAddress = Config.Requests[requestIndex].DestinationAddress;
            const uint32_t requestLength = Config.Requests[requestIndex].Length;
            const uint32_t requestDiscOffsetWords = Config.Requests[requestIndex].DiscOffsetWords;
            Config.CurrentRequestIndex += 1u;
            if (EnableApploaderReportTracing && ReportFunction != nullptr) {
                ReportFunction(
                    "[HA] dol[%lu] dst=%08lX len=%08lX disc=%08lX\n",
                    static_cast<unsigned long>(requestIndex),
                    static_cast<unsigned long>(requestDestinationAddress),
                    static_cast<unsigned long>(requestLength),
                    static_cast<unsigned long>(requestDiscOffsetWords << 2u));
            }
            *destinationAddress = requestDestinationAddress;
            *length = requestLength;
            *discOffsetWords = requestDiscOffsetWords;
            return 1;
        }

        if (Config.CurrentRequestIndex == Config.RequestCount
            && Config.FstLoadAddress != 0u
            && Config.FstSize != 0u) {
            const uint32_t fstDestinationAddress = Config.FstLoadAddress;
            const uint32_t fstLength = Config.FstSize;
            const uint32_t fstDiscOffsetWords = Config.FstDiscOffsetWords;
            Config.CurrentRequestIndex += 1u;
            if (EnableApploaderReportTracing && ReportFunction != nullptr) {
                ReportFunction(
                    "[HA] fst dst=%08lX len=%08lX disc=%08lX\n",
                    static_cast<unsigned long>(fstDestinationAddress),
                    static_cast<unsigned long>(fstLength),
                    static_cast<unsigned long>(fstDiscOffsetWords << 2u));
            }
            *destinationAddress = fstDestinationAddress;
            *length = fstLength;
            *discOffsetWords = fstDiscOffsetWords;
            return 1;
        }

        if (EnableApploaderReportTracing && ReportFunction != nullptr) {
            ReportFunction("[HA] done\n");
        }
        return 0;
    }

    static uint32_t ApploaderClose() {
        if (Config.FstLoadAddress != 0u && Config.FstSize != 0u) {
            *reinterpret_cast<volatile uint32_t*>(ArenaHighLowMemoryAddress) = Config.FstLoadAddress;
            *reinterpret_cast<volatile uint32_t*>(FstAddressLowMemoryAddress) = Config.FstLoadAddress;
            *reinterpret_cast<volatile uint32_t*>(FstSizeLowMemoryAddress) = Config.FstSize;
        }

        if (EnableApploaderReportTracing && ReportFunction != nullptr) {
            ReportFunction(
                "[HA] close entry=%08lX arena=%08lX fst=%08lX size=%08lX\n",
                static_cast<unsigned long>(Config.EntryPoint),
                static_cast<unsigned long>(*reinterpret_cast<volatile uint32_t*>(ArenaHighLowMemoryAddress)),
                static_cast<unsigned long>(*reinterpret_cast<volatile uint32_t*>(FstAddressLowMemoryAddress)),
                static_cast<unsigned long>(*reinterpret_cast<volatile uint32_t*>(FstSizeLowMemoryAddress)));
        }

        ZeroBssRange();
        return Config.EntryPoint;
    }
}
