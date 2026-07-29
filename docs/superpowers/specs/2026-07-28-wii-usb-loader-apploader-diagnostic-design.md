# Wii USB Loader Apploader Diagnostic Design

## Goal

Determine whether the Wii U/vWii black-screen failure occurs while USB Loader GX reads the generated apploader/DOL/FST or after it transfers control to the Helengine executable. The diagnostic must preserve automatic IOS selection and produce filesystem evidence without requiring a USB Gecko.

## Current Evidence

USB Loader GX blacks the display and shuts down WPAD before it opens the game partition and invokes the disc apploader. Therefore, an immediate black display and disconnected Wii Remote do not prove that the generated DOL started.

The loader checks the reads for the apploader header and payload, but it ignores the return value of each DOL/FST read requested by the apploader. The generated image currently uses several lengths that are not multiples of 32 bytes. Separately, packaged runtime startup attempts to mount and read the physical optical drive before initializing `/dev/di`, which may hang after a successful loader handoff. Either defect fits the visible symptom, so the first failing boundary must be measured before either production path is changed.

## Considered Approaches

### Instrument USB Loader GX at the apploader boundary

Enable the loader's existing SD-file logger and record every apploader read request and result. This is the selected approach because it can distinguish a pre-entry read failure from a post-entry runtime hang in one hardware boot.

### Instrument only the Helengine executable

Add startup traces earlier in the game. This cannot diagnose failures that occur before the DOL is loaded or entered, and the existing ISFS trace destination has already produced no data. It is therefore insufficient as the first diagnostic.

### Patch both suspected defects immediately

Align generated reads and replace physical-drive startup in one build. This may produce a working build, but it would not reveal which boundary failed and would make regressions harder to isolate. It is deferred until the diagnostic identifies the first failure.

## Diagnostic Changes

The USB Loader GX diagnostic build will make only two source changes:

1. Enable the existing `DEBUG_TO_FILE` path in `source/gecko.c`, which appends loader diagnostics to `sd:/debug.txt` and falls back to `usb1:/debug.txt` if the game IOS reload makes the SD mount unavailable.
2. In `source/usbloader/apploader.c`, log the destination, requested length, partition-relative byte offset, and `WDVD_Read` return value for every request returned by `appldr_main`. If a read fails, return the error immediately instead of registering or flushing unread data.

The diagnostic will also log the apploader header values, payload read result, callback completion, and final entrypoint when those values are available. These messages provide an unambiguous sequence even if the custom apploader's callback output is absent.

## Data Flow

USB Loader GX opens the selected WBFS game partition, reads the apploader header and payload through `/dev/di`, invokes the generated apploader, and services each returned DOL/FST request through `WDVD_Read`. Every completed boundary is appended to `sd:/debug.txt` while SD is mounted or `usb1:/debug.txt` after an automatic game IOS transition. If all requests succeed, the log records the final entrypoint before the loader performs its existing shutdown and jump.

## Result Interpretation

- No apploader-start record means execution did not reach `Apploader_Run` or SD logging was unavailable.
- Header failure identifies partition/apploader-header access.
- Payload failure identifies the generated apploader image read.
- A failed DOL/FST request identifies the exact destination, length, offset, and error returned by d2x.
- Successful callbacks and a valid final entrypoint place the failure after apploader loading, making Helengine runtime startup the next boundary to instrument or correct.

## Error Handling

The diagnostic must not continue after a failed DOL/FST read. Returning the actual negative result preserves the failure at its source and prevents a jump into stale or uninitialized memory. Successful retail and custom reads retain the existing behavior.

## Build and Delivery

Build the loader using its checked-in devkitPro/Docker build path. Preserve the stock loader artifact and give the diagnostic binary a distinct delivery directory or filename. The SD card deployment should replace only the Homebrew Channel loader executable needed for this test; the user's configuration and game WBFS remain unchanged.

Before the hardware run, remove or rename existing `debug.txt` files from both the SD and USB roots so the resulting files contain one boot attempt. After the black-screen attempt, power back to the Homebrew Channel when possible and retrieve both files that were created.

## Validation

- Confirm the external USB Loader GX checkout has no unrelated modifications before editing.
- Review the diff to ensure only logging and failed-read propagation changed.
- Build the loader successfully with the repository's supported toolchain.
- Confirm the output artifact exists and record its size and checksum.
- The decisive validation is one hardware boot followed by inspection of the new `sd:/debug.txt`; no root-cause fix will be implemented until that evidence is available.

## Out of Scope

This diagnostic does not change Helengine, generated ISO/WBFS contents, cIOS configuration, video settings, loader patches, or runtime storage initialization. Those changes require a separate test-first implementation after the failing boundary is known.
