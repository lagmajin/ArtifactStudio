# Composition Background Confusion Note

## Summary

`Artifact/src/Composition/ArtifactAbstractComposition.cppm` line 57 contains only the default initializer:

```cpp
FloatColor backgroundColor_ = { 0.1f, 0.1f, 0.1f, 1.0f };
```

This is not a hardcoded final value. It is overwritten by:

- the `ArtifactAbstractComposition` constructor via `params.backgroundColor()`
- runtime changes via `setBackGroundColor(...)`
- JSON save/load paths

## Why it still looks fixed

The visible background is not always the same surface that reads `backgroundColor_`.
There are separate paths for:

- editor/theme background
- viewport clear color
- composition background fill

So even if `backgroundColor_` is correctly overwritten, the user may still be looking at a different background surface and conclude that the value is "stuck".

## Follow-up

- Confirm which surface the user means when they say "background"
- If needed, make the editor viewport background and the composition background contrast more clearly
- Keep the existing fill-not-visible report as the primary bug report:
  - [docs/bugs/COMPOSITION_FILL_NOT_VISIBLE_2026-04-05.md](./COMPOSITION_FILL_NOT_VISIBLE_2026-04-05.md)

