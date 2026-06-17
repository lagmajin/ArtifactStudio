# JS アニメーション出力 実現可能性レポート — 2026-06-16

作成日: 2026-06-16
目的: ArtifactStudio のソースコードを基にして、コンポジション / シェイプレイヤーを **Web 上の JS アニメーション形式** にエクスポートできるかを評価する。
調査範囲: `Artifact/`, `ArtifactCore/`, `ArtifactPr/` の `.cppm` / `.ixx`。

---

## 1. TL;DR (要約)

- **既存の SVG / CSS animation export は部分実装あり** (`ArtifactCore/src/IO/VectorExport.cppm`, 233 行)
- **`ShapeLayer` ↔ `SVG` 双方向実装あり** (`ShapeLayer::toSvg / fromSvg`)
- **ただし「フレームごとに SVG を書き出す SVG シーケンス」が主機能**。**JS アニメーション (Lottie / Three.js / GSAP / Web Animations API 等) の実装はゼロ**
- **最も現実的な経路は SVG → Lottie の代替として SVG + CSS keyframes を使う方式**。これはすでに foundation あり。Phase 1 として着手可能
- **完全 Lottie 互換にはマッピング層が必要**: AE のすべてのプロパティ (Blend Mode / Track Matte / Mask / Effect / Text Animator) を Lottie shape / opacity / time スキーマに変換する必要がある。Phase 3 以降

---

## 2. 既存資産 (実装済み / OK)

| 機能 | 件数 | 場所 |
|---|---:|---|
| `.svg` ファイル export | 577 | `ArtifactWebUIHost` + render presets |
| `Composition snapshot` | 301 | composition 全体ダンプ |
| `TimeCode` | 200 | time 表現 |
| `Time remap` | 174 | time warping |
| `Expression` | 100 | AE expression engine |
| `Project save (JSON)` | 85 | `QJsonDocument::toJson` |
| `Text animator` | 68 | AE TextAnimator 相当 |
| `SVG layer` | 42 | `ArtifactLayerFactory` |
| `GLTF / glb` | 25 | 3D 用 |
| `Animation value` | 15 | animatable props |
| `CSS animation export` | 2 | **`VectorExport.cppm` の `CssAnimationExporter`** |
| `Path shape` | 2 | `ShapeGroup.cppm / ShapePath.cppm` |
| `FramePosition` | 2 | `FramePosition.cppm` |
| `Shape layer` | 3 | shape レイヤー |
| `Primitive shape (rect/ellipse)` | 3 | `ShapeLayer.cppm` |
| `Mask path` | 1 | `MaskPath.cppm` |
| `FrameRange` | 1 | `FrameRange.cppm` |
| `RationalTime` | 1 | `RationalTime.cppm` |
| `Text layer` | 1 | `ArtifactTextLayer.cppm` |
| `Solid image layer` | 1 | `ArtifactSolidImageLayer.cppm` |
| `WebGL / WebGPU export` | 2 | Diligent backend (内部) |

→ **SVG フレーム export と CSS animation export の infrastructure は整っている**。

### 2.1 重要ファイル

| ファイル | 役割 | 評価 |
|---|---|---|
| `ArtifactCore/include/IO/VectorExport.ixx` | API 定義 | OK |
| `ArtifactCore/src/IO/VectorExport.cppm` | **233 行実装**。`SvgFrameExporter` + `CssAnimationExporter` | **部分実装** |
| `ArtifactCore/src/Shape/ShapeLayer.cppm:687` | `ShapeLayer::toSvg()` / `fromSvg()` | **完全実装** (双方向) |
| `ArtifactCore/src/Shape/ShapeGroup.cppm` | コンテナ | ベクター変換 OK |
| `ArtifactCore/src/Shape/ShapePath.cppm` | パス (rect/ellipse/poly/star) | ベクター変換 OK |

### 2.2 何が既に動くか

```cpp
// VectorExport の使い方
QString svg = SvgFrameExporter::exportCompositionFrame(
    layers, QSize(1920, 1080), frameNumber);
bool ok = SvgFrameExporter::writeSvgSequence(frames, outputDir, "anim");

// CSS keyframes 生成
auto css = CssAnimationExporter::extractAnimationData(layer, "intro", 2.0, 0, 60, 30);
QString cssStr = CssAnimationExporter::generateCss(css, QSize(1920, 1080));
```

→ **SVG フレーム単位書き出し** と **CSS keyframes ベースアニメーション** は動作可能。

