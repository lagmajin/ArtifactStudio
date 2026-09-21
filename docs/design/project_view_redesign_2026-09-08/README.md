# Project View Redesign Concepts

**最終更新:** 2026-09-21

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

## 2026-09-21 スクリーンショットからの調整

- 検索、Tree、Tile、新規Composition、新規Folder、Proxy、削除の7アイコンを
  `Artifact/App/Icon/Studio/project_*.svg` として作成。16px基準の太い形状と淡いグレーへ統一。
- SVGを16px固定Pixmapへ変換せずQIconのSVG描画を保持し、高DPIでの拡大ぼけを抑える。
- 操作ボタンを28px、アイコンを16px基準に設定し、既存Accessibility倍率を適用。
- タイルの名前・種別・Readyの重複を省き、解像度・タイミングを先に表示。
  詳細ペインの全メタデータは維持。
- 通常の単一レイアウトではLayoutバッジを省略。複数variantがある場合は表示を維持。
- 種別／状態バッジの強い輪郭を抑え、未選択時の `0 selected` を省略。

ソースとSVG XMLの静的確認のみ。ビルド・実機のDPI／視覚比較は未実施。
アイコンは既存のQt resource収集対象に入り、次回の通常ビルドでバンドルされる。

## 2026-09-21 Tree runtime review反映

- project未読込時は右detail paneを隠し、browse paneのempty stateだけを主表示にした。statusは`0 items`へ統一。
- type filterを固定幅へ縮め、Tree／Tileを同時表示するcompact switchへ変更。Unusedと検索を含むheader間隔も圧縮した。
- browse contextはview mode／filterの重複列挙をやめ、`Project / project name / scope`のbreadcrumbと結果件数に整理した。
- Tree列を`Name / Type / Status / Size / Modified`へ変更し、主要5列が標準browse幅へ収まる幅へ再配分した。内部ID列は引き続き非表示。
- empty state iconを72px・低opacityへ縮小し、既存action barへの案内を短く併記した。
- action iconは`project_*.svg`のfilesystem fallbackも解決し、resource未解決時に色付きstandard iconへ落ちにくくした。
- header、row separator、context bandのcontrastを抑え、item stateとselectionを優先した。

既存item model、selection同期、Tree／Tileの同一データ、Asset Browserとの責務境界は維持。ビルド・runtime比較は未実施。
