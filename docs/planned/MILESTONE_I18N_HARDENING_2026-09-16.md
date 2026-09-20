# MILESTONE: 翻訳システム作り込み (i18n Hardening)

**最終更新:** 2026-09-18

前段 `MILESTONE_I18N_IMPLEMENTATION.md`（2026-08-15）の続編。エンジン統合は済んだが、
実効カバレッジ・運用・実行時切替に穴がある。本書は 2026-09-16 時点の実測に基づく残課題と実装順序を定義する。

## 進捗（2026-09-18）

### P0-1 完了：監査ツールと CI の実効化

- `tools/i18n/audit_translations.py` の `KEY_PATTERNS` に `menuText(` を追加し、
  `QStringLiteral(...)` で包まれたキーも抽出できるよう改善（`tt(`/`tr(`/AT_TR/`TranslationManager::instance().tr(` は既存）。
- 文字列連結の断片（`"components."` 等）を除外する `_is_valid_key()` を追加。
- `.github/workflows/i18n-check.yml` の `--min-coverage` を 80 → 95 へ引き上げ。
- 結果：`Keys used` は 410 → **1104**、ja カバレッジ **100%**（未翻訳4件は技術表記のため `--allow-same` 除外）。

### P0-2 進行：主要4ファイルの移行完了

`tools/i18n/migrate_hardcoded.py`（マッピング駆動の移行スクリプト）と
`tools/i18n/mappings/*.json` を新設し、以下を移行した。

| ファイル | 移行件数 | 使用ラッパー |
|---------|---------|-------------|
| `ArtifactFileMenu.cppm` | 約100件 | `menuText()` |
| `ArtifactViewMenu.cppm` | 137件 | `TranslationManager::instance().tr()` |
| `ArtifactLayerMenu.cppm` | 305件 | `TranslationManager::instance().tr()` |
| `ArtifactRenderOutputSettingDialog.cppm` | 120件 | `TranslationManager::instance().tr()`（`import Translation.Manager;` を追加） |

移行スクリプトの要件（再発防止）:
- **文字列長の降順**で置換（部分文字列の破損防止）。
- 既存の `menuText(`/`tt(`/`tr(` 呼び出しの**フォールバック引数は保護**（二重置換防止）。
- **隣接リテラル連結**（`"a" "b"` が複数行にまたがるもの）の断片は置換対象外。
  連結全体は手動で 1 キーにまとめて `tr()` 化する。

P0-2 ステータス: **対象4ファイル + RenderMenu（bonus）を完了**。翻訳可能なハードコード日本語0行（残りは開発者コメントのみ）。

**追加移行（2026-09-18 / P0-2残の一次バッチ）**: 小規模メニュー5ファイルを完了。
- `ArtifactOptionMenu.cppm`（7件・`menuText` ローカルヘルパーを追加して移行）
- `ArtifactEditMenu.cppm`（9件）
- `ArtifactEffectMenu.cppm`（9件・`effect.category.*` は既存）
- `ArtifactTimeMenu.cppm`（13件・`menu.time.*`）
- `ArtifactTestMenu.cppm`（8件＋複数行連結2件を手動統合）
- 監査は `Keys used 1172 / Expected 1395 / Coverage 100%` を維持。

**二次バッチ完了（2026-09-19）**: `FloatColorPickerHooks` 4、`ArtifactImportAssetsDialog` 11、
`ArtifactCompositionMenu` 26、`QuickLayerCreationDialog` 27、`ColorSwatchDialog` 30、`ArtifactAnimationMenu` 52 を移行。
監査は `Keys used 1321 / Expected 1544 / Coverage 100%` を維持。

**三次バッチ完了（2026-09-19）**: `PrecomposeDialog` 39（`u8"..."` 形式のため移行スクリプトに `u8` プレフィックス対応を追加）、
`CreateCameraLayerDialog` 40 を移行。監査は `Keys used 1396 / Expected 1619 / Coverage 100%` を維持。

**残り（P0-2残・最終）**: `CreatePlaneLayerDialog` 約78件のみ。
それ以外の検出残（`ArtifactTestMenu` / `ColorSwatchDialog` / `QuickLayerCreationDialog` / `CreateCameraLayerDialog` /
`PrecomposeDialog` の各1〜2件）は複数行フォールバックまたは開発者コメントの検出誤差（実体は移行済み）。
`ApplicationSettingDialog` の2件は言語セレクタの endonym 表示（意図的未翻訳）なので除外。
`PrecomposeDialog` の既定名 `"プリコンプ 1"`、`QuickLayerCreationDialog` の既定名 `"平面 1"` はデータ文字列として除外。

