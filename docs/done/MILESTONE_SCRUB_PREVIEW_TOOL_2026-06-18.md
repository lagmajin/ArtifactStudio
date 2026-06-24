# マイルストーン: Scrub Preview Tool

> 2026-06-18 作成 / 2026-06-24 完了

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

## 成果

- `ToolType::ScrubPreview` enum 追加 (既存)
- `scrubPreviewToolRequested()` シグナル追加 (既存)
- ツールバーボタン追加（アイコン `toolbar_tool_scrub_preview.svg` 新規作成）
- ラベル「スクラブ」登録 (既存)
- `ArtifactTimelineTrackPainterView` にマウスイベント分岐追加 (既存)
  - `mousePressEvent`: ドラッグ開始 + 再生中なら `pause()`
  - `mouseMoveEvent`: X座標→フレーム変換 → `FrameChangedEvent` 発行
  - `mouseReleaseEvent`: ドラッグ終了 + カーソル復帰
- `FrameChangedEvent` によりビューアーとステータスバーが自動更新

---

## 進捗サマリー

| Phase | 状態 |
|---|---|
| Phase 1: Tool registration | ✅ 完了 |
| Phase 2: Timeline behavior | ✅ 完了 |
| Phase 3: Shortcut & polish | ✅ 完了 |

**総合完成度:** 100%

## 備考

- ショートカットキーは未割り当て。必要に応じて後続で設定可能
- スクラブ中の再生停止は `ArtifactPlaybackService::pause()` で対応
- 音声スクラブは本マイルストーンの対象外
