# Milestone M-WEBEXPORT-1: SVG + CSS Keyframes + HTML Player Export

| 項目 | 値 |
|---|---|
| Status | proposed |
| Owner | (TBD) |
| Target phase | Phase 1 (短期, 1〜2 sprint) |
| Subsystem | `ArtifactCore/IO/VectorExport` + `Artifact/Render/Export` |
| Created | 2026-06-16 |
| Related | `REPORT_JS_ANIMATION_EXPORT_2026-06-16.md` / `M-EXPORT-1` / `M-PSD-1` |
| Supersedes | (なし) |
| Touches submodule | `Artifact` / `ArtifactCore` のみ (`ArtifactWidgets` 不可) |

---

## 1. 背景

### 1.1 痛み

- コンポジションを **Web / SNS / メール** で再利用するための **軽量 export** が無い
- 既存 export は画像シーケンス / 動画のみ。SVG / HTML / Lottie など Web 標準形式の export 機能はゼロ
- Lottie 形式 (Adobe AE / Figma 互換) の export も未着手
- 軽量な preview を **ブラウザで直接開く** ことができない

### 1.2 既存資産

| ファイル | 状態 |
|---|---|
| `ArtifactCore/include/IO/VectorExport.ixx` | ✅ API 定義済み |
| `ArtifactCore/src/IO/VectorExport.cppm` | ⚠️ 233 行実装。ただし `CssAnimationExporter::extractAnimationData` は **スタブ** (`addProperty("transform")` のみ) |
| `ArtifactCore/src/Shape/ShapeLayer.cppm:687` | ✅ `ShapeLayer::toSvg()` / `fromSvg()` 完全実装 |
| `ArtifactCore/src/Shape/ShapeGroup.cppm` | ⚠️ `toSvg()` 実装あり、ただし **shape operator (TrimPaths / Repeater) 非対応** |
| `ArtifactCore/src/Shape/ShapePath.cppm` | ✅ `toPath() / fromPainterPath / toJson` 実装 |
| `AnimatableValueT / AnimatableValueF` (15 hit) | ✅ animatable property 値を取り出せる |
| `ArtifactWebUIHost` (`Artifact/src/Widgets/WebUI/`) | ✅ Web UI 基盤あり (WebSocket live preview で活用可能) |

### 1.3 不足

- `CssAnimationExporter::extractAnimationData` の **keyframes 抽出ロジック** (中身のスタブ解消)
- Shape operator (TrimPaths / Repeater) の **SVG export 適用**
- Mask / Effect / Blend Mode / Track Matte の **SVG export 拡張**
- `.html` + `.css` + 1 フレーム SVG を **self-contained** で書き出す機能
- Export ダイアログの **Web Animation (HTML) プリセット**

## Update 2026-08-15

- `SvgFrameExporter` は ShapeLayer／composition frame の SVG 化と SVG sequence 書き出しを実装済み。
- `HtmlPlayerWriter` と Render Queue の `html` 経路は、SVG／CSS を埋め込む self-contained player と、連番フレームを切り替える HTML player を生成する。Web Animation HTML プリセットと出力設定 UI も存在する。
- `CssAnimationExporter::extractAnimationData()` は現時点でも transform 値を各フレームへ複製する実装で、実際の animatable keyframe／opacity／fill／stroke 等の抽出には未到達。Shape operator、mask／effect／blend／track matte の SVG 化、単一 HTML への画像埋め込み、ブラウザ見た目一致は未検証または未実装。
- 判定: **SVG frame／sequence と HTML player の基盤は実装済み。完全な CSS keyframe export と SVG parity は未完了。ビルド・ブラウザ runtime 確認は未実施。**

---

## 2. ゴール

- 60s コンポジション (shape レイヤー中心 + text + 簡単なエフェクト) を **`.html` ファイル 1 個** に export 可能
- ブラウザで開くだけで再生 (CDN 不要、no JS bundle)
- 既存の GPU render と **見た目が同等** (許容誤差 < 1 px)
- `SvgFrameExporter::writeSvgSequence` の API を変更せず、後方互換を維持

