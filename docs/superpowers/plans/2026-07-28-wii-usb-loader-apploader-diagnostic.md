# Wii USB Loader Apploader Diagnostic Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a distinct USB Loader GX `boot.dol` that records every custom-disc apploader boundary and disc-read result in `sd:/debug.txt`.

**Architecture:** Make a diagnostic-only build from an isolated worktree of the clean USB Loader GX repository. Reuse its existing SD logger, add boundary records around `WDVD_Read` and apploader callbacks, and stop on failed DOL/FST reads so the loader cannot jump into unread memory.

**Tech Stack:** C, devkitPPC/libogc, USB Loader GX, Docker with `devkitpro/devkitppc:20250527`, PowerShell.

## Global Constraints

- Do not change Helengine, the generated ISO/WBFS, cIOS settings, or runtime storage initialization.
- Preserve the reference USB Loader GX checkout and edit an isolated worktree.
- Limit source changes to `source/gecko.c` and `source/usbloader/apploader.c`.
- Write diagnostics to `sd:/debug.txt` without USB Gecko hardware.
- Preserve successful boot behavior and return the real error after a failed disc read.
- Keep existing user changes in `C:\dev\helworks\helengine-wii` untouched.

---

### Task 1: Create the isolated diagnostic worktree

**Files:**
- Use: `C:\dev\helworks\reference\wii\usbloadergx`
- Create: `C:\tmp\usbloadergx-apploader-diagnostic`

**Interfaces:**
- Consumes: clean USB Loader GX reference checkout.
- Produces: isolated diagnostic worktree.

- [ ] **Step 1: Confirm the reference checkout is clean**

```powershell
git -c safe.directory=C:/dev/helworks/reference/wii/usbloadergx -C C:\dev\helworks\reference\wii\usbloadergx status --short
```

Expected: no output.

- [ ] **Step 2: Create the worktree**

```powershell
git -c safe.directory=C:/dev/helworks/reference/wii/usbloadergx -C C:\dev\helworks\reference\wii\usbloadergx worktree add -b codex/apploader-diagnostic C:\tmp\usbloadergx-apploader-diagnostic HEAD
```

Expected: the current reference commit is checked out.

- [ ] **Step 3: Record baseline behavior**

```powershell
rg -n -C 3 "DEBUG_TO_FILE|WDVD_Read\(dst" C:\tmp\usbloadergx-apploader-diagnostic\source\gecko.c C:\tmp\usbloadergx-apploader-diagnostic\source\usbloader\apploader.c
```

Expected: `DEBUG_TO_FILE` is commented out and the callback-loop read result is ignored.

### Task 2: Add diagnostic boundary logging

**Files:**
- Modify: `C:\tmp\usbloadergx-apploader-diagnostic\source\gecko.c:8`
- Modify: `C:\tmp\usbloadergx-apploader-diagnostic\source\usbloader\apploader.c:42`

**Interfaces:**
- Consumes: existing `gprintf` and `WDVD_Read`.
- Produces: records prefixed with `[HAD]`; a failed callback-loop read returns its negative result.

- [ ] **Step 1: Enable SD logging**

Change `source/gecko.c` to:

```c
#define DEBUG_TO_FILE
```

- [ ] **Step 2: Log header and payload boundaries**

After the header read, log its result, offset, and length. After calculating `appldr_len`, log `buffer[4]` and the payload length. After the payload read, log its result, offset, and length:

```c
gprintf("[HAD] header read result=%d offset=0x%08x length=0x%08x\n", ret, APPLDR_OFFSET, 0x20);
gprintf("[HAD] header entry=%p payload_length=0x%08x\n", (void *) buffer[4], appldr_len);
gprintf("[HAD] payload read result=%d offset=0x%08x length=0x%08x\n", ret, APPLDR_OFFSET + 0x20, appldr_len);
```

- [ ] **Step 3: Log callbacks and requests**

Log before and after `appldr_entry`, after `appldr_init`, then replace the ignored loop read with:

```c
gprintf("[HAD] request dst=%p length=0x%08x offset_words=0x%08x offset_bytes=0x%08llx\n",
        dst, len, offset, (unsigned long long) ((u64) offset << 2));
ret = WDVD_Read(dst, len, (u64) offset << 2);
gprintf("[HAD] request result=%d\n", ret);
if (ret < 0)
    return ret;
```

- [ ] **Step 4: Log final callback and entrypoint**

```c
gprintf("[HAD] calling final callback\n");
*entry = appldr_final();
gprintf("[HAD] final entry=%p\n", *entry);
```

