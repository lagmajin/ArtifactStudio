# マイルストーン: Layer Feature Expansion Gaps (画像・平面・テキスト・シェイプ)

**最終更新:** 2026-08-21
**ステータス:** Not Started
**優先度:** High
**関連:** `docs/planned/MILESTONE_SOLID_LAYER_NOISE_FILL_2026-08-18.md`, `docs/planned/MILESTONE_TEXT_GLYPH_SUBMITTER_2026-08-14.md`, `docs/planned/MILESTONE_SHAPE_PATH_CORE_IMPLEMENTATION_2026-04-16.md`, `docs/planned/MILESTONE_STILL_IMAGE_LAYER_PRODUCTION_READINESS_2026-08-08.md`, `docs/planned/MILESTONE_AUTOMATED_TESTING_FOUNDATION_2026-08-21.md`

## 目的

4重点レイヤー(静止画・平面・テキスト・シェイプ)の成熟度監査(2026-08-21)で判明した機能ギャップのうち、軽微な修正は即時対応済みとし、残りの構造的なギャップを段階的に解消する。共通テーマは「キーフレーム可能フラグと描画評価の断線」「runtime 受入の欠如」の2点。

## 即時対応済み(2026-08-21)

- シェイプ: HandDrawnWobble を Add Operator メニューへ接続(`ArtifactRenderLayerWidgetv2.cppm`)。
- 平面: `solid.color` キーフレームの保存消失を修正。`solidColorKeyframes` として `PropertySerializationBridge` 経由で保存/復元(`ArtifactSolidImageLayer.cppm`)。
- 静止画: 連番画像レイヤーのフレーム選択に Time Remap を接続。VideoLayer と同じ `getSourceFrameAtCompFrame()` 経路(`ArtifactImageLayer.cppm` の `draw()` / `toQImage()`)。

## 残存ギャップ(優先度順)

### P1 — キーフレームと描画の断線解消

| # | 項目 | 現状 | 根拠 |
|---|---|---|---|
| 1 | シェイプのパス頂点・演算子パラメータのキーフレームが描画に反映されない | `draw()` が時間評価なし。Core の `ShapePath::interpolate()`(モーフィングAPI)が実装済みで未接続 | `ArtifactShapeLayer.cppm:1768`、`ShapePath.ixx:168` |
| 2 | 平面グラデーション8パラメータがキーフレーム不可 | gradientStartColor 等が素のメンバ変数で `setAnimatable()` 未呼び出し | `ArtifactSolidImageLayer.cppm:85-104` |
| 3 | 静止画 Source Reframe(crop/pan/zoom)がキーフレーム不可 | `sourceCrop.*` が static 値のみ | `ArtifactAbstractLayer.cppm:6375` 付近の対比 |

いずれも Text レイヤーの適用ループ(`ArtifactTextLayer.cppm:2883-2894` の `interpolateValue()→setLayerPropertyValue()`)が雛形になる。

### P2 — 平面レイヤーの機能拡充

| # | 項目 | 現状 |
|---|---|---|
| 4 | Noise Fill の露出 | Core 側の `ProceduralTexture`(7種ノイズ+GPUパイプライン)完成済み。`ArtifactSolidFillType` に Noise=6 が未定義で露出ゼロ。専用マイルストーン `MILESTONE_SOLID_LAYER_NOISE_FILL_2026-08-18.md` は未着手 |
| 5 | 4-Color Gradient / Motion Tile 相当のエフェクト | AE定番の2種が `src/Effects/` に存在しない |

### P3 — テキストレイヤーの編集・品質

| # | 項目 | 現状 |
|---|---|---|
| 6 | in-canvas エディタ最終形 + Text tool 作成導線 | 一部経路が未だ `QInputDialog` モーダル。G9 未完了(`MILESTONE_TEXT_ANIMATOR_COMPLETION.md`) |
| 7 | Expression Selector / `textIndex`/`textTotal` 評価接続 | 構造体のみで評価経路分離(G5) |
| 8 | HarfBuzz 本線化・bidi・縦書き品質 | HarfBuzz backend は Qt フォールバックのまま。bidi は恒等写像。縦書き+ruby は CPU fallback |
| 9 | テキスト塗りグラデーション | `TextStyle.ixx` にデータモデル自体が不存在 |

