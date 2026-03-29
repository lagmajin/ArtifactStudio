# Milestone: バッチレンダリング (2026-03-28)

**Status:** Not Started
**Goal:** 複数コンポジションを一括でレンダーキューに追加し、一括レンダリングを実行する。

---

## 現状

| 機能 | 状態 | 場所 |
|------|------|------|
| レンダーキューへの個別ジョブ追加 | ✅ 完成 | `ArtifactRenderQueueService.cppm` |
| 一括ジョブ追加 | ❌ 未実装 | — |
| プロジェクト全コンポジション追加 | ❌ 未実装 | — |
| テンプレートプリセット一括適用 | ❌ 未実装 | — |
| レンダーキューの永続化 | ✅ 完成 | `toJson` / `fromJson` |
| キューシリアライズ/デシリアライズ | ✅ 完成 | — |

---

## Architecture

```
ArtifactBatchRenderer (新規サービス)
  ├── addAllCompositions()          ← プロジェクト全コンポを追加
  ├── addCompositions(ids)          ← 選択コンポを追加
  ├── applyPresetToAll(presetId)    ← 全ジョブにプリセット適用
  ├── applyTemplate(template)       ← テンプレート適用
  └── execute()                     ← 一括レンダリング開始

BatchTemplate:
  ├── outputDirectory
  ├── fileNamePattern  ("%compName%_%frame%")
  ├── codec / preset / resolution
  └── startFrame / endFrame (or "comp defaults")
```

---

## Milestone 1: 一括ジョブ追加

### Implementation

1. `addAllCompositions()` メソッド:
   - プロジェクトの全コンポジションを取得
   - 各コンポジションをレンダーキューに追加
   - デフォルト設定 (1920x1080, 30fps, H.264, プロジェクトフォルダ出力)

2. `addCompositions(compIdList)` メソッド:
   - 選択されたコンポジションを追加

3. ファイル名パターン:
   - `%compName%` — コンポジション名
   - `%date%` — 日付 (YYYYMMDD)
   - `%time%` — 時刻 (HHMMSS)
   - `%frame%` — フレーム番号 (画像シーケンス用)

### 見積: 4h

---

## Milestone 2: バッチテンプレート

### Implementation

1. `BatchTemplate` 構造体:
   - 出力ディレクトリ
   - ファイル名パターン
   - コーデック/プロファイル/プリセット
   - 解像度/FPS
   - フレーム範囲 ("comp" = コンポ既定 or 指定値)

2. テンプレート保存/読込:
   - JSON ファイルとして保存
   - プリセットディレクトリに配置

3. テンプレートプリセット:
   - "YouTube 1080p" — H.264, 1920x1080, 30fps, slow, CRF 18
   - "YouTube 4K" — H.264, 3840x2160, 30fps, slow, CRF 18
   - "ProRes 422 HQ" — ProRes, プロジェクト解像度, 422 HQ
   - "PNG Sequence" — PNG, プロジェクト解像度
   - "Web/WebM" — VP9, 1920x1080, 30fps

### 見積: 3h

---

## Milestone 3: UI

### Implementation

1. レンダーキューメニューに項目追加:
   - "Add All Compositions to Queue"
   - "Add Selected Compositions to Queue"
   - "Apply Batch Template..."

2. バッチテンプレートダイアログ:
   - テンプレート選択コンボボックス
   - 出力ディレクトリブラウザ
   - プレビュー (追加されるジョブ一覧)
   - 適用ボタン

3. レンダーキューマネージャーウィジェット:
   - "Batch Add" ボタン追加

### 見積: 3h

---

## Recommended Order

| 順序 | マイルストーン | 見積 |
|---|---|---|
| 1 | **M1 一括ジョブ追加** | 4h |
| 2 | **M2 テンプレート** | 3h |
| 3 | **M3 UI** | 3h |

**総見積: ~10h**
