# Wii IOS56 Disc Metadata Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every generated Wii image request IOS56 so USB Loader GX automatic selection uses the standard d2x base56 path.

**Architecture:** Add the WIT `--ios 56` patch option at the existing image-packaging command boundary. Lock both native and WSL-wrapped command construction with focused tests, then verify the final demo image TMD.

**Tech Stack:** C#/.NET 9, xUnit, Wiimms ISO Tools.

## Global Constraints

- Users must not change USB Loader GX settings.
- Do not patch generated output.
- Do not alter runtime, apploader, DOL, or asset behavior.
- Preserve existing user changes in the main checkout.

---

### Task 1: Request IOS56 during image packaging

**Files:**
- Modify: `builder.tests/WiiWiimmsIsoToolsImagePackagerTests.cs`
- Modify: `builder/WiiWiimmsIsoToolsImagePackager.cs`

**Interfaces:**
- Consumes: `ProcessStartInfo.ArgumentList` assembled by `WiiWiimmsIsoToolsImagePackager`.
- Produces: WIT invocations containing adjacent arguments `--ios` and `56`.

- [ ] **Step 1: Add focused tests**

Add `Package_WhenWitRunsDirectly_RequestsIos56`, which invokes the real packager command-construction path with `FakeWiiProcessRunner` and asserts that `--ios` is immediately followed by `56`. Add `PackageSource_WhenWslWrapsWindowsWit_RequestsIos56`, which reads `WiiWiimmsIsoToolsImagePackager.cs` and locks the platform-specific PowerShell command fragment to `'--ios' '56'` because that branch cannot execute on the Windows test host.

- [ ] **Step 2: Verify RED**

Run:

```powershell
dotnet test builder.tests\helengine.wii.builder.tests.csproj -p:HelEngineRoot=C:\dev\helworks\helengine --filter FullyQualifiedName~WiiWiimmsIsoToolsImagePackagerTests
```

Expected: failure because the current command omits `--ios`.

- [ ] **Step 3: Add the minimal packager arguments**

For direct execution, append:

```csharp
startInfo.ArgumentList.Add("--ios");
startInfo.ArgumentList.Add("56");
```

For the WSL wrapper, add `'--ios' '56'` to the quoted WIT command.

- [ ] **Step 4: Verify GREEN**

Run the focused tests, then the complete Wii builder test project with `-p:HelEngineRoot=C:\dev\helworks\helengine`.

- [ ] **Step 5: Commit**

Commit only the spec, plan, packager, and packager tests.

### Task 2: Rebuild and inspect the demo image

**Files:**
- Build from: `C:\dev\helprojs\demodisc`
- Verify: newly generated Wii ISO/WBFS artifacts.

**Interfaces:**
- Consumes: verified builder commit applied to the main checkout.
- Produces: demo image whose TMD requests IOS56.

- [ ] **Step 1: Apply the verified builder commit to main**

Cherry-pick the isolated commit without disturbing existing Wii runtime changes.

- [ ] **Step 2: Run the established latest Wii build**

Use the repository's documented verified platform-build command for `C:\dev\helprojs\demodisc`.

- [ ] **Step 3: Verify TMD and image structure**

Run `wit dump` and `wit verify`; confirm the TMD system version is IOS56 and no partition hash errors are reported.

- [ ] **Step 4: Produce WBFS**

Convert the verified ISO to WBFS for USB Loader GX while preserving the IOS56 TMD.

- [ ] **Step 5: Hardware test**

Boot the new WBFS with unchanged automatic USB Loader GX settings and collect the diagnostic log if startup does not progress.
