# MILESTONE: 変数展開エンジン（Houdini クラスへの引き上げ 第1段）

**最終更新:** 2026-08-22

## 目的

任意の文字列パラメータで `$VAR` / `${VAR}` / `$F4`（幅付きフレーム番号）を評価時展開できるようにする。Houdini の「パラメータ欄すべてで変数が効く」コア機能相当を、既存の `EnvironmentVariableManager` と Expression Engine の接続点の上に実装する。

## 展開ルール仕様

| トークン | 意味 | 例 |
|---|---|---|
| `$NAME` | 変数展開（`[A-Za-z_][A-Za-z0-9_]*`） | `$PROJECT_DIR/textures/bg.png` |
| `${NAME}` | 波括弧付き変数展開（境界明確化） | `${TEST_HOME}/img.png` |
| `$F<n>` / `$F` | フレーム番号を幅 n（1-10, 既定 1）で 0 埋め | `seq.$F4.png` → `seq.0007.png` |
| `$$` | リテラル `$` のエスケープ | `$$home` → `$home` |

- 解決順: `ExpansionContext.customResolver` → `EnvironmentVariableManager`
- **未解決変数は元のまま残す**（空化しない。Houdini 挙動）
- `$F` と F で始まる変数名（`$FRAME` 等）の競合は「F 直後が数字 or 終端」のみフレーム扱いすることで回避
- 空文字列に解決された変数は空文字列になる（未解決とは区別）

## 実装内容 (2026-08-22)

### 新規

| ファイル | 内容 |
|---|---|
| `ArtifactCore/include/EnvironmentVariable/TokenExpansion.ixx` | header-only モジュール `EnvironmentVariable.Expansion`。`expandTokens(input, ctx)` / `containsExpansionMarker(input)` / `ExpansionContext{frame,fps,customResolver}`。純関数で自己状態なし |

### 修正

| ファイル | 内容 |
|---|---|
| `ArtifactCore/include|src/EnvironmentVariable/*` | `EnvironmentVariableManager` に `QReadWriteLock` 追加（レンダースレッドからの読み取り安全化）＋ `revision()` 世代カウンタ（setVariable/clear/loadFromSystemEnvironment で increment、将来のキャッシュ無効化契機） |
| `ArtifactCore/src/Property/AbstractProperty.cppm` | `evaluateValue` の式成功 return と通常値 return で、String 型＋マーカー含有時に `expandTokens` を通す。ガードは `$` 包含チェックのみ（ホットパス影響ゼロ）。外部オーバーライドは展開しない |
| `Artifact/src/Layer/ArtifactImageLayer.cppm` | `loadFromPath` 入口で展開。**メンバにはテンプレートを保持**しファイル IO のみ展開結果（保存 JSON もテンプレート維持） |
| `Artifact/src/Layer/ArtifactAudioLayer.cppm` | 同上 |
| `Artifact/src/Layer/Artifact3DModelLayer.cppm` | 同上（`loadFromFile`） |
| `Artifact/src/Project/ArtifactProjectImporter.cppm` | プロジェクトオープン時に `$PROJECT_DIR` / `$PROJECT_NAME` / `$PROJECT_FILE` を env マネージャへ登録 |
| `tests/ArtifactCore/CMakeLists.txt` | `ArtifactCoreTokenExpansionTest` 登録 |

### テスト

`tests/ArtifactCore/TokenExpansionTest.cpp`: マーカーなし高速パス / `$NAME` / `${NAME}` / 未解決パススルー / `$$` エスケープ / $F 幅指定 / `$FRAME` 変数との非競合 / customResolver 優先とフォールバック / 空文字列値 / 複合トークン / 終端 `$` / 未閉波括弧。

## 設計判断

- **評価時展開**: 文字列プロパティの評価ごとに毎回展開し、保存値はテンプレートのまま。式エンジン（数式）とトークン展開（文字列前置）は住み分け
- **メンバ正本問題への対応**: パス系レイヤーは QString メンバが正本のため Property 側だけでは届かない → loadFromPath 入口でも展開し、テンプレート保持と IO 展開を分離
- **header-only**: 新規 .cppm の CMake force list 登録を回避するためユーザーの好む単一 .ixx 形式

## 未検証事項（ビルド検証待ち）

J:\dev\ArtifactStudio には有効な build dir がなく（X:\ 別コピーに紐づく問題は Insight.md 参照）、今回もビルド検証は未実施:

1. TokenExpansionTest の実行
2. ImageLayer で `$PROJECT_DIR/textures/bg.png` がプロジェクトオープン後に読めること
3. 連番パス `seq.$F4.png` のフレーム追従 — **現状 loadFromPath 経路は frame=0 固定のため連番追従は未達成**（将来課題: シーケンス再生経路への frame 注入）
4. 既存式プロパティ（wiggle 等）の非影響確認
5. OS 環境変数（USERNAME 等）の文字列プロパティ展開

## スコープ外（次段候補）

- houdini.env 相当の起動時 env ファイル読み込み（packages/*.json 相当も含む）
- 変数一覧ダイアログ＋入力補完 UI
- ArtifactPr の SourceRef.uri 相対パス対応
- 出力パス欄・ExportDialog への展開適用
- 連番シーケンスの $F フレーム追従
