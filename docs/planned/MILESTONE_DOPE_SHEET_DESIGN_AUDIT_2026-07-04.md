# マイルストーン: Dope Sheet 機能監査 (2026-07-04)

**最終更新:** 2026-08-15

## 2026-08-15 現行コード照合

- `ArtifactDopeSheetWidget` は `AppMain` から遅延生成され、選択レイヤーの `collectDopeSheetKeyframesForLayer()` 結果を一覧表示する基盤として存在する。
- ただし現行 widget の選択モードは `NoSelection` で、Dope Sheet 自身の keyframe 移動／削除／時間方向スケール／Summary Row／channel grouping／regex filter／Frame-Seconds 切替は確認できない。
- キーフレームの copy/paste、補間・roving・Undo は Timeline／Curve Editor 側に実装があるが、Dope Sheet の編集面へ統合された証拠はない。
- 判定は **一覧表示のみ部分実装、編集 Dope Sheet は未完了**。この文書の P0/P1 不足表は現状にも概ね適用できるが、Timeline／Curve Editor の既存機能を Dope Sheet 未実装と混同しない。

ビルド・テスト・実機 UI 確認は未実施。

> Blender Dope Sheet / Maya Dope Sheet / AE Timeline Keyframe View 比較。

## 🔴 P0

| 機能 | 参照元 | 状態 |
|---|---|---|
| **全キーフレーム一覧表示** | Blender | ✅ |
| **キーフレーム選択/移動/削除** | Blender | ⚠️ |
| **時間方向スケール（選択キーフレーム伸縮）** | Blender/AE | ❌ |
| **キーフレームコピー＆ペースト** | Blender/AE | ⚠️ |
| **Summary Row（プロパティ単位の全キーフレーム表示）** | Blender | ❌ |

## 🟡 P1

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Channel Grouping（Position/Rotation/Scale グループ）** | Blender | ❌ |
| **Filter: Only Selected Objects** | Blender | ❌ |
| **Filter: Only Visible Channels** | Blender | ❌ |
| **Search Filter with Regex** | Blender | ❌ |
| **Auto-Keying インジケーター** | Blender | ❌ |
| **キーフレーム色分け（補間タイプ別）** | AE | ⚠️ |
| **Frame/Seconds 表示切替** | Blender | ❌ |

---

## Static audit follow-up (2026-07-25)

`ArtifactDopeSheetWidget` と `ArtifactTimelineKeyframeModel` を確認した。現行 surface は選択レイヤーのキーフレームを一覧表示する read-only 寄りの基盤で、Dope Sheet の編集機能一式は未実装である。

| 機能 | 現行確認 | 判定 |
|---|---|---|
| 全キーフレーム一覧 | 選択レイヤーから `collectDopeSheetKeyframesForLayer()` で収集し一覧表示 | 部分実装 |
| 選択 / 移動 / 削除 | widget の selection mode は `NoSelection` で、編集操作は未確認 | 未実装 |
| 時間方向スケール | 専用操作・コマンドは未確認 | 未実装 |
| コピー / ペースト | Dope Sheet surface の導線は未確認 | 未実装 |
| Summary Row | property 単位の全体 summary row は未確認 | 未実装 |
| Channel grouping / filters / regex | 専用 group/filter UI は未確認 | 未実装 |
| Auto-key / interpolation color | 専用 indicator・色分けは未確認 | 未実装 |
| Frame / Seconds 切替 | 現行一覧は frame 値表示で、切替 API は未確認 | 未実装 |

**判定**: P0 の「一覧表示」は最小基盤のみ実装済み。Dope Sheet を編集 surface として完成させるには、まず選択・移動・削除・copy/paste の command 経路を追加し、その後 grouping/filter/表示モードへ進む必要がある。
