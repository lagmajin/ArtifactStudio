# MILESTONE: ArtifactPr シーケンス合成プレビュー

**最終更新:** 2026-08-21

## 追加実装 (2026-08-21 第2回: 完成度向上総合対応)

`docs/planned/` の完成度向上計画に基づき以下を追加実装した:

### データ整合 (NLE ストア一元化)
- split/duplicate/paste/lift/rippleDelete/insert/overwrite を NLE ストア経由に書き換え
  (`ArtifactPrEditorEngine.cppm`)。確立済みの before/after `nleSnapshot()` + `NLEStateCommand`
  パターンで統一。legacy フォールバックは旧 ID 形式の互換用に残置
- marker 系 6 関数 / transition 系 4 関数も NLE 同期化。Core 側へ
  `NLEProjectStore::removeMarker()` を新設 (`ArtifactCore/include/NLE/Core.ixx:328`)
- TransitionType(4種)→TransitionKind 写像 (DipToBlack→Dissolve, WipeLeft→Wipe+RTL)
- `rebuildLegacySnapshotFromNLE()` が markers/transitions も復元するよう拡張
  (従来は legacy のみ編集分が消失する破壊経路だった)
- saveProject が `json["nle"]` として NLE スナップショットを併せて永続化。
  loadProject は nle 有無で復元 or legacy→NLE 再構築 (`importLegacyProjectToNLE()`)
- addMediaToPool が cv::VideoCapture/imread プローブ → SourceRef 登録
  (frameSize/timeBase/availableRange を実ファイルから取得)
- DemoClip に enabled 追加、NLE Clip.enabled 転記

### 音声プレビュー
- 新規モジュール `AudioPreviewMixer` (.ixx/.cppm):
  FFmpegAudioDecoder(48kHz/stereo float 正規化) 事前デコード → float ミックス →
  AudioRenderer(WASAPI) enqueue。level callback でメーター実データ化
- ProgramMonitorPanel が sequenceChanged で audioTracks を mixer へ反映
  (mute/solo 考慮)、playbackStateChanged で play/pause
- AudioMeterPanel の QRandomGenerator ランダム表示を廃止し実レベルへ
- TimelineClipWidget.setWaveformPeaks() + TimelinePanel.setWaveformProvider()
  でタイムライン波形を実データ化 (0.5 秒バケット peak envelope)

### TrackHeader
- addTrackRow 内に mute/solo トグルボタン (M/S) 追加
  → engine->setTrackMuted/setTrackSolo (NLE Track.mute/solo 変更+Undo)

### プレビュー強化
- トランジション区間の opacity 変調 (Crossfade/Wipe=線形, DipToBlack=三角カーブ)
- 合成プレビューの solo/mute/enabled 反映
- ClipPropertiesPanel に Opacity slider 追加 → setClipOpacity (NLE 同期+Undo)

### Export
- 既存 `SequenceExporter` + `startExport`/FFmpegEncoder 実装を確認
  (本対応前にすでに実装済みだったため対応不要。ExportDialog も createRenderPlan を実利用済み)

### ビルド復帰
- root CMakeLists.txt L708-712: `add_subdirectory(ArtifactPr)` 有効化
  (「W_Object MOC 問題」コメントは不正確 — 全ターゲット Verdigris ベースで MOC 不要)。
  CXX_MODULE_STD 正規化ループより上への配置が必要なため子ディレクトリ列挙位置に追加

## 未検証事項（ユーザー判断で今回ビルド検証スキップ）

- J:\dev\ArtifactStudio には有効な build ディレクトリが無く、既存 build/ は別コピー
  (X:\Dev\ArtifactStudio) に紐づいているため、今回の変更は静的実装相当。
  ビルド検証は次回以降のタスクで行う

---

# 初回実装記録 (2026-08-21)

## 目的

ArtifactPr の Program Monitor を「クリップ単体再生(QMediaPlayer)」から
「タイムライン全ビデオトラックを指定フレーム位置で合成して表示するシーケンスプレビュー」に置き換える。

## 実装内容 (2026-08-21)

### 新規モジュール

| ファイル | 責務 |
|---|---|
| `ArtifactPr/include/MediaFrameDecoder.ixx` + `src/MediaFrameDecoder.cppm` | 非同期フレームデコードサービス。worker QThread + latest-wins queue。動画は `cv::VideoCapture`(FFmpeg backend)、静止画は `cv::imread`。VideoCapture を filePath ごとにキャッシュし、連続フレームは seek なしの逐次 read fast path。結果は canonical RGBA の `ImageF32x4_RGBA`。GUI thread へは std::function コールバックで返す(signal 新規追加なし) |
| `ArtifactPr/include/SequenceCompositor.ixx` + `src/SequenceCompositor.cppm` | 純粋な合成ロジック(UI 非依存・同期)。`composeSequenceLayers(canvasSize, layers, background)`: 各レイヤーをアスペクト維持で canvas に fit させ、配列順(下→上)に `ImageF32x4_RGBA::alphaBlend` で合成 |

