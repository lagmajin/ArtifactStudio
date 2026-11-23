# Retarget Lab — image sources

**最終更新:** 2026-11-16

rd1（実写）で使う写真素材の出所・ライセンス・hash。素材は複製せず、既存ラボの作業コピーを参照する。`photo_study.py` は実行時に SHA-256 を照合し、一致しなければ停止する。

## Portrait 2024（fixture `portrait`）

- 出所: [Wikimedia Commons — Portrait 2024.jpg](https://commons.wikimedia.org/wiki/File:Portrait_2024.jpg)
- ライセンス: CC0 1.0（説明ページ記載）。撮影者は埋め込みメタデータの Louis Rivest-Henault、uploader/copyright holder は `MarieClaudeGendron2`
- 使用ファイル: `experiments/CinematicLooksLab/assets/portrait-2024-1536.jpg`、1536 × 1024
- SHA-256: `9c14e3c0206ad6fe080c349368bf29219071892002fc01f1b6fc947af3bd40e2`
- 原板: `portrait-2024-source.jpg`（5184 × 3456、`50b3f431675b55d53fefa13451d734af13988c43585a5236c39b2644a47a4f07`）
- 出所記録: `experiments/CinematicLooksLab/assets/SOURCES.md`
- 評価対象: 髪・肌・窓のハイライト

## Landscape of a wild forest（fixture `forest`）

- 出所: [Wikimedia Commons — Landscape of a wild forest.jpg](https://commons.wikimedia.org/wiki/File:Landscape_of_a_wild_forest.jpg)
- ライセンス: CC0 1.0（説明ページ記載）。撮影/uploader: SpessiG
- 使用ファイル: `experiments/CinematicLooksLab/assets/forest-wild-1536.jpg`、1536 × 1024
- SHA-256: `b417d6ffe3b494ee80906d6b433544ee7a2afa51c1557dcdf3b8feae9536ce6c`
- 原板: `forest-wild-source.jpg`（3136 × 2091、`2f9f01326230bf9166eec430eb6d14cac69412d6cf6e2ecc7a25970683e4ddea`）
- 出所記録: `experiments/CinematicLooksLab/assets/SOURCES.md`
- 評価対象: 葉・枝の細部、深い影、ハイライト

## Neon streets at night（fixture `neon`）

- 出所: [Wikimedia Commons — Neon streets (Unsplash).jpg](https://commons.wikimedia.org/wiki/File:Neon_streets_(Unsplash).jpg)
- ライセンス: CC0 1.0（Commons 記載）。撮影: JINDONG H.
- 使用ファイル: `experiments/GlowLab/assets/cc0-neon-streets-1536.jpg`、1536 × 1025
- SHA-256: `8791bd593b699be76dcfb181d72dd8655caaae2e88c235b89ed5d9097aa319d4`
- 原板: `cc0-neon-streets.jpg`（4496 × 3000、`401ba941fd51c203b79f734d76c2279ffde61d8c2f644a53bf3130370cab22d7`）
- 出所記録: `experiments/GlowLab/assets/SOURCES.md`
- 評価対象: 看板・文字様構造、点光源、夜景テクスチャ

## 取り扱い

- CC0 は著作権ライセンスの記載であり、写真を見た目・測色・構図の正解として扱わない。
- 前処理（高さ 432 px への LANCZOS 縮小）で内容が変わるため、作業コピーの PNG と SHA-256 を `captures/rd1/` と `captures/rd1/metrics.json` に記録する。
- 実写には正解 matte が無い。subject mask を作らず、`subject_retention` 系のゲートは実写に適用しない。
