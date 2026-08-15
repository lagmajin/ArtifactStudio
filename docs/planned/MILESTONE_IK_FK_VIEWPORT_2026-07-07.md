# M-RIG-2 IK/FK Switch + Pole Vector Viewport Milestone

作成日: 2026-07-07
**最終更新:** 2026-08-15
ステータス: Rig core／骨表示・選択・FK相当ドラッグは実装済み、IK/FK切替とPole Vector UIが未完了
対象: `ArtifactCore/include/Rig/Rig2D.ixx`,
      `ArtifactCore/src/Rig/Rig2D.cppm`,
      `ArtifactCore/include/Rig/RigController2D.ixx` (計画),
      `ArtifactCore/src/Rig/RigController2D.cppm` (計画),
      `Artifact/src/Layer/ArtifactAbstract2DLayer.cppm`,
      `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`,
      `Artifact/src/Widgets/Render/TransformGizmo.cppm`
位置づけ: 既存の `Rig2D`（Bone2D 階層 + TwoBoneIK + CCD IK）をビューポート直接操作に接続し、
          Maya 風の IK/FK 切替とポールベクトルハンドル編集を実現する。
参照:
- `ArtifactCore/include/Rig/Rig2D.ixx`（実装済み: Bone2D, TwoBoneIK, CCD IK, RigConstraint）
- `ArtifactCore/docs/MILESTONE_2D_RIG_SYSTEM_2026-04-15.md`（Phase 1-5 設計）
- `docs/planned/2D_RIG_CORE_CONTRACT_2026-04-29.md`（ID ベース骨操作 API, JSON 保存復元済み）
- `ArtifactCore/include/ImageProcessing/OpenCV/OpenCVPuppetEngine.ixx`
- `Artifact/src/Widgets/Render/TransformGizmo.cppm`（ギズモ描画パターン）
- `docs/planned/MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md`（Puppet Tool ⚠️）

---

## 1. 目的

`MILESTONE_2D_RIG_SYSTEM_2026-04-15.md` で設計された 2D リグシステムのうち、
**ビューポート直接操作** に直結する部分を実装する。

現在の `Rig2D` は `ArtifactCore` 側で自己完結しており、ボーン階層、TwoBoneIK (`solveTwoBoneIK()`)、
CCD IK (`solveCCDIK()`)、制約（Parent, MapRange, Aim, TwoBoneIK）、
コントロール（Slider, Point, Angle）のコード実体が存在する。

しかし、これらをビューポート上で操作する UI が一切接続されていない。
本 milestone は、既存の Rig2D コアを **ビューポート上の直接編集** に接続し、
次の Maya 風操作を実現する:

- ボーンをビューポート上で選択・回転（FK モード）
- IK ターゲットをドラッグしてエフェクタを動かす（IK モード）
- ポールベクトル（膝/肘の曲げ方向）をハンドルで操作
- IK/FK スイッチのシームレスな切替
- Stretchy Limb（Rubber Hose）のビューポート表示

> 重要: Puppet Tool（`OpenCVPuppetEngine` を使う MLS/TPS メッシュ変形）とは別系統。
> 本 milestone は Bone ベースのリグ操作に特化する。Puppet Tool の実装深度不足は別 milestone。

> 重要: コアロジック（FABRIK IK, StretchyLimb）は `ArtifactCore` に追加。
> ビューポート描画とハンドル操作は `Artifact` 側の RenderController / Gizmo に追加。

---

## 2. 現状整理 (2026-07-07 基準)

### 2.1 既存資産

| 資産 | ファイル | 内容 |
|---|---|---|
| `Rig2D` | `ArtifactCore/include/Rig/Rig2D.ixx` | Bone2D 階層、addBone/removeBone、JSON 保存復元 |
| `TwoBoneIK` | `Rig2D.ixx:423` | `solveTwoBoneIK(bone1, bone2, effector, target)` 実装済み |
| `CCD IK` | `Rig2D.ixx:424` | `solveCCDIK(effector, target, iterations, tolerance)` 実装済み |
| `TwoBoneIKConstraint2D` | `Rig2D.ixx:339` | upper/lower/effector/target bone + poleAngle |
| `RigControl2D` | `Rig2D.ixx:102` | Slider, Point, Angle コントロール |
| `RigController2D` | `MILESTONE_2D_RIG_SYSTEM` Phase 2 設計 | Joystick 'n Sliders 相当。未実装 |
| `TransformGizmo` | `Artifact/src/Widgets/Render/TransformGizmo.cppm` | 2D 変形ギズモ。ドラッグハンドル描画パターン |
| `Bone2D::evaluate(time)` | `Rig2D.ixx:83` | 評価入口。現時点では静的ローカル変換を返す |

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| FABRIK IK（多関節チェーン） | 設計のみ。コードなし | 尻尾・脊椎・髪の IK 不可 |
| StretchyLimb（Rubber Hose） | 設計のみ。コードなし | 弾性肢のベジェ変形不可 |
| Pole Vector ハンドル UI | なし | TwoBoneIK の膝/肘方向を操作不可 |
| IK/FK 切替状態管理 | なし | ボーン単位のモード切替不可 |
| Bone のビューポート描画 | なし | ボーンが見えない |
| IK ターゲットのビューポートドラッグ | なし | ターゲットをドラッグ操作不可 |
| `ArtifactBoneLayer` | 設計のみ。未実装 | ボーンをレイヤーとして扱えない |
| `ArtifactIKTargetLayer` | 設計のみ。未実装 | IK エフェクタ/ポールターゲットの独立レイヤーなし |
| RigController2D ビューポート統合 | 設計のみ | Joystick のビューポートドラッグ不可 |

