# Milestone: バッチレンダリング (2026-03-28)

**最終更新:** 2026-08-15
**Status:** M1〜M2 と実行導線は実装済み。M3 の専用 UI と、複数 Composition を対象にした runtime 受入れは未確認。
**Goal:** 複数コンポジションを一括でレンダーキューに追加し、一括レンダリングを実行する。

## 2026-08-15 現行コード監査

`RenderFarmMaster`／`RenderFarmWorker` は frame range の分割、local／remote slice、依存関係、retry／backoff、履歴、進捗、cancel、output versioning を実装している。Farm 設定と RPC／worker process の導線も存在するため、旧文書の「バッチレンダリング未着手」は現状と一致しない。

一方、`ArtifactBatchRenderer` 相当の addAllCompositions／addCompositions、BatchTemplate の JSON 管理、複数 Composition への preset 一括適用、Render Queue UI の一括追加導線は、この監査では確認できなかった。Farm の frame 分割と Composition キューの一括編成は別レイヤーとして扱う。

判定: **レンダー実行基盤は実装済み、Composition batch enqueue／template／UI は pending。runtime の farm end-to-end 検証は未実施。**

## Update 2026-08-15

`ArtifactBatchRenderer` の実装を再確認した。全 Composition／指定 Composition の追加、出力パターン、動画・音声・テンプレート系の一括追加、preset／range／padding／output settings の適用まで API と JSON の基盤がある。

- 旧本文の M1／M2 の実装手順は現行コードでは完了済みとして扱う。
- 専用 Batch UI、template preview、複数 Composition を実プロジェクトで投入して完走する runtime 受入、farm end-to-end は未確認。
- 判定は **batch enqueue／template 基盤は実装済み、ユーザー向け M3 UI と runtime 受入は未完了** を維持する。

---

## 現状

| 機能 | 状態 | 場所 |
|------|------|------|
| レンダーキューへの個別ジョブ追加 | ✅ 完成 | `ArtifactRenderQueueService.cppm` |
| 一括ジョブ追加 | ✅ 実装済み | `ArtifactBatchRenderer.cppm` |
| プロジェクト全コンポジション追加 | ✅ API 実装済み | `ArtifactBatchRenderer.cppm` |
| テンプレートプリセット一括適用 | ✅ API／JSON 実装済み | `ArtifactBatchRenderer.cppm` |
| レンダーキューの永続化 | ✅ 完成 | `toJson` / `fromJson` |
| キューシリアライズ/デシリアライズ | ✅ 完成 | — |

## 2026-08-15 判定補足

`ArtifactBatchRenderer.cppm` には、全 Composition／指定 Composition のキュー追加、出力パス生成、動画・音声・テンプレート系の一括追加、preset／frame range／frame padding／output settings の適用がある。したがって、本文の M1・M2 を未実装として扱うのは現行コードと一致しない。

ただし、専用の Batch Template ダイアログやメニュー導線、テンプレートの UI プレビュー、全 Composition を実プロジェクトで追加してから完走させる runtime 検証は、この静的監査では確認できない。`ArtifactBatchRenderer` の API 実装と、ユーザー向け M3 UI／受入れを分けて管理する。

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

## Update 2026-08-15 — M-RD-8 統合先監査

M-RD-8 は本書へ統合済み。`ArtifactBatchRenderer` の API／JSON、Render Queue の Add All／Batch Template UI、preset／output directory 適用まで現行コードで確認できるため、追加実装は行わない。専用テンプレート preview、複数 Composition の runtime 完走、farm end-to-end 受入れは未検証として残す。
