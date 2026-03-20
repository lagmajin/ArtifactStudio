# プレイヘッド実装調査レポート

> 2026-03-20 調査

## 概要

プレイヘッド（赤い縦線＋三角デコレーション）は、タイムライン上の現在フレーム位置を示す UI コンポーネント。
`WIDGET_MAP.md` の定義によると、UI 上では「シークバー」と呼ばれ、コード上では "playhead" として実装されている。

## 関連ファイル一覧

| ファイル | 主要行 | 役割 |
|----------|--------|------|
| `Artifact/src/Widgets/ArtifactTimelineWidget.cpp` | 130-261, 737-738, 1119-1195, 2052-2153 | **主要ファイル** — `TimelinePlayheadOverlay`, `PlayheadSyncFilter`, `HeaderSeekFilter` の定義と配線 |
| `Artifact/include/Widgets/ArtifactTimelineWidget.ixx` | 108-154, 157-199 | タイムラインウィジェットとトラックビューの公開 API |
| `Artifact/include/Widgets/Timeline/ArtifactTimelineScrubBar.ixx` | 18-57 | スクラブバー公開インターフェース (`F<frame>` と `HH:MM:SS:FF` 表示) |
| `Artifact/src/Widgets/Timeline/ArtifactTimelineScrubBar.cppm` | 100-411 | スクラブバー実装 — 描画、マウスドラッグシーク |
| `Artifact/include/Widgets/Timeline/ArtifactTimelineTrackPainterView.ixx` | 18-76 | フラット QWidget のトラックビュー (代替クリップ描画面) |
| `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp` | 339-365 | フラット QWidget のシーク処理 |
| `Artifact/include/Service/ArtifactPlaybackService.ixx` | 55-144 | シングルトン再生サービス — `frameChanged` シグナルがプレイヘッドを駆動 |
| `Artifact/src/Service/ArtifactPlaybackService.cppm` | 57-480 | 再生サービス実装 |

## クラス構造

```
ArtifactTimelineWidget (QWidget) — 親オーケストレータ
 ├── ArtifactTimelineWidget::Impl
 │    ├── TimelinePlayheadOverlay*       — 視覚オーバーレイ (赤縦線＋三角)
 │    ├── PlayheadSyncFilter*            — オーバーレイ位置同期用イベントフィルタ
 │    ├── TimelineTrackView*             — QGraphicsView ベースのシーン (position_ を保持)
 │    ├── ArtifactTimelineTrackPainterView* — フラット QWidget ミラー (currentFrame_ を保持)
 │    ├── ArtifactTimelineScrubBar*      — フレーム/時間ラベル付きレール
 │    ├── WorkAreaControl*               — IN/OUT 範囲バー
 │    └── ArtifactTimelineNavigatorWidget* — ズーム範囲制御
 │
 ├── TimelinePlayheadOverlay (QWidget, final)
 │    Properties: playheadX_ (int), activeRect_ (QRect), headColor_ (QColor)
 │    赤い 1px 縦線 + 下向き三角形を描画。マウス透過 (WA_TransparentForMouseEvents)
 │
 ├── PlayheadSyncFilter (QObject, final)
 │    rightPanel / trackView / trackView->viewport() にイベントフィルタとしてインストール
 │    trackView の scene position → overlay 座標への変換を担当
 │
 ├── HeaderSeekFilter (QObject)
 │    navigator / workArea / trackView viewport にインストール
 │    マウスクリック/ドラッグ → フレーム位置への変換
 │
 └── TimelineTrackView (QGraphicsView)
      └── position_ (double) — フレーム単位の正規プレイヘッド位置 (正規化ソース)
```

### 補足: 外部コンポーネント

`ArtifactPlaybackService` (シングルトン) はウィジェット階層外に存在し、`frameChanged(FramePosition)` シグナル経由でプレイヘッドを駆動する。
内部的には `ArtifactPlaybackEngine` と `ArtifactCompositionPlaybackController` に接続し、`frameChanged` を転送する。

