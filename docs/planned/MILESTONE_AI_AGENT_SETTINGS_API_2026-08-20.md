# M-AI-SETTINGS-1 AI Agent Settings API

**最終更新:** 2026-08-20
ステータス: In Progress（viewport設定のdescribe・読み取り・validate・patch・preview・restore APIを追加、実機受入は未完了）

## 1. 目的

AIエージェントが自然言語からArtifactStudioのアプリ設定・Viewport設定・プロジェクト設定を安全に変更できる、型付きの設定APIを提供する。

## 2. 既存基盤

- `ArtifactCore::ArtifactAppSettings`
- `Artifact::WorkspaceAutomation`
- `ArtifactProjectService`
- `Artifact::Grid::GridSettings`
- 既存のAIコマンド引数スキーマと`QVariantMap`応答

### 2026-08-20 実装確認

`WorkspaceAutomation::describeViewportSettings()`、`getViewportSettings()`、`validateViewportSettings()`、`patchViewportSettings()`、`restoreViewportSettings()`を追加し、AIがキー・型・範囲を取得したうえで、グリッド表示・ガイド表示・主間隔・分割数・スナップ・Major／Minor／軸／数値表示の読み取り、検証、before／after付きの適用、スナップショット復元を行えるようにした。`previewOnly`指定時は変更せず差分だけ返す。実機受入は未完了。

AIエージェントは`QSettings`を直接操作せず、設定サービスまたは`WorkspaceAutomation`の公開APIだけを使用する。

## 3. 最小API

```text
settings.describe(scope)
settings.get(scope, key)
settings.validate(scope, patch)
settings.patch(scope, patch, previewOnly)
settings.reset(scope, key)
```

### スコープ

- `application`: テーマ、UI、アクセシビリティ、既定値
- `viewport`: グリッド、ガイド、スナップ、ギズモ、オーバーレイ
- `composition`: 背景、表示補助、構図設定
- `project`: プロジェクト固有の設定
- `layer`: Channel Boxやロック状態などの編集設定

## 4. P0要件

- 設定キーの型・範囲・列挙値を記述するスキーマ
- `get`／`describe`による現在値と既定値の取得
- `validate`による変更前検証
- `previewOnly`による差分プレビュー
- 変更結果にbefore／after／warningsを含める
- 設定変更を既存Undoまたは安全なロールバック経路へ接続
- application／project／layerの境界を混同しない

## 5. 例

```json
{
  "scope": "viewport",
  "patch": {
    "grid.visible": true,
    "grid.plane": "XZ",
    "grid.majorInterval": 100,
    "grid.subdivisions": 10,
    "grid.snap": true
  },
  "previewOnly": false
}
```

## 6. 安全策

- 範囲外の数値、未知キー、不正な列挙値を拒否
- ファイルパス、外部実行、GPU設定など高リスク項目は確認必須
- 大量変更は一括パッチの差分を提示
- 変更履歴をAI実行ログへ記録
- 新規signal／slotを追加せず、既存サービスとイベント経路を再利用

## 7. 実装順序

1. `viewport.grid`／`viewport.guides`／`viewport.snap`の読み取りスキーマを追加
2. `patch`のvalidate／preview／applyを追加
3. `ArtifactAppSettings`の既存getter／setterへマッピング
4. WorkspaceAutomationの公開コマンドとして登録
5. Undo／ロールバックとAI実行ログを接続
6. application／project／layerスコープへ拡張

## 8. 完了条件

- 「XZグリッド、間隔100、10分割、スナップON」をAPI経由で適用できる
- 適用前の差分を取得できる
- 不正値を変更前に拒否できる
- 現在値をAIが読み返せる
- 直接`QSettings`操作なしで設定変更が完了する
