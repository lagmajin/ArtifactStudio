# Image / ImageProcessing 詳細監査

**日付**: 2026-08-02
**調査範囲**: ソースコード直接読み込み（~60ヘッダ）

---

## 1. ImageF32x4_RGBA — 主力画像型 🟢 90%

`Image/include/ImageF32x4_RGBA.ixx`

| 機能 | 状態 | 備考 |
|------|------|------|
| 32bit float 4ch 内部表現 | ✅ | CV_32FC4（OpenCV Mat） |
| 8bit RGBA 内部表現 | ✅ | CV_8UC4 |
| QImage 変換 | ✅ | toQImage() |
| OpenCV Mat 変換 | ✅ | toCVMat() / toCanonicalRGBA32FC4() / toCanonicalBGRA32FC4() |
| 生データアクセス | ✅ | rgba32fData() / rgba8Data() |
| ファイルI/O | ✅ | load() / save()（OIIO 使用） |
| 色記述子 | ✅ | `SurfaceColorDescriptor` を保持。BGRA↔RGBA の違いを明示的に管理 |
| GPU アップロード連携 | ✅ | `ImageSurfaceView` 経由で `GpuImageUploadBuffer` に変換可能 |

**アーキテクチャ上の注意**: `toCVMat()` は **Backing-memory order** でデータを返す（BGRA の可能性あり）。
`toCanonicalRGBA32FC4()` が論理 RGBA 順を保証する。GPU テクスチャにアップロードする前に **BGRA→RGBA の正規化**が必要。

---

## 2. MultiChannelImage — AOV 画像 🟢 85%

`Image/include/MultiChannelImage.ixx`

| チャンネル種別 | 状態 |
|---------------|------|
| R, G, B, A | ✅ ベース |
| Depth | ✅ |
| Normal X, Y, Z | ✅ |
| Velocity X, Y | ✅ |
| ObjectId | ✅ |
| MaterialId | ✅ |
| Albedo R, G, B | ✅ |
| Emission | ✅ |
| Custom | ✅ |

16チャンネル完全サポート。`ChannelMap = std::map<ChannelType, SharedPtr<VideoChannel>>`。
`toVideoFrame()` / `copyFrom(VideoFrame)` で VideoFrame との相互変換も可能。

---

## 3. SurfaceColorDescriptor — 色契約 🟢 95%

`Graphics/include/SurfaceColorContract.ixx`

画像が抱える「自分は何色か」を宣言する記述子。この設計は非常に良い。

| フィールド | 型 | 説明 |
|-----------|-----|------|
| `storage` | `SurfacePixelStorage` | RGBA8UNorm / RGBA8UNormSrgb / RGBA16Float / RGBA32Float |
| `channelOrder` | `SurfaceChannelOrder` | RGBA / BGRA / RGB / BGR / Gray |
| `primaries` | `SurfaceColorPrimaries` | sRGB_Rec709_D65 / DisplayP3_D65 / Rec2020_D65 / ACES_AP0 / ACES_AP1 |
| `transfer` | `TransferFunction` | 16種の伝達関数から選択 |
| `alphaMode` | `SurfaceAlphaMode` | Opaque / Straight / Premultiplied / Coverage |
| `range` | `SurfaceColorRange` | SceneReferred / DisplayReferred / Data |
| `transferKnown` | `bool` | 伝達関数が明示的に設定されているか |

**プリセット**:
- `canonicalLinearPremultiplied()` — RGBA32Float, RGBA, Linear, Premultiplied, SceneReferred（内部作業用の正準形式）
- `encodedSrgbRgba8Premultiplied()` — 8bit sRGB Premultiplied（Web/UI 出力用）
- `legacyOpenCvBgra32Float()` — 旧 OpenCV BGRA 互換
- `isFullySpecified()` / `isCanonical()` 検証メソッド
- `static_assert` でコンパイル時に検証

---

## 4. ImageF32x4RGBAWithCache — GPU連携型 🟡 70%

`Image/include/ImageF32x4RGBAWithCache.ixx`

CPU と GPU の両方にバッファを持つデュアルバッファ。

