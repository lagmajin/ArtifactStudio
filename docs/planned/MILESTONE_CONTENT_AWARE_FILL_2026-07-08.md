# Milestone: Content-Aware Fill（コンテンツに応じた塗りつぶし） (2026-07-08)

> 状態: 部分実装あり（静止画 Wire / Object Remover に複数方式と mask 入力が存在。動画の時間伝播・専用 Content-Aware Fill workflow は未確認）

**最終更新:** 2026-11-16
**Status:** 静止画の wire/object removal と Telea/NS inpaint は既存実装あり。専用 Content-Aware Fill workflow、時間方向の背景伝播、製品品質評価は未完了。

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

- `docs/analysis/REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` は当時の調査結果であり、現行状態の根拠としては古い。
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

## 2026-11-16 Object Removal R&D 初期着手

- 既存コードを調べ直し、`ArtifactCore/src/ImageProcessing/WireRemove.cpp` と `Artifact/src/Effect/ArtifactCreativeEffects.cpp` の静止画経路、複数 fill method、mask/clean inputs、GPU implementation の存在を記録した。専用 GPU 経路の runtime parity は未検証。
- 新しい評価計画・deterministic fixture generator を `experiments/ObjectRemovalLab/` に追加。計画は品質 gate を候補比較前に固定する手順と、mask 内/境界帯/変更漏れの測定を定義する。
- fixture は seed `20261116` の勾配、周期模様、交差細線、決定論的テクスチャ各512×320を生成する。clean/input/mask とSHA-256 manifest を保存する設計。
- rd0 fixture generator を実行し、4ケースの clean/input/mask、8-bit preview、manifest/hash を生成した。寸法と mask pixel 数の consistency を確認。`measure_baseline.py` は候補処理なしで masked-region/boundary error と outside-mask change を記録する。
- candidate output、quality gate、人手レビュー、製品 runtime、preview/final parity、性能は未実施。
- 次段階: mask 内/境界帯/変更漏れの baseline sanity を取得し、画像を確認した後、候補比較より先に評価 gate を固定する。
