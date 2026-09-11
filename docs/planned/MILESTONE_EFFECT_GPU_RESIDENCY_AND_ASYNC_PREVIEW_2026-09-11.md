# Milestone: GPU常駐エフェクト連鎖 + 非同期プレビュー再描画

**最終更新:** 2026-09-11
**Status:** Design / 未着手
**Goal:** エフェクト連鎖のGPU→CPU読み戻しを設計で消し、構造変更時 (有効切替等) のプレビュー再描画を非同期化する。直近の個別最適 (ブラー半分化、共有ヘルパーの使い回し) の次段。

## 背景 (コード確認済み)

- 連鎖の実態はCPU画像渡し: `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cppm:222-251` の `applyRasterizerEffects` が QImage→cv::Mat→CPU画像→段毎 `applyConfigured(current, next)` →QImage に戻す。GPU対応段も内部で都度アップロード＋読み戻す。
- 読み戻しの正体: `CopyTexture → Flush → WaitForIdle → Map` (`Artifact/src/Effects/Blur/BlurEffect.cppm:146-174`, 共有ヘルパー `Artifact/src/Effect/ArtifactCreativeEffects.cppm` の readback 部も同型)。毎フレームのパイプライン停止になる。
- 器は対応済み・利用ゼロ: `ImageF32x4RGBAWithCache` は GPU テクスチャ内蔵＋SRV/UAV口＋CPU/GPU同期関数を持つ (`ArtifactCore/src/Image/ImageF32x4_With_Cache.cppm:53-152`) が、利用者は存在しない。
- 切替の重さの正体: トグル自体は軽い (`ArtifactPropertyWidget.cppm:1820` のサービス呼び出しのみ)。重いのは直後の `scheduleRebuild(0)`＋ソフトウェア再描画のUIスレッド同期実行で、キャッシュ切替目の全面再計算が固まる。
- 世代カウンタの前例あり: `ArtifactPlaybackService.cppm` に generation＋破棄判定 (`:214`, `:655`, `:679`) とワーカースレッド (`previewDiskWriterThread_:219`) の実績がある。流用する。
- ビューポートは QTimer 駆動 (`ArtifactCompositionEditor.cppm:4639-4643`) で、固まりの主犯はソフトウェア効果連鎖側。

## Design A: GPU常駐セグメント連鎖 (読み戻しゼロ)

### A-0 済み (前提)

- ブラーCPU半分化、共有ヘルパーのパイプライン・テクスチャ使い回し＋lease返却。

### A-1 セグメント化 (本体)

- Rasterizer段のうち連続するGPU対応段を1セグメントとし、段間をGPUテクスチャ受け渡しにする。セグメント入口で1回アップロード、出口で1回読み戻す。
- GPU非対応段はセグメント境界 (強制往復) とする。
- 対応判定は `apply()` の AUTO 解決と同一述語 (`supportsGPU() && gpuImpl_`, `ArtifactAbstractEffect.cppm:662-664`) を使い、二重定義しない。
- テクスチャと一緒に運ぶもの: 色記述子 (RGBA/BGRA! Blur で赤青反転の実績あり)、premultiplied約束、サイズ。段毎の前提が違う場合はセグメントを切る。

### A-2 遅延読み戻し (任意)

- CPU消費 (表示・符号化・CPU専用段) が要るまで読み戻さない。器の `UpdateCpuDataFromGpuTexture / UpdateGpuTextureFromCpuData` を正として使う。
- CPU/GPU の dirty 管理を器に一本化し、各エフェクトの自前テクスチャ管理は段階的に器へ寄せる。

### 受入条件

- [ ] EffectProfile で GPU 対応連鎖の frame 時間が改善し、CPU/GPUセグメント出力の画素差が許容内
- [ ] CPU専用段混じりで正しく往復し、色反転・α壊れの回帰なし
- [ ] 新規グローバル signal なし、既存 Undo 経路・保存形式の変更なし

## Design B: 構造変更時の非同期プレビュー再描画

### B-1 最終絵の保持＋世代破棄 (本体)

- 切替時は最終正常フレームを置いたまま世代番号だけ進め、ワーカーが新フレームを計算する。古い結果は世代不一致で捨てる (disk writer の `:655/:679` と同型)。
- ソフトウェア効果連鎖は CPU のみなのでワーカー実行に適する。Diligent immediate context はスレッドセーフでない前提を守り、GPU アップロードは GUI スレッドに戻す。

### B-2 連打の合流 (任意)

- scheduleRebuild のデバウンスに合わせ、描画要求も合流させる。連打は最後の1回だけ描く。
- QImage のスレッド越しは implicit sharing の detach 規律を守る (書き側で複製)。

### 受入条件

- [ ] 切替→表示の体感遅延が短縮し、破れたフレームが出ない
- [ ] 連打時に描画回数が合流する
- [ ] Undo との順序が崩れない (世代と Undo 適用順の整合)
- [ ] 新規グローバル signal なし (既存 LayerChangedEvent＋世代で足す)

## 非目標・制約

- ソフトレンダラーの新機能化はしない (GPU優先方針)。ワーカー化は CPU 連鎖の実行場所を移すだけ。
- ホットパスに重い確保・ロック付き汎用アロケータ・文字列整形ログを持ち込まない。診断は category/flag 遅延評価。
- QImage 新規採用・QPainter 新規合成・QtCSS・QColorDialog・新規単一キー・新規シグナル接続を追加しない。
- Diligent 低レベルは最小接触。texture 再生成・大容量 readback を増やさない方向のみ。
- 子リポジトリの変更は別途指示。ビルド・テスト・CMake はユーザー許可後。
- Point cube shadow / Area 近似 / 複数キャスター / CSM は本書の範囲外 (別途起案)。

## 順序案

A-1 → B-1 → A-2 → B-2。A-1 と B-1 は独立に着手可。いずれも EffectProfile と画素差分の前後比較を条件とする。
