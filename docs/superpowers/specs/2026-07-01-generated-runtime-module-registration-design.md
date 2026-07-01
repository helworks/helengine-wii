# Generated Runtime Module Registration Design

## Goal

Replace platform-specific optional runtime registration seams with one generated-core-owned bootstrap contract.

Platforms such as Wii and GameCube must not know about physics, BEPU, or any other optional runtime library by name. They should only know that generated core may expose runtime module bootstrap work and should call one generic function after core initialization.

## Problem

The current Wii and GameCube pattern probes for `Physics3DRuntimeComponentRegistration.hpp` and conditionally calls `Physics3DRuntimeComponentRegistration::Register(core)`.

That approach has three architectural problems:

1. It makes one optional runtime library look platform-native.
2. It does not scale to additional optional runtime modules or plugins.
3. It forces platform hosts to know library-specific header names and registration entrypoints.

## Desired Outcome

Generated core owns the final runtime bootstrap surface.

Platforms should:

- include one generated header when generated core is present
- call one generic bootstrap function after `EngineCore->Initialize(...)`
- remain completely unaware of which optional modules were included

Optional runtime libraries and plugins should:

- declare their bootstrap contract explicitly in C#
- participate only when their runtime-owned types are actually used by the cooked build

## Recommended Design

### Generated Bootstrap Artifact

Generated core emits:

- `GeneratedRuntimeModuleRegistration.hpp`
- `GeneratedRuntimeModuleRegistration.cpp`

The generated header exposes:

```cpp
void RegisterGeneratedRuntimeModules(Core* core);
```

The generated source contains deterministic calls to the included runtime modules:

```cpp
void RegisterGeneratedRuntimeModules(Core* core) {
    Physics3DRuntimeComponentRegistration::Register(core);
}
```

If no optional runtime modules are active, the generated file still exists and emits an empty no-op body. This keeps the platform seam stable and avoids module-specific probing logic.

### Runtime Module Manifest

Optional runtime libraries or plugins that require runtime bootstrap declare one explicit C# manifest.

Not every plugin needs this. Only plugins that require generated runtime bootstrap participate.

Each manifest declares:

- `ModuleId`
- `RegistrationType`
- `RegistrationMethodName`
- `ActivationTypes`

`ActivationTypes` are the runtime-owned types that activate the module when they are present in the final used-type set discovered from cooked content and build usage.

Example direction:

- `ModuleId = "physics3d"`
- `RegistrationType = typeof(Physics3DRuntimeComponentRegistration)`
- `RegistrationMethodName = "Register"`
- `ActivationTypes = [typeof(RigidBody3D), typeof(BoxCollider3D), typeof(SphereCollider3D)]`

### Inclusion Rule

The build includes a runtime module only when its runtime-owned types are actually used.

Mere assembly reference is not enough.

The editor/build pipeline already knows the cooked scene/build usage graph. The runtime module generator should reuse that information.

Inclusion flow:

1. Cook/build computes the runtime-used type set from cooked scenes and related asset usage.
2. Generated-core scans manifests from the final build closure assemblies.
3. For each manifest, it checks whether any declared `ActivationTypes` are present in the used-type set.
4. Matching manifests are emitted into `GeneratedRuntimeModuleRegistration`.
5. The generated call order is deterministic, sorted by `ModuleId`.

### Platform Contract

Platform hosts must depend only on the generated bootstrap artifact.

They should:

1. include `GeneratedRuntimeModuleRegistration.hpp` when generated core is present
2. call `RegisterGeneratedRuntimeModules(EngineCore);`
3. perform that call immediately after `EngineCore->Initialize(...)`
4. perform it before startup scene load

Platforms must not probe for:

- `Physics3DRuntimeComponentRegistration.hpp`
- `BepuRuntimeComponentRegistration.hpp`
- any future optional runtime module by name

### Engine and Build Ownership

Responsibilities are split as follows:

- helengine/editor build pipeline owns manifest discovery and activation filtering
- generated core owns native bootstrap emission
- optional runtime modules own their manifest and registration implementation
- platform hosts own only lifecycle timing

## Plugin Model

This seam supports build-time plugin participation.

Plugin authors can contribute runtime bootstrap only when:

- their assembly is part of the final build closure
- they declare an explicit runtime module manifest
- their declared activation types are actually used by the cooked build

This keeps plugin participation explicit, deterministic, and build-owned.

## Failure Behavior

The build must fail loudly when a manifest is invalid.

Examples:

- registration type cannot be resolved
- registration method is missing
- activation types cannot be resolved
- duplicate module ids

This is build contract metadata and should not degrade into best-effort runtime behavior.

## Testing Strategy

### Editor and Build Tests

Add tests that verify:

- manifests are discovered from the build closure
- modules are included only when activation types are present in the used-type set
- unused modules are excluded
- emitted module order is deterministic
- invalid manifests fail the build with clear diagnostics

### Generated Output Tests

Add tests that verify:

- `GeneratedRuntimeModuleRegistration.hpp/.cpp` are emitted
- included modules produce the expected registration calls
- zero active modules still emit a valid no-op bootstrap artifact

### Platform Tests

Update Wii and GameCube source-contract tests so they verify:

- platform source includes only `GeneratedRuntimeModuleRegistration.hpp`
- platform source calls `RegisterGeneratedRuntimeModules(EngineCore);`
- platform source does not reference physics-specific registration headers or macros

## Migration Plan

1. Add the generic runtime module manifest contract in helengine.
2. Add generated bootstrap emission in the generated-core pipeline.
3. Move physics3d onto the manifest-driven generic bootstrap path.
4. Update Wii and GameCube to consume only `GeneratedRuntimeModuleRegistration`.
5. Remove platform-specific physics registration probing and tests.

## Scope Boundaries

This design covers build-time runtime module bootstrap only.

It does not introduce:

- dynamic runtime plugin loading
- runtime reflection-based registration
- platform-managed optional module registries
- best-effort fallback registration paths

## Recommendation

Implement the generated bootstrap artifact and manifest-driven activation flow now, then migrate physics3d to it first.

This gives immediate platform cleanup while establishing the correct seam for future optional runtime libraries and plugins.
