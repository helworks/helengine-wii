# Wii Dolphin ISO Launcher Design

## Goal

Add one repo-local PowerShell script that launches an existing Wii ISO in Dolphin using the established logging profile.

The script is a developer utility only. It does not build, patch, inspect, or capture anything. It prepares a clean Dolphin session and launches the requested ISO.

## User-Facing Behavior

The script requires an explicit `-IsoPath` parameter.

If `-IsoPath` is missing, empty, or points to a file that does not exist, the script fails immediately with a clear error.

Before launching Dolphin, the script force-closes all running `Dolphin.exe` processes so the new session starts from a known state.

Before launching Dolphin, the script prints:

- the resolved ISO path
- the ISO last write timestamp
- the Dolphin executable path
- the isolated Dolphin user directory path

The script then launches Dolphin normally and leaves it running. It does not auto-close Dolphin after launch.

## Runtime Setup

The script uses an isolated Dolphin user directory inside the repository `tmp/` tree so the launch behavior is repeatable and does not depend on the caller's default user profile state.

The script seeds that isolated user directory from the known logging profile inputs already used by the Wii investigation workflow:

- `Qt.ini`
- `Dolphin.ini`
- `Logger.ini`

The seed source remains the existing global Dolphin config directory already used by the current helper scripts unless the repo already contains a better dedicated seed directory for this exact purpose.

Before each launch, the isolated user directory is deleted and recreated so stale state does not leak between runs.

## Scope Boundaries

The script does not:

- build Wii binaries
- patch `main.dol` into an ISO
- choose a default ISO automatically
- capture screenshots
- inspect windows
- collect logs after launch

Those behaviors stay in the existing diagnostic helpers.

## Script Placement

The launcher script should live in the repository as a Wii-specific helper alongside the current Dolphin investigation scripts so developers can find it near the rest of the workflow.

The preferred path is:

`tmp/launch_wii_iso_in_dolphin.ps1`

## Error Handling

The script should fail fast when:

- `-IsoPath` is not provided
- the ISO file does not exist
- the Dolphin executable does not exist
- the logging profile seed files cannot be found

It should not silently fall back to different paths or degraded behavior.

## Verification

Implementation is complete when:

1. Running the script with a valid ISO path force-closes existing Dolphin processes.
2. The script prints the ISO last write time before launch.
3. Dolphin starts with the requested ISO and the isolated logging profile.
4. Running the script without `-IsoPath` fails clearly.
