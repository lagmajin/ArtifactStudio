# Tracker panel design

**最終更新:** 2026-09-21

## 採用モック

- [Tracker panel](tracker-panel-concept-2026-09-21.png)

トラッキングのセッション管理、モード選択、解析、品質確認、結果適用を
独立した `Tracker` Dock に集約する。Composition Viewport は追跡点／平面領域の
表示と直接編集だけを担当し、解析状態や適用操作を所有しない。

## 実装方針

- `Tracker` は `Inspector` と同じ右側 Dock グループに置き、分離・移動・閉じる操作は
  共通 Dock 機構へ委譲する。
- Point／Planar の切り替え、解析方向、停止、問題フレーム確認、平滑化、外れ値処理、
  Position／Anchor／Null／Corner Pin への適用は既存 controller API を再利用する。
- Composition Viewer 上部ツールバーへ Tracker 専用のモード／解析ボタンを置かない。
- モックにある Source／Reference Frame／Motion Model／Destination の詳細編集は、
  対応する永続化契約が確定するまで表示だけを先行させず、既存 API で成立する操作を優先する。
- 新規 Qt stylesheet、`QColorDialog`、シグナル／スロット経路は追加しない。

画像は UI の情報階層と密度の参照であり、表示値やセッション名はダミーである。
