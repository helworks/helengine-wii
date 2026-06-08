# Wii Packaged Disc Final Status

## Outcome

The packaged-disc Wii path now boots through the project-owned apploader, reaches `main()`, mounts packaged runtime content, reads cooked assets through DI, and renders the authored main menu in Dolphin.

Confirmed visible menu entries:

- `Demo Scenes`
- `Physics Scenes`
- `Options`
- `wii`
- `wii-headless`

## Primary spec and plan

- Spec: `docs/superpowers/specs/2026-06-07-helengine-wii-packaged-disc-main-menu-visible-text-clean-startup-design.md`
- Plan: `docs/superpowers/plans/2026-06-07-helengine-wii-packaged-disc-main-menu-visible-text-clean-startup.md`

## Starting checkpoint

- Commit `33366ad` on `main`
- Message: `Add self-generated Wii apploader packaging flow`

## Final working artifacts

- Latest packaged ISO:
  - `tmp/self-apploader-package-v3/city-self-apploader-v26.iso`
- Strongest late-frame visual proof:
  - `tmp/packaged-disc-proof-life/warning-windows-isolated-v25-late/window-00.png`
- Clean runtime log proving stable packaged frames and glyph submission:
  - `tmp/self-apploader-package-v3/dolphin-window-text-report-v25.txt`

## Root causes resolved

1. The self-generated Wii apploader entry contract was wrong.
   - Dolphin skips the 0x20-byte apploader header before transferring control.
   - The entrypoint and linker base needed to move from `0x81200020` to `0x81200000`.
   - The apploader also needed retained runtime state plus explicit BI2/FST/arena low-memory handoff and report logging.

2. The packaged runtime was opening the Wii partition with byte units instead of word units.
   - `DI_OpenPartition` needed `partitionOffset >> 2U` to match the Dolphin/libogc contract.

3. Packaged content reads needed to use the decrypted DI path correctly.
   - The runtime now uses the apploader-loaded FST from low memory.
   - File entry offsets remain `fileOffsetWords << 2U`.
   - Reads are chunked and aligned through `DI_Read` using partition-data-relative offsets.

4. Empty text lines were tripping the generated native text layout path during menu rendering.
   - The Wii render bridge now treats empty lines as zero-width lines instead of measuring them through the generated helper.

## Files that matter most

- `builder/WiiGeneratedApploaderImageBuilder.cs`
- `builder.tests/WiiRuntimeSourceTests.cs`
- `src/main.cpp`
- `src/platform/wii/WiiApplication.cpp`
- `src/platform/wii/WiiDiscFileSystem.cpp`
- `src/platform/wii/WiiDiscFileSystem.hpp`
- `src/platform/wii/WiiRenderManager2D.cpp`
- `src/platform/wii/WiiSceneBootstrap.cpp`
- `src/platform/wii/apploader/WiiDiscApploader.cpp`
- `src/platform/wii/apploader/WiiDiscApploader.ld`

## Verification performed

- `dotnet test builder.tests/helengine.wii.builder.tests.csproj --no-restore --filter PackagedGpuText_ExposesHostVisibleDiagnosticsForMenuTextPath`
- `dotnet test builder.tests/helengine.wii.builder.tests.csproj --no-restore --filter Packaged`
- `rtk proxy docker run --rm -v C:/dev/helworks/helengine-wii:/workspace -w /workspace -e HELENGINE_CORE_CPP_ROOT=/workspace/tmp/generated-core-wii -e HELENGINE_WII_BOOT_MODE=packaged-disc helengine-wii sh -lc "make 2>&1 | tail -c 12000"`
- Rebuilt disc layout and packaged fresh ISOs through the checked-in builder plus local `wit.exe`
- Manual visible Dolphin confirmation on `city-self-apploader-v26.iso`

## Current caveat

The automated isolated screenshot helper was inconsistent on the last clean `v26` capture and produced one all-black game-window image even while the logger window showed stable packaged frames and `submittedGlyph=true`. Manual visible launch on the same `v26` ISO confirmed the menu renders correctly, so this is a capture artifact concern, not an active packaged Wii boot/render blocker.

## Recommended next step

Treat packaged-disc Wii boot-to-menu as complete and move to the next runtime or gameplay task. If more automated proof is needed later, improve the screenshot helper separately from the packaged Wii runtime path.
