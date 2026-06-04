# Critical Render / Media Smoke Checklist

This checklist is the short manual gate for the `M-CE-CRIT-1` stability program.
Use it together with `DebugRenderHarnessWidget` and `AppDebuggerWidget`.

## Preconditions

- Open the current composition or the debug harness surface.
- Make sure a `FrameDebugSnapshot` is available.
- Keep the current report bundle visible or saved for notes.

## Particle Smoke

1. Select the `particle-only` preset in `DebugRenderHarnessWidget`.
2. Verify the report summary shows a non-empty `particleState`.
3. Verify the App Debugger export text also shows `particleState`.
4. Verify the particle fixture is visible over both dark and light backgrounds.
5. If it fails, record whether the report points to `no RTV`, `PSO null`, `empty particle`, or `blend invisible`.

## Video Smoke

1. Select the `video-only` preset in `DebugRenderHarnessWidget`.
2. Verify the report summary shows a non-empty `videoState`.
3. Verify the App Debugger export text also shows `videoState`.
4. Verify frame 0 and a middle frame are both visible after decode.
5. If it fails, record whether the report says `open failed`, `decode failed`, `frame not ready`, or `transparent output`.

## Blend Smoke

1. Select the blend-focused preset or the current composition blend/mask contract view.
2. Verify the report summary includes `blendState` and `glyphState`.
3. Verify the App Debugger export text includes `blendState`, `glyphState`, and `blendMaskContract`.
4. Verify the report includes the blend / mask contract resource and `blendMaskContract`.
5. Verify the output is not a uniform fill when the expected blend result should preserve detail.
6. If it fails, record the blend mode, opacity, and texture-format notes.

## Pass Criteria

- `particleState`, `particleDetail`, `textState`, `videoState`, `blendState`, `glyphState`, `blendMaskContract`, `cacheHealth`, `resourceNotes`, and `skippedReasons` are all present in the saved bundle.
- The App Debugger export text mirrors the same `particleState`, `particleDetail`, `textState`, `videoState`, `blendState`, `blendMaskContract`, and `glyphState` headings.
- `failureReason` is either empty or explicit enough to identify the bucket.
- The saved bundle includes `shortReason` and `blendMaskContract` alongside `resourceNotes` and `skippedReasons` when present.
- Particle, video, and blend outcomes can be repeated without guessing which subsystem failed.

## Notes

- Keep this checklist aligned with `docs/technical/DEBUG_RENDER_HARNESS_SMOKE_CHECKLIST_2026-04-30.md`.
- If a new failure bucket appears, add it to the bug report before expanding the harness surface.
