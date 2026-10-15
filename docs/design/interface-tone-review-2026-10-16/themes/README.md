# Theme candidates

**最終更新:** 2026-10-16

**状態:** 03 Modo Graphite の配色方向をユーザー依頼により既存 ModoStyle へ反映。実行時確認待ち。他4案は未採用・未実装。

Maya、After Effects、Modo を好むというユーザーの希望から、ArtifactStudio の同じ空ワークスペースを題材に5方向を生成した。各製品の公式テーマやバージョン別配色を厳密に再現したものではない。内蔵 image_gen を使用。

| 案 | 画像 | 方向 | パネル / アクセントの概念値 |
|---|---|---|---|
| 01 Maya Slate | [画像](01-maya-slate.png) | 中明度のニュートラルグレーとティール | `#414345` / `#65BFC4` |
| 02 AE Carbon | [画像](02-ae-carbon.png) | 深い無彩色チャコールと青 | `#252525` / `#559FFF` |
| 03 Modo Graphite | [画像](03-modo-graphite.png) | 暖かいグラファイトとアンバー | `#383633` / `#D99A4C` |
| 04 Silver Studio | [画像](04-silver-studio.png) | 明るい中明度シルバーと濃い青 | `#B6B8BA` / `#356C98` |
| 05 Ink Slate | [画像](05-ink-slate.png) | 青みのスレートと淡いシアン | `#2C333A` / `#79BCCB` |

## 比較条件と制限

元画像のパネル構成、幅比率、既存の操作、空の状態を共通条件とし、色を中心に比較する。生成画像のため文字、寸法、アイコンなどには差があり、ピクセル単位で色だけを置換した比較ではない。表示された色見本や HEX 表記も実測値ではなく生成時の指定値。実装、アクセシビリティ、コントラスト、実行時表示の検証はしていない。

新機能や描画内容、ギズモ、HUD の追加を意図しない。既存 File Menu アイコンの形状や承認済み Viewport 仕様は変更しない。これらのモックで生じた細部の差は仕様変更の根拠にしない。

元画像と初回診断は [親 README](../README.md) を参照。

## ModoStyle への反映

既存 ModoStyle の背景・面・入力・文字・境界・選択・ボタン・スライダー・フォーカスの色をグラファイトとアンバーへ変更。他プリセットの色定義は維持した。Info / Success などの意味色は既存の派生値を維持する。

`UI/ThemeName` のスキーマ既定値、取得時の未設定 fallback、現在テーマの初期値を Modo に揃え、設定画面の候補でも Modo を先頭にした。

追加のユーザー指示により、起動時は保存済み設定より Modo を優先する。`Artifact/src/AppMain.cppm` の初回テーマ適用前に、既存の非永続 Session 層へ `UI/ThemeName=Modo`、`UI/ThemePresetPath` と `UI/AccentColor` は空文字を設定する。外部プリセットやアクセント指定が保存されていても、この起動では Modo の組み込み配色を使用する。永続設定は書き換えず、起動後は設定画面から変更可能。次回起動時は再び Modo を優先する。

変更箇所は `ArtifactCore/include/Utils/WindowStyleCSS.ixx`、`ArtifactCore/src/Application/ArtifactAppSettings.cppm`、`Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`。新規依存・シグナル接続・QtCSS は追加していない。画像のレイアウトや文字サイズは実装対象外。ビルド、テスト、UIキャプチャ、コミットは行っていない。

## 生成プロンプト

全5案の共通条件と個別パレット指定は [prompts.json](prompts.json) に保存した。

## 参考ページ

各製品の UI 文脈を確認するために参照。パレット値は本検討で独自に設定した。

- [Maya: Navigating the Interface](https://area.autodesk.com/learn/courses/maya-intro/01-navigating-the-interface/)
- [After Effects: Setting preferences](https://helpx.adobe.com/sg/after-effects/desktop/get-started/preferences-and-settings/preferences.html)
- [Modo: User Interface Conventions](https://modosdk.foundry.com/modo/content/help/pages/modo_interface/ui_conventions.html)
