# M-2DTRACK-1 2D Point Tracker Milestone

作成日: 2026-06-16
ステータス: Draft
対象: `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`,
      `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`,
      `Artifact/src/Layer/ArtifactAbstractLayer.cppm`,
      `Artifact/src/Layer/ArtifactNullLayer.cppm`,
      `Artifact/src/Service/ArtifactLayerService.cppm`,
      `Artifact/src/Project/ArtifactProjectManager.cppm`,
      `ArtifactCore/src/Math/Noise.ixx`,
      `ArtifactCore/src/Image/ImageF32x4_RGBA.ixx`,
      `ArtifactCore/src/Image/ImageUtils.ixx`
位置づけ: `MILESTONE_2D_POINT_TRACKER_2026-06-02.md` を正式に実行可能にする foundation。NCC ベースのトラッキングと、layer position 適用。
参照:
- `docs/analysis/REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.6
- `docs/analysis/MOTION_GRAPHICS_ARTIST_PAIN_POINTS_2026-04-19.md`
- `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` (P1 2D Tracker)
- `docs/planned/MILESTONE_2D_POINT_TRACKER_2026-06-02.md` (parent)
- `docs/planned/MILESTONE_MOTION_PATH_EDITING_2026-04-29.md` (motion path 編集)
- `docs/planned/MILESTONE_TRACK_MATTE_DRAG_LINK_UX_2026-06-01.md`
- `docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` (#1 Easy Ease など参考)

---

## 1. 目的

`REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.6:

> - 2D point tracker: 0 hit
> - Mocha-style planar tracker: 0 hit
> - Tracker node UI: 0 hit

`MILESTONE_2D_POINT_TRACKER_2026-06-02.md` で 2D ポイントトラッカーの概要は書かれているが、実装は伴っていない。本 milestone はそれを **実装フェーズ** に進める。

モーションデザイナーが 1 日に何十回も使う「**画面上の特徴点を追いかけて、位置データとして取り出す**」操作が、`MILESTONE_2D_POINT_TRACKER_2026-06-02.md` でいう「未着手」状態。

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/` 側に閉じる。サブモジュール（`ArtifactWidgets`）には触らない。

---

## 2. 現状整理 (2026-06-16 基準)

### 2.1 既存資産

- `MILESTONE_2D_POINT_TRACKER_2026-06-02.md` — 概要
- `MILESTONE_MOTION_PATH_EDITING_2026-04-29.md` — motion path 編集 (keyframe を直接編集)
- `ArtifactNullLayer` — null layer (parent 用)
- `ArtifactCore/src/Image/ImageF32x4_RGBA.ixx` — image データ型
- `ArtifactCore/src/Math/Noise.ixx` — 既存 Math
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` — composition editor
- `Artifact/src/Service/ArtifactLayerService.cppm` — layer service

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| Tracker Point データモデル | 0 hit | 概念が無い |
| Inner Box / Outer Box UI | 0 hit | 視覚化なし |
| NCC tracking algorithm | 0 hit | 自動追跡が動かない |
| 前方 / 後方 / 全フレーム | 0 hit | 探索方向が選べない |
| 結果の layer 適用 | 0 hit | position keyframe に出せない |
| 4 点平面トラッキング | 0 hit | 平面移動が拾えない |
| 手動修正 | 0 hit | 追跡失敗時に救えない |
| アンカーポイント自動 | 0 hit | 中心合わせが面倒 |
| Undo | 0 hit | 1 段の undo が無い |
| Project 保存 | 0 hit | tracker 設定が消える |

### 2.3 既存 milestone との関係

- `MILESTONE_2D_POINT_TRACKER_2026-06-02.md` — 親。本 milestone は実装 phase に進む
- `MILESTONE_MOTION_PATH_EDITING_2026-04-29.md` — motion path 編集。本 milestone は tracker 出力を motion path に流す
- `MILESTONE_TRACK_MATTE_DRAG_LINK_UX_2026-06-01.md` — 別 topicだが、track matte でも tracker は将来使える
- `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` — tracker も render 経由。controller に low-level call site を増やさない

---

## 3. 設計の柱

### 3.1 Tracker Point データモデル

