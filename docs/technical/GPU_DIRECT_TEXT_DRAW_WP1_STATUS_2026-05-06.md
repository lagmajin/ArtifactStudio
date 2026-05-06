# GPU Direct Text Draw WP-1 Status - 2026-05-06

WP-1 for GPU direct text draw is effectively in place: the CPU-side glyph atlas exists, is used by the 2D primitive renderer, and already provides the basic cache / upload boundary needed by later GPU text work.

## Implemented pieces

- `ArtifactCore/include/Text/GlyphAtlas.ixx`
- `ArtifactCore/src/Text/GlyphAtlas.cppm`
- `Artifact/src/Render/PrimitiveRenderer2D.cppm`
- `Artifact/src/Render/DiligentImmediateSubmitter.cppm`

## What WP-1 provides

- CPU rasterization of glyph bitmaps via `QRawFont`
- Atlas packing and cache reuse
- UV rectangle metadata for later GPU sampling
- Dirty tracking for texture upload

## Why this matters

- The later WP-2+ text path can build on a stable atlas boundary instead of re-deriving glyph rasterization.
- Text layout and primitive rendering already have a shared entry point for glyph acquisition.
- The remaining work is now mostly about draw-path parity and backend upload policy, not atlas creation.

## Practical next steps

- keep the atlas boundary explicit and non-implicit
- feed the atlas through the GPU text path without reintroducing `QImage` as the hot-path payload
- align later text draw work with the existing primitive renderer contract

