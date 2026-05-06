# Critical Render / Media Smoke Checklist

**Date:** 2026-04-30  
**Related Milestone:** [`../planned/MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md`](../planned/MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md)

---

## Particle Visibility Smoke

Goal: distinguish "no particles emitted" from "particles emitted but not drawn".

Steps:

1. Create or open a small composition.
2. Add a particle layer.
3. Scrub to frame 10.
4. Scrub to frame 30.
5. Try a dark background.
6. Try a light background.
7. Confirm whether particles are visible.

Record:

| Field | Value |
|---|---|
| Composition | |
| Frame | |
| Background | Dark / Light |
| Alive count log | |
| Draw call log | |
| RTV warning | |
| PSO/SRB warning | |
| Visible output | Yes / No |

Expected useful logs:

- `[ParticleLayer]`
- `[ParticleRenderer] Drawing`
- `[ParticleRenderer] No active RTV`
- `[ParticleRenderer] PSO creation FAILED`
- `[ParticleRenderer] prepare() called but PSO/SRB is null`

---

## Video Decode Smoke

Goal: distinguish decode failure from layer visibility or blend failure.

Steps:

1. Import the preferred short MP4 fixture from [`CRITICAL_RENDER_MEDIA_SMOKE_FIXTURE_2026-04-30.md`](./CRITICAL_RENDER_MEDIA_SMOKE_FIXTURE_2026-04-30.md).
2. Add it as a video layer.
3. Verify frame 0.
4. Seek to the middle frame.
5. Seek near the final frame.
6. Confirm the composition draw path receives a frame buffer.
7. Confirm the layer is visible and not transparent.

Record:

| Field | Value |
|---|---|
| File | |
| Codec | |
| Duration | |
| Frame tested | |
| Open result | |
| First frame decode | |
| Async pending | |
| Sync fallback result | |
| Has frame buffer | |
| QImage fallback null | |
| Visible output | Yes / No |

Expected useful logs:

- `[VideoLayer] loadFromPath`
- `[VideoLayer] initial decode FAILED`
- `[VideoLayerT] decodeCurrentFrame starting bg decode`
- `[VideoLayer] async decode null frame`
- `[VideoLayer] decodeFrameToQImage failed`
- `[VideoLayerT] drawLayerForCompositionView`
- `[MediaPlayback] direct decode failed`
- `[FFmpegBackend]`
- `[MFBackend]`

---

## Pass / Fail Rule

Pass:

- particle smoke shows alive particles and visible output
- video smoke shows decoded frame and visible output after seek

Fail:

- any silent null frame
- any silent draw skip
- particle alive count > 0 with no visible output and no diagnostic reason
- video layer loaded with no visible output and no diagnostic reason
