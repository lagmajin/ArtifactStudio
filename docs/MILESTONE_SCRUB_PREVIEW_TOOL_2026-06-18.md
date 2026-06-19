# マイルストーン: Scrub Preview Tool

> 2026-06-18 作成

## 目的

タイムライン上でマウスをドラッグしながら、フレームを素早くスクラブプレビューできるツールを追加する。

---

## 背景

既存のタイムライン操作はクリップ移動やキーフレーム編集が中心で、再生ヘッドを自由にスクラブするためにはルーラー部分を掴む必要がある。専用のスクラブツールを用意することで、タイムラインのどこでもドラッグしてフレームを探せるようになる。

---

## 概念

- **Scrub Preview Tool**: ツールバーから選択し、タイムラインキャンバス上でドラッグすると再生ヘッドが追従する
- **リアルタイムプレビュー**: `FrameChangedEvent` を発行し、ビューアー側が最新フレームを描画
- **直感的なカーソル**: ドラッグ中は `ClosedHandCursor`

---

## フェーズ設計

### Phase 1: Tool registration

**目的:** ツールとして認識される状態にする。

**作業項目:**
- `Artifact/include/Tool/ArtifactToolManager.ixx`
  - `ToolType` enum に `ScrubPreview` を追加
  - `toolName()` にケース追加
- `Artifact/include/Widgets/ArtifactToolBar.ixx`
  - `scrubPreviewToolRequested()` シグナル追加
- `Artifact/src/Widgets/ArtifactToolBar.cppm`
  - ボタン追加（既存 `Studio/toolbar_tool_*.svg` 系のアイコンを流用、なければ新規 SVG はこのマイルストーン完了後に検討）
  - ラベル「スクラブ」を返すケース追加

**完了条件:**
- ツールバーに Scrub Preview Tool ボタンが表示される
- ボタンクリックで `ToolChangedEvent` が発行される

**完成度:**
- [ ] enum 追加
- [ ] シグナル追加
- [ ] ボタン追加
- [ ] ラベル追加

---

### Phase 2: Timeline behavior

**目的:** タイムライン上でドラッグしてフレームをスクラブできるようにする。

**作業項目:**
- `Artifact/src/Widgets/ArtifactTimelineTrackPainterView.cpp`
  - `ToolType::ScrubPreview` アクティブ時の分岐を追加
  - `mousePressEvent`: 左ボタンでドラッグ開始、現在フレームを発行
  - `mouseMoveEvent`: X 座標からフレームを計算し `FrameChangedEvent` 発行
  - `mouseReleaseEvent`: ドラッグ終了、カーソルを元に戻す
- `ArtifactTimelineWidget` 側でフレーム変更を受信し、プレビュー反映

**完了条件:**
- Scrub Preview Tool 選択中、タイムライン上でドラッグするとフレームが変わる
- ビューアーが最新フレームを描画する

**完成度:**
- [ ] マウスイベント分岐
- [ ] フレーム計算
- [ ] FrameChangedEvent 発行
- [ ] カーソル切り替え

---

### Phase 3: Shortcut & polish

**目的:** キーボードからも切り替えやすくし、細かい挙動を整える。

**作業項目:**
- `ArtifactTimelineWidget` のキーバインディングに Scrub Preview Tool 切り替えショートカット追加
  - 未割り当てキーを選定（例: `C` など、要確認）
- ドラッグ中のステータスバーに現在フレーム/タイムコードを表示
- スクラブ中は一時停止状態を維持

**完了条件:**
- ショートカットキーで Scrub Preview Tool に切り替え可能
- ドラッグ中にステータスバーが更新される

**完成度:**
- [ ] ショートカット追加
- [ ] ステータスバー更新
- [ ] 再生状態との整合

---

## Non-Goals

- 音声スクラブ（本マイルストーンでは映像フレームのみ）
- タッチパッド/ペンタブレット特有のジェスチャー
- スクラブ速度の可変（後続改善で検討）

---

## 技術方針

- `ArtifactToolManager` / `ArtifactToolBar` 側の変更は最小限に抑える
- タイムライン側のマウスイベントは既存 `Selection`/`Hand` モードと排他する
- `FrameChangedEvent` を利用し、ビューアーとの直接結合を避ける

---

## 進捗サマリー

| Phase | 状態 |
|---|---|
| Phase 1 | 未着手 |
| Phase 2 | 未着手 |
| Phase 3 | 未着手 |

**総合完成度:** 0%
