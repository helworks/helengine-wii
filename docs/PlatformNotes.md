# Wii Platform Notes

## Current Status

- The shared editor CLI can build Wii packages with platform id `wii`.
- Direct-`DOL` developer boot is the default generated-core mode for Dolphin verification.
- The Wii runtime can also build an explicit packaged-disc boot variant for `dvd:/` startup.
- The Wii builder can stage packaged-disc outputs, runtime scene manifests, and final image artifacts.

## Packaged-Disc Note

The native host still supports an explicit packaged-disc boot mode for `dvd:/` startup when you need to validate extracted disc layouts or final Wii image builds. The shared editor CLI build remains the preferred day-to-day workflow.

## Low-Level Builder Helpers

`helengine.wii.builder` still exposes the low-level helpers used by the editor build graph:

```bash
dotnet run --project builder -- --stage-runtime-content <source-root> <runtime-root>
dotnet run --project builder -- --write-runtime-scene-manifest <generated-core-root> <startup-scene-id> <scene-id> <cooked-relative-path>
dotnet run --project builder -- --stage-runtime-generated-modules <generated-core-root> <code-root> <cooked-scene-asset-path> <module-id> <assembly-path> [<module-id> <assembly-path> ...]
dotnet run --project builder -- --write-disc-layout <staging-root> <native-executable> <disc-root> <disc-id> <disc-title>
dotnet run --project builder -- --package-image <disc-root> <output-image>
```
