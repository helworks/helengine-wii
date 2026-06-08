# Wii Packaged ISO Handoff

## Goal

Continue the packaged-disc Wii boot path until the title reaches `main()` and then continue toward the authored main menu on Wii.

## Primary spec and plan

- Spec: `docs/superpowers/specs/2026-06-07-helengine-wii-packaged-disc-main-menu-visible-text-clean-startup-design.md`
- Plan: `docs/superpowers/plans/2026-06-07-helengine-wii-packaged-disc-main-menu-visible-text-clean-startup.md`

## Important committed checkpoint

- Commit `33366ad` on `main`
- Message: `Add self-generated Wii apploader packaging flow`

That commit removed the external apploader dependency and replaced it with a project-owned generated apploader path.

## Current state

- Packaged-disc only. Direct-`DOL` is not the target.
- `wit` packaging works.
- Clean startup in Dolphin was achieved for the self-generated apploader ISO line.
- Current strongest goal is no-warning packaged boot plus proof that title code reaches `main()`.
- Host-readable debugging should use Dolphin logs and window-text capture, not screenshots.

## Relevant artifacts

- Latest clean-startup ISO line:
  - `tmp/self-apploader-package-v3/city-self-apploader-v3.iso`
- Latest captured startup report:
  - `tmp/self-apploader-package-v3/dolphin-window-text-report.txt`
- Native apploader build products:
  - `build/helengine_wii_apploader.elf`
  - `build/helengine_wii_apploader_template.bin`
  - `build/helengine_wii_apploader.map`

## What is already proven

- The builder no longer needs an external `apploader.img`.
- `--write-disc-layout` now derives `sys/apploader.img` from the native template emitted beside `helengine_wii.dol`.
- `--package-image` works with the supported `wit` flow.
- The self-generated apploader image format was corrected to include a real Wii apploader header.
- The custom apploader link step was corrected to use `powerpc-eabi-ld` plus the explicit linker script, so `ApploaderEntry` is actually first at `0x81200020`.
- The packaged ISO can boot in Dolphin without the old startup warning.

## Current blocker

The packaged ISO still has no proof that execution reaches `main()`.

The latest investigation concluded that the self-generated apploader was still too thin. It only handled DOL section loads plus BSS zeroing. The next fix in progress adds:

- FST load request handoff through the apploader
- low-memory writes for:
  - `0x80000034` arena high
  - `0x80000038` FST address
  - `0x8000003C` FST size
- apploader-side progress logging through Dolphin's apploader report callback so Dolphin can show each boot step in host-visible logs

## In-progress uncommitted work

These files were actively being changed for the next apploader handoff attempt:

- `builder/WiiGeneratedApploaderImageBuilder.cs`
- `builder/WiiDiscSystemAreaWriter.cs`
- `builder.tests/WiiDiscSystemAreaWriterTests.cs`
- `builder.tests/WiiRuntimeSourceTests.cs`
- `src/platform/wii/apploader/WiiDiscApploader.cpp`

## Immediate failure to fix next

The last build failed in the native apploader link step after adding apploader-side report logging:

- symbol `ReportFunction` was placed in a discarded section during `powerpc-eabi-ld`
- the custom apploader linker script / section retention needs to be adjusted so this runtime state survives garbage collection

The failure happened while running the packaged native build:

```powershell
rtk proxy docker run --rm -v C:/dev/helworks/helengine-wii:/workspace -w /workspace -e HELENGINE_CORE_CPP_ROOT=/workspace/tmp/generated-core-wii -e HELENGINE_WII_BOOT_MODE=packaged-disc helengine-wii sh -lc "make clean && make 2>&1 | tail -c 12000"
```

## Recommended next steps

1. Fix the apploader linker-script / section-retention issue so `ReportFunction` is not discarded.
2. Rebuild the packaged native Wii outputs.
3. Rebuild a fresh packaged ISO from the self-generated apploader path.
4. Launch with the checked-in Dolphin packaged debug flow.
5. Confirm that Dolphin logs now show the apploader stages:
   - init
   - each DOL load request
   - FST load request
   - close / entry handoff
6. If apploader logs appear but `main()` still does not, compare the remaining low-memory boot contract against the Dolphin Wii BS2 path.

## Key references already used

- `src/platform/wii/apploader/WiiDiscApploader.cpp`
- `builder/WiiGeneratedApploaderImageBuilder.cs`
- `builder/WiiDiscSystemAreaWriter.cs`
- `reference/gc/dolphin/Source/Core/Core/Boot/Boot_BS2Emu.cpp`
- `reference/gc/swiss-gc/cube/swiss/source/swiss.c`

## Notes

- Do not switch to a piracy-dependent workflow or borrow a known-good retail Wii source artifact.
- The intended direction is a self-contained packaging flow, analogous to the GameCube approach: project-owned boot chain plus `wit` for final image composition.
