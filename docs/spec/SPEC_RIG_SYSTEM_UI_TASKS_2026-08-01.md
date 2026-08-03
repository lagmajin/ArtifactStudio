# リグシステム UI 対応タスク一覧

**日付**: 2026-08-01
**前提**: `Rig2D.ixx` / `Rig2D.cppm` に SmartBoneController, SkinMesh, PoseSnapshot 実装済み

---

## 1. VP — ボーン・コントロール描画

### 1.1 リグ表示の ON/OFF
- `CompositionRenderController` に `showRigOverlay_` フラグ追加
- `LineDebugKind` に `RigBone` / `RigControl` / `RigSkin` を追加
- 既存の `drawRigBone` / `drawRigControl` / `drawRigSkinWireframe` を `drawViewportCanvasOverlay` 内で呼び出す

### 1.2 描画呼び出し
```cpp
// drawViewportCanvasOverlay 内:
if (showRigOverlay_ && rig_) {
    drawRigBone(renderer_, rig_->rootBone(), layerTx, boneColor, 2.0f, zoom);
    for (auto* ctrl : rig_->controls()) drawRigControl(renderer_, ctrl, layerTx);
    if (auto* mesh = rig_->skinMesh()) drawRigSkinWireframe(renderer_, mesh, layerTx, wireColor);
}
```

### 1.3 必要なメンバー（Impl）
```cpp
bool showRigOverlay_ = false;
ArtifactCore::Rig2D* activeRig_ = nullptr;  // 現在編集中のリグ
LayerID riggedLayerId_;                       // リグが紐づくレイヤー
```

---

## 2. VP — ボーン・コントロールのピックとドラッグ

### 2.1 ボーンのピック
- `handleMousePress` で `activeTool == RigSelect` 時にボーンをヒットテスト
- ボーンの線分（bonePos → tip）とクリック位置の距離で判定
- ヒットしたら `selectedBoneId_` に設定

### 2.2 ボーンのドラッグ
- 選択ボーンをドラッグ → `bone->setLocalRotation()` を更新
- ドラッグ中にリアルタイムで `rig_->evaluate()` → `skinMesh->deform()`
- 修飾キー: Shift で角度スナップ（15°刻み）

### 2.3 コントロールのピックとドラッグ
- コントロールの円（半径8px + 2px）内かどうかで判定
- Slider: 水平ドラッグで value を 0.0-1.0 範囲で変化
- Point: 自由ドラッグで QVector2D を更新
- Angle: 円周ドラッグで角度変更

### 2.4 必要なツールタイプ追加
```cpp
enum class ToolType {
    // ... existing ...
    RigSelect,    // リグ編集モード（ボーン選択・ポーズ）
    RigWeight,    // ウェイトペイントモード
};
```

### 2.5 handleMousePress への分岐追加
```cpp
if (activeTool == ToolType::RigSelect && activeRig_) {
    // 1. try hit-test controls
    // 2. try hit-test bones
    // 3. begin drag
}
```

---

## 3. ウェイトペイント

### 3.1 VP オーバーレイ表示
- 選択ボーンのウェイトをカラーマップでメッシュ上に表示
  - 青 (0.0) → 緑 (0.5) → 赤 (1.0)
- 変形後の頂点位置で描画（`deform` 後の座標）

### 3.2 ブラシ描画
- ウェイトペイントツール選択時、キャンバスドラッグでウェイトを塗る
- `brushRadius` / `opacity` / `flow` パラメータ
- ドラッグ軌跡に沿って、半径内の頂点のウェイトを更新
- Ctrl+ドラッグでウェイト減算

### 3.3 ツール操作
- スムーズ（Smooth）: 周囲頂点の平均を取る
- 正規化（Normalize）: 全ボーンの合計 = 1.0
- ミラー（Mirror）: 左右対称ボーンにウェイトコピー

### 3.4 実装方針
- `CompositionRenderController::Impl` に `weightPaintRadius_`, `weightPaintOpacity_`, `selectedBoneIndex_` 追加
- `handleMouseMove` で `activeTool == RigWeight` 時、ブラシ描画処理
- ラスタライズされたウェイトマップをテクスチャとしてオーバーレイ表示

---