### 変更

- `ArtifactPr/include/ArtifactPrEditorEngine.ixx`: `DemoClip` に `opacity` 追加。`rebuildLegacySnapshotFromNLE()` で NLE Clip の opacity を転記
- `ArtifactPr/src/ArtifactPrMainWindow.cppm`:
  - ProgramMonitorPanel を書き換え。`SequenceCanvasWidget`(新規内部クラス、signal なし)へ合成フレームを表示。legacy `DemoSequence` を走査し表示範囲のクリップを収集 → generation 付き decode 要求 → 全フレーム揃い次第 z-order 順に合成 → 表示
  - SourceMonitorPanel の Insert/Overwrite が静止画(png/jpg/jpeg/bmp/tif/tiff/webp)を video track へ張れるように拡張(静止画はワークエリア長で張る)
- `ArtifactPr/src/MediaPanel.cppm`: インポートフィルタに静止画拡張子を追加。media type `"image"` を登録
- `ArtifactCore/include/Image/ImageF32x4_RGBA.ixx` + `src/ImageF32x4_RGBA.cppm`: **ムーブ演算の修正**。`= default` ムーブは生ポインタ `impl_` の浅いコピーでダブルデリートを起こすため、所有権移転型(moved-from は新しい空 Impl を持つ)に変更。既存のコピー動作は不変
- `ArtifactPr/CMakeLists.txt`: 新規 2 モジュール(.ixx/.cppm)登録

## データフロー

```
TransportBar QTimer → engine->setCurrentFrame() → currentFrameChanged(frame)
    → ProgramMonitorPanel.updateTimecode → requestPreviewFrame(frame)
        → legacy DemoSequence を走査 (track.muted / 範囲内 clip / sourceFrame 計算)
        → MediaFrameDecoder.request (generation 付き, worker thread で cv::VideoCapture/imread)
    → onFrameDecoded (全クリップ分揃ったら)
        → composeSequenceLayers (alphaBlend, z-order = videoTracks 配列順)
        → SequenceCanvasWidget.setFrame → toQImage() 明示変換 → paintEvent
sequenceChanged → requestGeneration++ + decoder cache clear + 再リクエスト
```

## 制約・設計判断

- **AGENTS.md 整合**: 合成は `ImageF32x4_RGBA::alphaBlend` のみ(QPainter 合成禁止)。QImage 化は表示境界での明示変換のみ。新規 signal/slot 接続なし(MediaThumbnailer 既存パターンと std::function callback)。QtCSS/setStyleSheet 不使用
- **GPU 経路不使用**: OffscreenCompositionRenderer は Artifact アプリ専属モジュールのため要大規模リファクタ。将来タスクとして分離
- **チャネル順**: BGRA を直書きせず `cv::cvtColor(COLOR_BGR2RGBA)` → `setFromRGBA8` で canonical RGBA 化(taste 整合)

## 未検証事項（ビルド復帰タスクで確認）

本マイルスタイン時点で ArtifactPr は root CMakeLists.txt により無効化中(W_OBJECT/MOC 問題)のため、
以下は静的実装相当であり build/起動 parity は未検証:

1. root CMakeLists.txt L727-730 の `add_subdirectory(ArtifactPr)` 有効化
2. ビルド通過(cv::VideoCapture CAP_FFMPEG リンク、OpenCV imgcodecs/imgproc/videoio)
3. 手動確認:
   - 映像 Import → V1 Insert → Program Monitor に再生位置追従フレーム表示
   - 別クリップ V2 配置 → 重複区間で V2 が手前に合成
   - クリップ速度 2x → ソース進行が速くなる
   - 静止画 png/jpg Import → Insert → 静止画がワークエリア長表示
   - Undo/Redo 後 sequenceChanged 経由で再合成
   - 既存回帰(TimelinePanel ドラッグ/トリム、TransportBar 再生)

## スコープ外(次フェーズ候補)

- トランジション(Crossfade 等)の視覚反映 — CompositeLayer.opacity 変調の拡張点は確保済み
- オーディオメーター/波形の実データ化、Export 実装、ProxyPanel/EffectsPanel
- クリップ opacity の UI 編集導線(DemoClip.opacity 追加済みだが編集 UI は未接続)
