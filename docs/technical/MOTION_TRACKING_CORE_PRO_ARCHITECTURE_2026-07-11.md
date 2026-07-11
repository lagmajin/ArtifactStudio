# Motion Tracking Core — Professional Architecture

Date: 2026-07-11  
Status: In Progress

## Scope

`Tracking.MotionTracker` is the compatibility facade. New production paths are divided into four Core responsibilities:

1. **Frame source boundary** — tracker consumes immutable, explicit CPU/GPU-compatible frame snapshots. `QImage` remains a compatibility adapter only and must not enter a render hot path.
2. **Solve pipeline** — point tracking, planar homography, and camera solve produce one normalized result schema with per-frame confidence, reprojection error, failure reason, and correction provenance.
3. **Job control** — a cancellable background job owns progress and partial result checkpoints; UI never performs solving work.
4. **Application bridge** — App code explicitly bakes reviewed results to properties, masks, stabilization, or camera layers. Core never mutates a layer directly.

## Result Contract

- Times are source times in seconds; callers retain the mapping to composition time.
- Each `TrackFrame` is monotonic and contains only stable point IDs.
- Homographies map reference-frame coordinates to the current frame.
- Camera solve is valid only when calibrated intrinsics, sufficient parallax, and an accepted reprojection threshold are present.
- Failed or rejected frames remain in the session with diagnostics; they are not silently interpolated into a bake.
- Manual corrections are retained separately from raw solver measurements so a re-solve does not destroy artist work.

## Delivery Sequence

1. Stabilize `MotionTracker` serialization, geometry extraction, and confidence handling.
2. Introduce native frame snapshots and a cancellable solve-job interface.
3. Replace the legacy point and planar algorithms with forward/backward validation and RANSAC validation.
4. Add calibrated camera solve, lens model, pose stream, and reprojection diagnostics.
5. Add the explicit App-side review/bake bridge, then editor and overlay surfaces.

## Implemented Core Slice

- Calibrated `solveCameraPose()` with RANSAC and reprojection diagnostics.
- `CameraPoseStream` for per-sample pose/error/diagnostic retention.
- JSON save/restore with finite-value validation and time normalization.
- `CameraSolveJob` with worker-thread execution, progress callback, cancellation, and partial result retention.
- Pose-stream quality summary and deterministic interpolation between neighboring valid calibrated poses.
- Pose-stream normalization now drops non-finite timestamps and invalidates non-finite pose payloads before deduplication.
- Directly-created pose streams also normalize intrinsics, distortion, confidence, and RANSAC bounds before export.
- Explicit `cv::Mat` frame input with `QImage` kept only as a compatibility adapter.
- Point-flow confidence, active state, velocity, aggregate frame confidence, and failure-frame recording.
- Planar homography application to tracked points with ECC-derived confidence.
- Method-aware point tracking: pyramidal LK for OpticalFlow/FeatureBased and NCC for template paths.

## Non-Goals for Core

- No direct layer/property mutation.
- No Qt painting/compositing implementation.
- No implicit CPU download or GPU upload.
- No hidden conversion from a UI image to a solve input.
