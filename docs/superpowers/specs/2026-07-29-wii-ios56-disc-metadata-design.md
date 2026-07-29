# Wii IOS56 Disc Metadata Design

## Goal

Make generated Wii ISO/WBFS images boot through USB Loader GX with normal automatic IOS selection, without requiring per-game settings.

## Root Cause

The packaged-disc builder invokes Wiimms ISO Tools without an IOS override. WIT therefore emits a TMD requesting IOS35. USB Loader GX reads that metadata, chooses the nearest installed d2x base (slot 248/base38 on the tested Wii U), and hangs while reloading that cIOS before the game apploader runs.

## Design

The image packager will pass `--ios` followed by `56` for both native WIT invocation and the PowerShell-wrapped Windows WIT invocation used from WSL. IOS56 maps to the standard d2x slot 249/base56 configuration and accurately expresses the generated title's intended modern Wii runtime baseline.

No loader settings, runtime startup, apploader, DOL, assets, or generated output will be patched.

## Validation

A focused builder regression test will assert that both invocation paths include `--ios 56`. After rebuilding the demo image, `wit dump` must report IOS56 in the TMD. The decisive hardware test keeps USB Loader GX on automatic IOS selection.
