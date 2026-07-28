# 採用候補技術調査 — アプリ改良のための技術選定メモ

- 日付: 2026-07-27
- 種別: 技術調査 / 採用候補の優先順位付け
- 前提: 開発優先方針（静止画 / 連番画像 / シェイプ / 画像処理・合成 / 3D レイヤーの完成度優先、動画対応は後回し）
- 参照した内部文書:
  - `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md`
  - `docs/analysis/REPORT_AE_GAP_UPDATE_2026-07-03.md`
  - `docs/analysis/RENDER_PERF_HOTPATH_INVESTIGATION_2026-07-08.md`
  - `docs/analysis/DESIRED_IMPORT_FORMATS_2026-04-19.md`
  - `docs/analysis/ARTIST_PAIN_POINTS_CORE_2026-04-19.md`
  - `docs/planned/MILESTONES_BACKLOG.md`
  - `docs/analysis/NEURAL_TEXTURE_COMPRESSION_FEASIBILITY_2026-07-27.md`（NTC は別文書で調査済み）

## 結論（推奨トップ 5）

| 順位 | 技術 | 対応するギャップ | 工数感 |
| --- | --- | --- | --- |
| 1 | **OpenColorIO 2.5** | OCIO/ACES 未着手（P3 だが基盤は既に用意済み）、LUT ブラウザ、.cube/.3dl | 中 |
| 2 | **ThorVG v1.0**（または Blend2D） | シェイプレイヤーの QPainterPath 依存脱却、SVG パスインポート、Lottie インポート | 中 |
| 3 | **ONNX モデル群の活用**（新規ネイティブ依存ゼロ） | Roto/Paint 未着手、タイムリマップの optical flow 欠落、アップスケール | 小〜中 |
| 4 | **zstd / lz4 + Tracy** | キャッシュ圧縮・ディスクキャッシュ欠落、性能調査の実測手段欠落 | 小 |
| 5 | **HarfBuzz + FreeType の正式依存化** | テキスト整形の Qt フォールバック脱却（CJK/絵文字 ZWJ/複雑文字） | 中 |

いずれも既存方針（QPainter/QImage 新規禁止、GPU パイプライン優先）と整合する方向の技術。

---

## 1. カラーマネジメント: OpenColorIO 2.5 【最優先】

- **現状**: `SurfaceColorContract` / `SurfaceColorDescriptor` / `SourceInterpretOverride` など解釈基盤は実装済みだが、production カラーパイプライン（ACES / LUT / ディスプレイ変換）が無い。LUT ブラウザは部分実装。
- **なぜ今か**: OCIO 2.5 は 2025-09 リリースで **VFX Reference Platform CY2026 採用**。AE 本家も OCIO/ACES ネイティブ対応済みで、「プロ用途の信用」に直結する。F32 リニア合成パスを既に持っているため、受け皿としての相性が良い。
- **統合ポイント**: OCIO は `GpuShaderDesc` で **GPU シェーダコード生成**に対応しており、Diligent の HLSL/GLSL パイプラインに変換シェーダとして組み込める（CPU パスも `CPUProcessor` で可能）。
- **入手**: vcpkg ポートあり（`opencolorio`）。
- **段階案**: ① ビュー変換（working → display）のみ OCIO 化 → ② 入力素材の IDT（`SourceInterpretOverride` と接続）→ ③ ACES config 同梱 + LUT ブラウザ完成。

## 2. ベクターグラフィックス: ThorVG v1.0（対抗: Blend2D）

- **現状**: シェイプレイヤーの単純形状は **QPainterPath による CPU 描画**で、プロジェクトルール（QPainter 新規禁止・縮小方向）と矛盾した状態。SVG は nanosvg でビットマップ化のみ。Lottie は未対応（`DESIRED_IMPORT_FORMATS` で高優先）。
- **ThorVG v1.0**（2025 年に v1.0 到達、MIT）:
  - SVG / **Lottie** のシーン読み込みを標準搭載。パス・シーングラフ API を持ち、「パスをアニメーション可能な状態でインポート」の要求に直接応える。
  - CPU ラスタライザは軽量・高速（代表ベンチで既存エンジン比 ~2.3 倍）。出力バッファを `ImageF32x4_RGBA` / GPU アップロード経路に直結でき、Qt 合成を経由しない。