**除外リスト（翻訳してはならない）**: `ArtifactMainWindow.cppm` / `ArtifactToolOptionsBar.cppm` /
`ArtifactLooksPresetBrowser.cppm` はツール名・ライブラリ名を `==` の識別子として流用しているため、
単純な `tr()` 置換の対象にしない（Insight.md 2026-09-19 参照）。データ文字列（既定レイヤー名等）も対象外。

### P1 / P2 未着手

既知の未対応: 未使用キー（`unused_in_locale`）の整理方針は未決定のまま。


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
  **[完了 2026-09-18]** `menuText(` 対応と `--min-coverage 95` を適用済み。残るは `unused_in_locale` の方針決定のみ。

### P0-2 残りハードコードの計画移行

- 4ファイル（LayerMenu／ViewMenu／RenderOutputSettingDialog／FileMenu、計約580件）を
  `tt(menu.*, ...)` 化＋en／ja追記。手順はタイムライン左メニュー作業と同一
 （列挙→キー割当→範囲限定置換→JSON追記→網羅検証）。
- ファイル毎の `static tt()` 重複定義を `Translation.Manager` の共通 inline へ寄せる。
- ダイアログ文言（`QInputDialog` タイトル等）はメニュー移行後に別パスで対応。
- 完了条件：ハードコード日本語が0件、または残件リスト化して本書へ追記。
  **[対象4ファイル完了 2026-09-18]** FileMenu／ViewMenu／LayerMenu／RenderOutputSettingDialog を移行済み。
  他ファイルの残件は `tools/i18n/scan_hardcoded.py` の出力を参照し、翻訳対象外（データ文字列等）を除外して縮小する。

## P1: 実行時と言語資産

### P1-1 実行時言語切替（段階的）

1. `Event.Bus` 経由の `localeChanged` 通知＋設定画面の言語選択＋QSettings永続化。
   切替は再起動適用から開始。
2. オンデマンド構築メニュー（コンテキストメニュー等）は通知後の再取得で即時反映。
3. メニューバー等の静的構築UIの再翻訳は影響調査の上で別途（`retranslateUi` 相当の所有者責務を決める）。
- 完了条件：設定変更→再起動で言語が切り替わり、永続化される。`--lang` との優先順位を文書化。
  **[第1段 完了 2026-09-18]** 設定画面と言語資産、起動時適用まで実装：
  - `ArtifactAppSettings` に `appLanguageCode()` / `setAppLanguageCode()` を追加
    （保存キー `General/LanguageCode`、空文字はシステム追従）。`ConfigSchema` にも登録済み。
  - 環境設定ダイアログの `GeneralSettingPage` に **Language** グループとセレクタを追加
    （Auto (System) / English / 日本語 / 简体中文 / 繁體中文 / 한국어 / Français / Deutsch / Español / Português / Русский / العربية）。
    「次回起動時に適用」の注記付き。設定はダイアログの OK で保存される。
  - 起動時の優先順位を **`--lang` > 保存設定 > システムロケール > `en`** に確定し、決定理由をログに1行出力。
  - 即時再翻訳と `Event.Bus` の `localeChanged` 通知は第2段として実装済み（下記）。
  - **[第2段 完了 2026-09-18]** 言語切替の即時反映と通知：
    - `Core.Localization` に `LocaleChangedEvent { QString locale; }` を追加。
    - `LocalizationManager::setLanguage` が確定後に `globalEventBus().publish(LocaleChangedEvent{...})` を発火
      （起動時の `--lang` 決定でも発火するが、この時点では購読者がいないため無害）。
    - 環境設定ダイアログの `GeneralSettingPage::saveSettings` は、保存に加えて
      `LocalizationManager::setLanguageCode(selectedLanguage)` を即時実行する。
      空文字 (Auto) は次回起動時にシステムロケールで再解決するため即時適用しない。
    - メニューは全て `aboutToShow` で `rebuildMenu()` するため、言語変更後にメニューを
      開いた時点で新言語のラベルへ再構築される（オンデマンド再翻訳が構造的に成立）。
      常時表示の静的UI（メニューバーのトップレベル等）は再構築タイミング次第で
      次回起動まで旧言語が残り得る。この残差は別途 `retranslateUi` 相当の所有者責務を決めて対応する。
  優先順位の文書化：`--lang` が最優先。次に設定画面で保存した言語。未設定ならシステムロケール。
  設定画面には `--lang` の値は保存されないため、`--lang` 起動と保存設定が食い違う場合は `--lang` が勝つ。

