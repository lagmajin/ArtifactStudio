# M-MO-1 Auto-Orient Milestone

作成日: 2026-06-16
ステータス: 部分実装（Auto-Orient 評価・Property 基盤は実装、永続化／統合検証待ち、静的確認 2026-07-29）
対象: `Artifact/src/Layer/ArtifactAbstractLayer.cppm`,
      `Artifact/src/Layer/ArtifactAbstractLayer.ixx`,
      `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`,
      `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.ixx`,
      `Artifact/src/Widgets/Inspector/ArtifactInspectorWidget.cppm`,
      `Artifact/src/Composition/ArtifactCompositionSettings.cppm`,
      `Artifact/src/Project/ArtifactProjectManager.cppm`,
      `Artifact/src/Undo/*`,
      `ArtifactCore/include/Transform/StaticTransform2D.ixx`,
      `ArtifactCore/include/Transform/Rotate.ixx`
位置づけ: `MILESTONE_MOTION_PATH_EDITING_2026-04-29.md` の上に **Auto-Orient** を追加。AE 風の「パスに沿う向き自動補正」を実現する foundation。
参照:
- `docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` (#12)
- `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` (P0)
- `docs/analysis/WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` §7
- `docs/planned/MILESTONE_MOTION_PATH_EDITING_2026-04-29.md`
- `docs/planned/MILESTONE_COMPOSITION_MOTION_PATH_OVERLAY_2026-03-28.md`
- `docs/planned/MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md`
- `docs/planned/MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md`

---

## 1. 目的

`FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md` (#12):

> Auto-Orient
> — 一致するコードなし。path に沿う向き自動補正がない

`AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` の P0 に分類。

AE では、layer の **position に motion path がある場合**、Auto-Oriented rotation で path の tangent 方向に layer を自動回転させることができる。例:

- 曲線に沿って動くロゴ
- パスに沿って走る車
- 楕円周を回転する惑星

これが **ない** ことで、rotation keyframe を path の tangent と一致するように手動で打つ羽目になり、path を編集するたびに追従が崩れる。

> 重要: `MILESTONE_MOTION_PATH_EDITING_2026-04-29.md` の motion path 編集結果に **計算だけで** 回転を加算する。本 milestone は motion path 経路の計算を **再利用** し、layer の rotation 評価に **tangent 補正** を合成する。motion path 自体の編集体験は変えない。

> 重要: `ArtifactCore` 配下のみを書き、UI 露出は `Artifact/.../Widgets/` 側に閉じる。サブモジュール（`ArtifactWidgets`）は触らない。

---

## 2. 現状整理 (2026-06-16 基準)

### 2.1 既存資産

- `MILESTONE_MOTION_PATH_EDITING_2026-04-29.md` — motion path 表示 / 編集（実装済み）
  - keyframe 点のドラッグ、追加、削除、補間変更
  - hover 強調
  - undo 整合
- `MILESTONE_COMPOSITION_MOTION_PATH_OVERLAY_2026-03-28.md` — overlay 描画
- `ArtifactCompositionRenderController` 側に motion path 評価 / キャッシュ
- `ArtifactCore/include/Transform/StaticTransform2D.ixx` — `QMatrix4x4` ベースの 2D transform
- `ArtifactCore/include/Transform/Rotate.ixx` — `Rotate { float rotate_ }`
- `ArtifactAbstractLayer` 側に `setRotation / rotation / rotationKeyframes` 想定の API

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| `AutoOrientMode` enum | なし | モード概念が無い |
| Layer 単位の autoOrient | なし | フラグが無い |
| Composition 単位の全体 ON/OFF | なし | 全体設定が無い |
| Tangent 計算 | motion path 側に既にあるが tangent export なし | 再計算が必要 |
| 既存 rotation keyframe との precedence | なし | 合成ルール未定義 |
| Inspector 露出 | なし | UI から ON/OFF できない |
| 評価経路 | 既存 transform 評価に auto-orient hook なし | 計算が反映されない |
| 永続化 | なし | 保存されない |
| Diagnostics | なし | 異常検出なし |

### 2.4 2026-07-29 実装監査

- `AnimatableTransform3D` に `AutoOrientMode::Off`、`AlongPath`、`AlongPathAtFrameStart` があり、position keyframe の前後差分から tangent 角度を計算して `rotationAt()` に返す評価経路が実装されている。
- `ArtifactAbstractLayer` の Transform property group に `transform.autoOrient` が追加され、Property 値から mode を切り替えられる。
- したがって Auto-Orient の mode／評価／Property 基盤は部分実装済みと判定する。
- 既存 rotation keyframe への加算・オフセット規則、composition 全体 ON/OFF、project JSON の明示保存復元、diagnostics、Undo／runtime 検証は未確認または未実装であり、Phase 2 以降は未完了とする。

### 2.3 既存 milestone との関係

- `MILESTONE_MOTION_PATH_EDITING_2026-04-29.md` — 下位。本 milestone はこの上に tangent 補正を **追加**
- `MILESTONE_COMPOSITION_MOTION_PATH_OVERLAY_2026-03-28.md` — overlay。本 milestone は tangent 計算を **再評価** するだけ
- `MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md` — frame cache。本 milestone は tangent を cache しない（軽い計算）
- `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` — `CompositionRenderController` に low-level call site を増やさない方針。本 milestone は transform 評価に閉じて hook する

---

## 3. 設計の柱

### 3.1 AutoOrientMode

`Artifact/include/Layer/AutoOrientMode.ixx` を新規追加:

```cpp
namespace Artifact {

enum class AutoOrientMode {
    Off,                        // 自動補正なし（default）
    AlongPath,                  // path tangent 方向に回転
    AlongPathAtFrameStart,      // その frame 区間の開始 tangent
    // 将来: CameraFacing, 2D LookAt 等
};

} // namespace Artifact
```

- `Off`: AE の "Off" 相当
- `AlongPath`: AE の "Along Path" 相当
- `AlongPathAtFrameStart`: AE の "Along Path at Frame Start" 相当

### 3.2 ArtifactAbstractLayer への組み込み

- `Artifact/include/Layer/ArtifactAbstractLayer.ixx` に `AutoOrientMode autoOrient_` 追加
- API:
  - `void setAutoOrient(AutoOrientMode mode)`
  - `AutoOrientMode autoOrient() const`
  - `void setAutoOrientEnabled(bool enabled)` (Off / not Off の boolean convenience)
- 既定値: `Off`

### 3.3 Tangent 計算

既存 `MILESTONE_MOTION_PATH_EDITING_2026-04-29.md` の `buildMotionPathSamples()` を再利用。新規に **tangent export** を追加:

```cpp
struct MotionPathTangentSample {
    FramePosition frame;
    QPointF position;
    float tangentRadians;     // atan2(dy, dx)
};

// 既存 buildMotionPathSamples() を tangent 込みで拡張
QList<MotionPathTangentSample> buildMotionPathSamplesWithTangent(
    const LayerID& layerId, FramePosition fromFrame, FramePosition toFrame);
```

- 既存 API は温存。新規メソッドを追加
- 数値安定性: `Δframe` が 0 になる区間では `tangentRadians = 0` を返す

### 3.4 Transform 評価 hook

`CompositionRenderController::evaluateLayerTransform(layer, frame)` の **最後** で:

```cpp
QMatrix4x4 evaluateLayerTransform(layer, frame) {
    QMatrix4x4 base = existingTransform(layer, frame);   // 既存評価

    if (layer->autoOrient() == AutoOrientMode::Off) return base;
    if (!layer->hasMotionPath()) return base;             // motion path 必須

    MotionPathTangentSample tangent = sampleTangent(layer, frame);
    if (tangent is null) return base;

    float autoRotation = ...;                              // 補正角度
    QMatrix4x4 rot;
    rot.rotate(/* base rotation + autoRotation */);
    return base * rot;
}
```

- **既存評価を破壊しない**。`Off` / motion path 不在時はそのまま返す
- 既存 `rotation` keyframe と **precedence ルール**:
  - **No rotation keyframe + AutoOrient = On**: tangent のみ
  - **Rotation keyframe + AutoOrient = On**: rotation + tangent offset
  - **AutoOrient = Off**: 従来通り

### 3.5 Composition 単位の全体 ON/OFF

`Artifact/src/Composition/ArtifactCompositionSettings.cppm` に:

```cpp
class ArtifactCompositionSettings {
    // 既存
    bool autoOrientEnabled() const;
    void setAutoOrientEnabled(bool enabled);
};
```

- 既定値: `true`（layer 単位の `Off` が default なので全体 ON でも実害なし）
- 全体 OFF の場合、layer 単位で `On` でも **補正しない**

### 3.6 Inspector 露出

`ArtifactInspectorWidget` の layer transform パネルに **`Auto Orient`** dropdown を追加:

- `Off` / `Along Path` / `Along Path at Frame Start`
- 隣に小さな help アイコン（hover で AE 風の説明 tooltip）

### 3.7 Undo

`Artifact/Undo/SetAutoOrientCommand.cppm` 新規:

```cpp
class SetAutoOrientCommand : public QUndoCommand {
public:
    SetAutoOrientCommand(const QString& layerId,
                         AutoOrientMode before,
                         AutoOrientMode after);

    void undo() override;
    void redo() override;
};
```

- 1 undo で 1 layer の mode 切替
- 全体 ON/OFF は composition settings の既存 undo 経路に統合

### 3.8 永続化

- `ArtifactProjectManager` の project JSON に `layer.autoOrient` (string) と `composition.autoOrientEnabled` (bool) 追加
- 旧プロジェクトは `autoOrient` 欠落を許容（default: `Off`）
- 既存プロジェクトの読み込みで **layer は Off**、全体は **ON** として復元

### 3.9 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 枠に:

- `auto-orient.no-motion-path` (severity=info, autoOrient=On で motion path 不在)
- `auto-orient.degenerate-tangent` (severity=warning, tangent 計算が退化)
- `auto-orient.contradiction` (severity=info, rotation keyframe と auto-orient の同時利用で precedence ルール適用)

### 3.10 不変条件 (Guardrails)

- 既存 `buildMotionPathSamples()` は **温存**。tangent 込み版を追加するだけ
- `Off` / motion path 不在時は既存 transform 評価と **同一の出力**（数値一致）
- 既存 `rotation` keyframe の **意味は変えない**
- 既存 `CompositionRenderController` の low-level call site は増やさない
- 新規 signal-slot 接続は `setAutoOrient` 1 個に限定
- `QImage` / `setStyleSheet` 流入禁止
- tangent 計算の数値安定性: 隣接 2 サンプルで `Δframe == 0` 時の特例処理

---

## 4. フェーズ計画

### Phase 1: AutoOrientMode + layer API (P0, 1 セッション)

- `Artifact/include/Layer/AutoOrientMode.ixx` 新規
- `Artifact/include/Layer/ArtifactAbstractLayer.ixx` に `autoOrient_` 追加
- `setAutoOrient / autoOrient / setAutoOrientEnabled` 実装
- 既定値 `Off`

**Done criteria:**
- `setAutoOrient(AlongPath)` 後 `autoOrient() == AlongPath`
- 既定値 `Off` が新規 layer で有効
- 既存 layer に副作用なし

### Phase 2: Tangent 計算の export (P0, 1〜2 セッション)

- `CompositionRenderController` の `buildMotionPathSamples()` を tangent 込みに拡張
- `MotionPathTangentSample` 構造体追加
- 既存 `buildMotionPathSamples()` は **シグネチャ維持** で温存

**Done criteria:**
- `buildMotionPathSamplesWithTangent()` が `QList<MotionPathTangentSample>` を返す
- 既存 API 利用箇所は **変更なし**
- 数値安定性: `Δframe == 0` 時に tangent = 0

### Phase 3: Transform 評価 hook (P0, 1〜2 セッション)

- `evaluateLayerTransform(layer, frame)` の最後で auto-orient 補正
- precedence ルール実装
- 既存評価結果と数値一致（`Off` / motion path 不在時）

**Done criteria:**
- `autoOrient = AlongPath` で layer 回転が path tangent に追従
- `Off` で既存評価と完全一致
- rotation keyframe + auto-orient の合成が precedence 通り

### Phase 4: Inspector 露出 + 全体 ON/OFF (P0, 1 セッション)

- Inspector の layer transform パネルに Auto Orient dropdown
- composition settings に全体 ON/OFF
- `SetAutoOrientCommand` 追加

**Done criteria:**
- Inspector から mode 切替
- 全体 OFF で layer 単位 On でも補正しない
- 1 undo で mode 切替復元

### Phase 5: 永続化 + Diagnostics (P1, 1 セッション)

- project JSON に `layer.autoOrient` と `composition.autoOrientEnabled` 追加
- 旧プロジェクトの後方互換
- Problem View への `auto-orient.*` 健全性 contribution

**Done criteria:**
- project 保存 → 再読込で mode 完全復元
- 旧プロジェクトが開ける
- Problem View に `auto-orient.degenerate-tangent` 等表示

### Phase 6: 3D / CameraFacing 拡張 (P2, 別 milestone 推奨)

- 3D カメラ向け `CameraFacing` mode
- 2D LookAt mode
- これは `MILESTONE_3D_VIEWPORT_ORBIT_PAN_PREVIEW_MODE_2026-06-07.md` と統合で別 milestone

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_AUTO_ORIENT_3D_2026-XX-XX.md` のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_MOTION_PATH_EDITING_2026-04-29.md` | 下位。本 milestone は tangent 計算を再利用。 |
| `MILESTONE_COMPOSITION_MOTION_PATH_OVERLAY_2026-03-28.md` | overlay 描画。並走。 |
| `MILESTONE_RAM_PREVIEW_CACHE_2026-03-26.md` | frame cache。並走。 |
| `MILESTONE_RENDER_BOUNDARY_SAFETY_GATE_2026-04-21.md` | 低レベル呼び出しを増やさない方針。 |
| `MILESTONE_3D_VIEWPORT_ORBIT_PAN_PREVIEW_MODE_2026-06-07.md` | 3D 拡張は Phase 6 で接続。 |
| `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12.md` | diagnostics 文法。 |

---

## 6. リスクと未解決論点

### 6.1 実装上のリスク

1. **Tangent 数値安定性**。`Δframe == 0` 時の特例処理。前後数 frame の平均で代替するか
2. **既存評価との数値一致**。`Off` / motion path 不在時で完全に同じ結果を出す
3. **`CompositionRenderController` への hook 位置**。`evaluateLayerTransform` の最後で安全か
4. **Precedence ルール**。rotation keyframe と auto-orient の合成は加算でよいか。AE 互換の挙動は加算
5. **3D layer 対応**。`Artifact3DLayer` の quaternion rotation との統合。Phase 6 で別途

### 6.2 契約上の未解決

- **`AlongPath` vs `AlongPathAtFrameStart`**。frame 区間の **どの時点** の tangent を使うか。AE 互換は `AlongPath` で中央値、`AtFrameStart` で区間開始
- **2 つの motion path**。position X/Y 個別の keyframe を持つ layer の tangent 計算。frame 毎に `atan2(dy, dx)` を取る方針で固定
- **Nested composition**。`ArtifactCompositionLayer` 内の layer の auto-orient
- **3D camera モード**。Phase 6 で `CameraFacing` を追加する前提
- **Effect 由来 rotation**。rotation effect が layer に適用されている場合の precedence

### 6.3 サブモジュール境界

- `Artifact/include/Layer/AutoOrientMode.ixx` を新規追加
- `Artifact/include/Layer/ArtifactAbstractLayer.ixx` に追加（破壊変更ではない）
- `ArtifactCore/CMakeLists.txt` には触らない
- `ArtifactWidgets` は触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- `autoOrient = AlongPath` で layer 回転が path tangent に追従
- `Off` で既存評価と完全一致
- rotation keyframe + auto-orient の合成が precedence 通り
- Inspector の dropdown で mode 切替
- composition 全体 ON/OFF 切替
- 1 undo で mode 切替復元
- project 保存 → 再読込で mode 完全復元
- 旧プロジェクトは layer `Off` / 全体 `ON` として開く
- Problem View に `auto-orient.*` 健全性表示
- 新規 `QImage` / `setStyleSheet` / 新規 signal-slot が増えていない
- 既存 `buildMotionPathSamples()` のシグネチャが温存
- 既存 `CompositionRenderController` の low-level call site が増えていない
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`WORKFLOW_GAP_DEEP_DIVE_2026-06-16.md` §7 を正式 milestone に起こした。AE 互換の auto-orient foundation。