## コアクラス詳細

### TimelinePlayheadOverlay (ArtifactTimelineWidget.cpp:130-188)

- **目的**: 赤い縦線と下向き三角形を描画する透明オーバーレイ
- **プロパティ**:
  - `playheadX_` (int): オーバーレイローカル座標でのプレイヘッド X ピクセル値
  - `activeRect_` (QRect): クリッピング領域 (タイムラインパネル領域のみ)
  - `headColor_` (QColor): デフォルト `{255, 80, 60, 220}` (半透明赤)
- **マウス透過**: `WA_TransparentForMouseEvents` 設定により入力を一切奪わない
- **描画**: `activeRect_` を横切る 1px 縦線 + 上部に三角形 (half-width=5px, height=8px)

### PlayheadSyncFilter (ArtifactTimelineWidget.cpp:190-261)

- **目的**: `TimelinePlayheadOverlay` を `TimelineTrackView` のシーン位置に正しく配置
- **`sync()` メソッド**:
  1. 非表示判定 → オーバーレイを隠す
  2. `overlayHostWidget_` へのリペアレント
  3. ホストウィジェットの rect にジオメトリ設定
  4. `trackView_->position()` を viewport → overlay 座標に変換
  5. **フレーム 0 クランプ**: プレイヘッドがフレーム 0 のスクリーン位置より左に表示されないよう制限 (line 225-231)
  6. `overlay_->setPlayheadLine(activeRect, xInOverlay)` を呼び出し
- **イベントフィルタ**: `Hide`, `ParentChange`, `Resize`, `LayoutRequest`, `Show`, `Move`, `WindowStateChange` で `sync()` を発火

### HeaderSeekFilter (ArtifactTimelineWidget.cpp:263-475)

- **目的**: タイムラインヘッダーウィジェット上のマウス操作 → プレイヘッドシーク変換
- **シーク動作**:
  - マウス位置 → ビューポート座標 → シーン座標 → クランプ済フレーム番号に変換
  - `trackView_->setPosition()` + `scrubBar_->setCurrentFrame()` を呼び出し
- **予約クリック機構**: ナビゲータ/ワークエリアのハンドル操作と競合しないよう入力を委譲
- **ホットゾーン**: トラックビュー上部 `kTopSeekHotZonePx` ピクセルのみシーク対象

### TimelineTrackView (lines 1608-2465)

- **`position_` (double, line 1613)**: フレーム単位のプレイヘッド位置。**正規化ソース (single source of truth)**
- **`setPosition(double)`** (line 1651): `[0, timelineFrameMax(duration_)]` にクランプし、`viewport()->update()` 呼び出し
- **`seekPositionChanged(double ratio)`** シグナル (line 149 of .ixx): ユーザー操作時に発火。ratio = `position_ / frameMax`
- **マウスシーク** (lines 1959-2028): トラックビュー上のクリック (クリップ/ハンドル以外) でシーク。上部ゾーンは空領域でもシーク可
- **キーボードシーク** (lines 2248-2381):
  - `Home`: position → 0.0
  - `End`: position → frameMax
  - `PageUp/PageDown`: ±10 フレーム
  - `Left/Right`: ±1 フレーム (クリップ未選択時)

### ArtifactTimelineScrubBar (別モジュール)

- **`currentFrame_` (FramePosition)**: スクラブバーに表示するフレーム番号
- **`setCurrentFrame(FramePosition)`** (line 139): クランプ、更新、`frameChanged` シグナル発火
- **描画** (lines 203-341): トラックレール、アクティブ進捗フィル、目盛り、現在フレームハンドル、`F<n>` ラベル (左)、`HH:MM:SS:FF` ラベル (右)
- **マウスシーク** (lines 343-410): レール上のクリック/ドラッグで `currentFrame_` 更新 + `frameChanged` 発火
- **再生中シークロック**: `seekLockDuringPlayback_` フラグで再生中のシークを防止 (line 346)

### ArtifactTimelineTrackPainterView