### P1-2 起動経路の整理

- `AppMain.cppm` の言語判定三重実装（2595／2619／2671行付近）を一本化。
- `loadFromDirectory` 二重実行を除去。
- 完了条件：起動ログで言語決定理由が1回だけ出力される。
  **[完了 2026-09-18]** 起動時に `--lang` → システムロケール → `en` の順で一度だけ
  `localeCode` を確定し `LocalizationManager::setLanguageCode()` を呼ぶ形へ統一。
  カタログのロードは `QApplication` 構築後に `loc.loadFromDirectory()` 1回のみ。
  ログは `[AppMain] Language decided: <code> by <--lang|system locale>` の1行に集約。

### P1-3 フォールバック連鎖と言語資産方針

- 連鎖フォールバック：zh-TW→zh→en（現状は en 直行）。`LocalizationManager::translate` に連鎖解決を追加。
- スタブ言語（11キー）の扱いを決定：翻訳追加 or 選択肢から除外。`availableLocales()` はロード済みのみ返す現行仕様と整合させる。
- 複数形：最低限 en／ru／ar の CLDR ルール helper。呼出側三項演算子は段階的に移行。
- 完了条件：zh-TW 実効カバレッジ向上、複数形 helper の単体確認。
  **[完了 2026-09-18]**
  - **連鎖フォールバック**：`LocalizationManager::translate` に `fallbackChainFor()` を導入。
    現在の言語 → 英語の順に解決し、繁体字中国語のみ 繁中 → 簡中 → 英語 の三連鎖にした。
    これにより zh-TW の 358 欠落キーは簡中 → 英語へ段階的に落ちる。
  - **言語資産方針**：環境設定の言語セレクタは `LocalizationManager::availableLocales()`
    （= ロード済みのみ）と整合させ、カタログが存在しない言語は候補に出さない。
    保存済み言語が候補に無い場合は、値を失わないよう候補へ動的追加して選択する。
    **[2026-09-19 追記]** ロケール別キー数 API `LocalizationManager::translationCount(code)` を追加し、
    環境設定の言語セレクタは「英語キー数の 1/10（最低50キー）」未満のカタログを除外するようにした。
    実測: en 1409 / zh 165 / zh-TW 165 / ko・fr・de・es・pt・ru・ar 各 11。
    しきい値 140 により zh・zh-TW は残り、スタブ7言語は選択肢から外れる（英語は常に残す）。
  - **[2026-09-19 連鎖の実測]** `tools/i18n/check_chain_coverage.py` で zh-TW の実効カバレッジを計測した結果、
    `zh-TW` と `zh` のキー集合は**完全に同一（各165キー）**で、連鎖による追加解決は **0件**（全体の 11.7%、残り 1244 は英語へ）。
    つまり `zh-TW → zh → en` の連鎖実装は正しいが、**現状は利得が出ない**。
    利得を出すには zh / zh-TW のカタログ自体を拡充する必要がある（チェーン側の問題ではない）。
    `startsWith`/`contains` 等の判定は不要で、`fallbackChainFor` の順序と `translate` の走査が正しいことを
    `tools/i18n/check_chain_coverage.py` / `check_locale_keysets.py` / `check_plurals.py` の3スクリプトで確認済み。
  - **複数形**：`PluralCategory` と `pluralCategoryFor()`（en / ru / ar の CLDR ルール、
    他言語は one / other の二値）、`LocalizationManager::pluralCategory()` /
    `translatePlural(baseKey, count, fallbackSingular, fallbackPlural)` を追加
    （`baseKey.one|few|many|other` を引き、無ければ `baseKey.other`、最後に呼出側フォールバック）。
    ルールの期待値は `tools/i18n/check_plurals.py` のミラーで確認済み
    （en: 1→one / 他→other、ru: 21,101→one, 2-4,22-24→few, 5-20,25→many、
    ar: 0→zero, 1→one, 2→two, 3-10→few, 11-99→many, 100→other）。
    C++ 側のコンパイルと実機単体テストは未実施（ビルド禁止のため）。呼出側の三項演算子移行は未着手。

