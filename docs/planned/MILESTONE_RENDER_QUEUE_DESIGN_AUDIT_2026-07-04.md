> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_RENDER_QUEUE_2026-03-22.md](MILESTONE_RENDER_QUEUE_2026-03-22.md)

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

## Static Audit (2026-07-25)

この監査表の状態は現行ソースに対して古くなっている。`ArtifactRenderQueueService` では remove、duplicate、move、start/pause/cancel、progress、error、rerun、failed-frame detection / selected rerender、preflight の API が確認でき、queue manager と job panel/preset UI も存在する。Render Farm、checkpoint、retry、progress aggregator は別基盤として追加され、P0/P1 の基本操作は少なくとも static surface 上で大きく進展している。

一方、表に残る未実装項目のうち、watch folder、post-render action、複数 output module、filename template、LUT/slate/burn-in、出力ファイル自動検証はこの監査では確認できない。進捗・失敗再試行・background rendering も API と実行経路は確認できるが、UI の長時間 runtime、再起動後の履歴、実成果物検査までは未検証である。したがって元表の ❌ をそのまま現在状態とみなさず、基本 queue 操作は実装済み、拡張 delivery 機能と runtime acceptance は未完了として扱う。