---

## 3. 設計の柱

### 3.1 データモデル

`ArtifactCore::CssKeyframeProperty` を拡張:

```cpp
struct CssKeyframeProperty {
    QString propertyName;            // "transform" / "opacity" / "fill" / "stroke-dashoffset" / ...
    std::vector<std::pair<double, QString>> keyframes; // (0.0〜100.0, value)
};
```

`Extract` 時に:

- `AnimatableValueT` を `start_frame 〜 end_frame` でサンプル
- `(frame / totalFrames) * 100.0` で percentage に変換
- `formatTransformValue()` と同様の helper で string 化

### 3.2 Shape operator export

`ShapeGroup::toSvg()` の拡張ポイント:

1. 子要素を順に emit する前に **operator を適用** したパスを生成
2. `TrimPaths` → SVG の `stroke-dasharray` + `stroke-dashoffset` で再現
3. `Repeater` → SVG の `<use>` で `xlink:href` 参照
4. `OffsetPaths` → path を `transform="translate(...)"` で複製
5. `MergePaths` → 複数 path を 1 つの `<path>` に統合

### 3.3 Mask / Effect / Blend

- **Mask** → SVG `<clipPath>` に変換。`feComposite` で feather 近似
- **Blend Mode** → CSS `mix-blend-mode: multiply / screen / overlay / ...` (CSS Compositing Level 1)
- **Effect** (blur / glow) → SVG `<filter>` の `feGaussianBlur` / `feMorphology` / `feColorMatrix`
- **Track Matte** → SVG `<mask>` に変換

### 3.4 HTML プレイヤ生成

```html
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>{composition name}</title>
<style>
{generateCss() /* composition size + @keyframes + .layer-* rules */}
</style>
</head>
<body>
<div class="composition">
{generateSvgLayers() /* inline SVG elements with class="layer-*" */}
</div>
</body>
</html>
```

### 3.5 設定

`Artifact/Config/WebExportConfig.json` (新規) で既定値管理:

```json
{
  "defaultFrameRate": 30,
  "defaultWidth": 1920,
  "defaultHeight": 1080,
  "looping": true,
  "includeHtmlViewer": true,
  "imageEmbedding": "base64",  // "base64" / "external"
  "svgMode": "inline",         // "inline" / "external_file"
  "maxFileSizeMB": 25
}
```

---

## 4. フェーズ計画

### 4.1 Phase 1: Foundation 完成 (C-EX-1) — 3〜5 day

| タスク | 担当 | 完了条件 |
|---|---|---|
| `CssAnimationExporter::extractAnimationData` 完全実装 | Core | `transform / opacity / fill / stroke / stroke-dashoffset` の keyframes を抽出できる |
| 各 animatable property の **format helper** 追加 | Core | 値 → CSS string の変換が型ごとに揃う |
| `ShapeGroup::toSvg()` に operator 適用 | Core | `TrimPaths` / `Repeater` が export 可能 |
| Unit test (GoogleTest / Qt Test) | Core | 主要 pattern (translate / rotate / scale / opacity) が一致 |

**依存**:
- 既存 `AnimatableValueT / AnimatableValueF` の API
- 既存 `SvgFrameExporter` の API

**触るファイル**:
- `ArtifactCore/include/IO/VectorExport.ixx` (API 拡張)
- `ArtifactCore/src/IO/VectorExport.cppm` (extractAnimationData 本体)
- `ArtifactCore/src/Shape/ShapeGroup.cppm` (operator 適用)
- `ArtifactCore/src/Shape/ShapePath.cppm` (必要なら追加)
- 新規 `ArtifactCore/test/VectorExportTest.cpp`

### 4.2 Phase 2: Mask / Effect / Blend 拡張 (C-EX-2) — 4〜6 day

