# Billboard Render Path Note

Observed during investigation on 2026-06-25:

- Billboard rendering can disappear even when the particle list is non-empty.
- The immediate submit path for billboards checks `ctx` / `pRTV`, but the actual draw is routed through `PrimitiveRenderer3D`.
- `PrimitiveRenderer3D::drawBillboard()` depends on its own `currentRTV()` / `hasRenderTarget()` state, so a mismatch between the submit-time RTV and the renderer's internal RTV can cause an early return.
- The most likely failure mode is an RTV/context routing mismatch, not particle generation itself.

Follow-up ideas:

- Log the active RTV path for billboard draws.
- Compare the submit-time `pRTV` with `PrimitiveRenderer3D::currentRTV()` in the failing frame.
- Consider making the billboard submit path draw against the provided render target directly.
