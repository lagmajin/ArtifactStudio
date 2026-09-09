# Screenshot Export Dialog design

**最終更新:** 2026-09-09

## Version 1

`screenshot-export-dialog-v1-summary-2026-09-09.png` は、現行の Screenshot Export 項目を整理し、書き出し内容を確認しやすくするための採用モックである。Version 1 は `ArtifactScreenshotExportDialog` に反映済み。

- File / Format / JPEG Quality / Capture whole editor window / Multi-channel EXR の既存項目を採用範囲とする。
- 右側は既存入力から導出したテキスト要約とし、サムネイル生成や追加の GPU readback を行わない。
- JPEG Quality は JPEG 選択時だけ有効であることを明確にする。
- モック内の出力例、件数、解像度はレイアウト参考用であり、実データや新仕様を規定しない。
- 画像形式変換、キャプチャ経路、ファイル出力処理の変更根拠にはしない。