## 4. ポーズライブラリパネル

### 4.1 UI レイアウト
```
┌─ Pose Library ──────────────────┐
│ [+New] [Capture] [Delete] [▸▸]  │  ← ツールバー
│ ┌────────────────────────────┐  │
│ │ Category: [Idle       ▾]   │  │
│ │ ┌──────┐ ┌──────┐ ┌──────┐│  │
│ │ │Stand │ │Sit   │ │Wave  ││  │  ← サムネイルグリッド
│ │ │      │ │      │ │      ││  │
│ │ └──────┘ └──────┘ └──────┘│  │
│ │ ┌──────┐ ┌──────┐ ┌──────┐│  │
│ │ │Lean L│ │Lean R│ │Jump  ││  │
│ │ └──────┘ └──────┘ └──────┘│  │
│ └────────────────────────────┘  │
│ Blend: [0.50] ════○════         │  ← ブレンドスライダー
└────────────────────────────────┘
```

### 4.2 機能
- Capture: 現在のリグ状態を PoseSnapshot として保存
- Apply: ダブルクリックで選択フレームに適用
- Blend: スライダーで blendWeight を調整しながら適用
- カテゴリ管理: Idle / Walk / Attack / Custom
- JSON 保存/読み込み

### 4.3 実装
- 新規 Panel クラス: `ArtifactPoseLibraryPanel`
- `QListWidget` またはカスタムグリッドビュー
- サムネイル: 現在のVPからキャプチャ（または簡易骨格SVG）

---

## 5. スマートボーンエディタ

### 5.1 UI レイアウト
```
┌─ SmartBone Editor ──────────────┐
│ Driver: [head_rotate       ▾]   │
│                                  │
│ Keys:                            │
│ Angle │ Targets                  │
│ ──────┼──────────────────────    │
│ -30°  │ ear_L.rot=45, eye_X=5   │
│  0°   │ ear_L.rot=0,  eye_X=0   │
│ +30°  │ ear_L.rot=-45,eye_X=-5  │
│                                  │
│ [+Add Key] [Remove Key]          │
│                                  │
│ ═══════●═══════════════  (0°)    │  ← カーブプレビュー（縦=ターゲット値, 横=driver angle）
└──────────────────────────────────┘
```

### 5.2 機能
- 駆動ボーン選択
- キー追加/削除
- ターゲットボーンとチャンネル（rotation.x, scale.y など）の指定
- キー間の補間プレビュー
- driverAngle スライダーで全キーのプレビュー

### 5.3 実装
- 新規 Panel: `ArtifactSmartBoneEditor`
- 駆動ボーン選択 → `SmartBoneController::addKey(driverAngle, targets)`
- カーブプレビューに `QPainter` で折れ線描画

---

## 6. リグ階層パネル

### 6.1 UI（ツリー表示）
```
┌─ Rig Hierarchy ─────────────────┐
│ [+Bone] [+Control] [+IK]        │
│                                  │
│ ▼ hip                            │
│   ▼ spine                        │
│     ▼ neck                       │
│       ▼ head                     │
│     ▼ arm_L                      │
│       ▼ elbow_L                  │
│         ▼ hand_L                 │
│   ▼ leg_L                        │
│     ▼ knee_L                     │
│       ▼ foot_L                   │
│                                  │
│ ── Controls ──                   │
│ ● head_ctrl     (Point)          │
│ ● hand_L_ctrl   (Point)          │
│ ● blink         (Slider)         │
│                                  │
│ ── Constraints ──                │
│ ● arm_L_IK      (TwoBoneIK)      │
│ ● head_aim      (Aim)            │
└──────────────────────────────────┘
```

### 6.2 操作
- 右クリック → Rename / Delete / Add Child / Add IK
- ドラッグ＆ドロップで親子関係変更
- 選択 → VP 上でハイライト

### 6.3 実装
- `QTreeWidget` ベース
- `Rig2D` からボーン/コントロール/制約をツリーに展開
- 選択変更 → `CompositionRenderController` の `selectedBoneId_` と連携

---

## 7. タイムライン連携

### 7.1 ボーンキーフレームの表示
- 各ボーンのトラックにキーフレームを表示
- 現在の `Bone2D::keyframes_` は既に `AnimatableValueT<BoneTransform>` として存在 → キーフレーム表示可能

