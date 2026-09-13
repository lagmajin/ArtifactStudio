# Media Import Picker 採用モック

**最終更新:** 2026-09-12

## 採用画像

- `media-import-picker-dcc-concept-2026-09-12.png`

## 採用範囲

- 左の Places / Projects、中央のサムネイルグリッド、右の選択詳細と Import Options の3ペイン構成
- 上部の breadcrumb、検索、メディア種別フィルター、grid/list 切替
- 下部の選択要約、`Use System Picker…`、`Cancel`、`Continue`
- charcoal の階層面、細い separator、cool blue の選択／primary accent、コンパクトなDCC密度

## 責務

- この画面はファイル探索、メディア確認、Import option の指定までを担当する。
- Project/Assets へのコピー、Asset 登録、source identity の変更は `Continue` 後の既存 Import 経路で行う。
- OS固有場所やネットワーク共有のため `Use System Picker…` を維持する。

## 実装時の注意

- 実レイヤーのサムネイルを毎選択時に再生成せず、既存 thumbnail cache を再利用する。
- 連番は個別ファイルの羅列ではなく、検出時に1タイルへ集約する。
- QtCSS を追加せず、既存 theme token、`QPalette`、owner-draw で近づける。
- 新しい signal / slot 配線は追加せず、既存イベント経路または直接 callback を使う。
- このモックを理由に Import 後のコピー／登録規則を変更しない。

## 実装状況

- `ArtifactMediaImportPickerDialog` を既存 `Artifact.Widgets.ImportAssetsDialog` module に追加した。
- File メニューと `ImportAssetsRequestedEvent` の両入口を専門ピッカーへ統一した。
- Places、パス入力、検索、種別フィルター、サムネイルグリッド、選択詳細、System Picker fallbackを実装した。サムネイルは Asset Browser の既存disk cacheが有効なら再利用し、未生成時はOSのfile iconへフォールバックする。
- `Detect image sequences` 有効時は、単一選択した番号付き静止画と同じ命名パターン／桁数の兄弟フレームを名前順で既存確認ダイアログへ渡す。
- Color space、alpha、proxyは現行Import APIに解釈optionの受け渡し契約がないため、誤って機能するように見せずImport後に編集可能であることだけを表示する。
- Project mutation、Asset ID登録、コピー、非同期処理は既存 `ArtifactProjectService::importAssetsFromPathsAsync()` に限定した。

## 生成プロンプト要約

高忠実度のDCC向け暗色デスクトップUIとして、Media Import Picker、3ペイン、サムネイルグリッド、連番・動画・音声・3D素材、ACEScg／Alpha／fps／Proxy設定、System Picker fallbackを指定した。配色は `#17191d`、`#202329`、`#292d34` と cool blue `#4b8fd8` を基調とし、黄色、glassmorphism、巨大な余白、内側focus枠を避けた。
