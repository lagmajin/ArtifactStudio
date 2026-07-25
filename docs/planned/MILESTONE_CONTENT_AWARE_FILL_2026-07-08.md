# Milestone: Content-Aware Fill（コンテンツに応じた塗りつぶし） (2026-07-08)

> 状態: DRAFT（新規・未実装・専用マイルストーン未作成を確認済み）

---

## 1. 概要

動画フレーム内の不要なオブジェクト・破損・ロゴを、周囲のテクスチャから自動補完して除去する機能。After Effects の「コンテンツに応じた塗りつぶし」相当。

## 2. なぜ必要か（AE ライクなモーショングラフィックスとして）

- 広告・バリアブル動画制作で「実写背景からテンプレ要素を消す」頻出。
- 既存の Roto/Paint レイヤー（`MILESTONE_PAINT_LAYER`）は手動塗り。自動補完がないと作業コストが極端に高い。
- トラッキング済みのマスク領域に対して適用できれば、動画全体へ展開可能。

## 3. 参照元ツール

- **After Effects** — Content-Aware Fill（静止補完 + 動画フレーム間伝播）。
- 技術的里付け: OpenCV `inpaint`（Telea / Fast Marching）+ PatchMatch 的フレーム間伝播。

## 4. 現状（ソース確認・2026-07-08）

- `docs/analysis/REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` で **0 hit / 完全未着手** を確認。
- リポジトリ内に関連基盤は存在: `docs/FEATURE_DICTIONARY_2026-04-17.md` に `ImageProcessing/OpenCV/OpenCVPuppetEngine`, `OpenCVRotoBrushEngine` の記載あり → OpenCV バインディングは利用可能。
- 専用マイルストーンは存在しない（grep で `Content.Aware` 0 hit）。

## 5. スコープ（提案 Phase）

- **Phase 1 — 静止フレーム補完**
  - マスク/ブラシ選択領域に対する `inpaint` 適用。
  - Paint Layer 上の「塗りつぶしモード」として組み込み。
- **Phase 2 — 動画伝播**
  - トラッキング/マスクキーフレームに沿ったフレーム間伝播（前後フレームからの PatchMatch）。
- **Phase 3 — 品質・UX**
  - 進捗 UI、プレビュー、キャンセル、Undo 統合。

## 6. リスク / 未確認事項

- OpenCV 依存は既存の `OpenCVRotoBrushEngine` 経路で解決可能か要確認。
- GPU 化（現在は CPU inpaint 中心）は後回し。

## 7. 関連文書

- `docs/planned/MILESTONE_PAINT_LAYER_2026-06-16.md`
- `docs/analysis/REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md`
- `docs/FEATURE_DICTIONARY_2026-04-17.md`

## 2026-07-25 実装監査

- 専用の Content-Aware Fill／inpaint／PatchMatch 実装、動画フレーム間伝播、専用 UI・進捗・キャンセル・Undo 統合は確認できない。
- 既存の RotoMask／RotoBrush、MaskCutout、ColorCorrection の Fill は関連する基盤または別責務であり、Content-Aware Fill の補完処理を実装済みとはみなさない。
- よって本マイルストーンは DRAFT／未着手の判定を維持する。最初の実装単位は OpenCV inpaint を使う静止フレーム補完である。