| タスク | 担当 | 完了条件 |
|---|---|---|
| `MaskPath` → SVG `<clipPath>` 変換 | Core | mask を持つレイヤーが export 可能 |
| Mask feather / expansion → `feGaussianBlur` | Core | feather が SVG で近似表現される |
| Blend Mode → CSS `mix-blend-mode` | Core | 18 種類のうち AE 主要 12 種類を対応 |
| Effect (Blur / Glow / DropShadow) → SVG `<filter>` | Core | 基本 effect が export 可能 |
| Effect extension → 不対応 effect は **fallback** でスキップ | Core | warning ログ + Undo ノードに反映 |

**触るファイル**:
- `ArtifactCore/include/IO/VectorExport.ixx` (新クラス / 新メソッド)
- `ArtifactCore/src/IO/VectorExport.cppm` (MaskExporter / EffectExporter)
- 新規 `ArtifactCore/include/IO/SvgEffectBridge.ixx`
- 新規 `ArtifactCore/src/IO/SvgEffectBridge.cppm`
- `ArtifactCore/src/Mask/MaskPath.cppm` (getMaskPathData() 追加)

### 4.3 Phase 3: HTML Player 生成 (U-EX-1) — 2〜3 day

| タスク | 担当 | 完了条件 |
|---|---|---|
| `HtmlPlayerWriter` 新規 | Core | `.html` + 1 frame SVG を **self-contained** で書き出す |
| Inline SVG mode (default) | Core | SVG が `<style>` 内で参照される |
| External SVG mode | Core | `<img src="frame_0001.svg">` で参照 |
| Image layer → base64 data URI | Core | `<image href="data:image/png;base64,...">` で埋め込み |
| Loop / autoplay 設定 | Core | `animation: ... infinite` / `forwards` の制御 |

**触るファイル**:
- 新規 `ArtifactCore/include/IO/HtmlPlayerWriter.ixx`
- 新規 `ArtifactCore/src/IO/HtmlPlayerWriter.cppm`

### 4.4 Phase 4: Export ダイアログ統合 (U-EX-2) — 2〜3 day

| タスク | 担当 | 完了条件 |
|---|---|---|
| `ArtifactPrExportDialog` 拡張 | App | `Web Animation (HTML)` プリセット追加 |
| または `ArtifactExportDialog` 拡張 | App | 既存 `Artifact` アプリ用 |
| プレビュー (browser で開くボタン) | App | 書き出し後 `QDesktopServices::openUrl` でブラウザ起動 |
| 設定保存 (`WebExportConfig.json`) | App | 既定値を永続化 |

**触るファイル**:
- `ArtifactPr/src/ExportDialog.cppm` (preset 追加)
- `Artifact/src/Dialogs/ExportDialog.cppm` (preset 追加)
- `Artifact/App/Config/WebExportConfig.json` (新規)

### 4.5 Phase 5: 検証 (T-EX-1) — 2 day

| タスク | 担当 | 完了条件 |
|---|---|---|
| 主要ブラウザで再生確認 (Chrome / Firefox / Safari / Edge) | Test | 60s clip が正常再生 |
| Sample composition で frame-by-frame 比較 | Test | SVG 書き出し vs GPU render の差分 < 1 px |
| Playwright で headless 検証 | Test | CI で smoke test |
| File size check | Test | 60s clip が 25 MB 未満 |
| Asset / 設定 / 永続化の Undo 整合 | Test | Undo / Redo で config が巻き戻る |

**触るファイル**:
- 新規 `Artifact/tests/integration/WebExportTest.cpp`
- 新規 `Artifact/tests/integration/BrowserRenderTest.cpp` (Playwright)

---

## 5. 既存 milestone / コードとの接続

### 5.1 既存 / 並走