- [ ] **Step 5: Review the diff**

```powershell
git -c safe.directory=C:/tmp/usbloadergx-apploader-diagnostic -C C:\tmp\usbloadergx-apploader-diagnostic diff --check
git -c safe.directory=C:/tmp/usbloadergx-apploader-diagnostic -C C:\tmp\usbloadergx-apploader-diagnostic diff -- source/gecko.c source/usbloader/apploader.c
```

Expected: only the logger define, `[HAD]` records, captured read result, and failed-read return are present.

### Task 3: Build and package the diagnostic loader

**Files:**
- Build from: `C:\tmp\usbloadergx-apploader-diagnostic\Dockerfile`
- Produce: `C:\tmp\usbloadergx-apploader-diagnostic-output\usbloader_gx.zip`
- Produce: `C:\tmp\usbloadergx-apploader-diagnostic-output\boot.dol`
- Produce: `C:\tmp\usbloadergx-apploader-diagnostic-output\SHA256SUMS.txt`

**Interfaces:**
- Consumes: Dockerfile pinned to `devkitpro/devkitppc:20250527`.
- Produces: Homebrew Channel diagnostic executable and checksum manifest.

- [ ] **Step 1: Build through Docker**

```powershell
docker build --output type=local,dest=C:\tmp\usbloadergx-apploader-diagnostic-output C:\tmp\usbloadergx-apploader-diagnostic
```

Expected: Docker exits successfully and exports `usbloader_gx.zip`.

- [ ] **Step 2: Extract the executable**

```powershell
Expand-Archive -LiteralPath C:\tmp\usbloadergx-apploader-diagnostic-output\usbloader_gx.zip -DestinationPath C:\tmp\usbloadergx-apploader-diagnostic-output\expanded -Force
Copy-Item -LiteralPath C:\tmp\usbloadergx-apploader-diagnostic-output\expanded\usbloader_gx\boot.dol -Destination C:\tmp\usbloadergx-apploader-diagnostic-output\boot.dol
```

- [ ] **Step 3: Record hashes**

```powershell
Get-FileHash -Algorithm SHA256 C:\tmp\usbloadergx-apploader-diagnostic-output\boot.dol,C:\tmp\usbloadergx-apploader-diagnostic-output\usbloader_gx.zip | Format-Table Algorithm,Hash,Path -AutoSize | Out-File -Encoding ascii C:\tmp\usbloadergx-apploader-diagnostic-output\SHA256SUMS.txt
```

- [ ] **Step 4: Verify artifacts and source state**

```powershell
Get-Item C:\tmp\usbloadergx-apploader-diagnostic-output\boot.dol,C:\tmp\usbloadergx-apploader-diagnostic-output\usbloader_gx.zip | Select-Object FullName,Length,LastWriteTime
Get-Content C:\tmp\usbloadergx-apploader-diagnostic-output\SHA256SUMS.txt
git -c safe.directory=C:/tmp/usbloadergx-apploader-diagnostic -C C:\tmp\usbloadergx-apploader-diagnostic status --short
```

Expected: both artifacts are non-empty and exactly two diagnostic source files are modified.

- [ ] **Step 5: Commit the diagnostic source**

```powershell
git -c safe.directory=C:/tmp/usbloadergx-apploader-diagnostic -C C:\tmp\usbloadergx-apploader-diagnostic add -- source/gecko.c source/usbloader/apploader.c
git -c safe.directory=C:/tmp/usbloadergx-apploader-diagnostic -C C:\tmp\usbloadergx-apploader-diagnostic commit -m "Add apploader disc-read diagnostics"
```

### Task 4: Collect hardware evidence

**Files:**
- Deploy: `C:\tmp\usbloadergx-apploader-diagnostic-output\boot.dol`
- Retrieve: `sd:/debug.txt`

**Interfaces:**
- Consumes: diagnostic loader and unchanged game WBFS.
- Produces: one clean log identifying the first failed or completed boot boundary.

- [ ] **Step 1: Prepare the SD card**

Back up the installed USB Loader GX `boot.dol`, install the diagnostic `boot.dol`, and remove or rename an existing `sd:/debug.txt`.

- [ ] **Step 2: Run one boot**

Launch the loader from Homebrew Channel and start the unchanged game. After the black-screen result, return or power-cycle as required.

- [ ] **Step 3: Retrieve evidence**

Copy `sd:/debug.txt` back unchanged. The last `[HAD]` record determines whether the next change belongs in generated apploader reads or Helengine startup.


