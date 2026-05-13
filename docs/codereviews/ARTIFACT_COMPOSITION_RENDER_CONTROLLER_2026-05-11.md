# Code Review: ArtifactCompositionRenderController

**Date:** 2026-05-11
**Reviewer:** Kilo AI
**Files Reviewed:**
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/include/Widgets/Render/ArtifactCompositionRenderWidget.ixx`
- `docs/COMPOSITION_EDITOR_CONTRACT.md`

## Summary

The `ArtifactCompositionRenderController` implements the render controller for the composition editor viewport. The implementation follows the contract well but has several areas requiring attention.

## Strengths

1. **Clear separation of concerns**: Shell/Controller/Renderer responsibilities are well-defined per the contract
2. **Comprehensive layer support**: Handles video, text, SVG, particle, 3D layers, compositions
3. **GPU texture caching**: Implementation includes cache with proper invalidation
4. **Frame debug snapshot**: Rich diagnostic reporting structure
5. **Motion path editing**: Full keyframe manipulation support

## Critical Issues

### 1. Memory Management (Line ~3117)
```cpp
CompositionRenderController::~CompositionRenderController() {
  destroy();
  delete impl_;  // ISSUE: Impl contains unique_ptr members
}
```
**Recommendation:** Use RAII properly - `std::unique_ptr<Impl> impl_` instead of raw pointer.

### 2. QImage in Hot Path (Line ~334, ~1458)
```cpp
bool buildRasterizedSurfaceBuffer(..., QImage &surface, ...) {
  cv::Mat mat = ArtifactCore::CvUtils::qImageToCvMat(surface, true);
  // ... processing ...
}
```
**Violates guardrail #122:** "Keep QImage out of hot-path editor state"
**Recommendation:** Use `ImageF32x4_RGBA` directly, convert only at boundaries.

### 3. Thread Safety Issues
- `CompositionChangeDetector::mutex_` is mutable but access patterns are inconsistent
- `renderDirty_.exchange()` at line ~3192 is correct, but `gizmoDragRenderTimer_.restart()` could race with render tick

### 4. Missing Error Handling
```cpp
// Line ~3258-3271
if (impl_->blendPipeline_) {
  impl_->blendPipelineReady_ = impl_->blendPipeline_->initialize();
  // No retry on failure
}
```
**Recommendation:** Add exponential backoff retry for shader compilation failures.

## Medium Issues

### 5. Cache Invalidation Inconsistency (Line ~3019-3026)
```cpp
const bool skipCacheInvalidation = impl_->gizmoDragActive_ && layerId == impl_->selectedLayerId_;
if (!skipCacheInvalidation) {
  impl_->invalidateLayerSurfaceCache(layer);
}
impl_->invalidateBaseComposite();  // Always called
```
**Issue:** Base composite invalidated but layer cache skipped - inconsistent state.

### 6. Event Bus Coupling (Line ~2962-3043)
Heavy reliance on global `ArtifactCore::globalEventBus()` for layer selection changes.
**Recommendation:** Consider direct service injection for composition-specific events.

### 7. Signal Reconnection Issue (Line ~2887-2896)
```cpp
void bindCompositionChanged(...) {
  if (compositionChangedConnection_) {
    QObject::disconnect(compositionChangedConnection_);
    compositionChangedConnection_ = {};
  }
}
```
Potential for recursive calls if disconnection triggers another `bindCompositionChanged`.

## Low Issues

### 8. Debug Logging Overhead
```cpp
qCDebug(compositionViewLog) << "[CompositionView] drawLayerForCompositionView: ...";
```
Many debug statements in hot paths - consider compile-time guards.

### 9. Magic Numbers
- Hardcoded thresholds: `16` ms debounce, `12` px handle size, `0.05f` zoom limits
- Should be named constants with documentation

### 10. Unused Parameters
Line ~1477: `videoDebugOut` parameter is passed but only used for logging - could be removed or made optional.

## Recommendations Priority

| Priority | Issue | Effort |
|----------|-------|--------|
| P0 | Memory management (unique_ptr) | Low |
| P0 | QImage hot path | Medium |
| P1 | Thread safety | Medium |
| P1 | Error handling | Low |
| P2 | Cache invalidation | Low |
| P2 | Event bus coupling | High |

## Test Coverage Gaps

1. No unit tests for `CompositionChangeDetector` thread safety
2. Missing integration tests for GPU fallback paths
3. No performance regression tests for cache invalidation

## Conclusion

The implementation is solid overall with good architecture alignment to the contract. The main concerns are memory safety, QImage usage in hot paths, and thread safety. These should be addressed before the next release.