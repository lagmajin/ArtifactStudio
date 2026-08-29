# マイルストーン: Timeline DCC-Feel Gaps (2026-08-29)

**最終更新:** 2026-08-29

> 2026-08-29 作成

## 目的

Artifact のタイムラインは planned milestone の大半が「機能の実装」段階で完了／部分実装を迎えており、**「ハイエンド DCC 感」が薄い**。本マイルストーンは、その差分を「機能の不足」ではなく **「触っている感を演出する視覚誘導の不足」** として切り出し、体系的に解消する。

参考: 関連棚卸し [MILESTONE_TIMELINE_STATUS_INDEX_2026-08-29.md](MILESTONE_TIMELINE_STATUS_INDEX_2026-08-29.md)

## 背景

観察で得られた兆候（2026-08-29）:

- TrackPainterView 本体で `QPainter::Antialiasing` の検索結果 0 件、`QPainter::TextAntialiasing` の検索結果 0 件
- `setRenderHint(QPainter::Antialiasing, true)` を立てているのは `TimelinePlayheadDraw.hpp` と LayerPanel 1 箇所のみ
- `TimelineScaleWidget` は逆に `setRenderHint(QPainter::Antialiasing, false)` を明示
- `setMouseTracking` は LayerPanel 1 箇所のみ（タイムライン右ペインには降りていない）
- status bar に `zoom level`／`frame rate`／`selection count` を出す経路が検索結果 0 件
- ruler は `"%1f"` の frame 整数固定、timecode 表記／秒換算／sub-frame tick／frame rate 反映なし
- `QFont::setFamily("Consolas")` のハードコード
- `ArtifactTimelineTrackPainterView.cppm` が 10,545 行、`ArtifactLayerPanelWidget.cppm` が 7,827 行
- Visual Language milestone は 🟡 部分実装（「共通 token/helper 整理、色覚差・light/dark theme 実機回帰、状態形状の横断統一」未完了）

→ AE / Blender / Houdini / Nuke / Cavalry が「触る前に単位と密度が読める」「触った瞬間に情報が降りてくる」「色を意味語彙として使える」「widget 1 個 = 機能 1 個の粒度感」を当たり前に備えているのに対し、Artifact は機能の総量はあるが表面仕上げ層が薄い。

## Goal

- タイムラインを「触っている感」を持つ surface として仕上げる
- 機能の有無ではなく **「触る前／触った瞬間／触った後」の 3 段階で情報が読める** 状態にする
- DCC 比較（AE / Blender / Houdini / Nuke / Cavalry）の "当たり前" を 1 つずつ Artifact に持ち込む

## Non-Goals

- 機能 milestone の新規追加（既存 planned の着手順は変えない）
- 巨大単一 widget の即時分割（AGENTS.md に従い別判断）
- QtCSS / `QColorDialog` の新規採用
- 新規 signal/slot の追加
- `QImage` の本流投入
- `QPainter::CompositionMode` による合成実装


- `QPainter::CompositionMode` による合成実装

## Design Principles

1. **3-stage info flow** — 「触る前（単位・密度）」「触った瞬間（hover/preview）」「触った後（feedback/undo）」の 3 段で必ず情報が流れる
2. **Render hint by default** — タイムライン主要 painter は `Antialiasing` / `TextAntialiasing` / `SmoothPixmapTransform` を **既定で ON** する。pixel-perfect が必要な ruler のみ例外
3. **Status bar as system** — zoom level、frame rate、selection 数、playhead 位置、scrub rate などを **status bar への常時露出** として扱う
4. **Color as vocabulary** — Visual Language milestone の 「色＝意味」 を **token / helper** にまとめ、light/dark 両対応
5. **Hover everywhere** — `setMouseTracking(true)` を Timeline right pane まで拡張し、hover 中は情報を painter 側で即時描画
6. **No regression in existing milestones** — 既存 planned（Operation Feel / Visual Language / InOut Slide / Layer Specialization）の進行を妨げない

## Phases

### Phase 1: Render Quality Baseline

- 目的: タイムライン主要 painter の AA / TextAA を既定で有効化
- 対象:
  - `ArtifactTimelineTrackPainterView` (paintEvent)
  - `ArtifactTimelineLabel`
  - `ArtifactTimelineNavigatorWidget`
  - `ArtifactTimelineScrubBar`
  - `ArtifactTimelineTimeRangeSlider`
  - `ArtifactWorkAreaControlWidget`
  - `ArtifactLayerPanelWidget` (paintEvent)