| メソッド | 説明 |
|----------|------|
| `UpdateGpuTextureFromCpuData()` | CPU→GPU アップロード |
| `UpdateCpuDataFromGpuTexture()` | GPU→CPU ダウンロード |
| `IsGpuTextureValid()` | GPU バッファの有効性チェック |
| `DeepCopy()` | 完全複製 |
| `image()` | 内部の `ImageF32x4_RGBA&` を取得 |

GPU テクスチャ自動管理はまだ完全ではなく、明示的な `Update` 呼び出しが必要。理想的には dirty flag で遅延同期すべき。

---

## 5. SurfacePixelConversion — ピクセル変換 🟢 80%

`Image/include/SurfacePixelConversion.ixx`

| ターゲット | 状態 |
|-----------|------|
| Rgba8SrgbStraight | ✅ |
| Rgba16LinearStraight | ✅ |
| Rgba32LinearStraight | ✅ |

`decodeToLinear()` が `SurfaceColorDescriptor` を読み取り、伝達関数に応じて自動的にリニア化。
`finiteOrZero()` で NaN/Inf を 0 に安全化。

---

## 6. GpuImageUploadBuffer — GPUアップロード 🟢 80%

`Image/include/GpuImageUpload.ixx`

| フォーマット | 状態 |
|-------------|------|
| Rgba16Float | ✅ |
| Rgba32Float | ✅ |

`SurfaceColorDescriptor` を付帯してアップロードするので、GPU 側でも色情報を保持できる。

---

## 7. OpenEXR — 🔴 空スタブ

`Image/include/OpenEXR.ixx` — 24行。コンストラクタ/デストラクタのみ。実装ゼロ。

---

## 8. ImageInterface — 🟡 最小限

`Image/include/ImageInterface.ixx` — `width()` / `height()` の純粋仮想のみ。名前空間未指定。

---

## 9. ColorTransform エフェクト 🟢 85%

| エフェクト | 状態 | 品質 |
|-----------|------|------|
| **LevelsCurves** | ✅ | ChannelLevelsSettings（inputBlack/White/Gamma, outputBlack/White）。RGB + 個別チャンネル対応 |
| **ColorBalance** | ✅ | Shadow/Midtone/Highlight 別 + preserveLuma。**プリセット**: neutral / coolShadows / warmHighlights / cinematic |
| **HueSaturation** | ✅ | マスター + 6チャンネル（Red/Yellow/Green/Cyan/Blue/Magenta）個別調整。AE 同等 |
| **FilmCurve** | ✅ | Toe（シャドウ）/ Shoulder（ハイライト）/ 中間調コントラスト。フィルム特性曲線模倣 |
| **ChannelMixer** | ✅ | RGB チャンネルミキサー（モノクロ変換や色味調整用） |
| **Colorama** | ✅ | 疑似カラー化 |
| **Fill** | ✅ | 単色塗りつぶし |
| **GradientRamp** | ✅ | グラデーション生成 |
| **HueVsCurves** | ✅ | 色相 vs カーブ |
| **PhotoFilter** | ✅ | 写真フィルター |
| **SelectiveColor** | ✅ | 特定色域の調整 |
| **TonalSplit** | ✅ | トーン分割 |
| **TriChromaticShift** | ✅ | 3色シフト |
| **Tritone** | ✅ | 3トーンカラー化 |

---

## 10. その他 ImageProcessing 🟢 80%

| エフェクト | 状態 |
|-----------|------|
| BroadcastColors | ✅ NTSC/PAL ブロードキャストセーフ |
| ChromaKey | ✅ クロマキー（別監査済み） |
| LumaKey | ✅ ルマキー |
| SimpleChoker | ✅ マットの縮小/拡張 |
| Emboss | ✅ エンボス |
| Median | ✅ メディアンフィルタ |
| Threshold | ✅ 2値化 |
| Echo / EdgeEcho | ✅ 残像 / エッジ残像 |
| Halftone | ✅ 網点 |
| Duotone | ✅ 2トーン |
| TiltShift | ✅ ミニチュア風ぼかし |
| MotionTrail | ✅ モーショントレイル |
| VectorFlowGlitch | ✅ ベクトル流れグリッチ |
| Scatter | ✅ 散乱 |

---

## 11. DirectCompute（GPU 版エフェクト）🟡 60%

