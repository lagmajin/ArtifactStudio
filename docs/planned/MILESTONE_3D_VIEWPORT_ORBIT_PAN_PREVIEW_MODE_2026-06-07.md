# MILESTONE: 3D Viewport Orbit / Pan / Preview Mode - 2026-06-07

作成日: 2026-06-07  
対象: 3D viewport の camera 操作と preview mode  
優先度: 🟠 高

---

## 目的

3D レイヤーの視点操作を、Blender / Unity 風の共通文法に寄せる。

解決したいのは次の 2 点。

1. カメラ操作が editor ごとに散らばっていて、視点が飛びやすい
2. 「カメラを動かす」のではなく「視点だけを動かす」preview mode が弱い

---

## 問題

### 1. Orbit / Pan 操作が直感的でない

不満:
- 回転させようとすると視点が飛ぶ
- ツール切り替えを挟んでドラッグする操作が煩雑
- editor 間で camera / viewport 操作の文法がずれる

改善:
- `Alt + Left Drag` を orbit に固定する
- `Middle Drag` を pan に固定する
- wheel は zoom に寄せる
- camera 操作の責務を viewport 側へ寄せる

完了条件:
- 視点操作の基本文法が 1 つにまとまる
- editor 間で操作の意味が大きくぶれない
- 3D 表示が予測しやすくなる

---

### 2. Preview Mode が弱い

不満:
- camera そのものを動かしているのか、視点だけを動かしているのか分かりにくい
- プレビューの状態が view state と混ざる
- 3D surface の「確認モード」が明確ではない

改善:
- `Preview Orbit Mode` を導入する
- 実 camera 変更と preview-only view state を分ける
- preview 中は HUD で状態を見せる

完了条件:
- preview-only 操作が明示される
- camera asset を壊さず視点確認できる
- editor の操作意図が読める

---

## 実装の読み替え

### Orbit / Pan

- まず viewport navigation として統一する
- camera matrix 更新と操作入力を分離する
- editor が違っても同じショートカット語彙を使う

### Preview Mode

- camera layer を直接変更するモードと、視点だけを変えるモードを分ける
- preview mode は temporary session として扱う
- preview の存在を overlay / badge で見える化する

---

## 詳細実装スライス

### A. Orbit / Pan / Zoom Shortcut Baseline

#### 入口
- 3D composition editor
- 3D model viewer
- camera overlay / viewport navigation

#### 触るもの
- `ArtifactCompositionEditor`
- `Artifact3DModelViewer`
- `ArtifactCompositionRenderController`
- 3D navigation session
- input handling / drag state

#### データ契約
- orbit state
- pan state
- zoom state
- active navigation mode
- modifier key state

#### 実装順
1. `Alt + Left Drag` を orbit に固定する
2. `Middle Drag` を pan に固定する
3. wheel zoom の挙動を統一する
4. 既存のツール切替依存を減らす

#### 失敗時の扱い
- modifier が取れない場合は従来操作にフォールバックする
- drag 中に mode が変わったら中断する
- view state が不整合なら強制リセットする

#### Phase 1: shortcut baseline
- [ ] `Alt + Left Drag` を orbit に固定する
- [ ] `Middle Drag` を pan に固定する
- [ ] wheel zoom を viewport 共通にする

#### Phase 2: state separation
- [ ] camera update と view navigation を分ける
- [ ] tool 切替なしで操作できるようにする
- [ ] editor 間で操作文法を揃える

#### Phase 3: feedback
- [ ] active navigation mode を HUD で出す
- [ ] orbit / pan / zoom の状態を短く表示する
- [ ] ドラッグ中の意図が読めるようにする

---

### B. Preview Orbit Mode

#### 入口
- 3D viewport toolbar
- overlay / HUD
- context menu / shortcut

#### 触るもの
- preview view state
- camera state
- viewport navigation session
- overlay badge
- composition editor 3D surface

#### データ契約
- live camera state
- preview-only state
- persisted camera state
- temporary navigation session

#### 実装順
1. preview-only view state を導入する
2. live camera state と分離する
3. preview 中の overlay を追加する
4. preview 終了時に状態を戻す

#### 失敗時の扱い
- preview 終了時に state が戻らない場合は rollback する
- live camera を誤更新したら undo path に乗せる
- preview 解除で視点が飛ぶ場合は原因を明示する

#### Phase 1: preview session
- [ ] preview-only view state を導入する
- [ ] live camera state と分ける
- [ ] session を開始 / 終了できるようにする

#### Phase 2: HUD / badge
- [ ] preview mode を badge で出す
- [ ] camera editing と preview viewing を区別する
- [ ] active state を短い文言で表示する

#### Phase 3: restoration
- [ ] preview 終了で state を戻す
- [ ] temporary navigation を破棄する
- [ ] undo で復元できるようにする

---

## 推奨実行順

1. Orbit / Pan / Zoom Shortcut Baseline
2. Preview Orbit Mode

理由:
- 操作の文法が先に揃うと、preview mode の意味が通りやすい
- preview-only は見た目より state 管理が主題なので、基礎操作の整理が先

---

## Next Execution Slice

Phase 1 を先に締めるなら、`Orbit / Pan / Zoom Shortcut Baseline` から入る。

### Phase 1A の着手点

1. `Alt + Left Drag` を orbit に固定する
2. `Middle Drag` を pan に固定する
3. wheel zoom を viewport 共通にする
4. 既存の tool 切替依存を減らし、操作文法を editor 間で寄せる

### Phase 1 完了条件

- orbit / pan / zoom の基本文法が 1 つにまとまる
- editor 間で操作の意味が大きくぶれない
- 3D 表示が予測しやすくなる

### Phase 2 の前提

- preview-only view state を導入する土台ができている
- live camera state と preview state を分ける準備が整っている
- HUD / badge は phase 1 の baseline が固まってから入れる

### Preview Mode への波及

- preview mode は「camera を壊さない」ことが前提
- まず操作文法を揃え、その後に preview session を分離する
- したがって preview 固有の UI は phase 1 後段に回す

---

## 関連

- [`docs/planned/MILESTONE_3D_VIEWPORT_SOLID_CAMERA_OVERLAY_2026-04-10.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_3D_VIEWPORT_SOLID_CAMERA_OVERLAY_2026-04-10.md)
- [`docs/planned/MILESTONE_3D_MODEL_REVIEW_IN_CONTENTS_VIEWER_2026-03-28.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_3D_MODEL_REVIEW_IN_CONTENTS_VIEWER_2026-03-28.md)
- [`docs/planned/MILESTONE_3D_MODEL_IMPORT_AND_CONTENTS_VIEWER_2026-03-29.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_3D_MODEL_IMPORT_AND_CONTENTS_VIEWER_2026-03-29.md)
- [`docs/planned/IMPLEMENTATION_PLAN_MULTI_VIEWPORT_2026-06-02.md`](X:/Dev/ArtifactStudio/docs/planned/IMPLEMENTATION_PLAN_MULTI_VIEWPORT_2026-06-02.md)

---

## 備考

- これは 3D ビューポートの操作文法を揃えるための計画文書。
- ビルドやテストは実施していない。
