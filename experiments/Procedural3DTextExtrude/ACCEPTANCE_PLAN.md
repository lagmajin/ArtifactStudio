# Procedural 3D Text Extrusion Acceptance Plan

**最終更新:** 2026-11-21
**Status:** In Progress — implementation contracts added; no build, test, or image capture has been run under the workspace execution rules.

## Acceptance goals

1. Detect missing shaped glyphs and report their UTF-16 source offsets and Unicode values, along with each resolved fallback family. Record SHA-256 fingerprints for resolved font files; persist the snapshot in layer JSON and report changed or unverifiable identities in the existing font property help.
2. Reject empty outlines, invalid dimensions, over-budget glyph counts, path/contour complexity, and estimated source/render output sizes, malformed render attributes, non-finite values, invalid indices, and zero-area triangles without returning a usable mesh.
3. Verify closed geometry for counter glyphs (`O`, `B`, `8`, `&`) and nested contours; verify consistent normals and stable output for identical inputs.
4. Verify Arabic joining/marks, Hebrew niqqud + mixed bidi + isolates, Devanagari/Bengali/Tamil/Gujarati conjuncts, Japanese combining marks, Hangul Jamo, and CJK using explicitly recorded fonts.
5. Compare a front view and a three-quarter view against reviewed baselines; inspect contours, counters, side walls, bevel continuity, RTL order, and fallback warnings.

## Capture matrix and manifest

- Use one explicitly loaded, license-reviewed font file per writing-system fixture. Record its source path, license, SHA-256, Qt-resolved family, OS, Qt version, generator settings, input text, capture date, and source commit in the manifest.
- Include a Latin font with counter glyphs, one complex-script font per fixture group, and a deliberate unsupported-glyph fixture.
- Save only individually reviewed reference images under this lab's `captures/` folder. Keep raw OBJ, intermediate images, and sweep outputs ignored; do not commit bulk captures.
- Capture front orthographic and three-quarter views at fixed camera, lighting, material, resolution, and color management. Use geometry assertions for exact mesh invariants and image review for shaping and appearance; do not treat a non-empty mesh as proof of font coverage.
- The headless OBJ capture hook writes `<capture.obj>.json` with exact input/settings, actual shaped-run families, Qt/OS, UTC capture time, artifact SHA-256, and any explicitly supplied font file/license/source commit metadata. It hashes both the font file and supplied license document. Set `ARTIFACT_TEXT_EXTRUDE_CAPTURE_FONT_PATH`, `ARTIFACT_TEXT_EXTRUDE_CAPTURE_FONT_LICENSE`, `ARTIFACT_TEXT_EXTRUDE_CAPTURE_FONT_LICENSE_PATH`, and `ARTIFACT_TEXT_EXTRUDE_CAPTURE_SOURCE_COMMIT` to complete those provenance fields. Missing provenance is listed as `unrecorded` and in `unrecordedFields`; this sidecar is geometry evidence, not a reviewed image baseline.

## Headless test and geometry capture

The test target is `ArtifactProcedural3DTextExtrudeContractTest`, registered in the root `x64-Debug-Agent` preset and marked `QT_QPA_PLATFORM=offscreen`. Complex-script tests skip with family diagnostics on developer hosts that lack coverage. Set `ARTIFACT_TEXT_EXTRUDE_REQUIRE_SCRIPT_COVERAGE=1` for acceptance/CI runs so any requested family substitution or missing glyph fails instead of skipping. After checking that `out/build/x64-Debug-Agent/CMakeCache.txt` matches the intended source, generator, compiler, and configuration, the permitted build/test sequence is:

```powershell
cmake --preset x64-Debug-Agent -DARTIFACT_BUILD_TESTS=ON
cmake --build --preset x64-Debug-Agent --target ArtifactProcedural3DTextExtrudeContractTest
$env:ARTIFACT_TEXT_EXTRUDE_REQUIRE_SCRIPT_COVERAGE = "1"
ctest --test-dir out/build/x64-Debug-Agent -C Debug `
  -R '^ArtifactProcedural3DTextExtrudeContractTest$' --output-on-failure
