# マイルストーン: プレイヘッド整備

> 2026-03-21 作成

## 現状サマリー

プレイヘッド（赤い縦線＋三角）は動作する基盤がある。`TimelinePlayheadOverlay` + `PlayheadSyncFilter` + `HeaderSeekFilter` + `ArtifactTimelineScrubBar` で構成。

### 動作しているもの
- 赤い縦線＋下向き三角の描画
- トラックビュー上のマウスシーク (クリック/ドラッグ)
- スクラブバー上のマウスシーク
- キーボードシーク (Home/End/PageUp/PageDown/Left/Right)
- 再生サービスの `frameChanged` シグナルによる駆動
- PlayheadSyncFilter によるシーン座標→オーバーレイ座標の変換
- フレーム 0 クランプ

### 未整備/問題点
- 状態が 3箇所に分散 (`position_`, `currentFrame_`, `playheadX_`) で同期が複雑
- フレーム表示 (`F<n>` ラベル) と `HH:MM:SS:FF` の不整合のおそれ
- スクロール/リサイズ時のオーバーレイ位置遅延
- 不感帯 (ヒット領域) の調整不足
- 再生中シークロック (`seekLockDuringPlayback_`) の挙動が不透明
- `seekPositionChanged` シグナルが片方向 (トラックビュー→スクラブバー) のみ
- プレイヘッドのスナップ (フレーム位置に吸着) がない

---

## Phase 1: 状態統一 (P0)

プレイヘッドの正規化ソースを 1箇所に集約し、残りは派生状態にする。

| タスク | 対象ファイル | 内容 |
|---|---|---|
| `position_` を唯一の権威にする | `ArtifactTimelineWidget.cpp` | `TimelineTrackView::position_` を正規化ソースとし、`ScrubBar::currentFrame_` と `PlayheadOverlay::playheadX_` を派生状態に変更 |
| 派生更新の統一 | `PlayheadSyncFilter::sync()` | `position_` → `currentFrame_` 変換 + `playheadX_` 計算を 1箇所で実行 |
| `seekPositionChanged` の双方向化 | `ArtifactTimelineScrubBar.cppm` | スクラブバーのドラッグ → `trackView_->setPosition()` + `sync()` の逆方向接続 |
| テスト: `position_` 変更が全 UI に伝播 | — | トラックビュー/スクラブバー/オーバーレイの同期確認 |

---

## Phase 2: シーク UX 改善 (P1) — スキップ

不感帯は改善済みのため省略。残タスクはスクロール追従のみ。

| タスク | 対象ファイル | 内容 |
|---|---|---|
| スクロール追従 | `PlayheadSyncFilter` | プレイヘッドがビューポート外に出た場合の自動スクロール |

---

## Phase 3: 表示/描画の改善 (P1) ✅ 完了

| タスク | 対象ファイル | 内容 | 状態 |
|---|---|---|---|
| `F<n>` ラベルと `HH:MM:SS:FF` の同期 | `ArtifactTimelineScrubBar.cppm` | FPS を `setFps()` 動的に変更、タイムラインから `frameRate()` 反映 | ✅ |
| プレイヘッドの高さ/余白調整 | `TimelinePlayheadOverlay` | トラック領域の上下余白を考慮 | ✅ (既存) |
| プレイヘッドの描画品質 | `TimelinePlayheadOverlay` | アンチエイリアス、高DPI 対応 | ✅ (既存) |
| コンポジションビューのプレイヘッド連携 | `ArtifactCompositionRenderController.cppm` | フレーム進行バー表示 | ✅ (Phase 2 で実装) |

---

## Phase 4: キーボード/マウス操作の拡充 (P2) ✅ 完了

| タスク | 対象ファイル | 内容 | 状態 |
|---|---|---|---|
| J/K/L シャトル操作 | `ArtifactTimelineWidget.cpp` | J=逆再生(x1→x2→x4), K=停止, L=再生(x1→x2→x4→x8) | ✅ |
| タイムコード入力 | 新規 or `ScrubBar` | `HH:MM:SS:FF` 形式で直接フレームジャンプ | 未着手 (別タスク) |
| マウスホイールシーク | `ArtifactTimelineWidget.cpp` | ホイール ±1F, Ctrl+ホイール ±10F | ✅ |
| プレイヘッドドラッグ | `TimelinePlayheadOverlay` | 三角を直接ドラッグしてシーク | 未着手 (別タスク) |

---

## 優先度マトリクス

| 優先 | タスク | 理由 |
|---|---|---|
| **最優先** | Phase 1: 状態統一 | 状態分散が全ての不具合の根因 |
| **高** | Phase 2: シーク UX | ユーザーの直感的操作に直結 |
| **高** | Phase 3: 表示品質 | 視覚的一貫性 |
| **中** | Phase 4: 操作拡充 | プロ仕様の効率化 |