| milestone / コード | 接続 |
|---|---|
| `M-EXPORT-1 Render Format Expansion` (PNG / ProRes / HAP) | 同 Export ダイアログに統合。プリセット一覧で並列 |
| `MILESTONE_RENDER_QUEUE_2026-03-22.md` | Render Queue に Web Export を追加 |
| `MILESTONE_ADVANCED_COPY_PASTE_2026-04-03.md` | コピー結果を SVG / CSS に paste 可能 (Phase 2 以降) |
| `MILESTONE_NUMERIC_FIELD_QUICK_CALC_2026-06-07.md` | keyframe 数値入力 |
| `ArtifactWebUIHost` | Live Preview 機能として並走可能 (別 milestone) |

### 5.2 触らないもの

- `ArtifactWidgets/` サブモジュール (明示依頼時のみ)
- `libs/DiligentEngine/` サブモジュール
- `third_party/*` サブモジュール
- 既存 `ShapeGroup::toSvg()` の API シグネチャ (互換維持)

---

## 6. リスクと軽減

| リスク | 影響 | 軽減策 |
|---|---|---|
| CSS keyframes の表現力不足 | AE の複雑な compound transform が完全再現できない | Phase 1 は simple shape + transform + opacity に限定。複雑なケースは warning + PNG sequence fallback |
| Lottie 完全互換にならない | Web embed の用途が限定的 | M-WEBEXPORT-1 は **foundation** と位置づけ。Lottie 完全互換は M-LOTTIE-1 で別途 |
| HTML ファイルサイズ | 60s clip が数十 MB になる | `WebExportConfig.json` で `imageEmbedding: external` / `maxFileSizeMB` を制御 |
| フォント埋め込み | テキストが豆腐になる | Phase 1 では **Web Font (Google Fonts CDN)** 参照を既定。Phase 2 で base64 embed 検討 |
| Blend Mode の CSS 対応差 | Safari / Firefox で挙動が異なる | 主要 6 種類 (normal / multiply / screen / overlay / darken / lighten) を対応。他は `mix-blend-mode` 不使用 |
| Effect / Mask の export 限界 | 複雑 effect は再現不可 | Phase 1〜2 は simple mask / blur のみ。複雑なものは PNG sequence fallback |
| Submodule bump コスト | push のたびに手順が必要 | `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠。Artifact 完了 → Core 完了 → gitlink 更新 → parent push |

---

## 7. 不変条件 (AGENTS.md / taste 整合)

### 7.1 禁止事項

- `QtCSS` / `QObject::setStyleSheet(...)` の新規追加禁止 (web export の inline style は別、文脈が違うが参考)
- `QImage` の新規採用禁止。既存 import / IO 境界のみ。export パスでは **`QPainterPath` + 文字列ベース SVG 生成** を維持
- 新規 signal/slot 接続の追加禁止。既存 `ArtifactExportDialog` の signal を再利用

### 7.2 推奨事項

- export 機能は **`ArtifactCore/IO/` に集約** (Core 側のみで完結)
- 既存 API の後方互換を維持。新規 export プリセット追加は dialog 側で完結
- HTML / SVG は UTF-8 で統一。文字化け防止
- 設定 / 永続化は atomic write (M-CRASH-1 と整合)

---

## 8. Done Criteria (Definition of Done)

この milestone が完了とみなされる条件:

- [ ] Phase 1: `CssAnimationExporter::extractAnimationData` が **keyframes 抽出可能**
- [ ] Phase 1: `ShapeGroup::toSvg()` で **TrimPaths / Repeater** が export 可能
- [ ] Phase 2: Mask / Blend Mode / Effect (Blur / Glow / DropShadow) が SVG で export 可能
- [ ] Phase 3: `.html` ファイル 1 個で self-contained な export
- [ ] Phase 4: Export ダイアログに `Web Animation (HTML)` プリセット追加
- [ ] Phase 5: Chrome / Firefox / Safari / Edge で再生確認
- [ ] Phase 5: 60s clip の出力サイズが 25 MB 未満
- [ ] Phase 5: Playwright による headless smoke test が CI で通る
- [ ] Unit test (GoogleTest / Qt Test) の coverage が 80% 以上
- [ ] 既存 export (PNG / ProRes) の API 後方互換
- [ ] 既存 milestone / API との接続ドキュメントを更新
- [ ] `RENDER_FORMAT_CONTRACT_2026-05-16.md` の linear canonical と整合
- [ ] `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の diagnostics 文法と整合

