# Quick Layer Creation Dialog design

**最終更新:** 2026-09-09

## Version 1

`quick-layer-creation-dialog-v1-2026-09-09.png` は、既存の `QuickLayerCreationDialog` の機能を増やさず、縦長の入力フローを整理するための提案モックである。

- Source / Size を左列、Mask / Placement を右列、Entry / Exit Envelope を下段に配置する。
- Entry / Exit は既存どおり有効化チェックのみとし、Timing / Curve / Length は共通設定を一組だけ表示する。
- Plane 選択中の Image 入力と Browse は無効状態を明確にする。
- 画像プレビュー、サムネイル生成、GPU readback、画像形式変換は追加しない。
- 既存の作成オプション、設定保存、Undo 経路、レイヤー生成責務を変更しない。
- QtCSS、新規 signal / slot 接続、`QImage` を追加しない。

## 実装状況

Version 1 の二列構成を `Artifact/src/Widgets/Dialog/QuickLayerCreationDialog.cppm` に反映済み。Source / Size、Mask / Placement、Envelope の既存入力・設定保存・作成オプションは維持している。