- 内容:
  - 共通 helper（`enableTimelinePainterHints(QPainter&)`）を新設し、3 つの render hint を一括設定
  - ただし `TimelineScaleWidget` の pixel-perfect tick 描画は helper の対象外（明示 false を維持）
  - `QFont::setFamily("Consolas")` ハードコードを `ArtifactCore::currentDCCTheme().monoFont` 経由に切替
- DoD:
  - タイムライン主要 widget で文字とアイコンのジャギが目視で消える
  - `QPainter::TextAntialiasing` を立てる helper が全主要 painter で呼ばれる

### Phase 2: Status Bar Constants

- 目的: 触る前の「単位と密度」を常時表示
- 対象:
  - `ArtifactMainWindow`（status bar）
  - `ArtifactTimelineWidget`（情報源）
  - `ArtifactTimelineScaleWidget`（zoom level / unit 切替）
- 内容:
  - status bar の右側へ `Zoom %`、`Frame Rate`、`Selection Count`、`Playhead TC` を widget として常設
  - ruler の単位切替: `Frame` / `Timecode (SMPTE)` / `Seconds` / `Bars+Beats` を右クリックメニューまたは status bar から切替
  - ruler の minor tick を zoom level に応じて動的密度（min label px 45 を維持しつつ major / minor を 1/2/5/10 倍で調整）
- DoD:
  - どの widget にフォーカスがあっても zoom / unit / selection 数が status bar から読める
  - ruler をクリックすると timecode 表示に切替できる

### Phase 3: Hover Reachdown

- 目的: 触った瞬間に情報が降りてくる
- 対象:
  - `ArtifactTimelineTrackPainterView`（right pane）
  - `ArtifactTimelineKeyframeModel`（hover 用 snapshot）
- 内容:
  - TrackPainterView の `setMouseTracking(true)` 有効化
  - 既存 keyframe tooltip を hover tracking に接続（mouseMoveEvent で hit test して painter 側に即時 tooltip 描画）
  - hover 中の clip / keyframe が「触る対象」として視覚的に強調される

### Phase 4: Color Vocabulary Consolidation

- 目的: 色＝意味を helper 経由で 1 箇所に集約
- 対象:
  - `ArtifactTimelineVisual`（新設 helper）または `Widgets/Utils/ThemeTokens.cppm` への追加
  - `ArtifactLayerPanelPresentation`（color source 切替）
  - `ArtifactTimelineTrackPainterView`（描画側消費）
- 内容:
  - `selectionColor / hoverColor / currentColor / disabledColor / focusedColor / inactiveColor` を token 化
  - 補間種別／easing／roving／color label を token 経由に切替
  - light / dark theme の両対応（`ArtifactCore::currentDCCTheme()` の派生）
  - 色覚差（deuteranopia / protanopia）向けバリアント
- DoD:
  - タイムライン全体の「色」が helper 経由になり、テーマ切替で一括追従
  - 同じ意味の要素が同じ色になる

### Phase 5: Numeric Handfeel

- 目的: 値を「手触りで」増減できる
- 対象:
  - `ArtifactPlaybackEngine`（scrub controller）
  - `ArtifactTimeCodeWidget`
  - `ArtifactTimelineScrubBar`
- 内容:
  - timecode 表示を **直接編集可能なスピンボックス化**（クリックで選択、上下で 1 frame / Shift で 10 frame / Alt で秒単位）
  - scrub 中の playhead 数値を status bar と同期
  - 数値入力中も ruler / track pane の playhead が追従
- DoD:
  - キーボードのみで timecode 入力 → playhead 移動が成立
  - scrub 速度が status bar に「Scrub 4x」等で表示

### Phase 6: Ruler Unit Switcher

- 目的: ruler の単位を動的に切替可能にする
- 対象:
  - `ArtifactTimelineScaleWidget`
  - `ArtifactTimelineKeyBinding.ixx`（shortcut 追加）
