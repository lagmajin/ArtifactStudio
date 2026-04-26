# Milestone Priority & Roadmap - 2026-04-27

## Executive Summary

Analyzed 65+ milestones across entire codebase.
**Result**: Identified 9 high-priority candidates for next phase work.

Ranked by:
- **Feasibility** (time estimate + dependencies)
- **Risk** (likelihood of blockers)
- **Strategic Value** (forward momentum)

---

## Priority Tier 1 (Start Next)

### 🥇 #1: Cloud AI Phase 6 - Export API

**Priority**: 90/100  
**Scope**: ~15 hours  
**Risk**: Low  
**Dependencies**: Cloud AI Phases 1-5 ✅ complete

**What it is**: Export composition/layers to formats (MP4, PNG sequence, etc.) via AI tool API.

**Why it matters**:
- Builds directly on existing Phase 1-5 infrastructure
- Clear contract already established
- Enables content creation workflows
- Complements keyframe/group layers work

**Estimated phases**:
1. Export format registry (2h)
2. MP4 export wrapper (3h)
3. PNG/JPG sequence wrapper (3h)
4. Documentation + test (3h)
5. AI method registration (2h)
6. Verification (2h)

**Implementation path**: WorkspaceAutomation.ixx → export methods

---

### 🥈 #2: Cloud AI Phase 7 - Timeline Operations

**Priority**: 85/100  
**Scope**: ~12 hours  
**Risk**: Low  
**Dependencies**: Cloud AI Phases 1-5 ✅ complete

**What it is**: Provide AI access to timeline operations (seek, play, scrub, trim, etc.).

**Why it matters**:
- Direct continuation of keyframe work (Phase 4)
- Unlocks timeline-aware AI workflows
- Low technical risk (mostly delegating to existing services)

**Estimated phases**:
1. PlaybackService API audit (2h)
2. Playhead/seek methods (2h)
3. Workarea/trim methods (2h)
4. Timeline marker methods (2h)
5. Documentation (2h)
6. AI registration (2h)

**Implementation path**: WorkspaceAutomation.ixx → timeline methods

---

## Priority Tier 2 (High Value)

### 🥉 #3: GPU Direct Text Draw - WP-1 (GlyphAtlas)

**Priority**: 75/100  
**Scope**: ~8 hours  
**Risk**: Medium  
**Dependencies**: TextStyle ✅, GlyphLayout ✅, TextLayoutEngine ✅ (all exist)

**What it is**: Create GPU glyph atlas texture for hardware text rendering.

**Why it matters**:
- Unblocks entire GPU text rendering path (WP-2 through WP-6)
- Isolated work; doesn't conflict with other milestones
- Improves text rendering performance significantly
- Self-contained deliverable (atlas can be used immediately)

**Estimated phases**:
1. GlyphAtlas class design (1h)
2. CPU glyph rasterization setup (2h)
3. GPU atlas texture allocation (2h)
4. UV mapping & caching (2h)
5. Test & validation (1h)

**Implementation path**: ArtifactCore/src/Text/GlyphAtlas.cppm

---

## Priority Tier 3 (Framework/Polish)

### #4: Composition Editor Implementation Rules

**Priority**: 70/100  
**Scope**: ~10 hours  
**Risk**: Very Low  
**Dependencies**: None (documentation only)

**What it is**: Formalize and document the composition editor's rendering contract, state machine, and extension points.

**Why it matters**:
- Prevents architectural drift
- Makes future work more maintainable
- Clarifies responsibilities (editor vs renderer vs viewport)
- No code risk; improves code clarity

**Estimated phases**:
1. State machine documentation (2h)
2. Render pass contract (2h)
3. Event flow documentation (2h)
4. Extension point guide (2h)
5. Review & polish (2h)

**Output**: docs/COMPOSITION_EDITOR_CONTRACT.md

---

## Priority Tier 4 (Stabilization)

### #5: 3D Gizmo Stabilization

**Priority**: 65/100  
**Scope**: ~20 hours  
**Risk**: Medium  
**Dependencies**: GizmoRenderer ✅ exists (mainly bug fixes)

**What it is**: Fix remaining 3D gizmo issues (rotation snapping, scale constraints, transform feedback).

**Why it matters**:
- Improves user experience (polishes existing feature)
- Not new scope; reduces user friction
- Straightforward iteration (no architectural risk)

**Estimated scope**: Bug fixes + edge case handling

---

### #6: Particle Render Path Stabilization

**Priority**: 60/100  
**Scope**: ~25 hours  
**Risk**: Medium  
**Dependencies**: ParticleRenderer ✅ exists (mainly diagnostics + contract clarification)

