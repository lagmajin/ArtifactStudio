# Milestone: Render Queue Hardware Encoding Support (M-RQ-ENC-1)
## 🎯 目的
レンダーキューの最終出力でハードウェアアクセラレーション（NVENC, QuickSync, AMF）を使用した高速動画エンコードを実現。従来のソフトウェアエンコード（libx264/libx265）を補完し、リアルタイムに近い出力速度を達成する。

## 🏗️ アーキテクチャ構成
1. **Hardware Encoder Manager**: GPU ハードウェアエンコーダーの検出と管理
2. **FFmpeg Hardware Backend**: FFmpeg の hwaccel API を活用したエンコードパス
3. **Encoder Quality Profiles**: ハードウェアエンコーダー向けの品質プリセット

## 📅 実装フェーズ

### Phase 1: Hardware Encoder Detection & Setup (2026-04-01 - 2026-04-10)
- [ ] GPU ハードウェアエンコーダーの可用性検出（NVENC, QuickSync, AMF）
- [ ] FFmpeg の hwaccel サポート確認と API 統合準備
- [ ] 基本的なハードウェアエンコードパス実装（H.264）

### Phase 2: Quality & Performance Optimization (2026-04-11 - 2026-04-20)
- [ ] ハードウェアエンコーダーの品質プリセット作成（速度 vs 品質トレードオフ）
- [ ] H.265/HEVC サポート追加
- [ ] メモリ帯域最適化（ゼロコピーエンコード）

### Phase 3: UI Integration & Fallback (2026-04-21 - 2026-04-30)
- [ ] レンダーキューのエンコーダー選択 UI 追加
- [ ] ハードウェアエンコード失敗時のソフトウェアフォールバック
- [ ] パフォーマンスメトリクス表示（エンコード速度、GPU 使用率）

### Phase 4: Custom Lossless Intermediate Codec (2026-05-01 - 2026-05-15)
- [ ] GPU 上データを置いたまま処理するロスレス圧縮の実装（CPU 転送最小化）
- [ ] エントロピー符号化の並列化（レガシー直列符号化を廃止）
- [ ] 圧縮率よりスループットを優先した適度な圧縮（PCIe 帯域節約程度）
- [ ] Compute Shader ベースの並列圧縮アルゴリズム開発

### Phase 5: Production-Ready Lossless Codec Implementation (2026-05-16 - 2026-06-15)
- [ ] **全Iフレーム**: 各フレーム独立圧縮（P/Bフレームなし）
- [ ] **ランダムアクセス強い**: ブロック単位アクセスでシーク高速化
- [ ] **RGBA完全対応**: RGBAチャンネル全てを正しく圧縮/展開
- [ ] **10bit/16bit拡張性**: HDR/float16ベースで10bit/16bit拡張容易
- [ ] **GPU resident**: CPUに戻さずGPU内でencode/decode完結
- [ ] **耐障害性**: 1フレーム破損が全体に波及しないブロック構造
- [ ] **コンテナ分離**: 圧縮データとメタデータを分離可能なフォーマット
- [ ] **FFmpegブリッジ対応**: 将来のFFmpeg統合を見据えたデータ構造
- [ ] HLSL を Windows D3D11 ネイティブ前提に最適化
- [ ] Diligent Engine の自動変換を bypass して D3D11 直接使用
- [ ] GPUCompressionPipeline を D3D11 Compute Shader で再実装
- [ ] Windows 専用高速パスとしてレンダーキューパイプラインに統合

### Phase 6: YUV Codec Support & Hardware Integration (2026-06-16 - 2026-07-15)
- [ ] **NV12 (YUV420) 形式対応**: GPU 上で RGB ↔ YUV 変換
- [ ] **色空間変換 Compute Shader**: BT.709/BT.2020 対応
- [ ] **複数 YUV 形式対応**: I420, NV12, P010 (10bit)
- [ ] **FFmpeg YUV ブリッジ**: YUV バッファ直接受け渡し
- [ ] **ハードウェアエンコーダー統合**: NVENC/VCE/QSV ネイティブ対応
- [ ] **GPU ゼロコピーエンコード**: CPU 転送なしの高速パイプライン
- [ ] **HDR YUV 対応**: BT.2020 PQ/HLG 色域

## 🚀 期待される成果
- ソフトウェアエンコード比で 5-10x の速度向上
- 高品質動画出力のリアルタイムプレビュー可能化
- GPU リソース管理の改善（エンコード中のレンダリング影響低減）
- **高速中間データ処理**: GPU resident 圧縮で PCIe 帯域を節約
- **並列処理最適化**: エントロピー符号化の並列化で CPU/GPU 効率向上

## 🔗 関連マイルストーン
- [M-RD-6 FFmpeg GPU Decode Backend](MILESTONE_FFMPEG_GPU_DECODE_BACKEND_2026-03-28.md)
- [M-RD-7 Unified Audio Video Render Output](MILESTONE_UNIFIED_AUDIO_VIDEO_RENDER_OUTPUT_2026-03-28.md)
- [M-RD-8 Integrated Rendering Engine](MILESTONE_INTEGRATED_RENDERING_ENGINE_2026-03-28.md)

## 📋 技術的要件
- FFmpeg 4.4+ の hwaccel API
- Vulkan/D3D12 との相互運用
- プラットフォーム別エンコーダーサポート:
  - NVIDIA: NVENC
  - Intel: QuickSync
  - AMD: AMF</content>
<parameter name="filePath">docs/planned/MILESTONE_RENDER_QUEUE_ENCODING_2026-04-01.md