# マイルストーン: 3D Model Review in Contents Viewer

> 2026-03-28 作成

## 目的

`Contents Viewer` で `OBJ` / `FBX` を確認し、素材確認の導線として実用化する。

このマイルストーンは、3D モデルを単に開ける状態から、`view / inspect / compare` の入り口として扱える状態へ進める。

---

## 背景

現状、3D モデルの読み込み経路はすでにある。

- `FileTypeDetector` が `obj` / `fbx` を `Model3D` として認識する
- `MeshImporter` が `obj` / `fbx` を読み込める
- `Artifact3DModelViewer` が Diligent ベースで表示できる
- `ArtifactContentsViewer` から 3D model viewer を開ける

ただし、まだ次の点が薄い。

- 3D モデルを「確認対象」として見せる文脈が弱い
- `Solid / Wireframe / Solid + Wire` の切替が viewer 側の主役になっていない
- `Contents Viewer` の metadata と model state の見え方が足りない
- review 用の flow として `source / final / compare` が 3D にどう効くか未整理

---

## 方針

### 原則

1. 3D モデルは `Contents Viewer` の中で確認する
2. 編集はしない。あくまで review に徹する
3. OBJ / FBX を最初のターゲットにする
4. モデルの view mode を分かりやすい状態として出す
5. Asset Browser / Project View から自然に開けるようにする

### 対象

- `.obj`
- `.fbx`
- 将来の `.gltf` / `.glb`

---

## Phase 1: Model Ingest And Open Flow

### 目的

3D モデルを `Contents Viewer` で確実に開けるようにする。

### 作業項目

- OBJ / FBX の file type 判定維持
- double click / selection から viewer open
- missing file の表示整理
- model load failure の表示整理

### 完了条件

- `Contents Viewer` で OBJ / FBX を開ける
- 失敗時に理由が分かる

---

## Phase 2: Model Inspection Surface

### 目的

モデル確認に必要な状態を viewer 上に出す。

### 作業項目

- vertices / polygons の表示
- loaded model path の表示
- display mode の状態表示
- `reset view` の導線整理

### 完了条件

- 何を読み込んだか分かる
- どういう見え方か分かる

---

## Phase 3: View Mode Integration

### 目的

`Contents Viewer` 側から model viewer の表示モードを切り替えられるようにする。

### 作業項目

- `Solid`
- `Wireframe`
- `Solid + Wire`
- 3D モデル時の mode button 可視化

### 完了条件

- モデル確認時に見え方を切り替えられる
- mode と preview が一致する

---

## Phase 4: Review Workflow Integration

### 目的

Project / Asset から 3D review に飛びやすくする。

### 作業項目

- Asset Browser から open
- Project View から open
- current selection と viewer の連動
- review 後の戻り導線整理

### 完了条件

- 素材選択から review までが短い
- model review が他の preview と同じ導線で開ける

---

## Phase 5: Future Compare Hooks

### 目的

将来の compare / before-after を入れやすくする。

### 作業項目

- source / final の mode 契約整理
- compare 候補の表示ルール
- render output review との接続余地

### 完了条件

- 3D モデルにも review 拡張を後付けしやすい

---

## 連携先

- `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cpp`
- `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- `ArtifactCore/src/File/FileTypeDetector.cppm`
- `ArtifactCore/src/Geometry/MeshImporter.cppm`
- `Artifact/src/AppMain.cppm`

---

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

## Current Status

**Status:** Phase 1〜4 implemented, Phase 5 compare hooks and runtime verification pending

- `Contents Viewer` で `Model3D` を開いて、solid preview 寄りの表示で確認しやすくした
- `Artifact3DModelViewer` の status で backend / vertex / polygon / bounds を追いやすくした
- preview が失敗しても backend / error を status で追いやすくした
- `Reset 3D` と `Ctrl+0` で view reset を押しやすくした
- status 表示は camera の細部を削って短くした
- Asset Browser の context menu から preview を開ける
- Project View の footage context menu からも preview を開ける
- Project View の `Open` も footage では preview と同じ導線に揃えた
- 次は review の導線をもう少し軽くしつつ、`source / final / compare` の 3D 表示ルールを整える段階

---

## Next Execution Slice

Phase 2 では、viewer に出す情報を「確認に必要な最小状態」に絞る。

### Phase 2A の着手点

1. loaded model path と backend の表示位置を固定する
2. vertices / polygons / bounds は review に必要な範囲だけ残す
3. display mode の状態表示を `Solid / Wireframe / Solid + Wire` に寄せる
4. `reset view` の導線を model review の文脈に合わせて短くする

### Phase 2 完了条件

- 何を読み込んだか分かる
- どういう見え方か分かる
- review 用の情報と雑多な状態が混ざらない

### Phase 3A の着手点

1. `source / final / compare` の 3D 表示ルールを決める
2. mode button の可視化条件を `Model3D` に限定する
3. comparison では viewer status を増やしすぎない
4. compare の導線は Phase 2 の簡素な表示を崩さない

### Phase 3 完了条件

- モデル確認時に見え方を切り替えられる
- mode と preview が一致する
- comparison のルールが viewer で読める

### Phase 4 への前提

- Project / Asset からの open 導線は既にある
- review workflow は表示状態と mode ルールが固まってから詰める

## Static audit follow-up (2026-07-25)

- Phases 1-4 are supported by the current `ArtifactContentsViewer` and `Artifact3DModelViewer` source: Model3D routing, importer/backend/error state, mesh inspection metadata, display-mode controls, reset view, and the existing Asset/Project open paths are present.
- The generic Contents Viewer compare surface (`Wipe`/`Split`/`Difference`) exists, but its Difference mode is explicitly restricted to two image sources and the compare canvas is not a 3D scene/model compare surface. Therefore Phase 5 remains incomplete for 3D models.
- Source/final role labels and compare routing exist at the generic viewer level, but model-specific camera/mesh comparison and annotation/diagnostic hooks are not confirmed.
- No build or runtime verification was performed under the repository policy.
