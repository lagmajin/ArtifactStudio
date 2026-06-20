# CompositionEditor / 低レベル Render / ROI 適合調査 — 2026-06-16

作成日: 2026-06-16
目的: `docs/analysis/REPORT_APP_PERF_BOTTLENECK_2026-06-16.md` を補完する形で、`Artifact/src` の **CompositionEditor の viewport / 入力系** と **Diligent 低レベル API の利用度**、および **ROI (Region of Interest) の適合** を走査。
調査範囲: `Artifact/src + Artifact/include` のみ。

---

## 1. 結果サマリ

- **CompositionEditor**: viewport / 入力系は **おおむね実装済み**。回転 / 解像度切替のみ 0 hit
- **Diligent 低レベル API**: 一部のみ利用。`IRenderCommandBuffer` / pipeline state / render pass / barrier / viewport set は **未配線**
- **ROI (Region of Interest)**: **`ArtifactRenderROI.ixx` 定義はあるが実装は完全に未着手**。ROI 計算 / UI / render path 統合すべて 0 hit
- **Diagnostics widget**: 完備。Problem View / Frame Debug / Harness / App Debugger が揃っている

---

## 2. CompositionEditor 詳細

### 2.1 実装済み (OK)

| 機能 | 件数 | 主な場所 |
|---|---:|---|
| Viewport background color | 197 | `ArtifactCompositionRenderController` |
| Viewport zoom | 91 | 同上 |
| Viewport orientation | 87 | `ViewOrientation` (Navigator) |
| Viewport fit / reset | 54 | `ArtifactIRenderer::fitToViewport` 系 |
| Viewport grid | 49 | grid overlay |
| Mouse press | 34 | Gizmo 入力 |
| Viewport guide | 28 | guide overlay |
| Key press | 27 | shortcut 入力 |
| Focus in/out | 27 | focus 移動 |
| Viewport size getter/setter | 25 | `setViewportSize` 系 |
| Mouse move | 24 | drag 入力 |
| Resize | 22 | `resizeEvent` |
| Viewport pan | 21 | `panBy` |
| Wheel | 12 | `wheelEvent` (zoom 等) |
| Viewport safe margin | 9 | safe area overlay |
| Context menu | 8 | 右クリック |
| Drop event | 5 | drag & drop |
| Drag enter | 5 | drag & drop |
| Picking ray | 4 | 3D 選択 |

→ **viewport のサイズ / pan / zoom / fit / orientation / grid / guide / safe margin は完備**。入力系 (mouse / key / wheel / drag&drop / focus / context menu) も揃っている。

### 2.2 未実装 (MISS)

| 機能 | 件数 | 影響 |
|---|---:|---|
| Viewport rotation (canvas rotate) | 0 | AE 風のキャンバス回転なし |
| Viewport resolution 即時切替 | 0 | 表示解像度の動的変更なし |

→ CE は **viewport 機能としては完成**。残るは **canvas 回転** と **表示解像度切替** の 2 機能。

---

## 3. 低レベル Render (Diligent) 詳細

### 3.1 実装済み (OK)

| API | 件数 | 評価 |
|---|---:|---|
| `IDeviceContext` (immediateContext) | 176 | 良好。中心 |
| `ITexture` (createTexture, UpdateTexture) | 176 | 良好 |
| `IRenderDevice` | 90 | 良好 |
| `ISwapChain` | 64 | 良好 |
| `IBuffer` | 62 | 良好 |
| `IShader` | 61 | 良好 |
| `IFence` | 29 | 良好 |
| `GpuContext` | 27 | 良好 (ProjectCore 配下) |
| GPU readback | 20 | 良好 (RenderFormatContract 整合) |
| `RenderCommandBuffer` (project) | 11 | 良好 (本プロジェクト独自抽象) |
| `ComputeExecutor` | 37 | 良好 |
| Draw / DrawIndexed / DrawInstanced | 43 | 良好 |
| Dispatch / DispatchCompute | 3 | 薄い |
| `IPipelineState` | 4 | **薄い** |
| `IQuery` | 1 | 薄い |

### 3.2 未実装 (MISS) — 重要発見