`ArtifactCore/include/Track/TrackerPoint.ixx` を新規追加:

```cpp
namespace ArtifactCore {

struct TrackerBox {
    QPointF position;        // center
    QSizeF  size;             // inner / outer で別
    float   rotation = 0.0f;  // 角度
};

struct TrackerResult {
    int64_t frame;
    QPointF position;         // tracked center
    float   confidence;       // 0.0 .. 1.0
};

class TrackerPoint {
public:
    QString id;
    QString name;
    int64_t startFrame = 0;
    int64_t endFrame = 60;
    TrackerBox innerBox;
    TrackerBox outerBox;
    bool    trackSubpixel = true;
    bool    trackRotation = false;

    QList<TrackerResult> results() const;
    void setResults(const QList<TrackerResult>& r);

    // 永続化
    QJsonObject toJson() const;
    static TrackerPoint fromJson(const QJsonObject& obj);
};

} // namespace ArtifactCore
```

- 1 tracker point = 1 つの inner / outer box 対
- 1 つの layer / composition に **複数** tracker point を持てる

### 3.2 NCC 追跡アルゴリズム

`ArtifactCore/src/Track/NccTracker.cppm` を新規追加:

```cpp
class NccTracker {
public:
    // 1 ステップ追跡
    static TrackerResult trackOneFrame(
        const ImageF32x4RGBAWithCache& prevFrame,    // template 抽出元
        const ImageF32x4RGBAWithCache& currFrame,    // 探索対象
        const TrackerBox& innerBox,                 // 特徴領域
        const TrackerBox& outerBox,                 // 探索領域
        QPointF prevCenter,                          // 前回 center
        bool subpixel = true,
        bool trackRotation = false);
};
```

- NCC (Normalized Cross-Correlation) で template matching
- `ImageF32x4_RGBA` 経由で動作 (QImage 禁止)
- `subpixel = true` で parabola fit による sub-pixel 精度
- `trackRotation = true` で 0..360° 回転も探索 (重い)

### 3.3 TrackerController

`Artifact/src/Track/ArtifactTrackerController.cppm`:

```cpp
class ArtifactTrackerController {
public:
    void addTrackerPoint(ArtifactComposition* comp, TrackerPoint p);
    void removeTrackerPoint(ArtifactComposition* comp, const QString& id);

    // 実行
    void trackForward(ArtifactComposition* comp, const QString& id);
    void trackBackward(ArtifactComposition* comp, const QString& id);
    void trackAll(ArtifactComposition* comp, const QString& id);
    void trackRange(ArtifactComposition* comp, const QString& id,
                    int64_t startFrame, int64_t endFrame);

    // 結果
    QList<TrackerResult> results(ArtifactComposition* comp, const QString& id);

    // 適用
    void applyToLayer(ArtifactComposition* comp, const QString& trackerId,
                      ArtifactAbstractLayer* target, bool toPosition,
                      bool toAnchor, bool toScale, bool toRotation);
};
```

- `trackForward` / `trackBackward` / `trackAll` の 3 モード
- 結果は `TrackerPoint::results()` に保存
- `applyToLayer` で position / anchor / scale / rotation の **いずれか** に適用

### 3.4 4 点平面トラッキング (Phase 4)

`ArtifactCore/src/Track/PlanarTracker.cppm`:

```cpp
struct PlanarCorner {
    QString trackerId;
    QPointF offset;     // 中心からの相対位置
};

class PlanarTracker {
public:
    // 4 tracker point から planar transform (homography) を推定
    static QMatrix4x3 estimateHomography(
        const QList<QPair<QPointF, QPointF>>& correspondences);

    // homography から 4 corner に transform を適用
    static void applyToCorners(const QMatrix4x3& H,
                               QList<PlanarCorner>& corners);
};
```

- 4 点追跡 → homography → 平面の移動 / 回転 / スケール / スキュー
- 1 corner = 1 tracker point
- 結果は layer の `cornerPin` 的なプロパティ (将来)

### 3.5 UI 露出 (Composition Editor)

`ArtifactCompositionEditor` に **`TrackerLayer`** (overlay) を追加:

- tracker point を **画面上に Inner / Outer Box で表示**
- 選択 / 移動 / サイズ変更
- 右クリック menu:
  - `Track Forward` (`Alt+→`)
  - `Track Backward` (`Alt+←`)
  - `Track All` (`Alt+Shift+→`)
  - `Track Range...`
  - `Apply to Layer Position` (`Ctrl+Shift+P`)
  - `Apply to Layer Anchor`
  - `Apply to Layer Scale`
  - `Apply to Layer Rotation`
  - `Convert to Null Layer` — 4 点で Null を生成
  - `Delete Tracker`

### 3.6 手動修正

`TrackerPoint` の **任意 frame の結果** を上書き可能:

- 修正した frame から **再追跡** (`Track From Here`)
- 1 個の frame 修正で全体傾向を改善
- 修正は Undo 可能

### 3.7 Project 保存

- `ArtifactProjectManager` の project JSON に `composition.trackerPoints[]` 追加
- 各 tracker: id / name / start / end / inner / outer / results[]
- 旧プロジェクトは trackerPoints 欠落を許容

### 3.8 Undo

`Artifact/Undo/TrackerCommand.cppm`:

```cpp
class TrackCommand : public QUndoCommand {
public:
    TrackCommand(ArtifactTrackerController* controller,
                 ArtifactComposition* comp,
                 const QString& trackerId,
                 const QList<TrackerResult>& before,
                 const QList<TrackerResult>& after,
                 int64_t rangeStart, int64_t rangeEnd);
    void undo() override;   // 結果 snapshot 復元
    void redo() override;
};
```

- 1 undo で 1 段の追跡結果を完全復元
- 複数 tracker point の一括 track は `QUndoStack::beginMacro`

### 3.9 不変条件 (Guardrails)

- `QImage` を **新規 hot path に入れない**。`ImageF32x4RGBAWithCache` 経由
- 既存 motion path 編集には触れない (出力先が motion path に流れるだけ)
- 既存 `ArtifactNullLayer` の `parentLayerId_` を活用
- `ArtifactWidgets` 触らない
- 新規 signal-slot 接続は `trackerPointAdded / trackerResultsChanged` 2 個に限定
- NCC 計算は **別 thread** (UI ブロックしない)
- worker thread 内で QObject API を呼ばない

### 3.10 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 枠に:

- `tracker.box.invalid` (severity=error, inner / outer box が不正)
- `tracker.confidence.low` (severity=info, 信頼度 < 0.3)
- `tracker.out-of-frames` (severity=warning, start > end)
- `tracker.layer.missing` (severity=error, 適用先 layer が無い)
- `tracker.4point.not-coplanar` (severity=warning, 4 点が同一平面にない)

---

## 4. フェーズ計画

### Phase 1: Core data + TrackerPoint (P0, 1 セッション)

- `ArtifactCore/include/Track/TrackerPoint.ixx` 新規
- `ArtifactCore/src/Track/TrackerPoint.cppm` 実装
- 永続化

**Done criteria:**
- 1 tracker point を追加 / 削除 / 永続化
- Inner / Outer Box を持つ
- `results` を保存 / 取得

### Phase 2: NCC tracking algorithm (P0, 1〜2 セッション)

- `ArtifactCore/src/Track/NccTracker.cppm` 実装
- `trackOneFrame` の基本実装
- sub-pixel 精度 (parabola fit)

**Done criteria:**
- 1 frame 追跡が 10ms 以内
- sub-pixel ON / OFF 切替
- 信頼度計算が正しい (template と完全一致で 1.0)

### Phase 3: TrackerController + UI (P0, 1〜2 セッション)

- `Artifact/src/Track/ArtifactTrackerController.cppm` 実装
- `ArtifactCompositionEditor` に TrackerLayer overlay
- 右クリック menu
- ショートカット

**Done criteria:**
- 4 モード (forward / backward / all / range) 動作
- 結果を `TrackerPoint::results()` に保存
- applyToLayer で position 適用

### Phase 4: 4 点平面トラッキング (P0, 1〜2 セッション)

- `PlanarTracker::estimateHomography` 実装
- 4 corner 変換
- `ArtifactNullLayer` 自動生成

