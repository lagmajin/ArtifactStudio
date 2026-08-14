# バッチ再リンク・一括参照更新

**最終更新:** 2026-08-14

## 進捗

- [x] 副作用なしの候補探索API（ファイル名、basename、連番パターン、サイズ、ディレクトリ末尾一致）
- [x] 連番全フレーム検証（既存FootageItemのsequencePathsが取得できる場合）
- [x] Asset Browserからの候補検索・単一候補適用導線
- [x] 複合Undoによる一括反映（FootageItem + image/video/audio/svg sourcePath + AssetDatabase移行API）
- [x] 複数候補の一括確定UI（Asset Browserの複数選択時）

## 目的

素材移動後に複数の Missing Footage を個別修正せず、候補を確認して一括再リンクできるようにする。

## 現状の確認

- `ArtifactProjectService::relinkFootageByPath()` は旧パスに完全一致する `FootageItem` を1件だけ検索する。
- `relinkFootageItems()` は複数項目を同じ新パスへ向けるAPIであり、候補探索は行わない。
- Asset Browser の `RelinkAssetCommand` は `relinkFootageByPath()` の呼び出しだけをUndo/Redoする。
- `ArtifactAbstractComposition::allLayer()` / `allLayerRef()` により、コンポジション内レイヤーの列挙は可能。
- FootageItem のパスと、レイヤーの `image/video/audio/svg.sourcePath` は、Asset Browserのバッチ導線では同一Undo単位にまとまる。レイヤー更新時のAssetManager再取得・旧参照解放はコード上確認済み。AssetDatabaseの直接同期APIも追加済みで、実データ実行検証が残る。
- 再リンク対象の検索・同一判定・AssetDatabase移行前判定は、canonical path、absolute fallback、clean path、Windows case foldingの共通規則へ統一済み。同一フレームは移行不要として成功扱いにする。

## 実装方針

### 1. 候補探索（副作用なし）

`RelinkCandidateResolver` 相当のサービスを追加し、次の順で候補をスコアリングする。

1. 相対パス・同一ファイル名
2. basename + 拡張子
3. 連番パターン（prefix / suffix / padding / frame range）
4. ファイルサイズ・更新時刻
5. 必要な場合のみハッシュ／フィンガープリント

候補が複数ある場合は自動確定せず、候補とスコアをUIに提示する。

### 2. 一括反映

候補確定後、次の変更を1つの複合Undoコマンドで処理する。

- FootageItem の `filePath`
- 連番の `sequencePaths`
- 影響する全コンポジション／プリコンポジションのレイヤーsourcePath
- AssetDatabase の旧パス→新パス移行（API追加済み、ID維持・衝突拒否・逆順ロールバック、実行検証待ち）

再リンク処理の途中で一部だけ成功した状態を残さない。候補不成立・連番欠落・対象参照消失時は反映前に失敗させる。

### 3. UI

- Missing assets の一覧から複数選択
- 「候補を検索」
- 候補、スコア／理由、対象参照数を表示
- ユーザー確定後に一括適用
- 既存の単一再リンク導線は維持

## 受け入れ条件

- 同一basenameの候補が1件なら候補として表示される。
- 曖昧な候補は自動適用されない。
- 連番は全フレームの存在確認後に適用される。
- FootageItemとレイヤーsourcePathが同じUndo/Redoで一貫して戻る。
- プリコンポジションを含む参照でも更新漏れがない。
- 既存の単一再リンクと旧形式プロジェクトの読み込みを壊さない。
- Windowsの大小文字差、canonical path差、同一フレーム混在を含む再リンクで、対象検索とAsset ID移行の判定が一致する。

## 残課題

- AssetDatabaseの移行APIを実データで実行確認する。

## 非対象

- 動画デコードやプロキシ生成
- ハッシュ計算の常時実行
- ユーザー確認なしの全自動再リンク
