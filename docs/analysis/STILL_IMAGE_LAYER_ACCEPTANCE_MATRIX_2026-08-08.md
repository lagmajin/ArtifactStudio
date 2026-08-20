# 静止画レイヤー制作受入マトリクス

**最終更新:** 2026-08-20

**対象マイルストーン:** `M-IMG-1 Still Image Layer Production Readiness`

## 目的

静止画の import から保存、再読込、preview、Render Queue までを同じ観点で確認し、未実装とruntime未確認を混同しないための受入基準を固定する。

## Preview／Render Queue 比較判定

保存フレーム比較は `ArtifactSoftwareRenderTestWidget` の判定基準に合わせ、各チャンネル差分が `2` 以下の画素を許容し、許容値を超える画素が全体の `0.1%` 以下であれば `PASS` とする。Preview と Render Queue に加えて Software Preview のキャプチャも読み込み可能で、3経路が揃った場合は3組のペア比較がすべて合格したときだけ全体を `PASS` とする。平均 RGBA 差分と最大チャンネル合計差分も記録し、サイズ不一致または許容超過時は `FAIL` とする。実素材での runtime 実行結果は未記録である。

## 現行経路

| 段階 | 正規経路 | 現在の静的根拠 | 状態 |
|---|---|---|---|
| Header preflight | OIIO `ImageInput::open()` / `ImageSpec` | `Artifact/src/Layer/ArtifactImageLayer.cppm` `loadFromPath()` | 実装済・runtime未確認 |
| Full decode | `AsyncAssetReadScheduler` → OIIO float decode | 同 `loadImagePairViaAsyncReader()` / `startPrefetch()` | 実装済・runtime未確認 |
| Working-space変換 | `ImageF32x4_RGBA` + `ArtifactOCIOManager` | 同 `applyInputInterpretation()` | 実装済・runtime未確認 |
| CPU frame供給 | `currentFrameBuffer()` | Composition View / Render Controllerから参照 | 実装済・runtime未確認 |
| GPU upload／共有 | `GPUTextureCacheManager` | source version + input interpretation別key | 実装済・runtime未確認 |
| Project保存／復元 | `toJson()` / `fromJsonProperties()` | source、identity、fit、crop、interpretation、PSD subimage | 実装済・runtime未確認 |
| Thumbnail／Qt境界 | `toQImage()` / `getThumbnail()` | 入出力・UI互換境界 | 変換済み buffer を使う静的修正済・runtime未確認 |

### 現行コードで確認した連番再生接続

旧レポートに残っていた「`ImageSequenceSource` → `ArtifactImageLayer::draw()` の接続未完了」という記述は、現行コードでは更新が必要である。`draw()` と `toQImage()` は `currentFrame()` から `resolveSequenceFrame()` を通して `refreshSequenceFrame()` を呼び、`ImageSequenceSource::tryFrameAt()` が対象フレームの cache lookup / prefetch を行う。したがって、連番再生の時間駆動接続は静的には実装済みで、残課題は runtime での frame advance、欠損、範囲外、保存・再読込の受入確認である。

## 素材マトリクス

| ID | 素材 | 必須観点 | 期待結果 | 現状 |
|---|---|---|---|---|
| IMG-01 | 8-bit sRGB RGB PNG/JPEG | dimensions、fit、save/reload | サイズと表示位置が往復後も一致 | decode／保存復元経路実装済・未実行 |
| IMG-02 | alpha付きRGBA PNG/TIFF | straight/premult境界、opacity、blend | fringeやalpha反転がなくpreview/render一致 | associated-alpha対策済・未実行 |
| IMG-03 | 16-bit integer TIFF/PNG | 階調、working-space変換 | 8-bitへの早期量子化がない | 未実行 |
| IMG-04 | half/float EXR | HDR値、負値、1超過値 | GPU uploadまでfloat値を保持 | 未実行 |
| IMG-05 | 1ch grayscale | RGBA展開、alpha既定値 | RGBへ同値展開、alpha 1 | channel mapping実装済・未実行 |
| IMG-06 | 2ch gray+alpha | channel mapping | grayをRGBへ展開し第2chをalphaに使用 | channel mapping実装済・未実行 |
| IMG-07 | EXIF/OIIO orientation付き | orientation、bounds、crop | 向き補正後のpixelとboundsが一致 | metadata保存対策済・未実行 |
| IMG-08 | pixel aspect付き | metadata保持、表示比率 | 非fit時の表示幅・bounds・hit-testがpixel aspectと一致 | 実装済・未実行 |
| IMG-09 | ICC／色空間metadata付き | 自動解釈、明示override | 保存／再読込で同じworking-space結果 | source metadata接続済・未実行 |
| IMG-10 | CMYK等の非標準channel | decode診断、色変換 | standard CMYKをRGB化し、未知channel構成は段階を診断 | standard CMYK対策済・未実行 |
| IMG-11 | PSD複数subimage | index保存、境界値 | 指定subimageが往復後も一致 | index保存／decode経路実装済・未実行 |
| IMG-12 | 16K境界／巨大画像 | UI応答、memory、failure | UI threadを同期decodeで停止させず、64 Mi pixelsを超える素材を確保前に拒否する | 総画素数上限対策済・未実行 |
| IMG-13 | corrupt／unsupported | failure stage、placeholder | stale pixelを残さず理由を診断 | placeholder failure経路実装済・未実行 |
| IMG-14 | missing／移動済source | missing、relink、保存 | unresolved pathを保持しrelink可能 | unresolved path保持実装済・未実行 |

