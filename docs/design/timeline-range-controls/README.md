# Timeline Range Controls

**最終更新:** 2026-09-07

## 承認済みリファレンス

- `approved-range-handle-redesign-2026-09-07.png`
  - 上段: 既存表示
  - 下段: 実装対象。Navigator は明るいブルーグレーの範囲と二本線グリップ、Work Area は暖色の範囲と二本線グリップを使う。

## 実装方針

- `ArtifactTimelineNavigatorWidget` は赤いグラデーションを使わず、ブルーグレーの選択範囲と明るいグリップで可視範囲を示す。
- `WorkAreaControl` は既存の暖色範囲を維持し、左右端を明確なグリップとして描く。
- 範囲の座標変換、ドラッグ、EventBus通知、playhead描画は変更しない。
