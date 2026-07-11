# Tracking App Bridge Design

## Responsibility boundary

`ArtifactCore::MotionTracker` owns frame sampling, solving, confidence, failure
diagnostics, corrections, and serialization. The App layer owns tracker
lifecycle, project/layer association, job presentation, undoable bake, and
editor selection. Core must never mutate a layer or property directly.

## TrackingService contract

The service keeps a tracker session keyed by project/document identity and
exposes commands for:

- create/remove tracker sessions;
- submit a range or camera solve job;
- cancel and observe partial results;
- request problem-frame and confidence summaries;
- apply an explicit correction to the Core result;
- bake a reviewed result into a property or camera layer.

Job callbacks are owned by the service and are translated to the existing App
task/progress mechanism. No new global signal path is required. A cancelled
job remains inspectable but cannot be baked until the user explicitly resumes
or accepts the partial result.

## Review and bake boundary

The editor reads immutable snapshots from Core. Review actions identify a frame
and point ID, then call the correction API. Bake is a separate command with a
captured result revision and target property path. If the revision changes
while the command is pending, the bake is rejected and must be retried from a
fresh snapshot.

## Camera solve flow

1. App collects calibrated intrinsics and frame correspondences.
2. `CameraSolveJob` runs in Core and retains partial `CameraPoseStream` output.
3. App displays valid/failed counts, average reprojection error, and diagnostics.
4. User reviews or corrects source correspondences.
5. App requests an explicit bake into the camera layer only after acceptance.

## Invariants

- Core results are immutable across the App boundary except through explicit
  correction methods.
- A cancelled or failed solve is never silently converted into keyframes.
- Every bake is undoable and carries the source result revision.
- UI selection and overlays do not become part of the Core data model.

## Acceptance checklist

- [ ] A range job can be cancelled without losing its partial Core snapshot.
- [ ] Failed/problem frames are visible before any bake command is enabled.
- [ ] Corrections target a stable point ID and source time.
- [ ] Bake rejects stale result revisions and creates one undoable command.
- [ ] Camera pose export preserves diagnostics and calibration settings.
