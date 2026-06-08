#include <stdint.h>

extern "C" {
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
        uint32_t Reserved0;
        uint32_t Reserved1;
        uint32_t Reserved2;
        ApploaderLoadRequest Requests[32];
    };

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
        (void)reportFunction;
        Config.CurrentRequestIndex = 0u;
    }

    static int ApploaderMain(uint32_t* destinationAddress, uint32_t* length, uint32_t* discOffsetWords) {
        if (Config.CurrentRequestIndex >= Config.RequestCount) {
            return 0;
        }

        const uint32_t requestIndex = Config.CurrentRequestIndex;
        *destinationAddress = Config.Requests[requestIndex].DestinationAddress;
        *length = Config.Requests[requestIndex].Length;
        *discOffsetWords = Config.Requests[requestIndex].DiscOffsetWords;
        Config.CurrentRequestIndex += 1u;
        return 1;
    }

    static uint32_t ApploaderClose() {
        ZeroBssRange();
        return Config.EntryPoint;
    }
}
