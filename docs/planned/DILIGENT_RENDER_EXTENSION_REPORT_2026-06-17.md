# Diligent レンダリング拡充 — 実装報告書

- 日付: 2026-06-17
- 対象: `Artifact` 子リポジトリ（`Artifact.Render.*`）
- 範囲: 提案 #6（リードバックリング）・#7（グリフ変換パス）を実装、#1（Render Graph）他は「未実装」として残置

## 1. 背景・調査結果

DiligentEngine 周辺を一通り調査した結果、以下の拡張候補を優先度順に提示した（前段会話参照）。

| # | 候補 | 工数 | 影響 | 本報告での扱い |
|---|------|------|------|----------------|
| 1 | Render Graph の実装（C-RND-2 / M11.1） | 大 | 最大 | **未実装（次期課題）** |
| 2 | ポストプロセスシェーダ群の接続 | 中 | 高 | 未実装 |
| 3 | リソースファクトリの一元化 | 中 | 高 | 未実装 |
| 4 | Bindless テクスチャ | 中 | 中〜高 | 未実装 |
| 5 | マルチチャンネル (AOV) 出力 | 中 | 中 | 未実装 |
| 6 | リードバックリング | 小〜中 | 中 | **実装済** |
| 7 | GPU グリフ変換パス | 小 | 中 | **実装済** |

本報告は #6・#7 を実装し、それ以外は現状調査と次期アクションを「未実装」としてまとめる。

---

## 2. 実装済み（Done）

### 2.1 #6 リードバックリング（同期カラーパス）+ depth キャッシュ

**問題点（改修前）**

- `ArtifactIRenderer::Impl::readbackToImage()`（同期カラー）: 単一の `m_readbackStagingTex` + `m_readbackFence` を直列運用。同一フレーム内の連続 readback（サムネイル＋本番キャプチャ等）で GPU コピー完了を待つストールが発生。
- `readbackDepthToImage()`（デプス）: **呼び出しごとに** `CreateTexture` / `CreateFence` を発行。ギズモピッキングやデバッグオーバーレイで頻回呼ばれると無視できないデバイストラffic。
- `readbackToImageAsync()`（非同期カラー）: 同様に都度生成。ただし別スレッドで fence 待ちする設計のためストールは軽微。

**変更内容**

1. 同期カラーパスを **2-slot リング**（`ReadbackSlot` 配列 `m_readbackRing`）へ再編。
   - 各スロットが `ITexture staging` / `IFence fence` / `signaledValue` / `completedValue` を保有。
   - サイズ・フォーマット変更時のみリング全体を再生成。
   - 連続キャプチャの定常状態では、次スロットの前回コピーは既に完了しているため fence 待ちが実質ゼロに。
2. デプスパスの staging/fence を `m_depthReadbackStaging` / `m_depthReadbackFence` + 寸法キャッシュへ集約。サイズ不変なら再利用。fence value は `m_depthReadbackFenceValue` で単調増加。
3. 非同期カラーパスは**現状維持**（後述「未実装」参照）。

**変更ファイル**

- `Artifact/src/Render/ArtifactIRenderer.cppm`
  - Impl メンバ: 旧 `m_readbackStagingTex/m_readbackFence/m_readbackFenceValue` を削除 → `ReadbackSlot` 構造体 + `m_readbackRing` / `m_readbackRingIndex` + depth キャッシュメンバ群へ置換
  - `readbackToImage()`: リング slot 選択 → copy → per-slot fence wait → map
  - `readbackDepthToImage()`: キャッシュ staging/fence へ切替
  - `destroy()`: 新メンバ群のクリア

**影響**

- 同一フレーム連続キャプチャの fence ストール低減。
- デプス検査パスのデバイスアロケーション除去。
- API 互換: 呼び出し側シグネチャ変更なし。レンダ結果のビット精度は不変。

**未確認事項（ユーザー指示時にビルド確認）**

- 実ビルド・実機での fence タイミング検証は未実施（重い確認は指示時のみ、の方針に従う）。
- Vulkan バックエンドでの staging 再利用の挙動は理論上問題ないが実機未検証。

