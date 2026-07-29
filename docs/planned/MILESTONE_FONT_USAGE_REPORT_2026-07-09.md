# MILESTONE: 使用フォントインベントリ出力（Font Usage Report）

**ステータス:** Phase 1〜4・出力導線実装済み、runtime/build検証 pending

> 2026-07-09 作成

## 目的

游技機メーカー等の提出要件（プロジェクトで使用したフォントをすべて証明・提出する）
に対応するため、プロジェクト内で実際に使用されたフォントを収集し、実ファイルと
メタデータマニフェスト（JSON / CSV）として出力する機能を追加する。

## 背景

- 游技機（パチンコ・パチスロ）や一部の納品案件では、使用フォントの一覧と
  ライセンス証明を提出することが求められる場合がある。
- 現状 Artifact に「使用フォント一覧」を出力する専用機能はない。
- ただし部品は揃っている:
  - `ArtifactTextLayer::fontFamily()` / `ShapeLayer` のフォント — レイヤー単位の使用フォント取得
  - `FontManager`（`ArtifactCore/include/Font/FreeFont.ixx`） — フォント解決・ロード
  - `FontDescriptor`（`ArtifactCore/include/Font/FontDescriptor.ixx`） — family / style / fullPath / weight / italic
  - `GlyphAtlas.cppm:91` の `QRawFont::fromFont(font, QFontDatabase::Any)` で実ファイルパスが解決可能
  - 出力受け皿候補: `ArtifactProjectExporter` / `ArtifactProjectPackager` / `ArtifactProjectStatistics`

## ターゲット像

- プロジェクト内のテキストレイヤー + シェイプ（テキスト化）レイヤーから使用フォントを抽出
- 各フォントの family / style / weight / italic / 実ファイルパス を解決
- フォント実ファイルを出力フォルダへコピー（提出用）
- メタデータマニフェスト（family, style, weight, italic, filePath, license attestation 列）を JSON / CSV で出力
- ライセンス証明欄はユーザー管理のライセンスレジストリで手動 / 半自動入力

## 非ゴール（このマイルストーンの範囲外）

- フォントライセンスの自動判定・自動証明書発行（法的効力を持つ証明は対象外）
- 動画プレビュー等の一時フォント・システム UI フォントの提出（アプリ自身の UI フォントは除外）
- フォントの埋め込み・サブセット化（必要なら別マイルストーン）
- 外部 DCC アセット内に埋まったフォントの深層走査（GLTF / SVG 内テキスト等は後段）

## 現状とギャップ

| 項目 | 現状 | ギャップ |
|---|---|---|
| レイヤー使用フォント取得 | `ArtifactTextLayer::fontFamily()` 等 | 走査ヘルパがなく個別取得のみ |
| 実ファイルパス解決 | `QRawFont::fromFont().fileName()` で可能 | 統一ヘルパが未整備 |
| メタデータ構造 | `FontDescriptor` に path/style あり | license 欄・出力形式が未定 |
| 出力基盤 | Exporter / Packager あり | フォント専用レポート経路なし |
| ライセンス管理 | なし | レジストリ（family→license）要新設 |

## 設計原則

1. アプリ自身の UI フォントは除外し、コンテンツ（レイヤー）が使用するフォントのみを対象とする。
2. 実ファイルコピーは「提出用フォルダ」に集約し、元ファイルを壊さない（read-only 扱い）。
3. `FontDescriptor` を壊さず、license 欄は後方互換な拡張とする。
4. ライセンス証明の法的効力はユーザー責任とし、ツールは「記録・出力」に留まる。
5. 既存の Exporter / Packager の責務を侵さず、独立したレポート生成として実装する。

## Scope（想定する変更ファイル）

- `ArtifactCore/include/Font/FontDescriptor.ixx`（license 列の拡張）
- `ArtifactCore/include/Font/FreeFont.ixx`（family→実ファイルパス解決ヘルパ）
- `Artifact/include/Project/ArtifactProjectExporter.ixx` または新規 `ArtifactFontInventoryReporter`
- `Artifact/include/Layer/ArtifactTextLayer.ixx`（使用フォント列挙は既存 fontFamily を利用）
- `Artifact/include/Layer/ArtifactShapeLayer.ixx` / `ShapeLayer.ixx`（テキスト化シェイプのフォント）
- `Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`（Export > Font Usage Report メニュー）
- ライセンスレジストリ: `ArtifactCore/include/Font/FontLicenseRegistry.ixx`（新規想定）

## Phases

### Phase 1: 使用フォント収集ヘルパ

プロジェクト内の実使用フォントを重複なく収集する。

- compositions → layers を走査しテキスト / シェイプレイヤーの fontFamily を抽出
- フォールバック解決済みの正規ファミリ名へ正規化（`FontManager::resolvedFamily` 利用）
- 重複排除して `std::vector<QString>` の使用リストを構築

**Done when:**

- プロジェクト内の使用フォントリストが一意に得られる
- アプリ UI フォントが混入しない

### Phase 2: フォント実体解決

各ファミリからメタデータと実ファイルパスを解決する。

- `FontManager` に family → `FontDescriptor`（fullPath / style / weight / italic）解決ヘルパを追加
- 既存 `GlyphAtlas.cppm` の `QRawFont::fromFont().fileName()` パターンを再利用
- 解決できないフォントは「不足 / フォールバック」として明記

**Done when:**

- 各使用フォントの実ファイルパスが特定できる
- 解決失敗がレポートに反映される

### Phase 3: ライセンスレジストリ

family → ライセンス情報の紐付けを管理する。

- `FontLicenseRegistry` を新設（family / style → licenseFilePath / attestation / note）
- ユーザーが UI または設定ファイルで登録
- 未登録はレポートで「未証明」として警告

