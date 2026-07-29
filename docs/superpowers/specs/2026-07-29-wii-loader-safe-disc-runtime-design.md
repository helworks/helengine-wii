# Wii Loader-Safe Disc Runtime Design

## Objective

Make packaged Wii builds boot and read cooked content through USB Loader GX on vWii without requiring per-game loader settings. The same ISO must continue to boot in Dolphin. A failure that returns control to the application must remain visible as a red framebuffer with its diagnostic code instead of immediately returning to Homebrew Channel.

## Evidence and Root Cause

USB Loader GX opens the DATA partition, runs the disc apploader, installs the DOL and FST handoff, shuts down its IOS/libogc subsystems, and branches to the DOL entrypoint. The current packaged runtime then assumes ownership of the physical drive again by calling `DVD_Init`, `DVD_MountAsync`, raw absolute DVD reads, and `DI_OpenPartition`.

That second mount path was validated only in Dolphin. On hardware, `ReadRawDiscBytes` waits without a timeout for a `DVD_ReadAbsAsyncPrio` callback. The callback is not guaranteed after the loader handoff, so the runtime can remain in an infinite wait with the display black and Wii Remote disconnected. This matches the earlier GameCube/Nintendont failure class in which a private libogc DVD completion path was not serviced by the loader.

The installed libdi implementation shows that `DI_Init` opens `/dev/di` when its descriptor is closed; it does not reset the drive or close the partition. The existing packaged file reads already pass partition-relative word offsets to `DI_Read`, which is the correct contract after the apploader has opened the partition.

## Considered Approaches

### Reuse the loader-open partition through libdi

This is the selected approach. Packaged startup calls `DI_Init`, trusts the apploader FST in low memory, and performs only partition-relative `DI_Read` operations. It removes the conflicting physical-drive ownership transition and requires no USB Loader settings.

### Reopen the partition using unencrypted DI reads

The runtime could replace raw `DVD_*` reads with `DI_UnencryptedRead`, rediscover the partition offset, and call `DI_OpenPartition`. This avoids the private DVD callback but still reopens state that the loader already established. It adds commands and failure modes without serving the runtime's actual needs.

### Keep the existing remount path with timeouts

Adding timeouts would prevent some infinite waits but would not make `DVD_Init` and `DVD_MountAsync` safe after a cIOS loader handoff. It would convert a lock into a visible failure while preserving the incorrect ownership model, so it is rejected.

## Runtime Architecture

`WiiSceneBootstrap::InitializePackagedStorage` will become a small loader-handoff initializer:

1. Call `DI_Init` to reopen the process-local `/dev/di` descriptor after USB Loader GX shuts down its own subsystems.
2. Fail normally if `/dev/di` cannot be opened.
3. Initialize `WiiDiscFileSystem` for an already-open encrypted partition.
4. Do not reset, mount, rediscover, close, or reopen the partition.

`WiiDiscFileSystem` will retain the apploader-provided FST snapshot at low-memory addresses `0x80000038` and `0x8000003C`. FST file offsets remain converted from words to bytes. `DI_Read` will continue receiving partition-relative word offsets.

The obsolete partition data offset is currently used only as an initialization flag and for diagnostic arithmetic. It will be replaced by an explicit initialized state so the API describes the actual runtime contract instead of implying that physical-disc offsets participate in decrypted reads.

## Failure Behavior

Storage initialization errors will use the existing `A003` packaged-storage failure checkpoint and remain displayed in the initialization failure loop.

Update or draw failures will present the existing red failure framebuffer and remain in a shutdown-aware presentation loop. They will not return immediately to the loader. Reset and power callbacks remain the supported way to leave a failed build.

The runtime will not show progress colors. Normal startup remains black until authored rendering begins; red remains reserved for failures.

## Tests

Source-contract tests will first fail against the current implementation and then require:

- packaged startup calls `DI_Init`;
- packaged startup contains no `DVD_Init`, `DVD_MountAsync`, `DVD_ReadAbsAsyncPrio`, raw partition-table parsing, or `DI_OpenPartition`;
- the disc filesystem exposes explicit initialization for an already-open partition;
- decrypted reads remain partition-relative;
- update and draw failures enter a shutdown-aware presentation loop rather than returning immediately.

The focused Wii runtime source tests will run after each change. A native packaged build will then produce one matched ISO and WBFS from the same DOL and cooked content. WIT verification and an exact DOL hash comparison will validate packaging before hardware testing.

## Scope

This change does not modify USB Loader GX, require users to change loader settings, add an alternate storage backend, or patch generated output. It changes only the Wii runtime's ownership of the already-open packaged-disc handoff and the persistence of existing failure diagnostics.
