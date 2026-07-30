# Wii Direct Disc Interface Design

## Problem

The packaged Wii build boots through USB Loader GX on Wii U/vWii but stops at visible failure code `C113`. That checkpoint is written immediately before `ContentManager.Load<SceneAsset>` and remains visible because the content load never returns. The same ISO loads and renders correctly in Dolphin.

The packaged-storage bootstrap currently calls libdi `DI_Init()` and accepts every non-negative result as success. The libdi implementation linked by the Docker build checks the AHBPROT register before opening `/dev/di`. When AHBPROT is unavailable, `DI_Init()` returns `0` without opening a descriptor. The first `DI_Read()` then returns `-ENXIO`, which causes the first packaged scene read to throw and leaves the application at `C113`.

USB Loader GX does not rely on libdi for its game reads. It opens `/dev/di` through IOS and submits encrypted partition reads using ioctl `0x71`. Its apploader successfully uses that path to load this title's DOL and FST before jumping to the title.

## Goals

- Make packaged content reads work without AHBPROT or per-game USB Loader GX settings.
- Use the cIOS-compatible `/dev/di` request contract already exercised by USB Loader GX.
- Preserve the opened encrypted partition and FST handoff established by the loader and project-owned apploader.
- Fail initialization when the title cannot actually open `/dev/di`; never report false success.
- Keep disc I/O isolated from scene loading, content parsing, and generated runtime code.

## Non-Goals

- Do not reload IOS or select a cIOS slot from the title.
- Do not reopen the encrypted partition or parse tickets and TMD data in the title.
- Do not require users to enable AHBPROT, block IOS reload, or change any loader setting.
- Do not patch, fork, or post-process libogc/libdi.
- Do not change the disc FST format, packaged asset format, scene serializer, or generated C++ output.
- Do not add retries or best-effort fallbacks that conceal failed disc requests.

## Considered Approaches

### Engine-owned direct `/dev/di` interface

This is the selected approach. A small Wii-specific interface opens `/dev/di` with IOS and issues ioctl `0x71` reads using the same command layout as USB Loader GX. It removes the AHBPROT gate while retaining the existing partition-relative offset and aligned-buffer behavior.

### Patch or replace libdi

Changing libdi's AHBPROT behavior would couple the engine to a modified external toolchain package. Docker image updates could silently replace the patch, and the project would still inherit unrelated libdi behavior. This approach is rejected.

### Add another diagnostic-only build

Additional checkpoints could distinguish FST lookup, the first DI request, stream construction, and binary parsing. The linked libdi implementation already exposes a concrete false-success path that predicts the observed hardware-only failure exactly, so another diagnostic-only hardware cycle is not required before testing the root fix.

## Architecture

Add one Wii-specific `WiiDiscInterface` type in its own header and implementation file. It owns the title's `/dev/di` file descriptor and exposes only initialization and encrypted partition reads.

`WiiSceneBootstrap::InitializePackagedStorage()` initializes `WiiDiscInterface` and returns false if IOS cannot open `/dev/di`. Once that succeeds, it initializes `WiiDiscFileSystem` against the already-opened partition handoff.

`WiiDiscFileSystem` continues to own FST capture, path normalization, file lookup, chunking, temporary aligned buffers, and memory-backed stream construction. Its low-level chunk read delegates to `WiiDiscInterface` instead of libdi `DI_Read()`.

The native Makefile compiles the new implementation and removes the now-unused libdi link dependency. No generated files are edited.

## IOS Request Contract

The device path storage, ioctl input buffer, and read destination must satisfy IOS alignment requirements. `WiiDiscInterface` therefore uses 32-byte-aligned storage for `/dev/di` and for the eight-word ioctl command buffer.

Initialization performs `IOS_Open("/dev/di", 0)`. A negative result is an initialization failure. A successful descriptor is retained for the title lifetime.

An encrypted partition read submits ioctl `0x71` with:

- command word 0: `0x71 << 24`;
- command word 1: aligned byte length;
- command word 2: partition-relative byte offset shifted right by two;
- input size: `0x20` bytes;
- output buffer and size: the caller-provided 32-byte-aligned destination and aligned length.

IOS returns `1` for a successful `/dev/di` read. Every other result is treated as a failure and is returned to `WiiDiscFileSystem` for its existing exception path. The interface does not reset the drive, change WBFS mode, or open a partition because USB Loader GX and the apploader have already established that state.

## Data Flow

1. USB Loader GX configures the WBFS fragment mapping and opens the game's encrypted partition.
2. The project-owned apploader loads the DOL and FST, publishes the FST address and size in Wii low memory, and returns the DOL entry point.
3. The title opens a fresh `/dev/di` descriptor directly through IOS without consulting AHBPROT.
4. `WiiDiscFileSystem` captures and indexes the apploader-provided FST.
5. A packaged content request resolves its partition-relative FST offset.
6. `WiiDiscFileSystem` performs aligned chunk reads through `WiiDiscInterface`.
7. The existing memory-backed `FileStream` and packaged asset serializer consume the returned bytes unchanged.

## Error Handling

- A negative `IOS_Open` result makes packaged-storage initialization fail visibly through the existing boot failure path.
- An invalid descriptor, unaligned request, or unsuccessful ioctl fails the read; it is not retried or converted into empty content.
- FST lookup and allocation failures retain their current exceptions.
- Existing failure-screen behavior remains authoritative. If the hypothesis is correct, hardware progresses beyond `C113`; if another content failure remains, the next existing checkpoint identifies the later boundary.

## Testing

Source-contract tests are written and observed failing before production edits. They verify that:

- packaged startup initializes the engine-owned interface instead of calling `DI_Init()`;
- packaged reads delegate to the engine-owned interface instead of `DI_Read()`;
- the interface opens `/dev/di` directly and rejects a negative descriptor;
- ioctl `0x71`, command fields, word-offset conversion, 32-byte input size, and success result handling match the USB Loader GX contract;
- the native build includes the new implementation and no longer links unused libdi;
- the existing FST handoff, chunk size, aligned buffers, and scene-load diagnostics remain intact.

After focused tests pass, build fresh ISO and WBFS artifacts. Verify both with WIT, launch the ISO in Dolphin, and inspect the runtime trace for successful scene loading and rendering. The decisive acceptance test is the fresh WBFS launched from USB Loader GX with default per-game settings.

## Success Criteria

- Focused source-contract tests pass.
- A fresh packaged build completes and WIT verifies both ISO and WBFS images.
- Dolphin still loads and renders the startup scene from `dvd:/`.
- USB Loader GX on Wii U/vWii progresses beyond `C113` without any settings change.
- The startup scene renders, or any remaining failure reports a later precise code that identifies an independent problem.
