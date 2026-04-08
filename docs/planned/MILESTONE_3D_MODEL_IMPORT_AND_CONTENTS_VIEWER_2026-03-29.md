# マイルストーン: 3D Model Import and Contents Viewer Integration

> 2026-03-29 作成

## 目的

`ArtifactCore` の 3D モデル読み込み経路を整え、`Contents Viewer` で `OBJ` / `FBX` を確実に確認できる状態へ持っていく。

このマイルストーンは、既存の `MeshImporter` と `Artifact3DModelViewer` を前提にして、`ufbx` / `tinyobjloader` の使い分けと viewer 側の導線を整理する。

---

## 背景

すでに基盤はある。

- `FileTypeDetector` は `obj` / `fbx` / `gltf` / `glb` を `Model3D` として認識する
- `MeshImporter` は `ufbx` を使って `obj` / `fbx` を読める
- `Artifact3DModelViewer` は Diligent ベースで表示できる
- `ArtifactContentsViewer` は `Model3D` を `Artifact3DModelViewer` に渡せる

ただし、実運用の確認導線としてはまだ薄い。

- `OBJ` / `FBX` の読み込み失敗理由が見えにくい
- `tinyobjloader` のような補助 importer を入れる余地が未整理
- viewer 側の metadata と model state がレビュー向きに十分ではない
- 今の model review は「開ける」より先の確認導線が弱い

---

## 方針

### 原則

1. `ufbx` を本線にする
2. `tinyobjloader` は OBJ の補助 fallback として扱う
3. `Contents Viewer` は編集ではなく inspection に徹する
4. 3D モデルの状態は viewer header で分かるようにする
5. Project / Asset から自然に開けるようにする

### 対象

- `.obj`
- `.fbx`
- 将来の `.gltf` / `.glb`

---

## Phase 1: Importer Hardening

### 目的

3D モデル読み込みの入口を安定化する。

### 作業項目

- `MeshImporter` のエラー表示を整理する
- `ufbx` の読み込み失敗理由を UI 側に渡せるようにする
- OBJ のみ `tinyobjloader` fallback を検討する
- `gltf` / `glb` の未対応理由を明示する

### 完了条件

- `OBJ` / `FBX` の読み込み失敗が説明できる
- OBJ fallback の要否を実装側で決められる

---

## Phase 2: Contents Viewer Hookup

### 目的

`Contents Viewer` から 3D モデルを自然に開けるようにする。

### 作業項目

- `FileType::Model3D` を `Artifact3DModelViewer` に渡す経路を確認する
- 3D モデル時の header と state 表示を整理する
- `reset view` の導線を model viewer 側で分かりやすくする

### 完了条件

- `Contents Viewer` で `OBJ` / `FBX` を開ける
- 何を読み込んだかが viewer で分かる

---

## Phase 3: View Mode Polish

### 目的

3D モデルの見え方を review 向けに整える。

### 作業項目

- `Wireframe`
- `Solid`
- `Solid + Wire`
- 背景色 / zoom / camera の state 表示

### 完了条件

- モデルの見え方を切り替えられる
- 現在の表示モードが分かる

---

## Phase 4: Review Navigation

### 目的

Project / Asset から 3D review までの導線を短くする。

### 作業項目

- Project View から open
- Asset Browser から open
- current selection と viewer を同期
- review 後に戻りやすくする

### 完了条件

- 素材選択から 3D review までが迷わない
- project / asset の両方から同じ viewer を開ける

---

## Phase 5: Future Extensibility

### 目的

将来の拡張を入れやすくする。

### 作業項目

- `gltf` / `glb` 追加の入口整理
- compare / annotation 方向の余地を残す
- import status を diagnostic とつなぐ

### 完了条件

- importer と viewer の責務が分離されている

---

## 連携先

- `ArtifactCore/src/Geometry/MeshImporter.cppm`
- `ArtifactCore/src/File/FileTypeDetector.cppm`
- `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cpp`
- `Artifact/src/Widgets/Render/Artifact3DModelViewer.cppm`
- `Artifact/src/AppMain.cppm`

---

## Recommended Order

1. Phase 1
2. Phase 2
3. Phase 3
4. Phase 4
5. Phase 5

## Current Status

- `MeshImporter` は `ufbx` 本線 + OBJ fallback まで入っている
- `Contents Viewer` 側は `Model3D` を `Artifact3DModelViewer` に渡せる
- Project View / Asset Browser からの open は既存の Contents Viewer 経路でそのまま使える
- Asset Browser には `Preview in Contents Viewer` の明示操作を足した
- Project View の footage context menu からも preview を開ける
- Project View の `Open` も footage では preview と同じ導線に揃えた
- Project View の selection summary に preview の案内を足した
- 3D モデル表示時に、preview 用の状態表示と mesh stats を出しやすくする初期整備を進めた
- Contents Viewer の 3D Source 表示は solid preview 寄りにして、開いた瞬間の見え方を軽くした
- viewer の mode 変更を外へ通知して、header / chip の表示が追従しやすいようにした
- preview 失敗時は `Preview unavailable` と backend / error を見せられるようにした
- Contents Viewer の 3D preview は `Reset 3D` ボタンと `Ctrl+0` で戻しやすくした
- 3D status は camera / bounds の細部を削って短くした
- glTF / glb は ufbx 経由で読む前提だと分かるようにした
- 次の焦点は、model review の導線をもう少し軽くすることと、`gltf` / `glb` の扱いを明確化すること
