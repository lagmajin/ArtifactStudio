# Noise Layer Dialog design

**最終更新:** 2026-09-09

## Version 1

`noise-layer-dialog-v1-preview-2026-09-09.png` は、現行の Noise Layer 作成項目を維持したまま、生成結果を把握しやすくするための採用モックである。Version 1 は `CreateNoiseLayerDialog` に反映済み。

- Name / Type / Seed / Size の既存項目を採用範囲とする。
- 右側のプレビューは設定値が変更されたときだけ更新し、毎フレーム再生成しない。
- Version 1 のプレビューは固定長 CPU バッファを保持し、GPU texture、staging resource、readback、`QImage` を使用しない。
- モック内のノイズ画像は視覚方向の参考であり、実際のノイズ生成結果やアルゴリズム仕様を規定しない。
- 新しいノイズ設定、描画経路、生成機能の追加根拠にはしない。