## 操作マトリクス

| ID | 操作 | 組み合わせ | 合格条件 | 現状 |
|---|---|---|---|---|
| OP-01 | Transform | position/scale/rotation/anchor | 編集中と保存／再読込後のboundsが一致 | 未実行 |
| OP-02 | Fit | on/off、composition resize | 意図しないstretchや旧surface残留がない | 未実行 |
| OP-03 | Source Reframe | crop/pan/zoom/rotation/anchor | GPU/CPU、preview/renderでpixel領域が一致 | 未実行 |
| OP-04 | Mask／Matte | mask、track matte、opacity | alpha境界と合成順が一致 | 未実行 |
| OP-05 | Blend／Effect |代表blend + raster effect | cache hit有無で結果が変わらない | 未実行 |
| OP-06 | Input Interpretation | 同一sourceへ異なる解釈 | decoded/GPU cacheを誤共有しない | 静的対策済・未実行 |
| OP-07 | Shared／Localized | 複数layer、localize、relink | shared時のみtexture共有しlocalize後は分離 | 未実行 |
| OP-08 | Source overwrite | decode中／表示後に上書き | version不一致のprefetchとtextureを採用しない | 静的対策済・未実行 |
| OP-09 | Save／Reload | sourceあり／なし／missing | dirty副作用や旧sourceの再decodeがない | 静的対策済・未実行 |
| OP-10 | Device再初期化 | cache保持中にdevice reset | stale handleやblank固定がない | 未実行 |

## 失敗診断の期待値

| 段階 | 代表理由 | 必須の観測情報 |
|---|---|---|
| source | missing、registry登録失敗、relink失敗 | source path、asset identity、source version |
| header | unsupported format／dimensions | OIIO error、width、height |
| decode | corrupt payload、channel展開失敗 | subimage、decode stage、placeholder移行 |
| color | 未導入色空間、transform失敗 | 保存された色空間名、transfer function、bypass理由 |
| upload | device／texture生成失敗 | GPU owner、cache key、fallback経路 |
| cache | version／interpretation不一致 | requested/current version、採用拒否理由 |
| composite | mask／matte／effect経路不一致 | layer ID、対象stage、preview/render経路 |

## 現時点の実装ギャップ

1. ICC profile本体を使う変換は未整備。standard CMYKは簡易RGB変換済みで、profile依存の精密変換と実素材runtime確認が必要。
2. project相対source pathの唯一の解決契約が未整備。Image Layer単独ではproject rootを取得できないため、AssetManager／project保存境界で設計する必要がある。
3. `toQImage()`／thumbnailは working-space変換済み`cacheBuffer_`を明示的に QImage 化するよう修正した。`currentFrameBuffer()`との色結果一致はruntime確認が必要。
4. CMYK等の4ch非RGBA素材は誤alphaを防ぐ診断を追加済みだが、source color modelに応じたRGB変換契約が必要。
5. 連番の時間駆動接続は静的実装済みだが、frame advance / 欠損保持 / 範囲外 clamp の runtime確認が未完了。
6. Render Queueを含む受入実行は、ビルド・テスト許可後に行う。
7. `ArtifactSoftwareRenderTestWidget` に保存済み Preview／Render Queue フレームの比較導線を追加した。`P`／`Q` で画像を読み込み、`D` で同一サイズを確認したうえで差分画素数・平均 RGBA 差・最大差を表示する。実素材での判定と Software Preview を含む三経路比較は未実行。

### 色経路の実装ハンドオフ

`ArtifactImageLayer::toQImage()` で raw `cache_` を早期返却せず、入力解釈後の `cacheBuffer_` を QImage 境界で明示変換してから crop を適用する修正を実装した。これにより GPU preview の `currentFrameBuffer()` と thumbnail / software export の色源を揃える。実装時は、(1) sequence frame 更新後の buffer、(2) associated alpha の unpremultiply / premultiply、(3) crop の pixel bounds、(4) no-color-transform 時の既存結果を個別に確認する。runtime検証は未実行。

## 完了判定

- 各行を `Pass`、`Fail`、`Not Applicable` のいずれかへ更新し、`未実行`を残さない。
- `Fail`にはsource、decode、color、upload、cache、compositeの失敗段階と修正先を記録する。
- PreviewとRender Queueの比較は同一project、同一frame、同一output transformで行う。
