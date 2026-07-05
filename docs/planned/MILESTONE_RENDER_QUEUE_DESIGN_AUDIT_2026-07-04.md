# マイルストーン: レンダーキュー 機能監査 (2026-07-04)

> 907行。AE Render Queue / Nuke Write / Resolve Deliver / Media Encoder 比較。

## 監査サマリー

`ArtifactRenderQueueManagerWidget` はレンダーキューの管理 UI。

---

## 🔴 P0: 基本キュー操作

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Add to Queue（現在の comp をキューに追加）** | AE | ✅ |
| **Remove from Queue** | AE | ❌ |
| **Queue の並び替え（ドラッグ）** | AE/Media Encoder | ❌ |
| **Start Queue（一括レンダリング開始）** | AE | ⚠️ |
| **Pause Queue** | AE/Media Encoder | ❌ |
| **Stop Queue** | AE | ❌ |
| **出力先フォルダを開く** | AE | ❌ |

---

## 🟡 P1: 出力設定・プリセット

| 機能 | 参照元 | 状態 |
|---|---|---|
| **出力フォーマット選択（MP4/MOV/PNG/EXR）** | AE | ⚠️ |
| **出力プリセット保存/呼出し** | AE/Media Encoder | ❌ |
| **出力ファイル名テンプレート（[compName]_[date]）** | AE | ❌ |
| **複数出力モジュール（1 comp → 複数フォーマット同時出力）** | AE | ❌ |
| **Post-Render Action（出力後に開く/通知）** | AE | ❌ |

---

## 🟡 P1: ジョブ管理

| 機能 | 参照元 | 状態 |
|---|---|---|
| **ジョブ進捗バー** | AE/Media Encoder | ❌ |
| **残り時間推測表示** | Media Encoder | ❌ |
| **ジョブ成功/失敗ステータス** | AE | ❌ |
| **失敗ジョブの再試行** | Media Encoder | ❌ |
| **失敗ログの詳細表示** | AE/Media Encoder | ❌ |
| **バックグラウンドレンダリング（作業継続可）** | AE/Media Encoder | ❌ |
| **Watch Folder（フォルダ監視で自動キュー追加）** | Media Encoder | ❌ |

---

## 🔵 P2: 高度機能

| 機能 | 参照元 | 状態 |
|---|---|---|
| **レンダーファーム対応** | Nuke/Deadline | ❌ |
| **Checkpoint/Resume** | - | ❌ |
| **出力ファイルの自動検証** | - | ❌ |
| **Slate / Burn-in 設定（TC/日付焼き込み）** | Resolve | ❌ |
| **LUT 埋め込み** | Resolve | ❌ |