- 内容:
  - Frame / Timecode / Seconds / Bars+Beats の 4 単位を `Alt+R` または右クリックでローテート
  - 単位状態は composition 毎に保持（JSON 永続化）
  - sub-frame tick（0.5f / 0.25f）の ON / OFF を zoom level 閾値で自動切替
- DoD:
  - どの単位でも ruler から timecode 入力が逆算できる
  - composition を再ロードしても単位選択が保持される

## Definition Of Done

- status bar の必須項目がすべてタイムライン focus 中も表示される
- タイムライン主要 painter で AA / TextAA が ON
- hover tracking が右ペイン全域で機能
- 色の token 化が完了し、light/dark 両方で色覚差検証を通過
- timecode を keyboard だけで編集できる
- ruler の単位切替が永続化される
- 既存 milestone（Operation Feel / Visual Language / InOut Slide / Layer Specialization）の進行を妨げない

## 既存 milestone との関係

- [MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md](MILESTONE_TIMELINE_OPERATION_FEEL_REFINEMENT_2026-04-03.md) — 上位。本マイルストーンは Phase 2（scroll/zoom 規則）の表示層を補強
- [MILESTONE_TIMELINE_VISUAL_LANGUAGE_2026-03-31.md](MILESTONE_TIMELINE_VISUAL_LANGUAGE_2026-03-31.md) — 色 token 化を Phase 4 で本格着手
- [MILESTONE_TIMELINE_ZOOM_PAN_2026-04-10.md](MILESTONE_TIMELINE_ZOOM_PAN_2026-04-10.md) — `Ctrl+wheel` zoom の % 表示を Phase 2 で status bar に常設
- [MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md](MILESTONE_TIMELINE_DESIGN_AUDIT_2026-07-04.md) — 監査表の "Shy / Solo / Frame Blend / Motion Blur / Keying Set / Auto-Key" などの不足項目とは別軸

## Recommended Order

1. Phase 1（AA baseline）— 1 widget 単位の差分なので着手が軽い
2. Phase 2（status bar constants）— 触る前の単位感を最優先で出す
3. Phase 6（ruler unit switcher）— Phase 2 と並走可能
4. Phase 3（hover reachdown）— TrackPainterView の責務と絡むため、Phase 1 後
5. Phase 4（color vocabulary）— Visual Language 🟡 の解消
6. Phase 5（numeric handfeel）— Phase 2〜4 と並走可能

## 想定効果

- AE / Blender / Houdini / Nuke / Cavalry の "当たり前" を 1 つずつ取り込み、Artifact のタイムラインを **触る前から触った後まで情報が流れる surface** に引き上げる
- 既存 planned milestone の着手テーマを「機能追加」ではなく「演出層追加」に振り直す補助線になる
- 巨大単一 widget（TrackPainterView 10,545 行）の責務分割を後段で進めるときの前提（hover / color / numeric を別 widget に切り出し可能）を作る

## Next Execution Slice

Phase 1 の最小着手点:

1. `enableTimelinePainterHints(QPainter&)` を `Widgets/Utils/PainterHints.cppm` に新設
2. `ArtifactTimelineTrackPainterView::paintEvent` の先頭で呼び出し
3. `ArtifactLayerPanelWidget::paintEvent` の先頭で呼び出し
4. `ArtifactTimelineNavigatorWidget` / `ScrubBar` / `WorkAreaControlWidget` / `Label` の `paintEvent` で呼び出し
5. `TimelineScaleWidget` は対象外であることをコメントで明示

完了条件: 主要 painter で `QPainter::Antialiasing` と `QPainter::TextAntialiasing` が ON になり、文字とアイコンのジャギが目視で消える。`QFont::setFamily("Consolas")` ハードコードが theme 経由の 1 箇所参照に置き換わる。

## 2026-08-29 現状確認

本マイルストーンは作成直後のため着手実績なし。Phase 1 から着手可能。ビルド・runtime 受入れは AGENTS.md に従いユーザー指示待ち。

  - 空白領域で hover 時は ruler 位置に **read-out TC**（例 `00:00:12:08`）を painter overlay で表示
  - clip edge に hover すると resize handle が強調表示され、Alt 修飾で slide handle に切替
- DoD:
  - マウスが track pane 内に入ってから 1 フレーム以内に情報が出る
  - hover 中の clip / keyframe が「触る対象」として視覚的に強調される

