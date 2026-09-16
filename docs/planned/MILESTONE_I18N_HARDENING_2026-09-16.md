# MILESTONE: 翻訳システム作り込み (i18n Hardening)

**最終更新:** 2026-09-16

前段 `MILESTONE_I18N_IMPLEMENTATION.md`（2026-08-15）の続編。エンジン統合は済んだが、
実効カバレッジ・運用・実行時切替に穴がある。本書は 2026-09-16 時点の実測に基づく残課題と実装順序を定義する。

## 実測スナップショット（2026-09-16）

- `tt()`／`tr()` 呼び出し 623件・26ファイル vs ハードコード日本語 1144件・57ファイル。
  - 上位: `ArtifactLayerMenu.cppm` 209件、`ArtifactViewMenu.cppm` 137件、
    `ArtifactRenderOutputSettingDialog.cppm` 128件、`ArtifactFileMenu.cppm` 110件。
- `tools/i18n/audit_translations.py` は `tt()` を抽出できず、実行結果は `Keys used: 3`。
  CI（`i18n-check.yml`）の coverage 99.6% は実態を表していない。
- ロケール資産: en 523キー・ja 523キー（完全）。zh／zh-TW 各165キー（358欠落）。
  ar／de／es／fr／ko／pt／ru は各11キーのスタブ。
- `localeChanged` 系の通知は存在しない（`retranslate`／`ocaleChanged` 検索で0件）。
  言語は起動時のみ確定（`--lang`→システムロケール→既定en の三重実装が `AppMain.cppm` に分散）。
  設定UI・永続化なし。
- `AppMain.cppm` 2702-2703行目で `loadFromDirectory` を二重実行（委譲先が同一シングルトンのため無害だが無駄）。
- `LocaleFormatting::` の実利用は0件。`AT_TR` マクロは未定義（監査ツールだけが参照）。
- 複数形は呼出側の三項演算子で個別対応。フォールバック連鎖なし（zh-TW→en 直行）。
- `missingKeys()` 系APIは定義のみでアプリ内呼出しなし（監査はスクリプト独自実装）。

## 翻訳してはならないもの（今回のメニュー作業で確定した guardrail）

- 保存値と往復する文字列（トランジション種別 `Crossfade` 等、イージング名、プロキシ品質enum）。
- 生成物の既定名（`Null Layer`／`Solid` 等のレイヤー名初期値）。
- Undo履歴ラベル（`Change Light Linking` 等）。
- 上記を機械変換する際は、表示専用ラベルとデータ文字列の分別を必ずレビューする。

## P0: 監査を真実にする

### P0-1 監査ツールとCIの実効化

- `tools/i18n/audit_translations.py` の `KEY_PATTERNS` に `tt(` を追加。
  `TranslationManager::instance().tr(` の既存パターンは維持。
- キー命名規約チェックを追加：`^[a-z][a-z0-9_]*(\.[a-z0-9_]+)+$`。
  英語文そのままキー（現行2件：`Double-click the value...`、`Put .csx files in %1`）は別キーへ移行。
- `unused_in_locale`（JSONにあってコードにないキー）の扱いを決定：削除 or 保持リスト化。
- 完了条件：監査の `Keys used` が `tt()` 込みの実数と一致し、CIが実効カバレッジを gate する。

### P0-2 残りハードコードの計画移行

- 4ファイル（LayerMenu／ViewMenu／RenderOutputSettingDialog／FileMenu、計約580件）を
  `tt(menu.*, ...)` 化＋en／ja追記。手順はタイムライン左メニュー作業と同一
 （列挙→キー割当→範囲限定置換→JSON追記→網羅検証）。
- ファイル毎の `static tt()` 重複定義を `Translation.Manager` の共通 inline へ寄せる。
- ダイアログ文言（`QInputDialog` タイトル等）はメニュー移行後に別パスで対応。
- 完了条件：ハードコード日本語が0件、または残件リスト化して本書へ追記。

## P1: 実行時と言語資産

### P1-1 実行時言語切替（段階的）

1. `Event.Bus` 経由の `localeChanged` 通知＋設定画面の言語選択＋QSettings永続化。
   切替は再起動適用から開始。
