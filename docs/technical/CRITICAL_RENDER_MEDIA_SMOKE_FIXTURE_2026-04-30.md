# Critical Render / Media Smoke Fixture

This note defines the preferred short video fixture for the critical render / media stability smoke cases.

## Preferred Fixture Path

- `Artifact/App/comp1.mov`

The debug harness already looks for this file in the app directory and nearby fallback paths.

## Fixture Requirements

- short clip, about 1 to 2 seconds
- deterministic first frame
- a middle frame that is visibly different from frame 0
- easy to decode on the supported backends
- no audio requirement for the smoke case

## Suggested Generation Command

If no checked-in fixture is available, generate one with `ffmpeg`:

```bash
ffmpeg -y \
  -f lavfi -i testsrc2=size=1280x720:rate=30 \
  -f lavfi -i sine=frequency=440:sample_rate=48000 \
  -t 2 \
  -c:v libx264 -pix_fmt yuv420p -preset veryfast -crf 20 \
  -c:a aac -b:a 128k \
  Artifact/App/comp1.mov
```

## Smoke Use

1. Put the generated file at the preferred path.
2. Open the video-only or mixed-media preset in `DebugRenderHarnessWidget`.
3. Verify frame 0, a middle frame, and the final frame all decode.
4. Use the report bundle to record the file name, open result, first-frame result, and fallback state.

## Notes

- If `comp1.mov` is absent, the harness falls back to other nearby search paths.
- Keep this fixture small and deterministic so it is safe to reuse across smoke runs.
