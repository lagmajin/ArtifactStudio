# Status Bar design reference

**最終更新:** 2026-09-09

## 状態

以下はいずれも ArtifactStudio のステータスバー専用の**提案モック**であり、未承認である。

- `status-bar-concept-compact-muted-2026-09-09.png`: 縦幅を約 20–25% 抑え、本文色を低輝度のウォームグレーにした改訂案。
- `status-bar-concept-semantic-text-2026-09-09.png`: compact 案を維持し、右側を暗い label、明るい value、amber の警告値という 3 段階に整理した改訂案。
- `status-bar-concept-2026-09-09.png`: 比較用の初稿。

## 参照範囲

- 横長 1 行の情報密度、区切り線、余白、文字階層、charcoal / amber の配色を参照対象とする。
- 左側は `Timeline Debug`、`Project`、`Layer`、右側は `Console`、`Coordinates`、`Frame`、`Selection`、`Zoom`、`FPS`、`Memory`、`Drops`、`Accessibility` の現行 `ArtifactStatusBar` item 順を示す。
- 表示値はレイアウト確認用の例であり、固定文言や新しい仕様を意味しない。
- hover、click、context menu の具体挙動はこの画像だけでは決定しない。既存 API と表示切替 context menu を維持する。

## 対象外

- GPU 使用率／温度、現在ツール、quick settings、自動保存、最終 render 時間、encoding／color space、通知 badge など、現行 item model にない機能の追加根拠にはしない。
- status bar 外の dock、viewport、timeline、playback control の配置や外観は参照対象外とする。
- 画像を理由に新しい signal／slot、イベント経路、描画経路を追加しない。

## 生成プロンプト要約

Built-in ImageGen の `ui-mockup` として生成した。ArtifactStudio の charcoal surface、現行 item model のみ、カード／pill／gradient／架空機能なしを指定した。改訂案では 22–24 px 相当の単一行、ウォームグレーの本文、低彩度 amber、短く控えめな区切り線を指定した。semantic-text 案では prefix label を `#858783` 相当、通常 value を `#BBB8B0` 相当、warning value を `#D6A44B` 相当に分けた。