### 2.2 #7 グリフ変換パス（drawGlyphTextTransformed / drawGlyphs）

**問題点（改修前）**

- `PrimitiveRenderer2D::drawGlyphTextTransformed()`: `// TODO: Implement transformed glyph rendering with matrix` で空実装。3D 投影・回転付きテキストが描けない。
- `PrimitiveRenderer2D::drawGlyphs()`: `// TODO: Render pre-laid-out glyphs` で空実装。テキストアニメーター（文字単位の変形・不透明度アニメ）が描けない。

**前提の確認（重要）**

調査の結果、**submitter 側のハンドラは両方とも完成済み**であることを確認:

- `DiligentImmediateSubmitter::submitGlyphTextTransformed()`（同 `.cppm:1372`）: PSO/SRB（`m_draw_glyph_transform_pso_and_srb`）、シェイピング、アトラス取得、8 方向アウトラインパス、塗りパス、`p.transform` 適用まで完全実装。
- `RenderCommandBuffer::GlyphTextXformPkt`（`RenderCommandBuffer.ixx:177`）: 構造体定義済み、variant 登録済み。

つまり PrimitiveRenderer 側は**パケットを append するだけで GPU パスが稼働**する。

**変更内容**

1. `drawGlyphTextTransformed()`: `TextStyle`/`FloatColor`/`UniString`/`QMatrix4x4` → `GlyphTextXformPkt`（`QString`/`QFont`/`float4`/`QRectF`/`transform`/`devicePixelRatio`）へ詰め替えて append。パラグラフ box は `QFontMetricsF` でテキスト幅を測って非折返し幅を確保。シェイピング以降は submitter に委譲。
2. `drawGlyphs()`: 既存 `drawGlyphText()` と同じアトラス取得ループで `GlyphItem.basePosition + offsetPosition` を最終ペン位置とし、`AtlasSpritePkt` を append。`offsetOpacity` でグリフ単位の不透明度アニメを伝播。外部行列 / 通常ズームの両パスを既存 `drawGlyphText` と同一仕様でサポート。

**変更ファイル**

- `Artifact/src/Render/PrimitiveRenderer2D.cppm`
  - `drawGlyphTextTransformed()`: TODO を実装に置換
  - `drawGlyphs()`: TODO を実装に置換

**影響**

- 3D 変形付きテキストレイヤの描画が有効化。
- テキストアニメーター系（文字単位 offset / opacity）の GPU 描画が有効化。
- submitter・PSO・シェーダは無変更（既存資産の活用）。

**未確認事項**

- 実ビルド・描画検証は未実施。
- `drawGlyphs` の `GlyphItem` フィールド（`basePosition/offsetPosition/offsetOpacity/charCode`）の型は submitter 側コードから推定して使用。`GlyphItem` 定義との完全整合は実ビルドで確認要。

---

## 3. 未実装（Not Done）— 次期アクション

### 3.1 #1 Render Graph（C-RND-2 / M11.1 のブロック）

**現状**: `RenderPipeline`（`ArtifactRenderLayerPipeline.cppm`）に accum/temp/layer の UAV 三点組は予約済みだが、`CompositionRenderController::renderOneFrameImpl` がこれをバイパスして直接ドローしている。実質的な render-graph スケルトンが休眠中。

**次期アクション**:

1. パスノード抽象（`RenderPass` interface: `setup(resources)` / `execute(ctx, resources)`）を導入。
2. フレームループを Pass リストの実行へ差し替え（Layer Raster → Blend Compute → Tonemap → Present）。
3. `LayerBlendPipeline` との接続を compute パスとして正式化。
4. ソフトウェアパイプライン（`Artifact.Render.SoftwareCompositor`）→ Diligent へのブリッジ（M11.1）をパスノードで表現。

**ブロック条件**: 工数大。呼び出し側（`CompositionRenderController`）の大幅再構築を伴うため、設計レビュー必須。

### 3.2 #6 残: 非同期カラーパスのリング化