### P4 — 静止画レイヤーの品質

| # | 項目 | 現状 |
|---|---|---|
| 10 | ICC プロファイル変換 | バイト数保持のみで変換未整備 |
| 11 | CMYK 精密変換 | 標準CMYK簡易変換のみ。ICC付き/CMYK+alpha 未対応 |

### P5 — 共通 runtime 受入

| # | 項目 | 現状 |
|---|---|---|
| 12 | 保存/再読込・複製・Undo/Redo の実測 | 4レイヤー全て「static 実装済み・runtime pending」 |
| 13 | プレビュー(GPU)/Software Preview/Render Queue の3経路一致 | 比較ハーネスはあるが実素材での判定記録ゼロ |
| 14 | ソフトレンダラーのブレンドモード縮退 | Hue/Saturation/Color/Luminosity が SourceOver フォールバックで GPU と結果が異なり得る(`ArtifactRenderQueueService.cppm:4100`) |

## 実施フェーズ

### Phase 1 — 断線解消(P1)

- シェイプ: `draw()` に時間評価ループを追加し、パス頂点キーフレームと `ShapePath::interpolate()` を接続する。演算子パラメータの Float プロパティ化(Repeater の String 型 anchorPoint/position/scale を含む)。
- 平面: グラデーション8パラメータを `AnimatableValueT` + プロパティ両書きに移行する(`setColor()` と同じパターン)。保存キーは `solidGradient*Keyframes` で `solidColorKeyframes` と同型。
- 静止画: `sourceCrop.*` に `setAnimatable(true)` を付与し、`buildRasterizedSurfaceBuffer()` 側で時間評価する。

### Phase 2 — 平面拡充(P2)

- `ArtifactSolidFillType::Noise = 6` を追加し、Core の `ProceduralTextureSettings` へのブリッジを実装する。GPU 経路優先(AGENTS.md のレンダラー優先順位に従う)。
- 4-Color Gradient / Motion Tile は既存 effect 契約(Enabled/Mix/Overscan 共通コントロール付き)で追加する。

### Phase 3 — テキスト編集・品質(P3)

- Text tool → クリックでレイヤー作成 → inline editor 起動の導線を完成させる。
- Expression Selector の評価を `TextAnimatorEngine` に接続する(`textIndex`/`textTotal` は `SelectorEvaluationContext` に既存)。
- HarfBuzz 接続と縦書き GPU 経路は規模が大きいため、専用マイルストーンへ分割して起票する。

### Phase 4 — runtime 受入(P5)

- `MILESTONE_AUTOMATED_TESTING_FOUNDATION_2026-08-21.md` のテスト基盤を使い、4レイヤーの保存/再読込往復テストを gtest で固定する。
- 3経路比較ハーネスで代表素材の判定を記録する(静止画は受入マトリクス IMG-01〜14、テキストは snapshot 契約、シェイプは pixel parity)。

## 対象外

- 動画レイヤーの新機能(開発優先方針により後回し)
- AI 系エフェクト(セグメンテーション/inpaint/超解像)は `MILESTONE_IMAGE_EFFECT_AI_GAP_2026-08-18.md` に委ねる
- GPU/Diligent 経路の大規模な構造変更
- シェイプの複数ストローク/塗りスタック(AE グループモデルへの移行)は Phase 1 の断線解消後に別途起票する

## リスクと確認方法

- **シェイプの時間評価追加は描画ホットパス**: 毎フレームの `interpolate()` 呼び出しコストが懸念される。キーフレーム不在時はゼロコストの early-return を必ず入れる。確認はプロファイリングと GPU/ソフト両プレビューの視覚比較。
- **平面グラデのキーフレーム化は既存プロジェクトとの互換**: 旧JSON(static 値のみ)からの読み込みは現行 setter 経路を維持し、キーフレーム配列は新キーでのみ書く。確認は旧プロジェクトファイルの再読込。
- **Time Remap 接続済みの連番**: 今回の修正は `isTimeRemapEnabled()` ガード付きで既存動作を変更しない。確認は Time Remap 無効時の連番再生が従来どおりであること。
- **ビルド・テスト実行**: AGENTS.md 制約によりユーザー指示が必要。Phase 1 の各修正後に確認を取る。
