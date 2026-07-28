# oneTBB ワークスティーリング適用候補 調査書 — 2026-07-28

**作成日:** 2026-07-28
**目的:** oneTBB（work-stealing scheduler）による CPU 効率化の余地がある箇所を棚卸しし、優先度・並列化パターン・注意点を整理する。
**調査方法:** vcpkg 依存確認、`tbb::` / `Core.Parallel` / `QtConcurrent` の全リポジトリ走査、Search エージェントによる 10 領域のソース検証、候補箇所の直接コード確認。docs のみの言及は対象外。

**凡例:** 期待効果 = 高/中/低、パターン = `parallel_for` / `task_group` / `parallel_pipeline` / `parallel_reduce`

---

## 1. 現状サマリ（既に並列化されている領域）

TBB は vcpkg 依存済み（`vcpkg.json` L25）で、既にかなり広く使われている。

| 領域 | 実装 | 備考 |
|------|------|------|
| 共通ラッパー | `ArtifactCore/include/Common/Parallel.ixx` — `Parallel::For(start,end[,workItems],func)` | 閾値: 64 反復未満 or 仕事量 4096 未満はシリアル。grain 16 固定 |
| 合成本流の matte 適用 | `ArtifactCompositionRenderController.cppm` L5950-6214 | alpha 構築/反転/適用とも行並列（256K px 閾値） |
| CPU エフェクト群 | `ArtifactCore/src/ImageProcessing/` の大半 + Artifact 側エフェクト | `EFFECT_MAP_2026-07-16.md` §CPU MT Progress 参照。空間系はほぼ行並列済み。Time Warp / Data Mosh / error-diffusion は**意図的にシリアル**（出力契約） |
| Color | `ColorLUT.cppm`（5 箇所 `Parallel::For`）、TimeRemap frame blend | LUT 適用・生成とも並列済み |
| 物理 | `FluidSolver2D.cppm`（全ソルバー段）、`SoftBodySolver.cppm`（tbb include あり） | |
| Volume/Render | VolumeRenderer / VolumeModifier / MeshToVolume / PyroSimulation / GPURayTracer / AtmosphereFog / NoiseField / IESProfile | `Core.Parallel` 使用 |
| Mask | RotoMask / VolumeMask / LayerMatte | |
| UI 周辺 | AssetBrowser エントリ処理（tbb）、サムネイル（QtConcurrent） | |

**スレッド基盤:** `sharedBackgroundThreadPool()`（`ThreadHelper.cppm` L26-36）= QThreadPool、**最大 4 スレッド固定**。QtConcurrent は I/O・単発タスク用、TBB は計算カーネル用という住み分けが事実上できている。

---

## 2. 適用候補（優先度順）

### 2.1 効果: 高

#### (1) `AudioSpectrum::computeFFT` — 素朴 DFT の O(n·k) シリアルループ
- **場所:** `ArtifactCore/src/Audio/AudioSpectrum.cppm` L19-32
- **現状:** 出力ビン k × 入力サンプル n の二重ループが完全シリアル。コメント自体が「簡易DFT」と明記
- **案:** 外側の k ループを `Parallel::For`（ビン間は完全独立、書き込み先も独立）
- **注意:** そもそも O(n log n) の実 FFT への置換が本筋。並列化はその前後どちらでも安全
- **効果:** 高（スペクトラム表示が毎フレーム走る場合、n=2048/k=64 でも約 13 万回の三角関数呼び出し）

#### (2) `FluidConstraint::resolve` — パーティクル O(n²) 近接排斥
- **場所:** `ArtifactCore/src/Particle/ParticleSystem.cppm` L545-583
- **現状:** 全ペア総当たりのシリアル二重ループ。i/j 両方の position/velocity を対称に書き換える
- **案:** ①空間ハッシュ（グリッド）導入で O(n) 化 → ②セル単位の `parallel_for`。対称書き込みがあるため、色分け（checkerboard）または「i 側のみ書き込み・2 パス」方式が必要
- **注意:** 単純な `parallel_for` 化は**データレース確定**（j 側書き込み）。アルゴリズム改修とセットでないと危険
- **効果:** 高（n=10k で 5000 万ペア。現状はパーティクル数増でフレーム時間が二乗劣化）