---

## 3. 完全未実装 (MISS)

### 3.1 アニメーション出力 (主要項目)

| 機能 | 件数 | 評価 |
|---|---:|---|
| JS export (generic) | 0 | **MISS** |
| Lottie export | 0 | **MISS** |
| SVG animation (SMIL) | 0 | **MISS** |
| Canvas2D export | 0 | **MISS** |
| Three.js export | 0 | **MISS** |
| PixiJS export | 0 | **MISS** |
| GSAP export | 0 | **MISS** |
| Anime.js export | 0 | **MISS** |
| Mo.js export | 0 | **MISS** |
| JSON animation | 0 | **MISS** |
| JSON layer format (Lottie 互換) | 0 | **MISS** |
| Mask keyframe export | 0 | **MISS** |
| Effect keyframe export | 0 | **MISS** |
| Effect snapshot | 0 | **MISS** |
| JavaScript engine (V4 / Hermes / etc) | 0 | **MISS** |
| WebSocket (live preview) | 0 | **MISS** |
| CDN (unpkg / jsdelivr) | 0 | **MISS** |
| `.html` export | 0 | **MISS** (WebUIHost 経由以外) |
| `.css` export | 1 | **部分** (VectorExport のみ) |
| `.js` file export | 0 | **MISS** |

### 3.2 重要な未対応ギャップ

| 項目 | 影響 |
|---|---|
| **Shape operator (TrimPaths / Repeater)** | `VectorExport` で適用外。`ShapeGroup::toSvg()` は child を直接展開するだけで operator は無視 |
| **CssAnimationExporter の keyframes 抽出が stub** | `addProperty("transform")` のみ。keyframes 配列は空のまま。実動作しない |
| **Text / Mask / Effect** | export 不可 |
| **Blend Mode** | Lottie 互換 schema 変換なし |
| **Track Matte** | Lottie 互換 mask mode 変換なし |
| **Audio** | export 範囲外 (JS アニメーションは video / audio 出力に非対応) |
| **Image layer** | SVG として画像埋め込み不可 |

---

## 4. 実現可能性評価

### 4.1 Phase 1 — 軽量・短期 (1〜2 sprint)

| 方式 | 既存 foundation | 工数 | 用途 |
|---|---|---:|---|
| **SVG + CSS keyframes** | ✅ `VectorExport` あり (keyframes 拡張のみ) | 3〜5 day | Logo / icon アニメ / Web banner / Simple motion graphics |
| **SVG sequence** | ✅ 完全実装 | 1 day | アニメーション GIF 化 / frame-by-frame 表示 |
| **SVG + SMIL** | ✅ SVG 生成可能、SMIL は `<animate>` 挿入のみ | 2〜3 day | CSS 非対応環境向け (メール埋め込み) |

→ **CSS keyframes 拡張 + 簡易 HTML プレイヤ生成** で **最初の実用 export 機能** が完成する。

### 4.2 Phase 2 — 中期 (1 sprint)

| 方式 | 工数 | 用途 |
|---|---:|---|
| **Lottie 互換 JSON schema (部分)** | 1〜2 month | Lottie player / lottie-web で再生。Adobe AE / Figma 互換 |
| **Web Animations API (JSON)** | 2〜3 week | Chrome / Edge 等の主要ブラウザで再生 |
| **GSAP (JS bundle)** | 3〜4 week | 高機能なサイト内アニメーション |

### 4.3 Phase 3 — 長期 (数 sprint)

| 方式 | 工数 | 用途 |
|---|---:|---|
| **Lottie 完全互換** (mask / effect / blend / matte) | 3〜6 month | After Effects / Figma 代替出力 |
| **Three.js / WebGL export** | 2〜4 month | 3D モーション / 立体ロゴ / データビジュアライゼーション |
| **PixiJS / WebGPU export** | 2〜3 month | 高性能 2D ゲーム UI |

---

## 5. 推奨される新規 milestone

### 5.1 M-WEBEXPORT-1: SVG + CSS Animation Export (短期)

**目的**: 既存の `VectorExport` を完成させ、SVG + CSS keyframes の実用 export を実現する。

**Phase 1: Core 拡張**
- `CssAnimationExporter::extractAnimationData` を完全実装
  - 現状 stub の keyframes 抽出ロジックを完成
  - transform / opacity / fill / stroke / path データを収集
  - `AnimatableValueT` / `AnimatableValueF` (15 hit) から 1 フレームごとの値を取り出す