`ImageProcessing/DirectCompute/` 以下に GPU コンピュートシェーダー版が存在:

| エフェクト | 状態 |
|-----------|------|
| AutoContrast / AutoGamma | ✅ |
| Blend2D | ✅ |
| BroadcastColors | ✅ |
| ChromaKey | ✅ |
| Emboss | ✅ |
| GaussianBlur | ✅ |
| Glow | ✅ |
| LeaveColor | ✅ |
| LumaKey | ✅ |
| Median | ✅ |
| Monochrome | ✅ |
| Negate | ✅ |
| Scatter | ✅ |
| Sepia | ✅ |
| SimpleChoker | ✅ |
| Threshold | ✅ |
| ThreeWayColorCorrection | ✅ |

CPU 版と同じ名前が並立しているが、実装が重複しているのか、CPU/GPU 切り替え可能なのか未確認。

---

## 12. OpenCV 連携層 🟡 65%

`ImageProcessing/OpenCV/` 以下:

| コンポーネント | 状態 |
|---------------|------|
| CvUtils | ✅ 基本ユーティリティ |
| PuppetEngine | ✅ メッシュ変形 |
| RotoBrushEngine | ✅ ロトブラシ（詳細未確認） |
| FaceDetection / FaceTracker | ✅ 顔検出・追跡 |
| G-API | ✅ OpenCV Graph API 使用（Affine, Blur, FilmGrain, Merge） |

---

## 13. Halide 層 🟡 40%

`ImageProcessing/Halide/`:

| コンポーネント | 状態 |
|---------------|------|
| HalideEffectManager | ✅ マネージャー |
| HalideImageF32x4RGBA | ✅ Halide 用バッファ |
| Lift | ✅ |
| MonochromeHalide | ✅ |
| HalideTest | ✅ テスト |

Halide での高速 CPU 画像処理に対応しているが、実際のパイプラインに統合されているか未確認。

---

## 14. コード重複問題 🟡

CPU 版（ColorTransform + ImageProcessing）+ GPU 版（DirectCompute）+ Halide 版（Halide）の 3 系統が並立。
同じエフェクトが 3 つの実装を持つ可能性がある。統合・抽象化が必要。

---

## 15. EXR / Deep / Cryptomatte 🟡

| コンポーネント | 状態 |
|---------------|------|
| OpenEXR | OIIO flat/deep RGBA read/write facade を実装 |
| Deep Image | `DeepImageBuffer`、merge、holdout、flatten、ranked output を実装 |
| Cryptomatte | ranked coverage、manifest、EXR writer を実装 |

---

## 全体評価

| レイヤー | スコア | 所見 |
|----------|--------|------|
| ImageF32x4_RGBA | 🟢 90% | 主力画像型として完成。QImage/OpenCV/OIIO 統合。BGRA↔RGBA 管理が丁寧 |
| MultiChannelImage | 🟢 85% | 16ch AOV。クリーンな設計 |
| SurfaceColorDescriptor | 🟢 95% | 色契約の標準化。全画像が自分が何者かを宣言できる。static_assert 検証付き |
| GPU 連携 | 🟡 70% | ImageF32x4RGBAWithCache はあるが同期は手動 |
| 色変換エフェクト | 🟢 85% | Levels/Curves/ColorBalance/HueSat/FilmCurve。AE 同等以上 |
| 画像処理エフェクト | 🟢 80% | CPU版 + GPU版 + Halide版の3系統。機能は豊富 |
| EXR | 🟡 75% | flat named-channel と Deep RGBA IO を実装。multi-part は未確認 |
| Deep / Cryptomatte | 🟡 70% | DeepImageBuffer / DeepData / ranked Cryptomatte を実装。仕様互換は未検証 |
| コード重複 | 🟡 60% | 3系統の重複実装。抽象化が必要 |

**総合**: 🟡 75% — 主力画像型と色変換は完成度が高い。EXR/Deep/Cryptomatte の不在とコード重複が課題。

---

## 16. 追跡確認（2026-08-03）

### GPU連携評価の補正

`ImageF32x4RGBAWithCache` の宣言上は CPU/GPU デュアルバッファ、dirty flag、同期 API が存在するが、実装を確認すると以下は未実装である。