2. オンデマンド構築メニュー（コンテキストメニュー等）は通知後の再取得で即時反映。
3. メニューバー等の静的構築UIの再翻訳は影響調査の上で別途（`retranslateUi` 相当の所有者責務を決める）。
- 完了条件：設定変更→再起動で言語が切り替わり、永続化される。`--lang` との優先順位を文書化。

### P1-2 起動経路の整理

- `AppMain.cppm` の言語判定三重実装（2595／2619／2671行付近）を一本化。
- `loadFromDirectory` 二重実行を除去。
- 完了条件：起動ログで言語決定理由が1回だけ出力される。

### P1-3 フォールバック連鎖と言語資産方針

- 連鎖フォールバック：zh-TW→zh→en（現状は en 直行）。`LocalizationManager::translate` に連鎖解決を追加。
- スタブ言語（11キー）の扱いを決定：翻訳追加 or 選択肢から除外。`availableLocales()` はロード済みのみ返す現行仕様と整合させる。
- 複数形：最低限 en／ru／ar の CLDR ルール helper。呼出側三項演算子は段階的に移行。
- 完了条件：zh-TW 実効カバレッジ向上、複数形 helper の単体確認。

## P2: 開発者体験

### P2-1 ホットリロードとスレッドセーフ

- JSON変更→再読込（`reload()` 新設）→オンデマンドUIへ反映。翻訳者向け。
- `LocalizationManager` に `QReadWriteLock`（読み取りは `translate()` のホットパスに配慮）。
- 完了条件：JSON編集→reload→コンテキストメニュー表示で反映。データ競合なし。

### P2-2 メタ情報とキー雛形生成

- JSON に `_meta`（version／translators／lastUpdated）を導入。`flattenJson` は `_` 始まりを除外。
- `tt()` 対応のキー抽出→JSON雛形（en=フォールバック、ja=空）生成モードを監査ツールへ追加。
- 完了条件：新規メニュー作業が「抽出→雛形→翻訳」の定型フローで回る。

### P2-3 `LocaleFormatting` の実利用

- 数値・日付表示を `LocaleFormatting` 経由へ段階移行（現状0件）。
  タイムコード・フレーム番号はロケール非依存のまま（SMPTE維持）。
- 完了条件：ステータスバー・情報パネルの主要表示がロケール対応。

## 対象ファイル一覧

| Phase | ファイル |
|-------|---------|
| P0-1 | `tools/i18n/audit_translations.py`、`.github/workflows/i18n-check.yml` |
| P0-2 | `Artifact/src/Widgets/Menu/*.cppm`、`Artifact/src/Widgets/Dialog/*.cppm`、`Artifact/translations/{en,ja}.json`、`Artifact/{src,include}/Translation/TranslationManager.*` |
| P1-1 | `ArtifactCore/{include/Utils/Localization.ixx,src/Localization/Localization.cppm}`、設定ダイアログ、`Event.Bus` 系 |
| P1-2 | `Artifact/src/AppMain.cppm` |
| P1-3 | `ArtifactCore/src/Localization/Localization.cppm`、各ロケールJSON |
| P2-1 | `ArtifactCore/src/Localization/Localization.cppm` |
| P2-2 | `tools/i18n/audit_translations.py`、各ロケールJSON |
| P2-3 | `ArtifactCore/include/Localization/LocaleFormatting.ixx`、各表示呼出し側 |

## 優先度・工数

| Phase | 優先度 | 工数 | 理由 |
|-------|--------|------|------|
| P0-1 監査実効化 | **P0** | 小 | 計測なしに改善は回らない。`tt` パターン追加＋lint |
| P0-2 ハードコード移行 | **P0** | 中 | 4ファイル約580件。機械的だが量が多い |
| P1-1 実行時切替 | **P1** | 中 | 通知＋設定＋永続化。静的UI再翻訳は別途 |
| P1-2 起動整理 | **P1** | 小 | 一本化と二重ロード除去 |
| P1-3 連鎖・複数形 | **P1** | 小〜中 | 連鎖は小、複数形と資産方針は判断が必要 |
| P2-1/2/3 開発体験 | **P2** | 小 | 余力で順次 |
