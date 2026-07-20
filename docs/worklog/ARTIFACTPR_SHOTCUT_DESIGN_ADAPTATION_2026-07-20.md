# ArtifactPr 編集基盤設計の適用メモ (2026-07-20)

## 対象

外部のプロフェッショナル映像編集ツールで一般的な、次の2つの設計方針だけをArtifactPrへ適用した。

### 1. 構造化スナップショットUndo

ArtifactPrには既に `NLEStateCommand`、`nleSnapshot()`、`restoreNLESnapshot()` があり、編集前後のNLE JSONをUndo/Redoに保存している。今回新しいUndo方式を追加せず、既存経路を正規の方式として扱う。

### 2. Preview / Export共通RenderPlan

`ArtifactPr::RenderPlan` と `EditorEngine::createRenderPlan()` を追加した。

- 編集中のNLE JSON snapshotを固定
- sequenceの解像度・フレームレートを保持
- in/outまたは明示範囲を保持
- `Draft / Preview / Full` の品質プリセットを共有
- Draft / Previewではproxy利用を示す
- Fullではoriginal利用を示す

これにより、PreviewとExportがlive editor stateを直接読むのではなく、同じRenderPlanを入力にできる。今回の変更は実レンダラーやエンコーダーの追加ではなく、両経路を接続するための入力契約に限定している。

## 参照・ライセンス境界

外部リポジトリのソースコードはコピーしていない。一般的な設計方針のみを独自のArtifactPr型へ落とし込んでいる。具体的な参照元とライセンスは `docs/THIRD_PARTY_NOTICES.md` および内部監査資料で管理する。