**Done criteria:**
- 4 tracker point で homography 計算
- 結果を Null layer position / rotation / scale に適用
- 平面歪み (skew) 検出

### Phase 5: 手動修正 + Undo (P0, 1 セッション)

- 任意 frame の結果上書き
- `Track From Here`
- `TrackCommand` 追加

**Done criteria:**
- 手動修正後、Track From Here で再追跡
- 1 undo で完全復元
- 複数 tracker の一括 Undo

### Phase 6: Project 保存 + Diagnostics (P0, 1 セッション)

- project JSON に tracker 保存
- 旧プロジェクトは default 補完
- Problem View への contribution

**Done criteria:**
- project 保存 → 再読込で復元
- 旧プロジェクトが開ける
- `tracker.confidence.low` 等が Problem View 表示

### Phase 7: Mocha 風 planar tracker (P1, 別 milestone 推奨)

- 4 点 + ロト領域 (spline mask)
- より高度な homography
- 別 milestone 推奨

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_MOCHA_PLANAR_TRACKER_2026-XX-XX.md` のエントリポイントを作る

### Phase 8: 3D camera tracker 接続 (P2, 別 milestone 推奨)

- 3D Camera Tracker が出した 3D ポイントと 2D トラッキングの統合
- 別 milestone 推奨

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_3D_2D_TRACK_BRIDGE_2026-XX-XX.md` のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_2D_POINT_TRACKER_2026-06-02.md` | 親。本 milestone は実装 phase。 |
| `MILESTONE_MOTION_PATH_EDITING_2026-04-29.md` | motion path 編集。tracker 出力先。 |
| `MILESTONE_TRACK_MATTE_DRAG_LINK_UX_2026-06-01.md` | track matte。並走。 |
| `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` | render 経由の方針。 |
| `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12.md` | diagnostics 文法。 |

---

## 6. リスクと未解決論点

### 6.1 実装上のリスク

1. **NCC 計算の数値安定性**。frame 単位の輝度差。Phase 2 で実測
2. **Sub-pixel 精度**。parabola fit の安定性
3. **追跡失敗時の挙動**。信頼度 < threshold で中止するかスキップするか
4. **メモリ**。結果配列が frame 単位。100,000 frame 越えプロジェクトで容量
5. **Thread 安全性**。worker thread 内で QObject API を呼ばない
6. **Rotation tracking**。0..360° 探索は重い。TrackAll で 60 fps 切る可能性

### 6.2 契約上の未解決

- **複数 tracker point の selection**。Phase 1 は 1 point 単位。複数選択は Phase 5 以降
- **4 点平面の fixed corner 設定**。どの tracker point がどの corner か
- **Render Farm での tracker 実行**。NCC は重い。並列化は Phase 7 以降
- **AI 補助 tracker**。track failure 時の AI 補完。Phase 8 以降

### 6.3 サブモジュール境界

- `ArtifactCore/include/Track/TrackerPoint.ixx` を新規追加
- `ArtifactCore/src/Track/TrackerPoint.cppm` を新規追加
- `ArtifactCore/src/Track/NccTracker.cppm` を新規追加
- `ArtifactCore/src/Track/PlanarTracker.cppm` を新規追加
- `Artifact/src/Track/ArtifactTrackerController.cppm` を新規追加
- `ArtifactCore/CMakeLists.txt` に登録
- `ArtifactWidgets` は触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- 1 tracker point を Composition Editor に追加できる
- Inner / Outer Box が画面に表示される
- 4 モード (forward / backward / all / range) すべて動作
- NCC 計算が 1 frame 10ms 以内
- 結果を layer position / anchor / scale / rotation に適用
- 4 点平面トラッキングで homography 計算
- 手動修正 + Track From Here で再追跡
- 1 undo で完全復元
- project 保存 → 再読込で復元
- 旧プロジェクトは trackerPoints 欠落を許容
- Problem View に `tracker.*` 健全性表示
- 新規 `QImage` / `setStyleSheet` / 新規 signal-slot が増えていない
- 既存 motion path 編集 API が温存
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.6 / §4 を正式 milestone に起こした。2D Point Tracker foundation。`MILESTONE_2D_POINT_TRACKER_2026-06-02.md` の実装 phase。
