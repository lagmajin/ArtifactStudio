# Milestone: Timeline Overscroll Toggle Worknote

> 2026-06-02 作成 / 2026-06-24 完了（実装済み確認）

## 目的

タイムラインウィンドウの横スクロールについて、コンテンツ端での overscroll 許可を On/Off できるようにする。— ✅ 完了

## 確認

- `ArtifactAppSettings::timelineAllowOverscroll()` / `setTimelineAllowOverscroll()` — Core設定実装済み
- `clampTimelineHorizontalOffset()` — 2箇所の clamp 関数ともに overscroll 分岐あり
- `ArtifactTimelineGlobalSwitches` — トグルボタン接続済み
- 初期値: Off
