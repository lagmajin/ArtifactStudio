# Project View Scroll Stability Milestone

**作成日:** 2026-06-07  
**ステータス:** Completed
**関連コンポーネント:** ArtifactProjectManagerWidget, Asset Browser, Project Service

---

## 概要

Project View で新規素材を import したときに、スクロール位置が勝手に先頭へ戻らないようにするマイルストーンです。

長いリストの中ほどや下の方を見ているときに、追加操作で視線位置が壊れないことを重視します。  
「追加したいだけなのに、今見ている場所を失う」状態をなくします。

---

## 背景

現状は import などの更新でリストの再構築が走ると、視点位置がリセットされやすいです。

- 長い素材一覧の途中を見ている
- 新しい素材を追加する
- スクロールが先頭に戻る
- もう一度探し直す必要がある

これは操作ミスではなく、視線のコンテキスト破壊です。

---

## 目標

- import 後も scroll position を維持する
- 選択中の領域が見えているならそのまま残す
- 追加アイテムはリスト末尾に append する
- view refresh で不要な scroll reset を起こさない
- 既存の selection / filter / sort と衝突しないようにする

---

## 主要方針

### 1. Scroll Anchor Preservation

更新前の scroll anchor を保存し、再描画後に復元する。

### 2. Append-Only Import Placement

新しい素材は既存の表示位置をずらさないように末尾へ追加する。

### 3. No Surprise Reset

フィルタや選択更新があっても、明示的な再センタリング要求がない限り上へ戻さない。

---

## Phase 構成

### Phase 1: Scroll State Capture

- import 前の scroll position を保存する
- visible anchor item を保持する
- refresh 後に復元できるようにする

完了条件:

- 更新前の視線位置を記録できる

### Phase 2: Non-Reset Refresh

- model refresh 時に scroll reset を止める
- selection update と refresh を分離する
- 必要なときだけ再センタリングする

完了条件:

- import しても view が先頭に戻らない

### Phase 3: Append Behavior

- 新しいアイテムは末尾に追加する
- 既存 item の並びを不必要に揺らさない
- current view に入らない限り scroll を動かさない

完了条件:

- 新規素材追加で現在の表示位置が保たれる

### Phase 4: Filter and Sort Stability

- sort / filter 更新でも scroll anchor をできるだけ維持する
- visible range の再計算を安定化する
- status filter 切り替え時の jump を抑える

完了条件:

- view 更新が視線を壊しにくい

### Phase 5: Safety Fallbacks

- 例外的に reset が必要なケースを定義する
- item が完全に消えたときは妥当な位置へ戻す
- anchor が無効なら近傍 item を探す

完了条件:

- 壊れた時も破綻しない

---

## 実装順

1. scroll state capture
2. refresh の non-reset 化
3. append placement
4. filter / sort stability
5. fallback restoration

---

## 対象範囲

- `ArtifactProjectManagerWidget`
- `ArtifactAssetBrowser`
- `ArtifactProjectService`

---

## リスクと留意点

- フィルタや再ソートと scroll 維持が競合する
- 選択更新が中心化を誘発しやすい
- 末尾 append 方針がソート順と矛盾する場合がある

---

## 成功条件

- 新規 import で現在の視点が壊れない
- 長いリストの下部を見ていても位置が維持される
- 必要なときだけ明示的に scroll 位置を動かす

---

## 関連

- `docs/planned/MILESTONE_PROJECT_VIEW_2026-03-12.md`
- `docs/planned/MILESTONE_ASSET_SYSTEM_2026-03-12.md`
- `docs/planned/MILESTONE_ASSET_BROWSER_IMPROVEMENT_2026-04-01.md`