| API | 件数 | 影響 |
|---|---:|---|
| `IRenderCommandBuffer` (Diligent 標準) | 0 | Diligent 標準 API を直接使わず、独自 `RenderCommandBuffer` で wrap |
| GPU upload (sync) | 0 | UpdateTexture / UpdateBuffer は string 上では 0 (match 条件が sync 限定) |
| GPU render pass | 0 | `beginRenderPass` / `endRenderPass` の直接呼出なし。独自抽象経由 |
| GPU viewport set | 0 | `SetViewport` / `SetScissor` の直接呼出なし |
| GPU barrier | 0 | `TextureBarrier` / `BufferBarrier` の直接呼出なし |

→ **Diligent のモダンな command buffer / render pass / barrier API は直接利用されておらず、独自抽象 `RenderCommandBuffer` 経由で運用**。これは **AGENTS.md / `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` の方針 (low-level call site を増やさない)** と整合する。

**`MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_2026-04-21.md`** は「ImmediateContext の直接利用を widget / layer から減らす」方針なので、本レポートの 0 hit は **意図された結果** と整合する。

---

## 4. ROI (Region of Interest) — 重要発見

### 4.1 完全未着手

| 軸 | 件数 | 詳細 |
|---|---:|---|
| ROI struct | 0 | `ArtifactRenderROI.ixx` は **宣言のみ** |
| ROI computation | 0 | `computeROI` / `roiBounds` 不在 |
| ROI in render path | 0 | `renderOneFrame` 内に ROI 計算なし |
| ROI editor UI | 0 | `roiEditor` / `setROIRect` 不在 |
| GPU memory tracker | 0 | `GpuMemoryTracker` 不在 |
| Frame timing breakdown | 0 | `frameTiming` / `FrameBudget` 不在 |

### 4.2 既存資産

`Artifact/include/Render/ArtifactRenderROI.ixx` (216 行) で **構造体のみ定義**:

```cpp
struct ArtifactRenderROI {
    ArtifactCore::FloatRect bounds;
    bool enabled;
    float priority;
};
```

ただし `.cppm` 実装 (`ArtifactRenderROI.cppm`) は **不在**。`renderOneFrame` 内で `roi.bounds` を使う経路も **完全に 0 hit**。

### 4.3 ROI が重要な理由

ROI は **AE 風の region of interest preview** の核:

- コンポジションの **特定領域のみ高解像度** で preview / render
- 編集中の **注目領域** に GPU 予算を集中
- 古い PC でも 4K timeline を 60fps で再生する **領域ベース** 最適化

DaVinci Resolve の "Optimized Media" や Premiere の "Render In/Out" も ROI ベース。

### 4.4 ROI が `ArtifactRenderContext.ixx:104-154` で言及あり

`docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` (#26 ROI) で:

> `Artifact/include/Render/ArtifactRenderContext.ixx:104-154`, `Artifact/include/Render/ArtifactRenderROI.ixx` struct あり
> debug draw コメントアウト。UI からの ROI 設定導線は未確認

→ **構造体は定義済み、UI なし、render 統合なし**。foundation までは揃っているが、実装は未着手。

---

## 5. Diagnostics widget (完備)

| Widget | 件数 | 評価 |
|---|---:|---|
| Debug render harness | 21 | 良好 (`DebugRenderHarnessWidget`) |
| Problem view | 17 | 良好 |
| Frame debug view | 17 | 良好 |
| Frame resource inspector | 16 | 良好 |
| Profiler panel | 16 | 良好 |
| App debugger widget | 15 | 良好 |
| Frame pipeline view | 15 | 良好 |
| Frame state diff | 13 | 良好 |
| Trace timeline | 18 | 良好 |

→ Debug widget 群は **完備**。`MILESTONE_APP_DEBUGGER_GOAL_FIRST_SUMMARY_2026-05-12.md` 系で整備済み。

---

## 6. 新規 milestone 候補

### 6.1 P0 (即着手)

| テーマ | 価値 |
|---|---|
| **M-PERF-1 Perf Hot Path 修正** | paintEvent 再帰 / QImage 排除 / GpuContext cache (重大度 S) |
| **M-ROI-1 Region of Interest Foundation** | `ArtifactRenderROI.cppm` 実装 + render 統合 + UI。ROI 経由で低スペック PC でも 4K timeline 動作 |
| **M-DEBUG-1 Dev Diagnostics 整備** | Performance overlay / GPU profiler / RenderDoc / qDebug 制御 (Diagnostics widget は完備済み) |

### 6.2 P0 (App レベル)

- **M-EDIT-1 Edit Menu 強化**
- **M-INTERACT-1 Pen / Touch / Joystick 入力**
- **M-CRASH-1 Crash-safe Save**
- **M-TEMPLATE-1 Project Template Gallery**
- **M-SCRIPT-1 Script Console (REPL)**

### 6.3 P2

- **M-XR-1 VR / XR Support**
- **M-CE-ROT-1 CompositionEditor Canvas Rotation** (0 hit。AE 風キャンバス回転)

---

## 7. ROI milestone の中身（推奨フェーズ）

1. **Phase 1**: `ArtifactRenderROI.cppm` 実装
   - `computeROI(layer, frame) -> FloatRect` 関数
   - bounds の union / intersection 計算
   - mask / matte 領域の反映

2. **Phase 2**: render path への統合
   - `renderOneFrame` 内で ROI を計算
   - ROI 外の layer は cull / 低解像度描画
   - GPU memory budget 連動

3. **Phase 3**: Editor UI
   - Composition Editor 上に ROI 矩形を表示
   - マウスドラッグで ROI 設定
   - 右クリック menu で ROI 削除 / 一時無効

4. **Phase 4**: Project 保存 + Diagnostics
   - project JSON に ROI 保存
   - Problem View への `roi.*` 健全性 contribution
   - Performance overlay で ROI 効率を表示

5. **Phase 5**: GPU memory tracker + Frame timing breakdown
   - GpuMemoryTracker 実装
   - FrameBudget 監視
   - ROI との連動

---

## 8. 既存 milestone との接続

| 既存 | 関係 |
|---|---|
| `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` | low-level call site を増やさない方針。本レポートの 0 hit (Diligent 深い API) は方針と整合 |
| `MILESTONE_IMMEDIATE_CONTEXT_BOUNDARY_2026-04-21.md` | ImmediateContext 境界。ROI 計算もここに閉じる |
| `MILESTONE_RENDER_FORMAT_CONTRACT_2026-05-16.md` | linear canonical。ROI 内の image は canonical で扱う |
| `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12.md` | diagnostics 文法。ROI の健全性も contribution |
| `MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_2026-06-02.md` | diagnostics 系。Density Monitor 等 |
| `MILESTONE_VISUAL_DENSITY_MONITOR_2026-06-03.md` | density monitor。ROI 効率を可視化 |

---

## 9. まとめ

- **CompositionEditor**: viewport / 入力系は完備。Canvas rotation / 表示解像度切替のみ 0 hit
- **低レベル Render**: Diligent 標準の command buffer / render pass / barrier は 0 hit。**意図された結果** (独自抽象経由) で方針整合
- **ROI**: **`ArtifactRenderROI.ixx` 宣言はあるが実装完全未着手**。ROI 計算 / UI / render 統合すべて 0 hit。foundation 候補として最有力
- **Diagnostics widget**: 完備 (Harness / ProblemView / FrameDebug / AppDebugger / FramePipelineView / FrameResourceInspector / FrameStateDiff / TraceTimeline / ProfilerPanel)
- **App レベル機能ギャップ**: M-EDIT / M-INTERACT / M-CRASH / M-TEMPLATE / M-SCRIPT / M-XR が未着手
- **Perf Hot Path**: 重大度 S 3 件 (paintEvent 再帰 / QImage 排除 / GpuContext cache)

着手順の推奨:
1. **M-PERF-1** (App 全体の応答性)
2. **M-ROI-1** (低スペック PC でも動く ROI foundation)
3. **M-DEBUG-1** (dev / GPU profiling)
4. **M-EDIT-1, M-CRASH-1, M-INTERACT-1** (App 機能)
5. **M-TEMPLATE-1, M-SCRIPT-1** (拡張性)
6. **M-XR-1** (将来)

---

## 10. 更新履歴

- 2026-06-16: 初版作成。`Artifact/src` を CompositionEditor / Diligent 低レベル / ROI / Diagnostics の 4 軸で走査。
