# Layer Property Envelope Architecture

**Status:** Proposed
**Date:** 2026-07-24
**Related:** `docs/planned/MILESTONE_LAYER_EFFECT_ENVELOPE_2026-06-19.md`, `docs/planned/MILESTONE_QUICK_LAYER_CREATION_DIALOG_2026-07-10.md`

## Decision

`LayerEffectEnvelope` will evolve from a layer-wide `effectStrength` scalar into
a property-targeted envelope system. An envelope drives existing animatable
properties; it does not introduce a new effect type and effect implementations
must not interpret a global strength value themselves.

## Motivation

Opacity, blur radius, glow intensity, exposure and transform values do not
share a universal definition of “strength”. A global scalar makes every effect
responsible for its own conversion and cannot express additive, multiplicative
or absolute value changes consistently.

The current `EffectContext::effectStrength` propagation is therefore a
transitional compatibility path, not the long-term evaluation contract.

## Data Model

One `LayerPropertyEnvelope` owns timing and a list of targets.

```text
LayerPropertyEnvelope
  enabled, entry, exit
  durationFrames, curve, timing, delayFrames
  targets[]

LayerPropertyEnvelopeTarget
  propertyPath
  operation: Replace | Add | Multiply
  entryStart, entryEnd
  exitStart, exitEnd
```

`propertyPath` uses the existing layer/effect property-path vocabulary:

- `layer.opacity`
- `effect.<effect-id>.<property-name>`
- future layer-owned animatable paths, such as transform or mask properties

Target values remain normal property values. The envelope supplies a sampled
value and combines it with the property evaluation result using `operation`.

## Evaluation Contract

1. Resolve the property’s authored value and ordinary keyframes.
2. Resolve active entry and/or exit envelope samples for the layer timeline.
3. Combine the sample in declared target order.
4. Clamp or validate only through the target property’s existing validation
   contract.

Exit evaluation is time-reversed relative to entry. This must be performed by
the shared envelope evaluator, rather than by individual render paths.

An absent target, unknown path or incompatible value type is skipped and
reported through the existing diagnostics path. It must never silently change
another property.

## Ownership Boundaries

- `ArtifactAbstractLayer` owns envelope data and serialization.
- The existing property resolver owns target lookup and final typed value
  evaluation.
- Effect classes expose their normal animatable properties only; they do not
  receive envelope-specific logic.
- Render and preview paths consume already-evaluated property values. They do
  not calculate envelope timing or read a global effect-strength scalar.
- Quick Layer Creation creates presets by writing ordinary envelope targets;
  it does not own evaluation logic.

## Migration

1. Add the generic value types and evaluator alongside `LayerEffectEnvelope`.
2. Migrate `layer.opacity` to one target, preserving existing project JSON.
3. Add an adapter that reads legacy `effectStart/effectEnd` data and produces a
   temporary target only when a concrete target is supplied.
4. Remove `EffectContext::effectStrength` reads after the first effect-property
   target (recommended initial target: Blur radius) is verified.
5. Upgrade Quick Layer presets to create `layer.opacity` and an explicitly
   selected effect-property target.

## Constraints

- No new dedicated Envelope effect.
- No global signal/slot additions.
- No implicit property-path fallback or string-name matching.
- The entire Quick Layer creation transaction, including its envelope targets,
  remains one undo operation.

## Acceptance Criteria

- A single evaluator produces equivalent results in preview and final render.
- A preset can independently drive opacity and one effect property with
  simultaneous, opacity-leading and effect-leading timing.
- Unknown targets fail diagnostically without modifying layer data.
- Saving and reopening preserves target paths, operations and timing.
