# MILESTONE: ArtifactPr Independent Project

> 2026-04-22 作成

`ArtifactPr` を、既存の `Artifact` と並ぶ別アプリとして立ち上げるためのマイルストーン。

土台は `ArtifactCore` を共有するが、UI / ワークフロー / タイムライン責務は `Artifact` と分ける。

---

## 目的

- `Artifact` に影響を与えずに、Pr ライクな編集ソフトを育てる
- `ArtifactCore` を編集基盤として共通利用する
- 先に骨組みを作って、後から機能を足す順序にする
- AE ライクとは別の設計判断をできる状態にする

---

## 非目的

- `Artifact` の置き換え
- `Artifact` と `ArtifactPr` の UI 共通化
- 最初から完全な Adobe Pr 互換を狙うこと
- 既存の AE ライクな画面構成をそのまま流用すること

---

## 立ち位置

`ArtifactPr` は次の関係で考える。

- `ArtifactCore`: 共通の編集・データ・基礎ロジック
- `Artifact`: 現在の AE ライクアプリ
- `ArtifactPr`: 別ディレクトリの Pr ライクアプリ

この分離により、機能の追加順序と UI の責務を独立に決められるようにする。

---

## 現状の出発点

2026-04-22 時点で、`ArtifactPr` の最小シェルを追加済み。

- ルート `CMakeLists.txt` に `ArtifactPr` を登録済み
- `ArtifactPr/CMakeLists.txt` を追加済み
- `ArtifactPr/src/main.cpp` に最小ウィンドウを追加済み

この時点では、アプリはまだ「起動する箱」であり、編集機能は最小限に留める。

#### Progress Notes

- `2026-04-22`: `ArtifactPr` をルートの別ターゲットとして登録した
- `2026-04-22`: `ArtifactPrMainWindow` を追加し、左の素材/プロジェクト、中央のタイムライン、右のプレビュー、下のトランスポートを持つ workspace frame を作った
- `2026-04-22`: 画面の見た目は `QPalette` ベースのダーク寄り作業空間にした
- `2026-04-22`: `ArtifactPrDemoData` を追加し、project / sequence / track / clip の仮データを UI に流し込む形へ寄せた

---

## 方針

### 1. 共有は `ArtifactCore` に寄せる

- 編集データ
- タイムラインの土台
- クリップ / 素材 / トランスフォーム / レンダー基盤
- 将来の書き出しや再生の共通ロジック

### 2. UI は分ける

- `Artifact` のパネル構成を流用しない
- `ArtifactPr` 専用のワークスペースを作る
- 既存のシグナル/スロット配線を増やしすぎない

### 3. 骨組み先行

- まずは起動・レイアウト・基本導線
- 次にタイムラインとメディア管理
- 最後に編集操作と書き出し

---

## 実装範囲

### Phase 0: Project Shell

目的:

- 別ディレクトリの独立アプリとして成立させる
- `ArtifactCore` をリンクして起動できる状態にする

対象:

- [`CMakeLists.txt`](x:/Dev/ArtifactStudio/CMakeLists.txt)
- [`ArtifactPr/CMakeLists.txt`](x:/Dev/ArtifactStudio/ArtifactPr/CMakeLists.txt)
- [`ArtifactPr/src/main.cpp`](x:/Dev/ArtifactStudio/ArtifactPr/src/main.cpp)

完了条件:

- `Artifact` に触れず `ArtifactPr` を追加できる
- `ArtifactPr` が別ターゲットとして認識される
- 共通基盤は `ArtifactCore` 経由で参照される

### Phase 1: Workspace Frame

目的:

- Pr ライクな画面の外枠を作る
- プロジェクト、素材、タイムライン、プレビューの最小配置を決める

完了条件:

- 画面レイアウトの骨格が固まる
- `Artifact` の AE ライク画面と混ざらない

### Phase 2: Media and Sequence Model

目的:

- 素材一覧とシーケンスの基本データを置く
- `ArtifactCore` のデータ型を使って、編集対象を表現する

完了条件:

- 素材とシーケンスの基本操作ができる
- UI はまだ薄くても、データの流れが追える

### Phase 3: Timeline Editing

目的:

- カット、トリム、並べ替えの最小編集を入れる
- タイムライン上の操作を Pr らしく作る

完了条件:

- クリップを並べて編集できる
- 再生位置と選択状態が安定して追える

### Phase 4: Playback and Export

目的:

- 再生と停止を安定させる
- 最小の書き出し導線を作る

完了条件:

- 画面上で編集結果を確認できる
- 最低限の export が成立する

---

## 変更しないこと

- `Artifact` の既存 UI
- `Artifact` の既存 CMake 構成
- `ArtifactCore` を何でも置くゴミ箱にしないこと
- 新しいプロジェクトで既存の責務を雑に共有しないこと

---

## リスク

- 共通化しすぎると `ArtifactCore` が肥大化する
- 早い段階で UI を詰めすぎると、Pr ライクの責務がぼやける
- `Artifact` と `ArtifactPr` の差分を曖昧にすると、どちらの改善か判別しづらくなる

---

## 次の一手

1. `ArtifactPr` の画面骨格を決める
2. `ArtifactCore` から共有したい最小データ型を選ぶ
3. タイムラインの責務を `Artifact` と切り分ける