- **`currentFrame_` (double)**: フラットウィジェット用のプレイヘッド位置ミラー
- **`setCurrentFrame(double)`**: 内部状態更新 + 再描画
- **`seekRequested(double frame)`** シグナル: マウスクリック時に発火 → `ArtifactTimelineWidget` 経由で `trackView_` / `scrubBar_` を更新

## 状態管理

プレイヘッド状態は複数コンポーネントに**分散しつつ同期**されている:

| コンポーネント | 状態変数 | 型 | 用途 |
|----------------|----------|-----|------|
| `TimelineTrackView::Impl` | `position_` | `double` | シーン座標での正規フレーム位置 |
| `ArtifactTimelineScrubBar::Impl` | `currentFrame_` | `FramePosition` | 表示/シーク用の整数フレーム |
| `TimelinePlayheadOverlay` | `playheadX_` | `int` | 描画用ピクセル X 座標 |
| `ArtifactTimelineTrackPainterView::Impl` | `currentFrame_` | `double` | フラットペインターウィジェット用ミラー |
| `ArtifactPlaybackService` | (engine/controller 経由) | `FramePosition` | 再生エンジンの権威フレーム |

### 同期フロー

```
ユーザー操作 (クリック/キー)
  → TimelineTrackView::setPosition()
  → seekPositionChanged シグナル
  → scrubBar->setCurrentFrame()
  → playheadSync_->sync()

再生エンジンのティック
  → ArtifactPlaybackService::frameChanged
  → 全ウィジェット更新 + playheadSync_->sync()
```

**ガード節**: composition ID の照合により、各タイムラインは自分のコンポジションのフレームのみ更新 (line 1178)。

## データフロー図

```
                    ┌─────────────────────────────────────────────┐
                    │         ArtifactTimelineWidget               │
                    │                                             │
  ┌─────────────┐   │  ┌──────────────────────────────────────┐   │
  │  Navigator  │───┼──│  HeaderSeekFilter                    │   │
  │  (zoom bar) │   │  │  (click → frame position 変換)       │   │
  └─────────────┘   │  └──────────────┬───────────────────────┘   │
                    │                 │                            │
  ┌─────────────┐   │                 ▼                            │
  │ Scrub Bar   │◄──┼──── seekPositionChanged ◄── TimelineTrackView│
  │ (F<n> label)│   │                 │              (position_)   │
  └──────┬──────┘   │                 │                            │
         │          │                 ▼                            │
         │ frameChanged              │                            │
         ▼          │    ┌────────────────────────┐               │
  ActiveContext ─────┼───►  PlayheadSyncFilter     │               │
  Service.seekToFrame│    │  (overlay X 位置同期)  │               │
                    │    └───────────┬────────────┘               │
                    │                │                             │
                    │                ▼                             │
                    │    ┌────────────────────────┐               │
                    │    │ TimelinePlayheadOverlay │               │
                    │    │ (赤線 + 三角形)         │               │
                    │    └────────────────────────┘               │
                    │                                             │
  ┌─────────────┐   │    ┌────────────────────────┐               │
  │ Playback    │───┼───►│ TimelineTrackView       │               │
  │ Service     │   │    │  setPosition()          │               │
  │ frameChanged│   │    └────────────────────────┘               │
  └─────────────┘   │                                             │
                    │    ┌────────────────────────┐               │
                    │    │ TrackPainterView        │               │
                    │    │  setCurrentFrame()      │               │
                    │    │  seekRequested() ──────►│ track に戻る   │
                    │    └────────────────────────┘               │
                    └─────────────────────────────────────────────┘
```

### データフロー要約

1. **再生駆動**: `ArtifactPlaybackService::frameChanged` → 全ウィジェット更新 + `PlayheadSyncFilter::sync()`
2. **ユーザー駆動 (トラックビュー)**: Click/keyboard → `setPosition()` + `seekPositionChanged` → `ScrubBar` + overlay 同期
3. **ユーザー駆動 (スクラブバー)**: Click/drag → `frameChanged` → `trackView_->setPosition()` + `ActiveContext::seekToFrame()`
4. **ユーザー駆動 (ペインタービュー)**: Click → `seekRequested` → `trackView_` / `scrubBar_` 更新 + overlay 同期
5. **スクロール/リサイズ**: `PlayheadSyncFilter` イベントフィルタ → `sync()` でオーバーレイ再配置