### 2.3 コード検索結果

- `solveTwoBoneIK` → `Rig2D.ixx` に実装あり
- `solveCCDIK` → `Rig2D.ixx` に実装あり
- `solveFABRIK` → 0 hit（設計のみ）
- `StretchyLimb` → 0 hit（設計のみ）
- `ArtifactBoneLayer` → 0 hit（設計のみ）
- `IKFKSwitch` → 0 hit
- `PoleVector` → TwoBoneIKConstraint2D に `poleAngle_` メンバあり。UI 露出なし

### 2.4 Current implementation audit (2026-08-15)

現行コードを確認した結果、当初のコード検索結果から core と viewport の基礎部分は大きく進んでいる。

| 軸 | 現行コードで確認できた実装 | 判定 |
|---|---|---|
| IK core | `Rig2D::solveTwoBoneIK`／`solveCCDIK` に加え、`solveFABRIK` と `StretchyLimbDescriptor`／`computeStretchyMidpoint` が存在 | 実装済み。専用の収束テストは未実行 |
| Rig evaluation / controls | `Rig2D::evaluate`、constraints、`RigController2D`、Point／Slider／Angle controls、skin mesh と pose snapshot が存在 | 部分〜実装済み |
| Bone viewport | RenderController が bone の global matrix から線・原点・階層 panel・選択ハイライトを描画し、hit test を持つ | 実装済み |
| Direct editing / Undo | `RigSelect` で bone 回転・control 値をドラッグし、release 時に `RigBoneTransformUndoCommand`／`RigControlValueUndoCommand` を積む | FK相当の編集は実装済み |
| Rig weight workflow | `RigWeight` の選択・paint・normalize／smooth／mirror と `RigSkinWeightsUndoCommand` を確認 | 実装済み。IK pose編集とは別責務 |
| IK/FK / pole | `IKFKMode`、bone 単位 blend、pole vector handle、IK target／pole target layer は未確認。`TwoBoneIKConstraint2D` の pole angle は存在するが viewport 導線なし | 未完了 |
| Persistence | `ArtifactAbstract2DLayer` が `rig2D` を layer JSON に保存／復元。Rig core の bones／constraints／controls／skin mesh も JSON 化 | 実装済み。IK/FK専用状態の再読込は未確認 |

**更新後の判定**: FABRIK／StretchyLimb と既存の bone/control viewport editing は成立しているが、M-RIG-2 の中心である IK/FK 切替、pole vector 操作、IK target の直接ドラッグ、専用 layer／Inspector 導線は未完了。次は既存 TwoBoneIK の pole angle を UI に誤流用せず、IK target／pole target の責務と状態保存形式を先に確定する。



## 3. Scope / Non-Goals

### Scope

- FABRIK IK ソルバー実装（`Rig2D` に追加）
- StretchyLimb（Rubber Hose）実装（`Rig2D` に追加）
- `PoleVector` を `TwoBoneIKConstraint2D` に追加し、ビューポートハンドルで操作可能に
- IK/FK 切替状態管理（Bone 単位）
- Bone のビューポート描画（菱形/矢印ギズモ）
- IK ターゲット + ポールターゲットのビューポートドラッグ操作
- `ArtifactBoneLayer` / `ArtifactIKTargetLayer` の最小実装

### Non-Goals

- `ArtifactRigControllerLayer`（Joystick 'n Sliders）→ 別 milestone
- `RigSkinningEngine` / GPU Skinning → Phase 3 延期
- Auto-Rig テンプレート → Phase 5 延期


---

## 4. Phases

### Phase 1: FABRIK IK + StretchyLimb コア実装 (P0, 2 セッション)

- `Rig2D.cppm` に `solveFABRIK()` を実装
  - `QList<Bone2D*> chain`（ルート→先端順）
  - 反復回数 20、トレランス 0.5f / ポールターゲットによる曲げ方向制御