---

## 9. 関連ファイル (新規 / 変更)

### 9.1 新規

- `ArtifactCore/include/IO/HtmlPlayerWriter.ixx`
- `ArtifactCore/src/IO/HtmlPlayerWriter.cppm`
- `ArtifactCore/include/IO/SvgEffectBridge.ixx`
- `ArtifactCore/src/IO/SvgEffectBridge.cppm`
- `ArtifactCore/test/VectorExportTest.cpp`
- `ArtifactCore/test/HtmlPlayerWriterTest.cpp`
- `Artifact/tests/integration/WebExportTest.cpp`
- `Artifact/tests/integration/BrowserRenderTest.cpp`
- `Artifact/App/Config/WebExportConfig.json`

### 9.2 変更

- `ArtifactCore/include/IO/VectorExport.ixx` (API 拡張、互換維持)
- `ArtifactCore/src/IO/VectorExport.cppm` (extractAnimationData 本体、operator 適用)
- `ArtifactCore/src/Shape/ShapeGroup.cppm` (operator export)
- `ArtifactCore/src/Mask/MaskPath.cppm` (getMaskPathData 追加)
- `ArtifactPr/src/ExportDialog.cppm` (preset 追加)
- `Artifact/src/Dialogs/ExportDialog.cppm` (preset 追加)

### 9.3 触らない

- `ArtifactWidgets/` サブモジュール
- `libs/DiligentEngine/` サブモジュール
- `third_party/*` サブモジュール
- 既存 `ShapeLayer::toSvg()` の API

---

## 10. 次の milestone (案)

本 milestone 完了後の自然な拡張:

| ID | テーマ | 概要 |
|---|---|---|
| **M-LIVE-1** | WebSocket Live Preview | `ArtifactWebUIHost` 経由で編集中をブラウザで確認 |
| **M-LOTTIE-1** | Lottie 互換 JSON Export | AE / Figma 互換の Lottie 形式。Web embed / メール埋め込み |
| **M-WEBGPUEXPORT-1** | Three.js / WebGPU Export | 3D / 立体ロゴ / データビジュアライゼーション |

---

## 11. 更新履歴

- 2026-06-16: 初版作成。Foundation 80% 完成の上に短期実装可能な milestone として起こす。

---

## 2026-07-25 現状確認

静的確認では、`VectorExport` に SVG フレーム列書き出し、HTML 生成、フレーム画像列を再生する HTML player、および CSS 生成 API が存在する。Render Queue から `.html` 出力を判定し、PNG フレームを保存して HTML player を生成する導線もある。したがって「Web export がゼロ」という背景記述は現状と一致しない。

一方、`CssAnimationExporter::extractAnimationData()` は実質的に `transform` の空プロパティを追加するだけで、フレーム値・keyframe の抽出は未実装。現行 HTML player は self-contained な SVG/CSS アニメーションではなく、外部 PNG フレーム列を参照する形式で、Shape operator、Mask / Effect / Blend / Track Matte の SVG 変換、Web Animation プリセット、設定ファイル、ブラウザ／サイズ／CI 検証も確認できない。よって本マイルストーンは「ラスター HTML player と SVG/CSS の骨格あり、仕様上のベクター Web export は未完了」と判定する。

確認範囲: `ArtifactCore/include/IO/VectorExport.ixx`、`ArtifactCore/src/IO/VectorExport.cppm`、`ArtifactCore/src/Shape/ShapeLayer.cppm`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`。ビルド・ブラウザ実行による動作確認は未実施。
