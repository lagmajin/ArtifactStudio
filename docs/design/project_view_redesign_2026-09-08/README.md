# Project View Redesign Concepts

**最終更新:** 2026-09-08

ImageGen による Project View のリデザイン参照案。現行の暗色・テーブル中心の構成を維持しながら、選択状態、検索／フィルター、インスペクターの責務を整理する方向を比較する。

- `project-view-focused-workbench.png`: 本命案。テーブルを主役にし、右側の選択詳細と操作を整理。
- `project-view-tile-workbench.png`: 本命案のタイル表示版。右側の選択詳細を維持し、フォルダーとアセットカードを中央グリッドに整理。
- `project-view-adaptive-inspector.png`: 比較案。選択時のコンテキスト操作を下部に寄せ、一覧の横幅を最大化。

これらは実装仕様ではなく、Project View の責務境界・既存テーマトークン・Asset Browser との分離を前提にした視覚探索用の参照画像。

## 2026-09-13 実装反映

`project-view-focused-workbench.png` を通常の Tree 表示における優先参照として、既存 `ArtifactProjectManagerWidget` の責務を維持したまま次を反映した。

- search／type／Tree・Tile／Unused を単一の低密度ヘッダーに集約
- browse context、件数、作成操作を一覧直上の一行へ固定
- projectなし／未選択時の右ペインを単一empty stateに統合
- 選択時の詳細をtitle／metadata／previewの縦構成へ変更
- 一覧、詳細、statusのsurface toneを既存theme token内で分離
- Project View固有の作成操作をstatus表示から分離し、一覧コンテキストへ配置

機能責務、selection同期、Tree／Tileの同一item model、既存context actionは変更しない。runtimeでの視覚比較はbuild許可後に行う。