Remove-Item Env:ARTIFACT_TEXT_EXTRUDE_REQUIRE_SCRIPT_COVERAGE
```

For a raw geometry capture, create a destination under this lab's ignored `captures/` directory, then set `ARTIFACT_TEXT_EXTRUDE_CAPTURE_OBJ`, `ARTIFACT_TEXT_EXTRUDE_CAPTURE_PROFILE`, and the font provenance variables above before running the same CTest selection. Supported profile names are declared in `Procedural3DTextExtrudeContractTest.cpp`; unknown names fail rather than silently capturing the default sample. The test refuses to write a capture when shaping reports missing glyphs. It writes an OBJ plus JSON sidecar; it does not launch Artifact or render a camera image. Do not add raw geometry to `manifest.json`; only reviewed image baselines belong there.

PowerShell setup template (replace the font and license values with a reviewed fixture):

```powershell
New-Item -ItemType Directory -Force experiments/Procedural3DTextExtrude/captures/raw | Out-Null
$env:ARTIFACT_TEXT_EXTRUDE_CAPTURE_OBJ = "experiments/Procedural3DTextExtrude/captures/raw/hebrew-niqqud.obj"
$env:ARTIFACT_TEXT_EXTRUDE_CAPTURE_PROFILE = "hebrew-niqqud"
$env:ARTIFACT_TEXT_EXTRUDE_CAPTURE_FONT_PATH = "<absolute-path-to-reviewed-font-file>"
$env:ARTIFACT_TEXT_EXTRUDE_CAPTURE_FONT_LICENSE = "<SPDX-license-id-or-proprietary-license-name>"
$env:ARTIFACT_TEXT_EXTRUDE_CAPTURE_FONT_LICENSE_PATH = "<absolute-path-to-license-text>"
$env:ARTIFACT_TEXT_EXTRUDE_CAPTURE_SOURCE_COMMIT = "<tested-source-commit>"
ctest --test-dir out/build/x64-Debug-Agent -C Debug `
  -R '^ArtifactProcedural3DTextExtrudeContractTest$' --output-on-failure
```

## Current evidence and outstanding work

- Added result status, missing-glyph count, resolved families, font file SHA-256 snapshots, settings persistence, malformed mesh checks, a glyph budget, and closed-edge contracts for `O`, `B`, `8`, and `&` counter/compound glyphs plus a beveled `O`, using the bundled Roboto fixture.
- Font-family and fingerprint snapshots are now lexically normalized on generation and load, so Bidi/fallback run-order changes do not report a changed font set; a JSON order-normalization contract covers this persistence behavior.
- Added a legacy JSON contract that removes every newly introduced text layout and diagnostic key, reloads the project, and checks documented defaults.
- Bounded generated and imported diagnostics: at most 64 font family/fingerprint entries, 256/512 characters per entry, 128 missing-glyph locations of at most 128 characters, and a 16,384 missing-glyph count. Truncation downgrades identity/location completeness instead of claiming a valid comparison.
- Added generation diagnostics to the existing font property help and layer debug state, plus persistent JSON metadata. Font mismatch comparison occurs after the first regeneration following load.
- Added pre-simplification path-element, flattened contour-count, and total flattened-point budgets so pathological fonts cannot pass the glyph-count check and then trigger unbounded contour processing.
- Added a conservative output-size preflight based on flattened contour points and effective bevel segments, rejecting estimated extrusion outputs above one million source vertices/faces or one million expanded render vertices before entering ShapeExtrude.
- The shaping budget counts LF, CRLF, CR, Unicode line/paragraph separators, and NEL as line breaks, and rejects text that would exceed 4,096 layout lines before constructing QTextLayout lines.
- Complex-script geometry tests now require the requested font to appear in an actual shaped run and inspect missing-glyph diagnostics. They explicitly skip with requested/resolved family details when the host substitutes another family or cannot fully cover a sample; non-empty fallback meshes no longer count as proof of requested-font shaping coverage. Covered meshes also check finite UVs, unit normals, in-range indices, non-degenerate triangles, exactly two incident triangles per welded edge, and opposite winding across each shared edge.
- Added quality scaling to bevel tessellation and made text geometry cache across frames when revision and LOD quality are unchanged.
- Added left/center/right paragraph alignment, letter tracking, line-spacing multiplier, JSON/property persistence, and source-indexed missing-glyph diagnostics.
- Not yet verified: any compilation or test execution; screenshot baselines; specific glyph-to-source-character mapping; passing watertight-edge assertions and self-intersection acceptance for the multilingual matrix; end-user display timing of diagnostics; behavior when font paths are unavailable. These remain release gates.