#### (3) `ParticleSystem::update` — 粒子更新メインループ
- **場所:** 同ファイル L305-460 付近
- **現状:** 発生 → 力場適用 → 寿命判定 → 統合まで 1 本のシリアルループ。ループ内で `pool_.spawn()` / `pool_.kill()`（サブエミッター Trails/Death）を呼ぶため、そのままでは並列化不可
- **案:** フェーズ分割
  1. 力場適用 + 位置積分（粒子独立）→ `parallel_for`
  2. spawn/kill 要求は `tbb::concurrent_vector` に積み、ループ後にシリアルで反映
- **注意:** サブエミッターの発生順序が変わると乱数系列・見た目が変わりうる。決定論を守るなら要求リストを index 順ソートしてから反映
- **効果:** 高（力場×粒子数の積が支配的。falloff 計算は独立）

#### (4) `MpmSolver2D` — MPM ソルバー全段シリアル
- **場所:** `ArtifactCore/src/Physics/MpmSolver2D.cppm`（tbb include なし）
- **現状:** P2G（粒子→グリッド散布）、グリッド更新、G2P（グリッド→粒子）ともシリアル
- **案:** G2P とグリッド更新は素直に `parallel_for`。P2G は散布書き込みが衝突するため、グリッドの行ブロック色分けか、スレッドローカルグリッド + `parallel_reduce` 統合
- **注意:** FluidSolver2D と同水準の並列化が可能な構造。ただし P2G の reduce 統合は浮動小数の加算順が変わり微小差が出る（ゴールデンテスト側に許容差が必要）
- **効果:** 高（グリッド×粒子で計算量が大きく、Fluid と違い現状ゼロ並列）

### 2.2 効果: 中

#### (5) レンダーキューの書き出しパイプライン化
- **場所:** `Artifact/src/Render/ArtifactRenderQueueService.cppm` L4941-5073（consumer ループ）
- **現状:** レンダーは worker 群（std::thread + outputBuffer + cv）で並列済みだが、consumer 側の「QImage 変換 → プレビュー縮小 → エンコード/ImageExporter 書き込み」がフレーム毎シリアル
- **案:** `tbb::parallel_pipeline` で `serial_in_order`（フレーム取得）→ `parallel`（RGBA 変換・プレビュー縮小・EXR 圧縮）→ `serial_in_order`（encoder.addFrame / ファイル書き込み）の 3 段構成。FFmpeg エンコーダはフレーム順序必須なので最終段は serial_in_order が必然
- **注意:** 既存の worker/buffer 機構と二重にしない。既存機構を pipeline に置換するか、consumer 内の変換だけ `task_group` で先行実行するかの設計判断が必要。EXR zip 圧縮（マルチチャンネル時）は 1 フレームでも重く、並列段に置く価値が高い
- **効果:** 中〜高（image sequence 書き出しでは I/O 以外の変換コストを隠蔽できる）

#### (6) `ShapePath::triangulate` / `flattenSubpaths` — サブパス単位の並列化
- **場所:** `ArtifactCore/src/Shape/ShapePath.cppm`（tbb なし、07-27 native 化直後）
- **現状:** flatten・ear-clipping・輪郭内外判定ともシリアル
- **案:** サブパス（輪郭）単位の `task_group` または `Parallel::For`。flatten は輪郭独立。ear-clipping は穴ブリッジ統合後の単一輪郭処理なので内部並列は不向き、輪郭分類（filled 判定）の総当たり部分が並列向き
- **注意:** 通常のシェイプはサブパス数が少なく（1〜数個）閾値未満。効くのはテキストのアウトライン化や複雑な MergePaths 結果など多輪郭時のみ。**三角形列の出力順は決定論を維持**すること（描画・キャッシュキーに影響）
- **効果:** 中（複雑シェイプ限定。native 化直後で回帰検証が必要な時期なので、品質検証完了後に着手が安全）

