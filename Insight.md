# Insight Log

実装・調査中に得た、将来の改善案、設計上の仮説、再利用できる知見を記録する。

このファイルの内容は仕様や実装指示ではない。採用・優先順位付け・実装の可否は、別途ユーザーまたは設計レビューで判断する。

## 記録ルール

- 事実と推測を分ける。
- 未検証の内容には `未検証` と付ける。
- 依頼外の変更を避けるため、記録だけで実装を始めない。
- 関連ファイルや次の検証方法を残し、後から再開できるようにする。

## Insights

### 2026-07-25 — テクスチャ画像形式の入口統合

- 状態: 有望
- 関連: `ArtifactCore/src/Asset/AssetImporter.cppm`、`Artifact/src/Asset/AssetDirectoryModel.cppm`、OIIO画像読込
- 事実: `FileTypeDetector` は GIF/HDR/WebP/ICO/DDS/KTX を画像として認識していたが、AssetImporter の対応拡張子一覧と Asset Browser の画像判定が一部一致していなかった。
- 閃き・仮説: 入口の拡張子判定を揃えることで、既存の OIIO 読込経路へ到達できる形式を増やせる。
- 価値・懸念: テクスチャ形式のインポート導線が統一される。一方、現行の `loadImageViaOIIO` は UINT8 RGBA へ正規化するため、HDR/float の完全な精度保持は別課題。
- 次の確認: OIIO ビルドで各形式の decode 可否を実ファイルで検証し、必要なら float バッファ経路を追加する。

<!--
テンプレート:

### YYYY-MM-DD — 短い題名

- 状態: 未検証 / 調査中 / 有望 / 採用見送り / 完了
- 関連: `path/to/file`、機能名
- 事実:
- 閃き・仮説:
- 価値・懸念:
- 次の確認:
-->
## 2026-07-26 - Resident Debug Agent boundary

- Related files/features: `Artifact/src/AppMain.cppm`, `tools/debug-mcp-server`, playback diagnostics.
- Confirmed fact: a resident app-side agent can publish a lightweight playback snapshot without opening `AppDebuggerWidget`, and can cooperatively pause playback from MCP session state.
- Hypothesis / unverified: the same checkpoint path can be extended to property, render-resource, and buffer health probes without materially perturbing playback if sampling remains bounded.
- Value / concern: this gives the AI a live semantic observation point; arbitrary GPU memory inspection remains outside the current boundary.
- Next check: add registered watch descriptors and a bounded before/after ring buffer, then validate the MCP break-hit path with a live playback session.