**Done when:**

- フォントごとにライセンス証明ステータスを保持できる
- 未証明フォントが可視化される

### Phase 4: レポート出力

実ファイルコピー + マニフェスト出力を行う。

- 出力フォルダへフォント実ファイルをコピー（上書き回避・リネーム）
- JSON / CSV マニフェストを出力（family, style, weight, italic, filePath, licensePath, attestation）
- 「Export > Font Usage Report」メニューから実行

**Done when:**

- 提出用フォルダにフォント実ファイル + マニフェストが揃う
- マニフェストが人間・ツール双方で読める

### Phase 5: 検証・UI 导線

- 未証明フォントの警告表示
- 既存 Exporter / Packager との重複を避け、独立導線で動作
- 出力結果の再読込・差分確認

**Done when:**

- レポート出力が安定して動く
- 未証明フォントが見落とされにくい

## Recommended Order

1. Phase 1 (使用フォント収集)
2. Phase 2 (実体解決)
3. Phase 3 (ライセンスレジストリ)
4. Phase 4 (レポート出力)
5. Phase 5 (検証・UI 导線)

### Why This Order

- Phase 1 がないと対象フォントが決まらず Phase 2 以降がブレる。
- Phase 2 で実ファイルが特定できてからコピー（Phase 4）が安全。
- ライセンスレジストリは Phase 3 で独立させ、出力形式に縛られない。
- UI 导線は最後に回しても、まず収集・解決・出力が固まっていれば検証しやすい。

## 連携先

- `Artifact/include/Layer/ArtifactTextLayer.ixx`（`fontFamily()`）
- `ArtifactCore/include/Font/FreeFont.ixx`（`FontManager`）
- `ArtifactCore/include/Font/FontDescriptor.ixx`
- `ArtifactCore/src/Text/GlyphAtlas.cppm`（`QRawFont::fromFont`）
- `Artifact/include/Project/ArtifactProjectExporter.ixx`
- `Artifact/src/Widgets/Menu/ArtifactFileMenu.cppm`
- `ArtifactCore/include/Font/FontLicenseRegistry.ixx`（新規想定）

## Validation Checklist

- プロジェクト内の使用フォントが一意に列挙される
- アプリ UI フォントが混入しない
- 各フォントの実ファイルパスが解決される
- フォント実ファイルが提出用フォルダへコピーされる
- マニフェスト（JSON / CSV）が正しく出力される
- 未証明フォントが警告される

## Notes

游技機メーカー等の提出要件は「使用したフォントをすべて提出」が典型。
法的証明の責任はユーザーにあり、本機能は「使用フォントの記録・実ファイル集約・
メタデータ出力」に留まる。ライセンス文書の真正性担保は別管理とする。

---

## Next Execution Slice

Phase 1 から入る。まずはプロジェクト内の実使用フォントを重複なく集める。

### Phase 1A の着手点

1. compositions → layers の走査経路を確認（`ArtifactProject` / composition / layer API）
2. テキストレイヤーの `fontFamily()` を収集し、`FontManager::resolvedFamily` で正規化
3. シェイプレイヤー内のテキスト化フォントも収集対象に含めるかを確認
4. アプリ UI フォント（defaultFontFamily 等）を除外する判定を入れる

### Phase 1 完了条件

- 使用フォントリストが一意に得られる
- UI フォントが混入しない
- 既存レイヤー挙動を壊さない

### Phase 2 の前提

- `QRawFont::fromFont().fileName()` のパス解決が本番環境で安定するか確認
- 複数 weight / style を持つファミリの代表選択ルールを決める
- 解決失敗時のフォールバック表記を先に決める

### Phase 3 への波及

- ライセンスレジストリは設定ファイル（JSON / ini）で永続化する方針
- 未証明フォントの警告は Phase 5 の UI 导線と共有する


---

## Static audit follow-up (2026-07-25)

Text／Shape layer の font family、FontDescriptor／FontManager、GlyphAtlas／QRawFont の既存資産は確認できるが、プロジェクト全体を走査する Font Usage Reporter、family→実ファイルの統一解決、license registry、JSON／CSV manifest 出力、提出用コピー導線は確認できない。

従って Phase 1 の専用収集ヘルパから Phase 5 の UI／検証まで未完了。既存の font 解決・glyph 資産は実装候補であり、使用フォントレポートの完了証拠とは扱わない。

## 2026-07-25 実装監査（更新）

判定: Phase 1〜4 の主要基盤と Phase 5 のメニュー導線は実装済み。runtime 上の全フォント種別・ライセンス登録・コピー結果の検証は未実施。

- `ArtifactProjectStatistics::collectMetadata()` は compositions / layers の JSON を走査し、`fontFamily` / `fontPath` / `fontFile` を重複排除して `FontUsageCollector` に渡す。アプリ UI フォントを走査対象へ直接追加する経路は確認できない。
- `FontUsageCollector` は `QRawFont::fromFont()` で実ファイル、resolved family、style、weight、italic を解決し、未解決数を report / manifest に記録する。
- `FontLicenseRegistry` と license status / attestation の JSON・CSV 出力、フォント実体と license file の提出用コピー、`font-usage.json` / `font-usage.csv` の生成が実装されている。
- File Menu に「使用フォントレポートを書き出す...」の導線があり、プロジェクト未選択時の無効化も実装されている。
- ただしシェイプレイヤー由来のフォント表現、複数 style / weight の代表選択、OS/Qt 環境差、license registry の UI 設定編集、実ファイルコピーの実環境成否は未確認。
- 従って、コード上は Phase 1〜4 と導線まで進捗しているが、受け入れ条件は build / runtime と実フォントを用いた検証待ち。

ビルド・実行確認はリポジトリ方針により未実施。
