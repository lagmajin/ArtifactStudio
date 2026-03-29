# マイルストーン: Composition Motion Path Overlay

> 2026-03-28 作成

## 目的

`ArtifactCompositionEditor` の viewport 上で、選択中レイヤーの motion path を AE 風に見える状態へ持っていく。

狙いは次の 4 つ。

1. キーフレーム付き position の軌跡をすぐ読めるようにする
2. 昔の position を点で表示し、現在位置との関係を分かりやすくする
3. current frame の位置だけを強調して、再生中の挙動を追いやすくする
4. path 表示を toggle できるようにして、作業文脈を壊さない

この文書は「タイムライン上の keyframe 編集」とは分けて、**コンポジション viewport 上の可視化** に絞る。

---

## 現状

今のコードベースには、motion path 表示の土台になりうるものがすでにある。

持っているもの:

- `AnimatableTransform3D` の position keyframe 時刻と補間値
- `ArtifactIRenderer` / `PrimitiveRenderer2D` の thick line / dashed line 描画
- `ArtifactCompositionRenderWidget` の overlay / gizmo 描画経路
- `ArtifactCompositionEditor` の current layer / selection 連携

一方で、まだ次のものがない。

- 選択レイヤーの motion path を viewport に重ねる専用 overlay
- keyframe 点を path 上に打つ表示
- current frame に対応する位置の強調表示
- 2D / 3D の表示切り替え方針
- overlay の on/off 導線

---

## 方針

### 原則

1. path の source of truth は `AnimatableTransform` / property keyframe に置く
2. viewport は path を読むだけにして、編集の正本は変えない
3. まずは selected layer の `position` に限定する
4. 2D と 3D は描画経路を揃えつつ、先に 3D を安定化する

### 想定する見え方

- キーフレーム点を小さい dot で表示
- キーフレーム間を線で結ぶ
- current frame の点だけ少し強調する
- 選択レイヤーが複数ある場合は主選択を優先表示する
- 必要なら dashed / faint 表示で文脈を保つ

---

## 非目的

- Graph Editor の全面実装
- tangent / bezier handle の完全編集
- rotation / scale を含む全 property の motion path 一括表示
- viewport 上で path 自体を直接編集する機能
- motion tracking と同一の実装にまとめること

---

## 対象候補

- `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `Artifact/src/Render/ArtifactIRenderer.cppm`
- `Artifact/include/Render/ArtifactIRenderer.ixx`
- `Artifact/src/Render/PrimitiveRenderer2D.cppm`
- `Artifact/include/Render/PrimitiveRenderer2D.ixx`
- `ArtifactCore/include/Animation/AnimatableTransform3D.ixx`
- 必要に応じて `ArtifactCore/include/Animation/AnimatableTransform2D.ixx`
- 必要に応じて `Artifact/src/Widgets/ArtifactTimelineWidget.cpp`

---

## Phases

### Phase 1: Path Data Extraction

- 目的:
  - 選択レイヤーの motion path を描くためのデータを集める

- 作業項目:
  - position keyframe 時刻を列挙する
  - 現在 frame ごとの position をサンプルする
  - 連続点を path として扱えるようにする
  - current frame の位置を 1 点として取得する

- 完了条件:
  - 選択 layer の position path を描くための点列が取得できる

### Phase 2: Viewport Overlay Rendering

- 目的:
  - motion path を viewport 上へ重ね描きする

- 作業項目:
  - path 線を描く
  - keyframe dot を描く
  - current frame の点を強調する
  - selected layer の overlay を gizmo と競合しないように積む

- 完了条件:
  - viewport で motion path が見える
  - path と gizmo が同時に破綻しない

### Phase 3: Toggle / Visibility / Density Control

- 目的:
  - path が邪魔なときにすぐ消せる
  - 点が多いときも読みやすさを保つ

- 作業項目:
  - overlay on/off
  - dot density の間引き
  - path が長いときの簡略化
  - zoom に応じた表示密度の調整

- 完了条件:
  - motion path が常時表示でも実用に耐える

### Phase 4: 2D / 3D Parity

- 目的:
  - 2D レイヤーでも 3D レイヤーでも同じ考え方で追えるようにする

- 作業項目:
  - 2D transform への接続
  - 3D transform への接続
  - layer type ごとの path source を整理する
  - current layer / selected layer の表示規則を揃える

- 完了条件:
  - 主要な transform layer で motion path を確認できる

### Phase 5: Navigation / Diagnostics

- 目的:
  - path を見ながら目的の keyframe へ辿り着きやすくする

- 作業項目:
  - keyframe 点クリックで 해당 frame へ seek
  - current frame と最寄り点の関係を示す
  - path 表示中の selection 状態を分かりやすくする
  - タイムライン側の選択と view 側の選択を食い違わせない

- 完了条件:
  - motion path を見て、そのまま編集や確認に移れる

---

## Implementation Tasks

### Phase 1 Task Breakdown

1. `AnimatableTransform3D` から path 用の点列を取る
   - keyframe 時刻の列挙
   - current frame の sample 値取得
2. layer ごとの path source を決める
   - selected layer の `transform.position`
   - 既存の property keyframe と矛盾しない参照にする
3. current frame の sample を 1 点として取る
   - 再生中でも追従できる最小単位を作る

### Phase 2 Task Breakdown

1. `ArtifactCompositionRenderWidget` に overlay 描画を追加する
   - gizmo の手前 / 奥のどちらに積むか決める
   - selected layer のみ描くか、複数選択対応にするか決める
2. path 線と dot を描く
   - `drawThickLineLocal` / `drawDashedLineLocal` を使い分ける
   - keyframe 点を小さく見やすく打つ
3. current frame の点だけ強調する
   - 色 / サイズ / alpha の差をつける

### Phase 3 Task Breakdown

1. overlay toggle を editor controls に載せる
   - 表示の有無をすぐ切り替えられるようにする
2. zoom 連動の密度制御を入れる
   - 遠いときは間引く
   - 近いときは詳細表示する
3. 長い path の簡略化を入れる
   - 点数の多い layer でも重くなりすぎないようにする

### Phase 4 Task Breakdown

1. 2D レイヤーの source を接続する
   - 位置のみでも先に path 化する
2. 3D レイヤーの source を接続する
   - `AnimatableTransform3D` を優先する
3. layer type 別の見せ方を整理する
   - `image / video / text / null / camera` で不要な path を出さない

### Phase 5 Task Breakdown

1. keyframe 点クリックの jump 導線を作る
   - click でその frame へ seek
2. path 上の nearest point を視覚化する
   - current frame の近傍を分かりやすくする
3. タイムライン選択と viewport selection の同期を崩さない
   - path overlay が selection state を独立に持ちすぎないようにする

---

## Current Status

- `AnimatableTransform3D` は position keyframe の時刻と値を取り出せる
- `PrimitiveRenderer2D` に dashed line が追加済みで、path overlay の描画基盤がある
- `ArtifactCompositionRenderWidget` は gizmo と viewport overlay を持っているので、motion path を重ねる入り口になれる
- `Timeline Keyframe Editing` 側で keyframe の見え方が伸び始めているため、viewport 側の path 表示を足すと文脈がつながる

---

## Suggested Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

