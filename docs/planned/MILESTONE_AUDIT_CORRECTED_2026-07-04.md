# 全ウィジェット監査 修正版 (2026-07-04)

**最終更新:** 2026-08-15

## 2026-08-15 現行コード照合

この文書は 2026-07-04 時点の横断監査スナップショットであり、以下は現行ソースとの再照合結果である。古い「不足」欄をそのまま現状判定として扱わない。

- **Animation Layers**: `AnimationLayerStackT<T>` の Additive / Override、Weight、Mute、Solo、保存・復元、Property Editor と Undo の接続を確認。旧 Timeline 欄の「未実装」は現状には適用しない。完全な runtime／全型の受入れは未検証。
- **Audio Mixer**: `ArtifactAudioMixer` と `ArtifactCompositionAudioMixerWidget` に channel/master、volume、pan、mute、solo、peak meter、および solo 時の自動ミュートを確認。旧「Track Mute/Solo 不在」は更新が必要。Clip Indicator は別途未確認。
- **Roving Keyframes**: Curve Editor／Dope Sheet 側に roving のデータ・表示準備はあるが、相対間隔を保った編集と保存・再生整合性は未確認。旧 ❌ を「部分実装／受入れ待ち」に修正する。
- **Dropped Frames / AI token-cost / Smart Bin / Viewport channel split・X-Ray**: 現行コードでこの監査が要求する完成導線までは確認できず、引き続き未完了または未検証とする。

ビルド・テスト・実機 UI 確認は実施していない。

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
