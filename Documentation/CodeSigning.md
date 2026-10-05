# Code signing plan (SignPath Foundation)

On hold since 2026-10-05: releases are unsigned for now, and `.github/workflows/release.yml` only signs when asked to in a manual run. The slim distribution build described below is what ships either way.

## Why

Windows 11 Smart App Control blocks unsigned executables that have no reputation. A new unsigned build cannot be launched at all, and the user has no click-through. Seen in Windows Sandbox on 2026-10-05: `Hyperion.Editor.exe` refused with "An Application Control policy has blocked this file". A release has to be signed to run on those machines.

## SignPath Foundation: what it requires

From signpath.org/terms and docs.signpath.io:

- OSI-approved license, no commercial dual licensing, actively maintained, already publicly released, described on the download page.
- No proprietary components, except system libraries. Aftermath, Steam and any asset we cannot redistribute have to go.
- Only the project's own binaries are signed, built from source in a verifiable way.
- GitHub-hosted runners only: every job leading up to the signing request must run on GitHub-hosted agents. A self-hosted runner cannot produce signed releases.
- Product name and version metadata must be set consistently on signed files.
- Every release needs manual approval in SignPath. MFA on SignPath and the repository for everyone involved.
- The repository must publish a "Code signing policy" section (draft below).

Not known (the application form could not be retrieved): exact form fields and review time. Ask or read signpath.org/apply directly.

## What we ship today (staged preview, 79 binaries)

| Group | Count | Status |
|---|---|---|
| Ours, native (hyperion*.dll, strata.dll, stratac.exe, hyperion-sample.exe, 4 commandlets, Hyperion.Editor.exe) | 11 | unsigned, no version metadata |
| Ours, managed (Hyperion.Editor.dll, Hyperion.NET.* x4) | 5 | unsigned, product name differs per file, version 1.0.0 / 0.5.1+sha |
| Third-party native (brotli, bz2, fmt, freetype, curl, libpng, zlib, OpenAL, LLVM-C, Aftermath) | 11 | unsigned |
| Third-party managed (Avalonia x23, Dock x11, DynamicData, MicroCom, Tmds.DBus) | 37 | unsigned |
| Already signed (VC++ runtime, dxcompiler, SkiaSharp, HarfBuzzSharp, Newtonsoft, System.Reactive) | 15 | fine |

Smart App Control evaluates every image a process loads, not just the exe, so signing our 16 files would not be enough. Step 0 measured it.

## Status (2026-10-05)

Implemented and built: `BuildHyperion.bat Release ninja distribution regenerate` (about 4 minutes from clean with Ninja and clang-cl), then `PackageEditorWindows.ps1`. The staged editor runs on the dev machine. In the sandbox probe (network on, Smart App Control on) all 14 third-party DLLs load and only our unsigned files are blocked: `Hyperion.Editor.exe`, `hyperion.dll`, `Hyperion.NET.Scripting.dll`. Those three are what SignPath has to sign.

How it is built:

- `HYP_MONOLITHIC` (CMake, set by the `distribution` argument) links hyperion-core, hyperion-net and Strata (LLVM JIT off) into `hyperion.dll`, skips the sample and commandlet executables, and adds the version resource. Core and net are placed ahead of the engine's objects on the link line so their static initializers still run first.
- Static vcpkg libraries come from the `x64-windows-hyp-static` triplet in `Tools/vcpkg` (`Tools\Scripts\InstallDistributionDependencies.bat`).
- The packaging script publishes the editor as a single file. `Hyperion.NET.Scripting.dll` stays a loose file because the engine loads it from disk.

Not in this build: Strata JIT, C# script compilation (Roslyn is not shipped), "Play As Dedicated Server" (needs CacheServerCommandlet.exe). The dev build has not been rebuilt with these changes yet.

## Target ship set (decided 2026-10-05)

Shrink what we ship so only these files need our signature (originally two, three once Hyperion.NET.Scripting.dll turned out to be loaded from disk):

| File | Signer |
|---|---|
| `Hyperion.Editor.exe` (single-file publish: Hyperion.Editor, Hyperion.NET.*, Avalonia, Dock and the other managed dependencies bundled in) | SignPath |
| `hyperion.dll` (core, net, Strata and the vcpkg libraries linked in statically) | SignPath |
| `dxcompiler.dll`, VC++ runtime DLLs, `runtimes/win-x64/native/*` (Skia, HarfBuzz, ANGLE) | already signed by Microsoft / Avalonia |

Dropped from the zip: all other exes (hyperion-sample, stratac, commandlets), `strata.dll`, `LLVM-C.dll` (Strata ships without its LLVM JIT), Aftermath, and the loose managed DLLs.

Why this is feasible (checked in the code): every C# P/Invoke targets `hyperion` only, so core, net and strata can move into `hyperion.dll`; cooking already runs in-process; `STRATA_ENABLE_LLVM` and `STRATA_SHARED` are existing CMake options; the editor only launches `CacheServerCommandlet` ("Play As Dedicated Server").

Work to get there:

1. Static third-party libraries: build the distribution configuration against a static vcpkg triplet (`x64-windows-static-md`) for zlib, libpng, freetype, bzip2, brotli, fmt, curl and OpenAL Soft. OpenAL Soft is LGPL: static linking is fine because the full source stays public, but update the notice text.
2. A CMake option that links `hyperion-core` and `hyperion-net` statically into the shared `hyperion.dll` for the editor build. Today static mode (`HYP_BUILD_STATIC`) is all-or-nothing and only for shipping/iOS.
3. Distribution build with `STRATA_ENABLE_LLVM=OFF` and `STRATA_SHARED=OFF`, and make sure the engine builds and the editor starts with Strata scripting reduced or off.
4. Publish `Hyperion.Editor` as a single file (`PublishSingleFile`, framework-dependent, natives left beside the exe) and sign the bundle after publishing. Verify the editor still loads Hyperion.NET.* from the bundle.
5. Replace the dedicated-server cache process: either run the cache server through the editor exe (`--exec`) or leave that feature out of the preview.
6. Version metadata on the two files only (see step 1 below).
7. Re-run the sandbox probe on the slim build. The expected result is that only our two unsigned files are blocked.

Known limits: C# scripts compile to unsigned assemblies, so they are blocked on machines with Smart App Control on, and C# scripting needs the .NET SDK anyway. Referencing `Hyperion.NET.*` from user script projects needs those assemblies on disk, which the single-file bundle removes, so C# scripting is out of scope for this build.

## Plan

### 0. Measure (no accounts needed)

```
powershell -ExecutionPolicy Bypass -File Tools/Scripts/Sandbox/TestEditorInSandbox.ps1 -ProbeDlls
```

Writes `dll_probe.txt`: which DLLs Smart App Control blocks when loaded. Decides how much of the third-party set needs handling.

Result (2026-10-05, run offline and again with `-Network`, identical both times): all 57 unsigned top-level DLLs failed to load (our 8, 10 native and 36 managed third-party plus 3 not logged), all 15 signed ones loaded. Reputation did not rescue any unsigned file, including the official Avalonia and Dock assemblies, so assume every binary the editor loads must be signed. The probe does not cover `runtimes/win-x64/native/*.dll`, and `-Network` connectivity was not verified in the guest log at that point (the config said Default). Decision: the target ship set above, so no SignPath question about third-party binaries is needed.

### 1. Make the repo eligible

1. Version metadata on the two signed files. Add a VERSIONINFO resource to `hyperion.dll` (generated from `HYP_VERSION_*` in `Source/CMakeLists.txt`) and set `Product`, `Company`, `Version` and `IncludeSourceRevisionInInformationalVersion=false` in the Hyperion.Editor project. Both: ProductName `Hyperion Engine`, ProductVersion `0.5.1`.
2. Add the "Code signing policy" section to README.md (draft below). Name the people for each role. Turn on MFA and required PR review for `dev`.
3. Distribution build (`BuildHyperion.bat Release distribution regenerate`) so Aftermath and the source path are gone.
4. Resolve the open items in THIRD_PARTY_NOTICES.md, especially the mannequin, grass and terrain assets.
5. Publish an unsigned pre-release first: SignPath requires an already released project. State the Smart App Control limitation in the release notes.
6. Hosted CI. Everything up to signing runs on GitHub-hosted Windows runners. Unverified: runner disk (the dev build tree is 27 GB), a Visual Studio version that matches `BuildHyperion.bat`, LLVM and vcpkg setup, build time.

### 2. Apply and onboard

1. Apply at signpath.org/apply.
2. After approval, in SignPath: create the project, paste `.signpath/artifact-configuration.xml`, create policies `test-signing` and `release-signing` (release needs manual approval), add the predefined "GitHub.com" trusted build system, install the SignPath GitHub App, create a submitter API token.
3. In GitHub: secret `SIGNPATH_API_TOKEN`, variable with the organization id.

### 3. Release workflow (outline, not committed)

```yaml
on:
  push:
    tags: ['v*']
jobs:
  build:      # windows, GitHub-hosted: build, PackageEditorWindows.ps1, smoke checks
    steps:
      - ...build and package (-NoZip off) ...
      - id: unsigned
        uses: actions/upload-artifact@v7
        with:
          name: unsigned
          path: PackagedBuilds/EditorPreview/Hyperion-Editor-${{ env.VERSION }}-win64.zip
  sign:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - uses: signpath/github-action-submit-signing-request@v3
        with:
          api-token: ${{ secrets.SIGNPATH_API_TOKEN }}
          organization-id: ${{ vars.SIGNPATH_ORGANIZATION_ID }}
          project-slug: hyperion-engine
          signing-policy-slug: release-signing
          artifact-configuration-slug: editor-preview
          github-artifact-id: ${{ needs.build.outputs.unsigned-artifact-id }}
          parameters: |
            version: ${{ env.VERSION }}
          wait-for-completion: true
          output-artifact-directory: signed
  release:    # verify every file in .signpath config is signed, create a DRAFT release with the zip, SHA256SUMS, symbols
```

Parameter syntax and the artifact id wiring need checking against the action's docs when we build it.

### 4. Verify

1. Re-run the sandbox test on the signed zip with Smart App Control on. This is the real acceptance test.
2. Confirm all 16 files in `.signpath/artifact-configuration.xml` verify with `Get-AuthenticodeSignature` and are timestamped.
3. Watch SmartScreen behavior on a clean machine.

## Draft README section (paste after approval)

> ## Code signing policy
>
> Free code signing provided by [SignPath.io](https://signpath.io), certificate by [SignPath Foundation](https://signpath.org).
>
> - Authors (commit access): ...
> - Reviewers (approve external contributions): ...
> - Approvers (authorize releases for signing): ...
>
> Privacy: this program will not transfer any information to other networked systems unless specifically requested by the user or the person installing or operating it.

Check that statement against the editor before publishing it: package restore during C# script builds and any update checks count.