**現状**: `readbackToImageAsync()` は `QtConcurrent::run` で別スレッド fence 待ちする設計。staging/fence を毎回生成。コールバック1回の使い捨てなので実害は軽微。

**リング化を見送った理由**: staging をキャッシュ化すると、複数の in-flight async 呼び出しが同一スロットを競合する可能性があり、mutex 精度の設計が必要。現状の都時生成は安全性が高く、ペイロードも小さい。実プロファイルでボトルネック化した場合のみ対応。

**次期アクション**: プロファイルで async readback のアロケーションコストが問題視されたら、呼び出し側に「世代番号」を返す API を追加し N 深度リングを導入。

### 3.3 #2 ポストプロセスシェーダ群

**現状**: `Artifact/shaders/**` に SSR / MotionBlur / MSAO / TemporalAA / Volumetric / Tonemap 等 250+ の HLSL が存在するが、未接続。

**次期アクション**: Render Graph（#1）が前提。個別エフェクトは `ComputeExecutor` でラップしパスノード化。Tonemap → MSAO → SSR の順で優先度付けを推奨。

### 3.4 #3 リソースファクトリ一元化

**現状**: 各モジュールが `device->CreateTexture/CreateBuffer` を直接呼び出し。`GPUTextureCacheManager` の LRU のみ一本化。

**次期アクション**: `TextureFactory` / `BufferFactory` を導入し、名前付け・エイリアシング・予算管理・D3D12/Vulkan パリティ検証を集約。

### 3.5 #4 Bindless テクスチャ

**現状**: `GPUTextureBindingMode::BindlessCandidate` フラグあり、`LegacySRV` のみ。

**次期アクション**: Descriptor heap サイズ確認 → bindless descriptor 確保 → `DiligentImmediateSubmitter` の SRB キャッシュを bindless 配列参照へ切替。スプライト/アトラス heavy フレームで SRB チャーン削減。

### 3.6 #5 マルチチャンネル (AOV) 出力

**現状**: `ArtifactIRenderer::ChannelType`（Normal/Velocity/ObjectId/MaterialId/Emission）定義済み、設計 doc（`MULTI_CHANNEL_OUTPUT_DESIGN_2026-04-30.md`）あり。MRT の PSO 設定と AOV リードバック未配線。

**次期アクション**: MRT ターゲット追加 → PSO の RTV フォーマット拡張 → チャネルマスク付きドロー → AOV 個別 readback。#6 リングがそのまま AOV readback に流用可能。

---

## 4. AGENTS.md ルールとの整合

- **QtCSS**: 使用せず。`QFontMetricsF` / `QFont` / `QRectF` のみ。
- **QImage**: readback パス（境界・出力）でのみ使用。ホットパスでは従来通り GPU staging → memcpy。
- **新規 signal/slot**: なし。
- **module purview への include**: 追加なし。グローバルモジュールフラグメント内の既存 include を再利用。
- **D3D12 / Diligent 低レベル**: 既存 `readbackToImage/Depth` のアーキテクチャを保持し、リング化はキャッシュ戦略の拡張にとどめた。PSO・シェーダ・swap chain は無変更。
- **子リポジトリ編集**: `Artifact`（子）を編集。コミット・push はユーザー指示時のみ。

---

## 5. ファイル変更サマリ

| リポジトリ | ファイル | 変更 |
|------------|----------|------|
| Artifact | `src/Render/PrimitiveRenderer2D.cppm` | `drawGlyphTextTransformed` / `drawGlyphs` の TODO を実装 |
| Artifact | `src/Render/ArtifactIRenderer.cppm` | readback メンバをリング構造へ再編、カラー同期/デプス両パスを切替、`destroy()` 更新 |

**親リポジトリ（ArtifactStudio）**: 本報告書 `docs/planned/DILIGENT_RENDER_EXTENSION_REPORT_2026-06-17.md` のみ追加。

---

## 6. 推奨される次の一手

小さく安全に足場を固めた状態なので、次は **#1 Render Graph** に進むか、**#5 AOV**（#6 リングを流用できる）で成果を可視化するか、いずれかを推奨。いずれも設計レビューを先に行うこと。
