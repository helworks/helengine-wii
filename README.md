# Helengine Wii Host

This repository contains the Wii native host, the Wii platform builder integration, and the Wii-specific runtime source audits for Helengine.

## Current status

- The shared editor CLI can build Wii packages with platform id `wii`.
- Direct-`DOL` developer boot is the default generated-core mode for Dolphin verification.
- The Wii runtime can also build an explicit packaged-disc boot variant for `dvd:/` startup.
- The Wii builder can stage packaged-disc outputs, runtime scene manifests, and final image artifacts.
- Source-audit coverage exists for the Wii startup bootstrap split, runtime manifest wiring, and packaged-disc file bridge.

## Editor CLI build

If your workspace keeps `helengine-wii`, `helengine`, and `helprojs` as sibling directories, use the shared wrapper like this:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File ..\helengine\artifacts\build-platform.ps1 `
  -Project ..\helprojs\city\project.heproj `
  -Platform wii `
  -Output ..\helprojs\city\wii-build
```

That wrapper runs the main editor CLI with `--build wii` and writes the generated Wii package to the output directory you provide.

## Launching in Dolphin

Use the checked-in launcher script:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tmp\launch_wii_iso_in_dolphin.ps1 `
  -IsoPath .\tmp\packaged-disc-proof-life\city.iso
```

The launcher requires an explicit `-IsoPath`. Before launch it force-closes any running `Dolphin.exe` processes, recreates an isolated Dolphin user directory under `tmp\`, copies the `Wii`, `Backup`, and `ResourcePacks` directories from the global Dolphin profile, seeds `Logger.ini` from the global Dolphin profile channel set, forces `WriteToConsole`, `WriteToFile`, and `WriteToWindow` on in the isolated profile, and forces the Dolphin logger window visible in the isolated `Qt.ini`.

The launcher prints:

- the ISO path
- the ISO last write time
- the Dolphin executable path
- the isolated user directory path
- the seeded `Logger.ini` path
- that the logger window is enabled
- the spawned Dolphin process id

It then starts Dolphin with `-u <userdir> -e <iso>`.

The script fails fast when:

- `-IsoPath` is missing
- the ISO file is missing
- the Dolphin executable is missing
- the logging profile seed files are missing

## Verification

Run the focused Wii audit suite:

```bash
dotnet test builder.tests/helengine.wii.builder.tests.csproj --filter "WiiRuntimeSourceTests|WiiRuntimeSceneManifestWriterTests|WiiPackagedBuildWorkspaceTests|WiiDockerNativeBuildExecutorTests|WiiWiimmsIsoToolsImagePackagerTests|WiiLooseContentStagerTests"
```

This verifies the current Wii-specific contract around:

- direct-`DOL` versus packaged-disc boot selection
- runtime scene manifest generation
- packaged-disc workspace staging and native build orchestration
- Wiimms ISO Tools image packaging
- loose-content staging support

## Packaged-disc note

The native host still supports an explicit packaged-disc boot mode for `dvd:/` startup when you need to validate extracted disc layouts or final Wii image builds. The shared editor CLI build remains the preferred day-to-day workflow.

## Low-level builder helpers

`helengine.wii.builder` still exposes the low-level helpers used by the editor build graph:

```bash
dotnet run --project builder -- --stage-runtime-content <source-root> <runtime-root>
dotnet run --project builder -- --write-runtime-scene-manifest <generated-core-root> <startup-scene-id> <scene-id> <cooked-relative-path>
dotnet run --project builder -- --stage-runtime-generated-modules <generated-core-root> <code-root> <cooked-scene-asset-path> <module-id> <assembly-path> [<module-id> <assembly-path> ...]
dotnet run --project builder -- --write-disc-layout <staging-root> <native-executable> <disc-root> <disc-id> <disc-title>
dotnet run --project builder -- --package-image <disc-root> <output-image>
```

`--write-disc-layout` derives `sys/apploader.img` from the native apploader template emitted beside the packaged Wii DOL; it no longer requires an external apploader path.

`--package-image` requires `HELENGINE_WII_WIT_PATH` to point at the installed `wit` executable from Wiimms ISO Tools.
