# Timeline Diligent GPU Surface

**最終更新:** 2026-08-29

**ステータス:** In Progress

## 目的

現在の `ArtifactTimelineTrackPainterView` を削除・置換せず、Diligent Engine の共有デバイス上に表示専用のタイムライン面を並行実装する。D3D12 と Vulkan は Diligent の backend-neutral な描画経路で共通化する。

## 不変条件

- 現在の QWidget/QPainter タイムラインを既定表示、編集入力、選択、Undo/Redo、フォールバックの正規経路として残す。
- GPU面からモデルを変更しない。初期段階は表示専用とする。
- UI状態から immutable な `DiligentTimelineVisualSnapshot` を作り、GPU面は最新の完成済みスナップショットだけを消費する。
- UIワーカースレッドから Diligent の immediate context や swap chain を直接操作しない。`setSnapshot()` はmutex保護された最新snapshotの交換とthread-safeなevent投稿だけを行い、初期化・resize・submit・presentはwindow所有threadに限定する。
- CPU readback、フレームごとの `WaitForIdle`、Qt合成への迂回をホットパスに入れない。
- GPU初期化失敗、software backend、device loss 時は既存タイムラインへ戻せる構造を維持する。

## 構成

1. `ArtifactTimelineWidget` が既存ビューと同じ表示状態からスナップショットを生成する。
2. 独立module `Artifact.Widgets.Timeline.DiligentRenderWindow` の `ArtifactDiligentTimelineRenderWindow` が共有Diligentデバイスと専用swap chainを所有する。
3. `PrimitiveRenderer2D` が矩形、線、三角形を `RenderCommandBuffer` に記録する。
4. `DiligentImmediateSubmitter` がD3D12またはVulkan backendへ送出する。

## 実装段階

### Phase 1: 並行表示面

- 背景、トラック行、時間グリッド、クリップ、再生ヘッド、キーフレームを描画する。
- 明示的なpreview切替時だけGPU面を表示する。
- 公開API `setGpuTimelinePreviewEnabled(true)` または環境変数 `ARTIFACT_GPU_TIMELINE_PREVIEW=1` を明示的なopt-in入口とする。
- 現在のタイムラインは常に生成・保持する。

### Phase 2: 表示品質と負荷制御

- 可視範囲だけをスナップショット化する。
- 矩形batchと更新世代番号を利用し、未変更フレームの再構築を避ける。
- worker生成されたsnapshotをlatest-winsで交換し、未処理のrender eventはatomic flagで1件にcoalesceする。
- GPUタイマーとCPU snapshot構築時間を別々に計測する。

### Phase 3: 機能パリティ

- GPU glyph atlasでラベルを追加する。
- 波形・サムネイルは非同期キャッシュからGPU textureへ供給する。
- 入力移管は表示パリティと安定性の確認後に別マイルストーンで判断する。

## 完了条件

- DX12/Vulkan双方でGPU previewを開閉できる。
- 既存タイムラインへ即時復帰でき、編集状態が失われない。
- スクロール、ズーム、再生ヘッド、選択表示が既存ビューと同期する。
- GPU面を無効化した通常動作に回帰がない。
