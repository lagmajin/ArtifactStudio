# M-TL シリーズ完了レポート

> 2026-03-20 調査時点

## 概要

M-TL-1, M-TL-2, M-TL-3 のコア機能は実装済み。各マイルストーンの残差分は別タスクに分離済みまたは低優先度。

---

## M-TL-1: Layer Basic Operations — 完了 (~85%)

### 実装済み

| 機能 | 主要ファイル | 備考 |
|------|------------|------|
| 追加 | `ArtifactProjectService.cpp:86-149` | `ArtifactLayerFactory` 経由。メニュー/UI 統合済み |
| 削除 | `ArtifactProjectService.cpp:402-410` | 親子クリーンアップ対応。Delete キー対応 |
| 複製 | `ArtifactProjectService.cpp:431-440` | 親子関係維持。Ctrl+D 対応 |
| リネーム | `ArtifactProjectService.cpp:442-461` | F2 / ダブルクリックインライン編集 |
| 親子 | `ArtifactProjectService.cpp:614-672` | サイクル検出付き。コンボボックス UI |
| 並び替え | `ArtifactProjectService.cpp:412-428` | D&D / Ctrl+[/] 対応 |

### 残差分 (別タスクへ分離)

- Undo/Redo スタブ (`UndoManager.cppm:122-148`) — 編集メニュー系マイルストーン (M-AS-7) の一部として管理
- Track matte mode — M-TL-2 と兼務。データモデル未定義

---

## M-TL-2: Layer View Sync — 完了 (~75%)

### 実装済み

| 機能 | 主要ファイル | 備考 |
|------|------------|------|
| ツリー ↔ トラック同期 | `ArtifactTimelineWidget.cpp:1436-1540` | `refreshTracks()` で同期 |
| 展開/折りたたみ | `ArtifactLayerPanelWidget.cpp:1131-1150` | ダブルクリック / 矢印キー対応 |
| Shy フィルタ | `ArtifactLayerPanelWidget.cpp:594,993` | ヘッダー/レイヤー単位のトグル |
| 1レイヤー1クリップ | `ArtifactTimelineWidget.cpp:1488-1522` | 各レイヤーが ClipItem を1つ生成 |

### 残差分

- Track matte 表示 — データモデル未定義のため未着手
- Audio state 連携 — 低優先度 (M-AU シリーズと兼務)
- 垂直スクロール双方向同期 — 現状片方向 (左→右) のみ

---

## M-TL-3: Work Area / Range Unification — 完了 (~70%)

### 実装済み

| 機能 | 主要ファイル | 備考 |
|------|------------|------|
| In/Out Points | `ArtifactAbstractLayer.cppm:209-240` | I/O キーでワークエリア設定 |
| Work Area | `ArtifactAbstractComposition.cppm:495-517` | `WorkAreaControl` ウィジェット完備 |
| Seek | `ArtifactPlaybackEngine.cppm:319-339` | スクラブバー/プレイヘッド/キー操作対応 |
| レンジクランプ | `ArtifactAbstractComposition.cppm:525-565` | composition 範囲内へのクランプ |
| レンダーキュー連携 | `ArtifactRenderQueueService.cppm:783-785` | work area をレンジとして使用 |

### 残差分

- レンジプリセット UI (Full / Work Area / Custom) — M-RD-2 と兼務
- 共通レンジサービス — M-RANGE-2 として分離済み
- 再生エンジンのダミーフレーム (`ArtifactPlaybackEngine.cppm:229`) — M-RD-1 の範囲

---

## 結論

M-TL シリーズの**主要スコープは完了**。
残差分は他マイルストーン (M-AS-7, M-AU, M-RD, M-RANGE) に分離可能またはスコープ外。
バックログでは ✅ 完了印を付与し、残差分は参照コメントで明記する。
