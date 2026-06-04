# Debug Render Harness Report Template

This template is the text-first report shape for smoke captures and failure bundles.

Keep it short, copyable, and consistent across particle / video / blend / overlay scenes.

---

## Header

- reportId: `<bundle-id>`
- createdAt: `<timestamp>`
- preset: `<preset-name>`
- frame: `<frame-number>`
- composition: `<composition-name>`
- selectedLayer: `<selected-layer-name>`
- renderBackend: `<backend>`

---

## Goal

- goal: `<what we are trying to prove>`
- expected: `<what should happen>`
- actual: `<what happened>`
- nextAction: `<what to try next>`

---

## Summary

- status: `<ok | skipped | failed>`
- shortReason: `<single-line summary>`
- viewport: `<width>x<height>`
- rtvState: `<ready | missing | invalid>`

---

## Media States

### Particle

- particleState: `<ok | skipped | failed | ...>`
- particleDetail: `<state=... or note>`
- particleCount: `<count>`
- drawState: `<drawn | skipped | failed>`
- skippedReason: `<reason>`
- blendMode: `<mode>`
- matrixMode: `<2d | 3d | unknown>`

### Video

- openState: `<opening | loaded | failed>`
- decodeState: `<pending | ready | failed | none>`
- targetFrame: `<frame>`
- sourceFrame: `<frame>`
- hasFrameBuffer: `<yes | no>`
- fallbackState: `<sync-ok | sync-miss | async-pending | async-failed>`
- lastError: `<text>`

### Blend

- inputCount: `<count>`
- opacity: `<value>`
- blendMode: `<mode>`
- outputState: `<visible | transparent | skipped>`
- blendMaskContract: `<phase=blend-mask-smoke-v1 ...>`
- maskContract: `<none | pending | resolved | empty | failed>`
- dispatch: `<count>`
- retryNormal: `<count>`
- directFallback: `<count>`

### Overlay

- grid: `<on | off>`
- anchor: `<on | off>`
- guidance: `<on | off>`

---

## Diagnostics

- traceFrames: `<count>`
- cacheHealth: `<text>`
- viewportNotes: `<text>`
- resourceNotes: `<text>`
- skippedReasons: `<text>`

---

## Notes

- `skipped` means the harness had an explicit reason not to draw.
- `failed` means a draw or decode action actively failed.
- `particleDetail` should carry the shortest useful note for the particle bucket.
- If the output is transparent, state why the output is still considered valid or invalid.