- **Blend2D**（zlib ライセンス、JIT 搭載 CPU 2D ラスタライザ）:
  - 純粋な描画速度は最速級。ただし SVG/Lottie ローダは無く、フォーマット対応は別途必要。
  - 「シェイプのラスタライズ品質・速度だけを差し替えたい」なら候補。
- **推奨**: フォーマット対応（SVG パス / Lottie）まで一気に得られる **ThorVG を第一候補**。ラスタライズ性能が課題になった箇所のみ Blend2D を局所採用する選択肢を残す。
- **入手**: 両方とも vcpkg ポートあり（`thorvg` / `blend2d`）。

## 3. AI 機能: 既存 ONNX Runtime + DirectML の上にモデルを載せる

新規ネイティブ依存が不要（`onnxruntime[directml]` は導入・実装済み、`OnnxDmlLocalAgent` あり）で、費用対効果が高い領域。

| モデル | 用途 | 対応ギャップ |
| --- | --- | --- |
| SAM 2 / BiRefNet / RVM | 自動ロトスコープ・被写体マット生成 | Roto/Paint 完全未着手（P3）を「AI マット」で先回り |
| RIFE / RAFT | フレーム補間・optical flow | タイムリマップ / フレームブレンドの「Optical Flow 相当がない」 |
| Real-ESRGAN | 静止画・連番素材のアップスケール | 静止画レイヤー品質向上（現行優先方針と合致） |
| LaMa | インペインティング（コンテンツ認識塗りつぶし） | AE の Content-Aware Fill 相当の差別化機能 |
| Depth Anything V2 | 単眼深度推定 | 3D レイヤー/擬似 3D・DOF エフェクトの素材化 |

- **注意**: モデル配布（ライセンス・サイズ・ダウンロード導線）の設計が必要。推論は非リアルタイム前提（適用時に生成してキャッシュ）から始めるのが安全。

## 4. キャッシュ・性能基盤: zstd / lz4 / Tracy

- **zstd + lz4**（vcpkg あり、工数小）:
  - RAM プレビューのフレームキャッシュ圧縮（F32 サーフェスは 16 bytes/px と巨大。lz4 で実質メモリ 2〜4 倍化）。
  - 未実装の**ディスクキャッシュ**（zstd + メモリマップ）とプロジェクト保存の圧縮。
  - 連番画像ワークフロー（M-AB-SEQ-2、次期着手予定）のプリデコードキャッシュにも直結。
- **F16（half）キャッシュ表現**: ライブラリ追加不要。キャッシュ専用に F32→F16 化するだけで RAM/VRAM 半減。`RENDER_PERF_HOTPATH_INVESTIGATION` の B4（毎フレーム再アップロード）対策と併せて検討。
- **Tracy Profiler**（vcpkg あり、開発ツール）:
  - 同文書が「最終確定はプロファイラ計測が必要（Nsight / GPA / Tracy）」と明記。B1〜B4 の修正効果検証に必須の実測手段で、ヘッダ 1 つ + ゾーンマクロで導入できる。リリースビルドからは除外可能。

## 5. テキスト整形: HarfBuzz + FreeType の正式依存化

- **現状**: GPU GlyphAtlas + PrimitiveRenderer2D の経路は良い設計だが、shaping は「HarfBuzz/Qt フォールバック」で `vcpkg.json` に harfbuzz が無く、Qt 私有 API 依存が残る。CJK/アラビア語/絵文字 ZWJ・書記素クラスタ処理に既知の弱さ。
- **提案**: `harfbuzz` + `freetype` を直接依存に昇格し、shaping → GlyphAtlas の経路を Qt 非依存にする。将来的にフォントフォールバックのグリフ単位制御も可能になる。
- **補助候補**: `msdfgen` / msdf-atlas-gen — 拡大縮小アニメーションが多いモーショングラフィックスでは MSDF グリフが有利（拡大でボケない・アトラス再生成不要）。GlyphAtlas の将来拡張として記録。

## 6. インポートフォーマット系（ユーザー需要ベース）

`DESIRED_IMPORT_FORMATS_2026-04-19.md` の需要に対応する採用候補:

| フォーマット | 候補技術 | 備考 |
| --- | --- | --- |
| PSD（レイヤー維持） | **psd_sdk**（Molecular Matters、MIT） | vcpkg 未収載のため third_party 取り込み。レイヤー/マスク/ブレンドモード読める |
| SVG（パスとして） | ThorVG（§2 と共通） | nanosvg のビットマップ変換から昇格 |
| Lottie | ThorVG（§2 と共通） | インポート需要「急増中」と内部文書に記載 |
| EXR | 追加依存不要 | OpenImageIO が EXR 対応済み。M-PRO-MEDIA-1 の解釈経路への配線のみ |
| PMD/PMX/VMD (MMD) | 保守されている決定版 OSS が無く**自前パーサ推奨** | 「これだけでユーザーが 3 倍」との内部評価。フォーマット仕様は公開されており実装可能 |
| LUT (.cube/.3dl) | OCIO（§1 と共通） | OCIO が両形式のリーダーを内蔵 |

## 7. エフェクトプラグイン: OpenFX 1.5（継続投資）

- **現状**: ArtifactCore に OFX render pipeline / PluginLoader / Sandbox / Registry が**実装開始済み**。
- **動向**: OpenFX は 2022 年に ASWF（Academy Software Foundation）入りし、2024 年の v1.5 でカラーマネジメント・GPU サポートが強化。Resolve/Nuke/Natron 等のプラグイン資産に接続できる唯一の現実的な規格。
- **提案**: 新規採用ではなく**公式 v1.5 ヘッダ/サポートライブラリへの追従**と、サードパーティ実プラグイン（例: 無償の OFX プラグイン）での実証を次の一手にする。

## 8. 今は採用を見送るもの（理由付き）

| 技術 | 見送り理由 |
| --- | --- |
| Skia | 巨大依存。ThorVG/Blend2D + 既存 GPU パイプラインで責務が足りる |
| OpenUSD | 内部需要文書でも「まだ一般的ではない」。3D レイヤー安定後に再評価 |
| ニューラルテクスチャ圧縮 (RTXNTC) | 別文書の結論どおり時期尚早（SDK BETA、on-sample 出荷不可） |
| 動画デコード刷新（PyAV/独自 FFmpeg 直叩き等） | 開発優先方針で動画対応は後回しと明記 |
| TensorRT / CUDA 系推論 | NVIDIA 専用になる。DirectML で全ベンダー対応済みのため不要 |
| 新スクリプト言語（Lua 等） | AngelScript + Expression エンジン既存。分散を避ける |

## 9. 推奨着手順（工数 × 効果）

1. **zstd/lz4 + Tracy**（工数小・即効）— キャッシュ圧縮とホットパス実測。B1〜B4 修正の効果測定基盤
2. **OCIO 2.5**（中）— ビュー変換から段階導入。プロ用途の信用に直結し、LUT 需要も同時解決
3. **ThorVG**（中）— シェイプレイヤーの脱 QPainter + SVG パス/Lottie インポートを一石三鳥で獲得
4. **ONNX モデル 1 本目**（小〜中）— まず BiRefNet（静止画マット）か Real-ESRGAN（アップスケール）。既存 DirectML 基盤の実証を兼ねる
5. **HarfBuzz/FreeType 正式化**（中）— テキストアニメーター品質向上と併走
6. **psd_sdk / PMX**（中）— インポート需要対応。ユーザー獲得インパクトは PMX が最大

## 未検証事項

- ThorVG の Lottie 対応カバレッジ（エクスプレッション付き Lottie 等の限界）は実データでの検証が必要
- psd_sdk の CMYK / スマートオブジェクト等の対応範囲
- OCIO GPU シェーダ生成と Diligent HLSL パイプラインの結合コスト（GLSL→HLSL 変換の要否）
- 各 ONNX モデルの DirectML 上での実測速度・商用利用ライセンス（モデルごとに要確認）
- vcpkg の `openfx` ポート有無（無ければヘッダのみ third_party 取り込みで足りる）

## 参照

- OpenColorIO 2.5: https://opencolorio.readthedocs.io/en/latest/releases/ocio_2_5.html
- ThorVG v1.0: https://www.thorvg.org/post/thorvg-v1-0-a-new-generation-released
- Blend2D: https://blend2d.com/performance.html
- OpenFX 1.5 (ASWF): https://www.aswf.io/blog/openfx-v1-5/
- psd_sdk: https://github.com/MolecularMatters/psd_sdk
- Tracy Profiler: https://github.com/wolfpld/tracy