#### (7) Animation Layer bake のフレームサンプリング
- **場所:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm` L3860-3888
- **現状:** `stack.evaluateWithBase()` をフレーム毎にシリアル呼び出しして samples を構築
- **案:** サンプル収集を `parallel_for`（各フレーム評価は読み取りのみ・独立）、`addKeyFrame` 反映はシリアルのまま。プロパティスタック毎の bake も `task_group` で並走可
- **注意:** `evaluateWithBase` が本当に const/再入安全か要確認（AnimatableValueT 内部にキャッシュがあれば不可）。bake は対話頻度が低いので優先度は範囲長次第
- **効果:** 中（長尺 work area の bake で体感差）

#### (8) `AudioWaveform::process` — RMS ビン抽出
- **場所:** `ArtifactCore/src/Audio/AudioWaveform.cppm` L28-41
- **現状:** resolution ビンのシリアルループ。ビン間独立
- **案:** `Parallel::For(0, resolution_, frames*channels, ...)`（workItems 付きで小バッファはシリアル維持）
- **効果:** 低〜中（resolution 既定が小さければ効果薄。長尺の波形キャッシュ一括生成時に効く）

### 2.3 効果: 低 / 非推奨

| 箇所 | 理由 |
|------|------|
| プロパティ毎のキーフレーム評価（`ArtifactAbstractLayer` L1890-1930） | 1 プロパティの評価が軽量すぎ、`propertyCacheMutex_` と QString キー操作が支配的。並列化のオーバーヘッドが上回る |
| `AudioMixer::process` のバス処理 | トポロジカル順序依存（routing/sidechain）。同一レベルのバスだけ `task_group` 化は可能だが、バス数が少なく、リアルタイム音声はレイテンシ一定性の方が重要 |
| Time Warp / Data Mosh / error-diffusion dithering | EFFECT_MAP に明記の通り、履歴所有権・ピクセル順序が出力契約。並列化禁止 |
| 動画デコード prefetch（`ArtifactVideoLayer` L1794-1877） | FFmpeg デコーダはシーケンシャルが前提。並列デコードはデコーダコンテキスト複数化が必要で、TBB の問題ではない |
| OCIOConfig | 設定パースのみでピクセルループなし |

---

## 3. 基盤面の観察

1. **プール二重化の現状は許容範囲**: QThreadPool（4 本、I/O・単発用）と TBB ワーカー（HW 並列数）が共存するが、用途分離できており oversubscription の実害は薄い。TBB 側を制御したくなったら `tbb::global_control`（max_allowed_parallelism）や `task_arena` を単一箇所（アプリ初期化）で導入するのが筋。**各所で arena を乱立させないこと**
2. **`Parallel::For` の grain 16 固定**: 行単位処理には妥当。候補 (1)(8) のような「1 反復が重い少数反復」には grain 1 相当が欲しくなるため、必要になった時点で grain 指定オーバーロードを検討（現時点では不要）
3. **決定論とテスト**: 行独立の `parallel_for` は決定論を壊さない。`parallel_reduce` 系（P2G 統合など）は浮動小数の加算順で微小差が出るため、導入時は比較テストに許容誤差を設けること
4. **Qt との境界**: TBB ワーカーから QWidget / QPixmap / signal 発火は不可。QImage は implicit sharing の detach に注意（並列区間前に `bits()` 等で detach を済ませる既存パターンを踏襲）

---

## 4. 推奨着手順

1. **(1) AudioSpectrum DFT** — 独立ビンで最も安全、数行で完了（FFT 化と併せて検討）
2. **(4) MpmSolver2D の G2P / グリッド段** — FluidSolver2D と同型のパターン適用、P2G は第 2 段階
3. **(3) ParticleSystem::update のフェーズ分割** — 力場+積分の並列化（spawn/kill 遅延反映とセット）
4. **(2) FluidConstraint** — 空間ハッシュ導入と同時に（アルゴリズム改修が主、並列化は従）
5. **(5) レンダーキュー pipeline 化** — 効果は大きいが既存 worker 機構との整合設計が必要
6. **(6) ShapePath** — native bezier 経路の品質検証（別レポート §4-4）完了後

---

## 5. 更新履歴

- 2026-07-28: 初版。全リポジトリの tbb/Core.Parallel/QtConcurrent 走査 + 候補箇所のソース直接確認に基づく。