## 注目すべき設計パターン

### 1. オーバーレイパターン
`TimelinePlayheadOverlay` は透明でマウス透過な QWidget として右パネル上に重ねられている。
`QGraphicsView` シーンの座標系を直接操作せず、外部オーバーレイとして描画することで座標変換の複雑さを回避。

### 2. イベントフィルタチェーン
`PlayheadSyncFilter`, `HeaderSeekFilter`, `HeaderScrollFilter` の 3 つのイベントフィルタが重複するウィジェットにインストール。
各フィルタは固有のイベントタイプを処理し、処理しないイベントは `false` を返してチェーンを継続。

### 3. 座標マッピングパイプライン
`PlayheadSyncFilter::sync()` は複数段階の座標変換を実行:
```
trackView_->position() → scene point → mapFromScene() → viewport point
  → mapTo(overlayHostWidget_) → overlay-local X
```
ズームレベルやスクロールオフセットに依存せず正しい配置を保証。

### 4. フレーム 0 クランプ
プレイヘッドがフレーム 0 のスクリーン位置より左に表示されないよう明示的にクランプ (line 225-231)。
タイムラインが左にスクロールされた場合の視覚的アーティファクトを防止。

### 5. デュアル描画サーフェス
`TimelineTrackView` (QGraphicsView ベース、ClipItem を保持) と `ArtifactTimelineTrackPainterView` (フラット QWidget ペインター) の両方がプレイヘッド状態を維持。
ペインタービューは `refreshTracks()` 時にトラックビューから同期される。

### 6. シークデッドゾーン
`HeaderSeekFilter` は「予約範囲」機構により、ナビゲータ/ワークエリアのハンドル操作を検出し、それらに入力を委譲。
ハンドルドラッグ中にプレイヘッドがマウスイベントを奪うことを防止 (lines 426-465)。

### 7. 再生中シークロック
`ArtifactTimelineScrubBar` の `seekLockDuringPlayback_` フラグが再生中のシークを防止 (line 346)。

### 8. 命名規則
`WIDGET_MAP.md` によると:
- 赤い縦線 → UI:「シークバー」/ コード: "playhead"
- `F<n>` / `HH:MM:SS:FF` 表示バー → UI:「スクラブバー」/ コード: "scrub bar"

## コンテキストメニュー操作 (ArtifactTimelineWidget.cpp:2052-2153)

プレイヘッド位置 (`impl_->position_`) は以下の操作で使用される:
- **プレイヘッド位置で分割**: 現在のプレイヘッド位置でクリップを分割
- **IN をプレイヘッドにトリム**: クリップの開始点をプレイヘッド位置に移動
- **OUT をプレイヘッドにトリム**: クリップの終了点をプレイヘッド位置に移動

## キーボードショートカット (line 1550-1569)

- `I` キー: ワークエリア IN ポイントを現在のプレイヘッドフレームに設定
- `O` キー: ワークエリア OUT ポイントを現在のプレイヘッドフレームに設定
- `Home` / `End`: 先頭/末尾へジャンプ
- `PageUp/PageDown`: ±10 フレーム
- `Left/Right`: ±1 フレーム

## バックログ参照

- `docs/MILESTONES_BACKLOG.md:8` — "playhead、不感帯、余白、行揃え、ホイール、ドラッグ挙動の最終整理"
- `Artifact/docs/MILESTONE_TIMELINE_RANGE_UNIFICATION_2026-03-17.md:123` — "playhead 表示が frame 表示と矛盾しない"
- `Artifact/docs/MILESTONE_EDIT_MENU_2026-03-13.md:127,133` — プレイヘッド依存操作の無効化条件