- `CreateGpuTextureInternal()`
- `UpdateGpuTextureFromCpuData()`
- `UpdateCpuDataFromGpuTexture()`
- `ResetDirtyBox()` / `UnionDirtyBox()`

さらに `GetGpuTextureUAV()` は CPU dirty 状態で `UpdateCpuDataFromGpuTexture()` を呼んでおり、CPU→GPU 同期の方向と一致していない。

したがって GPU連携は「契約・型の骨格: 🟡」であり、「実動作: 🔴 未検証／未実装」と分けて扱う。監査表の GPU連携 70% は、実動作評価としては過大である。

### 実装順の確定

1. `ArtifactCore` 側の GPU device/context 所有境界、upload format、readback policy を確認する。
2. CPU→GPU の明示同期と dirty region 契約を実装する。
3. flat named-channel EXR writer を実装する。
4. その後に Cryptomatte / Deep EXR を追加する。
5. CPU / DirectCompute / Halide の重複は、出力契約と精度差を比較してから統合方針を決める。

### 未検証のまま残る項目

- GPU texture の生成・upload・readback の runtime 動作
- OpenEXR の実ファイル入出力
- Cryptomatte の仕様準拠と Nuke 相互運用
- Deep EXR の仕様準拠と Nuke 相互運用
- CPU / DirectCompute / Halide の結果一致

### EXR出力経路の追加確認

`OpenExr` facade は OIIO の flat/deep RGBA read/write を提供する。EXR出力機能全体では、`IO.ImageExporter` の `writeMultiChannel()` が OIIO `ImageOutput` を使用し、`MultiChannelImage` のチャンネルを named channel として書き出している。

確認できた既存経路:

- `R`, `G`, `B`, `A`
- `Depth`, `Normal.X/Y/Z`, `Velocity.X/Y`
- `ObjectId`, `MaterialId`
- `Albedo.R/G/B`, `Emission`, `Custom`
- compression / color space / string metadata
- flat な Cryptomatte draft channel（`CryptoObject00.*`, `CryptoMaterial00.*`）

したがって EXR の現状評価は次のように分けるべきである。

| 範囲 | 状態 |
|------|------|
| `OpenExr` facade | 🟡 flat/deep RGBA read/write 実装あり |
| flat named-channel EXR writer | 🟢 実装あり（OIIO経由） |
| multi-part EXR | 🟡 未確認 |
| Cryptomatte 1.3準拠 | 🔴 未達／draft channelのみ |
| Deep EXR | 🟡 DeepData read/write 実装あり。Nuke 相互運用は未検証 |

監査上の「OpenEXR = 空スタブ」は facade の記述として維持し、flat AOV出力の欠落を意味しないよう補足する。

### 検証入口

現在確認できる呼び出し側は `ArtifactCompositionEditor` と `ArtifactRenderQueueService` の `ImageExporter::writeMultiChannel()` 呼び出しである。`ImageExporter::testWrite()` / `testWrite2()` は存在するが、named-channel、metadata、読み戻しまでを検証する専用受入れテストは検索上確認できなかった。

次回の検証は、最小の `MultiChannelImage` を一度だけ生成し、次を確認する範囲に限定できる。

1. EXR生成の成功とファイル存在
2. OIIOまたは既存importerでの named channel 一覧
3. float値と `compression` / `colorspace` / Cryptomatte metadata の保持
4. 異常サイズ・欠損channel時のエラー経路

### Cryptomatte draft の互換性境界

追加確認の結果、`ArtifactRenderQueueService` は `cryptomatte/<id>/name`、`hash`、`manifest` metadata を出力側へ付加している。一方、`ImageExporter` の draft channel は `CryptoObject00.*` / `CryptoMaterial00.*` という名前で、4本のうち実際に値を生成するのは ID と coverage に対応する先頭2本だけである。

また、render queue 側の診断文にも「single-hit coverage」「full ranked coverage layers 未提供」「MurmurHash-based standard hashing 未提供」と明記されている。よって現状は「Cryptomatte metadata付きの単一ヒットdraft」であり、標準Cryptomatteの複数rank・ハッシュ・アンチエイリアスcoverageを満たす実装とは判定しない。
