# Contents Viewer Compare Redesign

**ステータス:** In Progress

日付: 2026-07-12

## Goal

既存 `ArtifactContentsViewer` を、選定済みモック
`docs/design/content_viewer_mockups_2026-07-12/content-viewer-compare-selected.png`
に近い comparison-first Viewer へ整理する。

Asset Browser / Project View から素材を開き、同一キャンバス上で A/B を
wipe、split、difference 比較し、再生・フレーム検査・scope 確認までを短い導線で行える状態を目標とする。

## Confirmed Current State

- `ArtifactContentsViewer` は center dock として登録済み。
- Asset Browser のダブルクリックと Project View の footage 選択から Viewer を開ける。
- Source / Final / Compare mode、Viewer 1-4、A/B assignment、swap を持つ。
- A/B path、wipe percent、swap state は settings に保存される。
- 現行 compare は `QSplitter` で左右の表示幅を変える方式で、同一座標上の wipe ではない。
- image / video / audio / 3D の個別 preview と Parade scope が存在する。
- video / audio の再生は Viewer 内 media controller、composition の再生と RAM Preview は
  `ArtifactPlaybackService` が担当する。

## Responsibility Boundary

- `ArtifactAssetBrowser`: 探索、選択、フィルター、Viewer へ渡す素材の起点。
- `ArtifactContentsViewer`: source inspection、A/B compare、display transform、zoom、scope、素材再生。
- `ArtifactCompositionEditor`: composition 編集、viewport manipulation、composition playback。
- `ArtifactPlaybackService`: composition frame/range/playback/RAM Preview の authoritative state。
- compare 表示は Viewer 内に閉じ、Composition Editor の render surface を複製しない。

## Target Layout

1. 上段: A/B source selector、swap、Wipe/Split/Difference、channel、exposure、display transform、Fit/100%。
2. 中央: 単一座標系の compare canvas。A/B は同じ fit/zoom/pan/rotation を共有する。
3. 右: collapsible Scopes / Metadata。初期段階では既存 Parade を再利用する。
4. 下段: frame ruler、in/out、cache visibility、transport、timecode、fps、preview quality。
5. Asset Browser は独立 dock のまま維持し、Viewer 内へ複製しない。

## Interaction Baseline

- Asset Browser double-click: Viewer を開いて current source を設定。
- `1` / `2`: current source を A / B に割り当てる。既存 Viewer 1-4 shortcut と衝突するため、
  実装前に shortcut map 上の正式割り当てを確定する。
- `Tab`: A/B swap（既存経路を維持）。
- `J` / `K` / `L`: reverse / stop / forward shuttle。
- Left / Right: 1 frame step。Shift 併用は大きい frame step。
- `F`: fit。`100%`: actual pixels。
- wipe handle drag は compare position のみ変更し、A/B の framing は変えない。

## Phase 1: Compare Canvas Foundation

主対象:

- `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cpp`

作業:

- `QSplitter` compare surface を単一 compare canvas に置き換える。
- A/B の画像を同じ destination rect と transform で描画する。
- vertical wipe と draggable handle を最初の完成形とする。
- 現行の A/B assignment、swap、settings persistence を維持する。
- QImage / QPainter composition を新規本流にしない。最初に既存 GPU texture / renderer 経路の再利用可否を確認し、
  CPU fallback が必要なら `ImageF32x4_RGBA` ベースの明示処理に限定する。

完了条件:

- A/B が同一ピクセル座標で重なる。
- wipe 操作で fit/zoom が変化しない。
- resize 後も wipe percent と framing が維持される。

## Phase 2: Compare Modes and Viewer Chrome

進捗メモ:

- compare canvas の単一座標化は着手済み
- `Wipe / Split / Difference` の UI と内部状態は実装中
- compare 状態を読む下段 transport/info strip を追加中
- `Left / Right`、`Shift+Left / Right`、`J / K / L` の Viewer 内 key handling を追加中
- `Diff` が画像以外へ落ちたときは `Split` へフォールバックする暫定整理を追加

作業:

- Wipe / Split / Difference の mode selector を追加。
- A/B selector と swap を最上段へ集約。
- Source / Final / Compare の重複 chrome を整理し、Compare を主要モードとして読みやすくする。
- channel、exposure、display transform、Fit/100% を viewer tool row にまとめる。
- `setStyleSheet()` は追加せず、theme token、`QPalette`、owner-draw、既存 style primitive を使う。
- 既存 Material icon 参照を増やさず、必要な新規アイコンは `Artifact/App/Icon/Studio/` に置く。

完了条件:

- モックと同じ順序で主要操作が読める。
- 16 px 相当でも icon と active mode が識別できる。
- Difference は色空間と alpha の扱いが明示される。

## Phase 3: Playback and Frame Ruler

作業:

- video / image sequence 用の frame 単位 ruler と frame/timecode 表示を追加。
- J/K/L、frame step、loop、audio、in/out を Viewer context に統合。
- source media playback と composition playback の state を混同しない adapter を Viewer 内部に設ける。
- composition source の場合だけ `ArtifactPlaybackService` と RAM Preview state を読む。
- 新規 global signal/slot は追加せず、既存 shortcut routing、service、EventBus 経路を再利用する。

完了条件:

- source video と composition のどちらでも transport 表示が実状態と一致する。
- frame ruler、timecode、fps の換算が source metadata に従う。
- stop / seek / source switch 後に stale frame が残らない。

## Phase 4: Scopes and Metadata Rail

作業:

- 既存 Parade scope を右 rail へ移す。
- planned Scopes milestone と責務を合わせ、Vectorscope / Waveform の重複実装を避ける。
- metadata は filename、path、format、resolution、fps、frame range、color space、bit depth を優先する。
- rail は collapse 可能にして Viewer image area を確保する。

完了条件:

- scope 更新が playback responsiveness を阻害しない。
- metadata と A/B source の対応が明確。
- rail を閉じても compare 操作が欠けない。

## Phase 5: Asset Browser Workflow

主対象:

- `Artifact/src/AppMain.cppm`
- `Artifact/src/Widgets/Asset/ArtifactAssetBrowser.cppm`
- 必要最小限の既存公開 API

作業:

- 既存 double-click open を維持する。
- selection は current source の候補に留め、自動再生や composition import を暗黙に行わない。
- context action と shortcut から Assign A / Assign B を呼べる導線を検討する。
- Project View selection sync を壊さない。
- 新しい公開 signal/slot は原則追加せず、既存 `selectionChanged` / `itemDoubleClicked` と app orchestration を使う。

完了条件:

- Asset Browser から open、Assign A、Assign B、swap、compare まで一貫して到達できる。
- folder double-click、project import、selection sync の既存挙動を維持する。

## Recommended Implementation Slices

1. 単一 compare canvas + vertical wipe。
2. Wipe/Split/Difference と tool row 再配置。
3. frame ruler + transport adapter。
4. scopes/metadata rail。
5. Asset Browser の A/B 導線と polish。

各 slice は可能な限り既存 `.cpp` / `.cppm` に閉じる。公開 API が不要なら `.ixx` を変更しない。

## Guardrails

- child repo、Diligent fork、third-party は変更しない。
- QtCSS、`QColorDialog`、新規 global signal/slot を追加しない。
- compare 合成を `QPainter::CompositionMode` へ逃がさない。
- QImage を新しい再生・合成本流にしない。
- GPU backend の挙動を推測で変更しない。
- build / test / CMake はユーザーの明示許可を得てから実行する。

## Verification Plan

実装後、許可を得て次を確認する。

- image/image、video/video、image/video の A/B assignment。
- wipe 0/50/100%、swap、resize、Fit、100%、pan/zoom。
- source switch 中の playback stop と stale frame 防止。
- J/K/L、frame step、loop、in/out、timecode。
- dock hide/show、workspace restore、Viewer 1-4 state persistence。
- missing file、異なる解像度、異なる fps/color space の比較。
- Asset Browser / Project View の既存 selection と double-click。

## Current Audit Snapshot

コード上で確認できていること:

- compare surface は `QSplitter` 依存から専用 canvas へ移行中
- A/B assignment、swap、wipe percent、recent source、viewer assignment の既存 settings 経路は維持
- `Wipe / Split / Difference` の UI と表示状態は `ArtifactContentsViewer.cpp` 内に追加
- `Diff` は両方 image source の場合のみ有効化し、それ以外は `Split` に戻す暫定処理を追加
- 下段 transport/info strip は `updatePlaybackState()` から更新される
- `Left / Right`、`Shift+Left / Right`、`J / K / L` は `keyPressEvent()` から既存 playback 経路へ接続

まだ未検証のこと:

- 実ビルドが通ること
- image/image、video/video、image/video 各 compare の見え方
- `Difference` の画として十分かどうか
- seek / stop / source switch 時の stale frame や audio state の取りこぼし
- Compare と既存 Source / Final / 3D preview の UI 干渉
- Asset Browser / Project View 側導線の追加検証

## Non-Goals

- Composition Editor の置換。
- node graph の追加。
- final render queue の改修。
- Diligent backend / device / immediate context の変更。
- scopes subsystem 全体の同時再実装。
