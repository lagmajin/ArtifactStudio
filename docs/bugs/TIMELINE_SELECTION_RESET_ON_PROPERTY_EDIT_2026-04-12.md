# Bug Report: Layer Selection Lost on Property Edit

## Summary
Selecting a layer in the timeline widget and then editing a property in the property widget causes the current layer selection to be cleared, resulting in the property widget displaying "No Layer".

## Investigation & Hypothesis

**Event Flow Analysis:**
1.  **Property Edit:** User edits a property (e.g., Transform Position) in `ArtifactPropertyWidget`.
2.  **Event Publication:** The layer emits `layer->changed()`, which results in `LayerChangedEvent{Modified}` being published to the `EventBus`.
3.  **Timeline Reaction:** `ArtifactTimelineWidget` subscribes to `LayerChangedEvent`.
    *   In `ArtifactTimelineWidget.cpp` (L2737-2745), the subscription handler checks the `changeType`.
    *   For `ChangeType::Modified`, it calls `scheduleRefresh()`.
4.  **UI Rebuild:** `scheduleRefresh()` triggers `refreshTracks()`.
    *   `refreshTracks()` (L2928) performs a full rebuild of the timeline tracks (rows/clips) and calls `syncPainterSelectionState()`.
5.  **Selection Loss:**
    *   The heavy UI rebuild during `refreshTracks()` appears to inadvertently clear or reset the internal selection state (`impl_->selectedLayerIds_`) or trigger a selection sync that results in the selection being lost.
    *   Consequently, the `ArtifactLayerSelectionManager` is updated (or the Inspector perceives the state as cleared), causing `ArtifactInspectorWidget` to receive a "Nil" layer ID and display "No Layer".

**Hypothesis:**
The `ArtifactTimelineWidget` is overreacting to `LayerChangedEvent{Modified}` events. A property change (Modified) does not alter the *structure* of the timeline tracks (layer order, existence, etc.), so a full `refreshTracks()` is unnecessary and causes side effects like selection loss.

## Fix

**File:** `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`

**Change:**
Remove or comment out the call to `scheduleRefresh()` inside the `LayerChangedEvent` subscription for `ChangeType::Modified`.

```cpp
// Before
impl_->eventBus_.subscribe<LayerChangedEvent>(
    [this, scheduleRefresh](const LayerChangedEvent& event) {
        if (event.changeType == LayerChangedEvent::ChangeType::Created) {
            onLayerCreated(CompositionID(event.compositionId), LayerID(event.layerId));
        } else if (event.changeType == LayerChangedEvent::ChangeType::Removed) {
            onLayerRemoved(CompositionID(event.compositionId), LayerID(event.layerId));
        } else {
            if (!impl_ || !impl_->painterTrackView_) return;
            scheduleRefresh(); // <--- CAUSES SELECTION LOSS
        }
    }));

// After
impl_->eventBus_.subscribe<LayerChangedEvent>(
    [this, scheduleRefresh](const LayerChangedEvent& event) {
        if (event.changeType == LayerChangedEvent::ChangeType::Created) {
            onLayerCreated(CompositionID(event.compositionId), LayerID(event.layerId));
        } else if (event.changeType == LayerChangedEvent::ChangeType::Removed) {
            onLayerRemoved(CompositionID(event.compositionId), LayerID(event.layerId));
        } else {
            // Modified events (property changes) do not change track structure.
            // Calling scheduleRefresh() here causes unnecessary UI rebuilds and selection loss.
            // scheduleRefresh();
        }
    }));
```

## Verification
After applying the fix:
1.  Select a layer in the Timeline.
2.  Edit a property in the Property Widget.
3.  **Expected:** The layer remains selected, and the property updates correctly.
4.  **Expected:** Adding or removing layers still correctly refreshes the timeline.