- `StretchyLimbDescriptor` + `computeStretchyMidpoint()` を実装

**Done criteria:**
- 5 ボーンチェーンに FABRIK 適用し、ターゲット位置に収束
- StretchyLimb の伸縮が restLength 比で正しく計算 / 単体テスト完備

### Phase 2: PoleVector + IK/FK 状態管理 (P0, 1 セッション)

- `TwoBoneIKConstraint2D` に `poleVector_: std::optional<QVector2D>` 追加
- `solveTwoBoneIK()` に poleVector 引数追加
- `IKFKMode` enum: `FK`, `IK`, `Blend` (0.0〜1.0)
- `Bone2D` に `ikFkMode_`, `setIKFKMode()`, `ikFkBlend_` 追加

**Done criteria:**
- FK モードで Bone 回転が IK の影響を受けない
- IK モードでターゲット位置から Bone 角度が自動計算
- Blend=0.5 で FK と IK の中間姿勢 / poleVector で膝方向指定

### Phase 3: Bone ビューポート描画 (P0, 2 セッション)

- `CompositionRenderController` に Bone gizmo 描画追加
  - 菱形/矢印形状 + 親子接続線 + 選択ハイライト
- `TransformGizmo` ベースの Bone 回転ハンドル（FK モード時）
- IK ターゲットハンドル（十字形、ドラッグで移動）
- ポールベクトルハンドル（放射ライン + 円形ハンドル）

**Done criteria:**
- ボーンがビューポート上に可視化
- FK モードで Bone 選択→ドラッグ回転 / IK モードでターゲットドラッグ→チェーン追従
- ポールベクトルドラッグで膝/肘の曲げ方向変更

### Phase 4: ArtifactBoneLayer / ArtifactIKTargetLayer (P1, 2 セッション)

- `ArtifactBoneLayer` を `ArtifactAbstractLayer` 派生で実装
- `ArtifactIKTargetLayer` を実装（IK エフェクタ + ポールターゲット）
- レイヤーパネル登録と選択同期

**Done criteria:**
- Bone/IKTarget Layer をタイムラインで選択可能
- Inspector に IK/FK 切替 + pole target 指定
- IK Target Layer の位置キーフレームがタイムラインに表示

### Phase 5: Undo + 永続化 + Diagnostics (P1, 1 セッション)

- Bone 操作の undo 対応
- project JSON に `rig2D.ikFkMode`, `rig2D.poleVector` 追加
- 旧プロジェクト後方互換 / Problem View 診断

**Done criteria:**
- 1 undo 復元 / 保存→再読込で全 Rig 状態復元
- 旧プロジェクトが開ける / Problem View に Rig 診断表示

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_2D_RIG_SYSTEM_2026-04-15.md` | 上位設計。本 milestone は Phase 1/2/4 の一部を実装 |
| `2D_RIG_CORE_CONTRACT_2026-04-29.md` | ID ベース API の上に IK/FK 切替を追加 |
| `MILESTONE_PUPPET_ENGINE_2026-03-29.md` | 別系統。Puppet はメッシュ変形、本 milestone は Bone 操作 |
| `MILESTONE_VIEWPORT_DESIGN_AUDIT_2026-07-04.md` | Puppet Tool ⚠️。本 milestone で Bone ベース操作を先行実装 |

---

## 6. リスクと未解決論点

1. **FABRIK と TwoBoneIK の競合**。同一 Bone チェーンに両方を適用した場合、FK/IK モードで排他制御
2. **ビューポート座標系**。Bone のローカル座標とビューポートのワールド座標の変換。`TransformGizmo` の既存座標変換を流用
3. **パフォーマンス**。30 Bone × FABRIK 20 反復のコスト
4. **Bone の所有権**。`Rig2D` が Bone を所有し、`ArtifactBoneLayer` は参照のみ
5. **サブモジュール境界**: `ArtifactCore/include/Rig/Rig2D.ixx` に FABRIK, StretchyLimb, IKFKMode 追加。`ArtifactWidgets` は触らない

---

## 7. Done Criteria (全体)

- FABRIK IK が多関節チェーンで動作 / StretchyLimb が伸縮率に応じて変形
- IK/FK/Blend 切替が Bone 単位で動作 / ポールベクトル操作可能
- Bone がビューポート上に可視化 / 選択・回転可能（FK）/ ターゲットドラッグ可能（IK）
- `ArtifactBoneLayer` / `ArtifactIKTargetLayer` がレイヤーパネルに表示
- 全操作 undo 対応 / project JSON 保存→復元
- 新規 `QImage` / `setStyleSheet` / 新規 signal-slot なし
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-07-07: 初版作成。既存 `Rig2D` コアをビューポート直接操作に接続する設計。

- Mesh Warp（Puppet Tool）→ `OpenCVPuppetEngine` 側
