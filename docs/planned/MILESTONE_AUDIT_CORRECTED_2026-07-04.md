# 全ウィジェット監査 修正版 (2026-07-04)

> コード精読ベース。初回監査の誤認(70%+)を訂正。
> ✅=実装済み、⚠️=部分的、❌=コード不在、🎉=今回実装完了

## 実装完了 🎉

| 項目 | 内容 |
|---|---|
| 🎉 JKL シャトル競合解決 | J/K → Ctrl+Shift+J/K（ShortcutBindings.cppm） |
| 🎉 JKL 連打倍速 + K+Lスロー | TimelineWidget keyPressEvent 拡張 |
| 🎉 タイムコードクリック入力 | PlaybackTimecodeFrame 拡張 |
| 🎉 Auto-Key / Mute Preview トグル | PlaybackControlWidget に追加 |
| 🎉 Shift+Step 5フレームジャンプ | PlaybackControlWidget step handler |
| 🎉 Property Reset ボタン有効化 | g_showPropertyResetButtons=true + 配線 |

## Asset Browser (3,588行) — ほぼ完璧

| 唯一の不足 | 詳細 |
|---|---|
| ❌ 内部 D&D 移動 | フォルダ間のドラッグ移動。その他(検索/フィルタ/ソート/ブレッドクラム/Favorites/ホバー/サムネイル/D&Dインポート/ファイル監視)は✅実装済み |

## Contents Viewer (2,295行)

| 不足 | 詳細 |
|---|---|
| ⚠️ QPixmap/QImage 依存 | GPU パイプライン未使用 |
| ❌ 動画 FFmpeg 統一 | 音声のみ FFmpeg、動画は QMediaPlayer |

## Viewport / CompositionEditor (4,500+行)

| 不足 | 詳細 |
|---|---|
| ❌ チャンネル分離表示 (R/G/B/A) | Alt+2/3/4 相当 |
| ❌ X-Ray / 透過表示 | 遮蔽物越し編集 |
| ❌ Iris/ワイプ Compare in Viewport | Contents Viewer にはあるが Viewport にない |

## Timeline (5,400+行)

| 不足 | 詳細 |
|---|---|
| ❌ Roving Keyframes | 相対間隔保持移動 |
| ❌ Animation Layers | Maya/MotionBuilder 式 |
| ❌ トラック高さ個別調整 | |

## Project Manager (4,272行)

| 不足 | 詳細 |
|---|---|
| ✅ 検索バー | searchBar+ProjectFilterProxyModel+handleSearch 完全実装済み |
| ❌ Smart Bin | 条件付き仮想フォルダ |

## Playback Control — ほぼ完璧

| 不足 | 詳細 |
|---|---|
| ❌ Dropped Frames 表示配線 | PlaybackInfoWidget API+UI はあるが AppMain 未配線 |
| 🎉 Ping-Pong ループ | UI(右クリックメニュー) + Engine(範囲端速度反転) + Service 全レイヤー完了 |

## AI Cloud Widget (2,725行) — ほぼ完璧

| 不足 | 詳細 |
|---|---|
| ❌ Token 数/コスト表示 | StopGenerating/Cancel/Streaming は✅実装済み |
| ❌ 会話履歴サイドバー | |

## Inspector / PropertyEditor — ほぼ完璧

| 不足 | 詳細 |
|---|---|
| 🎉 Property Reset ボタン | g_showPropertyResetButtons=true 有効化。DragScrub/QuickCalc/検索/RelativeSpinBox ✅実装済み |
| ❌ 複数選択 Mixed 表示 | |

## Audio Mixer (2,291行)

| 不足 | 詳細 |
|---|---|
| ❌ Track Mute/Solo ボタン | |
| ❌ Clip Indicator | |