- `ShapeGroup` / `ShapePath` の `toSvg()` で **shape operator** (TrimPaths / Repeater) を適用するロジックを追加
- Lottie 互換の `time` を 0% 〜 100% にマッピング (durationSec ベース)

**Phase 2: Mask / Effect / Blend 拡張**
- `MaskPath` を SVG `<clipPath>` として出力
- Effect (blur / glow) は SVG `<filter>` に変換 (基本のみ)
- Blend Mode → SVG `mix-blend-mode` に変換

**Phase 3: HTML プレイヤ生成**
- `.html` + `.css` + 1 フレーム SVG を 1 つの self-contained ファイルとして export
- `<svg>` + `<style>` で `keyframes` を埋め込み
- `<img>` で SVG を background 化、または SVG inline

**Phase 4: UI 統合**
- Export ダイアログに `Web Animation (HTML)` プリセット追加
- 既存の `ArtifactPrExportDialog` に統合

**Phase 5: テスト**
- 主要ブラウザ (Chrome / Firefox / Safari) で再生確認
- Sample composition で 10s clip が正常に動くか

**Done Criteria**:
- 60s shape レイヤー中心のコンポジションを `intro.html` として export 可能
- ブラウザで開くだけで再生 (no JS bundle)
- 同一の見た目が GPU render と一致 (許容誤差 < 1 px)

---

### 5.2 M-LOTTIE-1: Lottie 互換 JSON Export (中期)

**目的**: After Effects / Figma 互換の Lottie 形式 JSON を出力可能にする。

**Phase 1: Lottie Schema 研究**
- lottie-web の `AnimationItem` schema を完全把握
- AE のプロパティマッピング表を作成:
  - Layer (Transform / Position / Anchor / Rotation / Opacity) → Lottie `ks` オブジェクト
  - Shape (Rect / Ellipse / Path / Group) → Lottie shape layer
  - Trim / Repeater → Lottie `tm` / `rp`
  - Track Matte → Lottie `tt` (matte mode)
  - Blend Mode → Lottie `bm` enum

**Phase 2: AE → Lottie mapper**
- `ArtifactComposition` を walk して Lottie JSON を構築
- フレームごとの状態は `op` / `ks` の `k` 配列で表現 (keyframe が複数あれば Lottie 形式で展開)
- `QJsonObject` / `QJsonArray` で構築

**Phase 3: 不対応プロパティの fallback**
- Effect (Blur / Glow) → Lottie effect スキーマに変換 (lottie-web サポート範囲内のみ)
- Text Animator → Lottie text animator 互換マッピング
- マスク → Lottie `masksProperties`

**Phase 4: Export 機能統合**
- `.json` ファイルとして出力
- 簡易 `.html` プレイヤも同時生成 (lottie-web CDN bundle 込み)
- Export ダイアログに `Lottie (Web Animation)` プリセット追加

**Phase 5: 検証**
- lottie-web で再生確認
- AE 公式プレイヤで再生確認
- Figma で import して同表示になるか確認

**Done Criteria**:
- 60s shape レイヤー中心のコンポジションを `intro.json` として export 可能
- Lottie 公式プレイヤ + Figma で開いて正常再生
- 既存 export (`PNG sequence / ProRes`) と同等の quality を提供

---

### 5.3 M-WEBGPUEXPORT-1: Three.js / WebGPU Export (長期)

**目的**: 3D / 立体ロゴ / データビジュアライゼーション用の export を実現。

**Phase 1: データ構造**
- AE 3D layer → Three.js scene graph
- 3D camera → Three.js camera
- Light → Three.js light
- Composition → scene

**Phase 2: Three.js template**
- HTML + JS + Three.js CDN bundle 込み self-contained file
- camera 移動 / rotation 等のアニメーションを再生

**Phase 3: WebGPU 移行**
- Three.js WebGPURenderer サポート
- Modern GPU でハードウェアアクセラレーション

**Done Criteria**:
- 3D composition (camera animation + lights + materials) を export
- ブラウザで再生可能
- Three.js / WebGPU の最新機能を活かす

---

### 5.4 M-LIVE-1: WebSocket Live Preview (副次)

**目的**: 編集中の composition を **Web ブラウザでプレビュー** する。

- `ArtifactWebUIHost` (既存) を活用
- WebSocket で WebUI にフレーム送信
- WebGL で表示
- 既存 composition の見た目 (mock 含む) を web で見られる