### 7.2 キーフレーム操作
- ボーン選択 → ポーズ変更 → 自動キーフレーム（または手動）
- キーフレームのコピー/ペースト
- カーブエディタでの補間編集

### 7.3 必要な対応
- `ArtifactTimelineTrackPainterView` にリグトラック表示モード追加
- `LayerType::Rig` またはリグ所有レイヤーの特殊トラック

---

## 8. リグレイヤーの作成・管理

### 8.1 リグの作成
- メニュー: `Layer → New → Rig Layer`
- 空の Rig2D を持つレイヤーを作成
- ソース画像レイヤーを指定（SkinMesh のテクスチャ）

### 8.2 リグレイヤーの描画
- `ArtifactIRenderer` に `drawSkinnedMesh` 追加（または既存の `drawMesh` を活用）
- `SkinMesh::deform()` → 変形済み頂点 → テクスチャマッピング → 三角形描画

### 8.3 実装
- `ArtifactAbstractLayer` に `ArtifactCore::Rig2D* rig()` / `void setRig()` 追加
- または `ArtifactRigLayer` 新設
- `draw()` 内で `rig_->evaluate(time)` → `skinMesh->deform()` → 描画

---

## 9. ツールバー / メニュー統合

### 9.1 ツールバー追加
```
既存ツールバーに Rig セクション追加:
[⟲ Select] [🖌 Weight] [⏺ Pose] [🎬 Animate]
```

### 9.2 右クリックメニュー（VP上）
- リグ編集中、ボーン右クリック → Add Child Bone / Add IK / Add Control / Delete
- コントロール右クリック → Edit Range / Add SmartBone Driver

### 9.3 キーボードショートカット
| キー | 操作 |
|------|------|
| `E` | 回転ツール（ボーン選択時はボーンを回転） |
| `W` | 移動ツール（コントロール選択時はPoint移動） |
| `B` | ウェイトペイントブラシ |
| `Ctrl+Tab` | リグ編集モード ON/OFF |

---

## 10. 優先順位

| 優先度 | タスク | 理由 |
|--------|--------|------|
| **P0** | 1. ボーン描画のON/OFFと呼び出し | 何も見えないと始まらない |
| **P0** | 2. ボーン/コントロールのピック | 操作できないと意味がない |
| **P1** | 8. リグレイヤーの描画 | スキンメッシュが表示されないと変形が見えない |
| **P1** | 3. ウェイトペイント | スキニング品質に直結 |
| **P1** | 6. リグ階層パネル | ボーン構造の管理に必須 |
| **P2** | 2. ボーン/コントロールのドラッグ | ポージングに必須 |
| **P2** | 5. スマートボーンエディタ | Spine互換の最重要差別化機能 |
| **P2** | 4. ポーズライブラリ | アニメーターの生産性向上 |
| **P3** | 7. タイムライン連携 | キーフレーム編集 |
| **P3** | 9. ツールバー/メニュー統合 | UX 完成度 |

---

## 11. 新規作成が必要なファイル（最小限）

| ファイル | 説明 |
|----------|------|
| `Artifact/include/Widgets/Rig/ArtifactRigHierarchyPanel.ixx` | リグ階層ツリー |
| `Artifact/src/Widgets/Rig/ArtifactRigHierarchyPanel.cppm` | 同上実装 |
| `Artifact/include/Widgets/Rig/ArtifactPoseLibraryPanel.ixx` | ポーズライブラリ |
| `Artifact/src/Widgets/Rig/ArtifactPoseLibraryPanel.cppm` | 同上実装 |

スマートボーンエディタは初期段階では PropertyEditor で代用可能。
ウェイトペイントは CompositionRenderController に直接実装で UIファイル不要。

既存ファイルへの変更：
- `CompositionRenderController`: ボーン/コントロール描画呼び出し、ピック、ドラッグ、ウェイトペイント
- `ArtifactAbstractLayer` または新設 RigLayer: Rig2D の所有
- `ArtifactToolManager.ixx` / `.cppm`: ToolType 追加
- `ArtifactMainWindow`: リグパネルの Dock 追加
