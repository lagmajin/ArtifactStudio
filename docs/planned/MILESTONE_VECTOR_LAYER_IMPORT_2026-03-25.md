# マイルストーン: Vector / SVG Layer Import

> 2026-03-25 作成

## 目的

SVG のようなベクター素材を、単なる画像ではなく **レイヤーとして取り込んで編集できる** ようにする。

このマイルストーンは「読み込める」だけではなく、

- composition に置ける
- transform / opacity / keyframe と一緒に扱える
- 再保存しても source を失わない
- software / Diligent の両方で破綻しにくい

ところまでを狙う。

---

## 方針

### 原則

1. SVG を最初の対象にする
2. source は可能ならベクターデータとして保持する
3. 表示はレンダリング結果、編集は layer と property で扱う
4. 既存の image layer を壊さずに別系統として追加する
5. まずは実務で使える最小機能を通す

### 想定する層

- `AssetBrowser` からの import
- `Project Service` への登録
- 新しい vector layer type
- `Composition Viewer` での表示
- `Property / Keyframe` との連携

---

## Phase 1: SVG Ingest

### 目的

SVG をプロジェクト資産として取り込み、layer に変換する入口を作る。

### 機能

- file drop / import dialog から SVG を受ける
- project asset として記録する
- layer factory から SVG layer を生成する
- missing / relink の対象に含める

### 完了条件

- `AssetBrowser` から SVG を追加できる
- `Project View` に SVG asset が見える
- composition に SVG layer を作成できる
- source path が保存される

### 連携先

- `ArtifactAssetBrowser`
- `ArtifactProjectService`
- `ArtifactProjectModel`
- `ArtifactLayerFactory`

---

## Phase 2: Vector Layer Representation

### 目的

SVG を「画像を貼る layer」ではなく、vector source を持つ layer として扱う。

### 機能

- source path / embedded data の保持
- preserve aspect ratio
- fit-to-layer / fit-to-comp
- opacity / blend / visibility
- basic transform property 連携

### 完了条件

- layer の property から SVG source を追える
- composition 再読込後も source が復元される
- `AbstractProperty` に乗る属性が壊れない

### 連携先

- `ArtifactAbstractLayer`
- `ArtifactImageLayer`
- `AbstractProperty`
- `AnimatableTransform3D`

---

## Phase 3: Rendering Path

### 目的

SVG を表示できる描画経路を安定化する。

### 機能

- software backend での SVG rasterization
- Diligent backend での preview texture 化
- zoom 時の品質維持
- bounds / crop / clip の整合

### 完了条件

- 拡大しても破綻が少ない
- comp 外にはみ出した場合の見え方が安定する
- software と Diligent の表示差が大きくない

### 連携先

- `ArtifactIRenderer`
- `CompositionRenderController`
- `PrimitiveRenderer2D`
- `QtSvg`

---

## Phase 4: Editing and Properties

### 目的

vector layer を他の layer と同じ編集体験に乗せる。

### 機能

- `F` focus
- transform gizmo
- property editor
- opacity / color / stroke 系の基本項目
- keyframe 対応

### 完了条件

- SVG layer を選択して普通に編集できる
- property 変更が keyframe に乗る
- timeline 上で layer として扱える

### 連携先

- `ArtifactCompositionEditor`
- `TransformGizmo`
- `ArtifactPropertyWidget`
- `ArtifactTimelineWidget`

---

## Phase 5: Persistence and Recovery

### 目的

保存再読込と再リンクを実用にする。

### 機能

- source path の保存
- missing asset の検出
- relink
- export 時の再利用
- thumbnail / preview cache

### 完了条件

- 保存後に SVG layer が壊れない
- missing source を UI で追える
- relink 後に layer が復活する

### 連携先

- `ArtifactProjectService`
- `ArtifactProjectModel`
- `ArtifactAssetBrowser`
- `ArtifactAbstractLayer`

---

## Phase 6: Vector Expansion

### 目的

SVG 以外の vector-like input に広げられる土台を作る。

### 候補

- PDF
- AI / EPS の変換経路
- Lottie / JSON vector animation
- icon font / shape asset

### 完了条件

- SVG 以外を後付けしやすい
- vector asset 取り込みの共通基盤がある

---

## 優先順位

### 最優先

1. SVG Ingest
2. Vector Layer Representation
3. Rendering Path

### 次点

1. Editing and Properties
2. Persistence and Recovery
3. Vector Expansion

---

## 実装順の提案

1. `ArtifactLayerFactory` に SVG layer type を追加する
2. `Project Service` で import / relink を通す
3. software backend で rasterization を確定する
4. `Composition Viewer` で表示・選択・focus を通す
5. property / keyframe に接続する

---

## 関連文書

- `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`
- `docs/planned/MILESTONE_LONG_RUNNING_FEATURE_WORKSTREAMS_2026-03-25.md`
- `Artifact/docs/MILESTONE_ASSET_SYSTEM_2026-03-12.md`
- `Artifact/docs/MILESTONE_COMPOSITION_EDITOR_2026-03-21.md`
- `docs/planned/MILESTONES_BACKLOG.md`