---

## 6. リスクと未解決論点

### 6.1 構造的リスク

1. **CSS keyframes の表現力不足**: SVG の `transform` 中心のアニメーションは、AE の複雑な compound transform (Track Matte / Precompose / Expression) を完全再現できない
2. **Lottie のスキーマは「位置 + 時間 + ease」のみ**: AE の **Blend Mode / Effect / Mask / 3D Camera** を完全サポートしない
3. **JS bundle のサイズ**: GSAP / Three.js / PixiJS は CDN 込み 100〜500 KB。**Self-contained export** のサイズ上限を意識する必要
4. **Web 上の色空間**: sRGB 前提。HDR / Dolby Vision は export 範囲外
5. **フォント**: Web Fonts 必要。Noto / Adobe Fonts の埋め込み方を考える必要

### 6.2 既存実装との接続

- ✅ `ShapeLayer::toSvg / fromSvg` 実装済み (双方向)
- ✅ `SvgFrameExporter` 実装済み
- ⚠️ `CssAnimationExporter` は **スタブ** (`addProperty("transform")` のみ)
- ⚠️ `ShapeGroup::toSvg()` は **operator 非対応**
- ❌ `Shape operator` (TrimPaths / Repeater) の export ロジックなし
- ❌ Effect / Mask / Text の export ロジックなし
- ❌ Blend Mode / Track Matte の export ロジックなし

### 6.3 サブモジュール境界

- `ArtifactCore/src/IO/VectorExport.cppm` は Core にあり直接編集可能
- `Artifact/` 側は Export ダイアログ統合のみ
- `ArtifactWidgets` には触らない (明示依頼時のみ)
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. 依存関係図 (推奨)

```
[現在]
ShapeLayer → toSvg() ✅
ShapeGroup / ShapePath → toSvg() ✅ (operator 適用外)
SvgFrameExporter → exportLayerToSvg / writeSvgSequence ✅
CssAnimationExporter → extractAnimationData (stub) / generateCss ⚠️

[Phase 1 — 短期]
CssAnimationExporter::extractAnimationData を完全実装
  + AnimatableValueT / AnimatableValueF から keyframes 抽出
  + Shape operator (TrimPaths / Repeater) を SVG 生成時に適用
  + Mask を SVG <clipPath> として出力
  + Blend Mode を mix-blend-mode に変換
  + .html プレイヤ生成

[Phase 2 — 中期]
AE → Lottie schema mapper
  + QJsonObject で JSON 構築
  + .html + lottie-web CDN bundle 同梱
  + Export ダイアログに統合

[Phase 3 — 長期]
AE → Three.js / WebGPU mapper
  + 3D scene graph
  + Camera / Light / Material 対応
```

---

## 8. まとめ

- **実装済み foundation**: SVG export, CSS animation export (部分), ShapeLayer ↔ SVG 双方向
- **未完成ポイント**: CssAnimationExporter stub, shape operator 適用外, Mask / Effect / Blend 非対応
- **Phase 1 (短期, 1〜2 sprint)**: SVG + CSS keyframes で **実用 export 完成**。Logo / banner / SNS 動画向け
- **Phase 2 (中期, 数 sprint)**: Lottie 互換 JSON で **AE / Figma 互換**。Web embed / メール / サイト内アニメ
- **Phase 3 (長期, 数 sprint)**: Three.js / WebGPU で **3D / 立体ロゴ**。データビジュアライゼーション / ゲーム UI

**推奨される着手順**:

1. **M-WEBEXPORT-1** (CSS keyframes 拡張 + .html プレイヤ) — **foundation 完成済み**で着手しやすい
2. **M-LIVE-1** (WebSocket Live Preview) — `ArtifactWebUIHost` 既存活用で短期
3. **M-LOTTIE-1** (Lottie 互換) — 既存 milestone と並走。広く採用されている標準形式
4. **M-WEBGPUEXPORT-1** (Three.js / WebGPU) — 3D pipeline 必須。`M-3DREFINE-1` と並走

M-WEBEXPORT-1 は **foundation 80% 完成**で工数も小さく、ROI が最も高い。最初に着手する価値あり。

---

## 9. 更新履歴

- 2026-06-16: 初版作成。`Artifact/`, `ArtifactCore/`, `ArtifactPr/` を 36 項目で走査。`VectorExport.cppm` 233 行を完全読了。