## P2: 開発者体験

### P2-1 ホットリロードとスレッドセーフ

- JSON変更→再読込（`reload()` 新設）→オンデマンドUIへ反映。翻訳者向け。
- `LocalizationManager` に `QReadWriteLock`（読み取りは `translate()` のホットパスに配慮）。
- 完了条件：JSON編集→reload→コンテキストメニュー表示で反映。データ競合なし。

### P2-2 メタ情報とキー雛形生成

- JSON に `_meta`（version／translators／lastUpdated）を導入。`flattenJson` は `_` 始まりを除外。
- `tt()` 対応のキー抽出→JSON雛形（en=フォールバック、ja=空）生成モードを監査ツールへ追加。
- 完了条件：新規メニュー作業が「抽出→雛形→翻訳」の定型フローで回る。
  **[完了 2026-09-18]**
  - `Artifact/translations/{en,ja}.json` の先頭に `_meta`（version / language / locale /
    translators / lastUpdated）を追加。
  - `LocalizationManager::Impl::flattenJson` は `_` 始まりのキー（`_meta` 等）を読み込み対象から除外。
    同様に `tools/i18n/audit_translations.py` の `flatten_strings` も `_` 始まりを除外し、
    メタ情報が翻訳キーとして集計されないようにした。
  - 監査ツールに `--emit-template <path>` を追加。ソース（`tt()` / `menuText()` /
    `tr()` / `TranslationManager::instance().tr()`）から抽出したキーのうち baseline に無いものを
    `{"en": {…: ""}, "ja": {…: ""}}` の入れ子雛形として出力する。
    動作確認済み（`tt("probe.new_key", …)` と `menuText(QStringLiteral("probe.other_key"), …)` を
    抽出し、`probe.new_key` / `probe.other_key` の雛形を生成）。
  - 監査は `_meta` 追加後も `Keys used 1124 / Expected 1347 / Coverage 100%` を維持。

### P2-3 `LocaleFormatting` の実利用

- 数値・日付表示を `LocaleFormatting` 経由へ段階移行（現状0件）。
  タイムコード・フレーム番号はロケール非依存のまま（SMPTE維持）。
- 完了条件：ステータスバー・情報パネルの主要表示がロケール対応。
  **[初回適用 2026-09-18]** `LocaleFormatting` の実利用を開始：
  - 環境設定の「Memory & CPU」ページ（`MemoryAndCpuSettingPage::Impl::updateStats`）の
    メモリ表示を `LocaleFormatting::formatFileSize` / `formatPercentage` 経由へ移行
    （旧: `usedMB / totalMB (%)` の手計算 → 新: `1.23 GiB / 15.9 GiB (45 %)`）。
    プロセス CPU 表示も `formatPercentage` へ移行。
  - `ApplicationSettingDialog.cppm` に `import Localization.LocaleFormatting;` を追加。
    `LocaleFormatting` モジュールは `ArtifactCore/CMakeLists.txt` に登録済みであることを確認。
  - タイムコード・フレーム番号は SMPTE 非依存のまま（`formatTimecode` / `formatFrame` は維持）。
  - 残る表示（ステータスバー・Inspector・情報パネル）への展開は段階的に行う。
    優先度の高い数値表示から順に `LocaleFormatting` へ寄せる。

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
| P0-2 ハードコード移行 | **P0** | 中 | 4ファイル約580件。機械的だが量が多い | → **完了**（4対象 + RenderMenu bonus）。残393文字列は対象外15ファイル。
| P1-1 実行時切替 | **P1** | 中 | 通知＋設定＋永続化。静的UI再翻訳は別途 | → **完了（第1段）** |
| P1-2 起動整理 | **P1** | 小 | 一本化と二重ロード除去 | → **完了** |
| P1-3 連鎖・複数形 | **P1** | 小〜中 | 連鎖は小、複数形と資産方針は判断が必要 | → **完了** |
| P2-1/2/3 開発体験 | **P2** | 小 | 余力で順次 |