**What it is**: Fix particle layer rendering visibility issues via clearer entry contracts and RTV state management.

**Why it matters**:
- Fixes longstanding bug (particles sometimes invisible)
- Documented root cause in bugs/
- Improves editor reliability

**Estimated phases**:
1. Draw entry audit (3h)
2. RTV/matrix state fixes (5h)
3. PSO/SRB initialization (4h)
4. Diagnostics (5h)
5. Testing & validation (3h)

---

## Priority Tier 5 (Future/Blocked)

### #7: GPU Direct Text Draw - Full Path (WP-2-6)

**Priority**: 55/100  
**Scope**: ~35 hours  
**Risk**: Medium  
**Dependencies**: WP-1 (GlyphAtlas) must complete first

**What it is**: Complete the GPU text rendering path after WP-1 atlas is done.

**Why it matters**:
- High performance gain for text rendering
- Prerequisite for text animation GPU optimization
- Depends on WP-1 success

**Status**: Blocked until WP-1 complete

---

### #8: Video QImage Retirement

**Priority**: 40/100  
**Scope**: ~55 hours  
**Risk**: High  
**Dependencies**: GPU texture cache system stable, color management foundation

**What it is**: Remove QImage from video rendering hot path; use GPU-native frames directly.

**Why it matters**:
- Improves video performance
- Enables color-managed video workflows
- Large scope; requires careful phasing

**Status**: Blocked by missing infrastructure. Defer.

---

### #9: OIIO Image Pipeline Migration

**Priority**: 30/100  
**Scope**: ~70 hours  
**Risk**: High  
**Dependencies**: Color management foundation, GPU buffer abstractions

**What it is**: Migrate image loading/processing to OIIO while keeping QImage as fallback.

**Why it matters**:
- Preserves image metadata better
- Enables wide-gamut workflows
- Very large scope; many moving parts

**Status**: Blocked by dependencies. Defer to Q3 or later.

---

## Recommended Implementation Order

### 🚀 **Immediate (Next 1-2 weeks)**

1. **Cloud AI Phase 6** (15h) - Export API
   - Relatively quick win
   - Builds on solid infrastructure
   - Delivers value immediately

2. **Cloud AI Phase 7** (12h) - Timeline Operations
   - Natural continuation
   - Completes AI widget coverage
   - Low technical risk

### 📅 **Short-term (Following 2-3 weeks)**

3. **GPU Direct Text Draw - WP-1** (8h) - GlyphAtlas only
   - Self-contained, unblocks WP-2-6
   - Medium risk but well-scoped
   - Can parallelize with other work

4. **Composition Editor Rules** (10h) - Documentation
   - Framework/clarity work
   - No code risk
   - Improves maintainability

### 🔧 **Medium-term (Following month)**

5. **Particle Stabilization** (25h) - Bug fixes
   - Clear root cause documented
   - Improves reliability
   - Well-understood scope

6. **3D Gizmo Polish** (20h) - Edge cases
   - User experience improvement
   - Iterative fixes
   - Medium complexity

### ⏸️ **Deferred (Q3+)**

- Video QImage Retirement (blocked by infrastructure)
- OIIO Migration (blocked by infrastructure)
- GPU Direct Text Draw WP-2-6 (depends on WP-1)

---

## Dependencies Map

```
Cloud AI Phase 6 → Export API (standalone)
Cloud AI Phase 7 → Timeline Operations (standalone)
    ↓ (both enhance AI widget coverage)
GPU Text WP-1 (GlyphAtlas)
    ↓ (enables)
GPU Text WP-2-6 (full path)
    ↓ (enables)
Text Animation GPU optimization

Particle Stabilize (standalone)
3D Gizmo Polish (standalone)
Composition Editor Rules (standalone)

Color Mgmt Foundation (missing)
    ↓ (blocks)
Video QImage Retirement
OIIO Image Pipeline Migration
```

---

## Decision Points for User

1. **Agree with Tier 1 priorities (Cloud AI 6 & 7)?**
   - If yes: Start Phase 6 next
   - If no: What would you prioritize instead?

2. **Should GPU Text WP-1 start during Cloud AI work?**
   - Can parallelize: one person on Cloud AI, another on WP-1
   - Or sequential: Cloud AI first, then WP-1

3. **Accept deferral of Video QImage / OIIO?**
   - Both blocked by missing color management foundation
   - Can't proceed without major prep work
   - Recommend return to these in Q3

---

**Created**: 2026-04-27  
**Next Action**: User confirms priority list and starts Phase 6
