# M-TRACK-REVIEW-1 Tracking Review / Bake Acceptance

**最終更新:** 2026-10-11

**ステータス:** In Progress

## 目的

既存の point / planar / camera tracking Core を新設し直さず、アーティストが
結果を確認、修正、再 solve、明示的に bake できる制作導線として完成させる。

## 現状の根拠

- `MotionTracker` には point flow、planar homography、camera pose、信頼度と失敗情報、
  cancellable job がある。
- 2D Point Tracker は editor 操作、Null / Corner Pin 適用、保存、diagnostics の主要経路が
  静的確認済みである。
- Core は layer を直接変更せず、App bridge が承認済み結果だけを bake する契約を維持する。

## 範囲

1. Track session の raw result、manual correction、accepted / rejected frame を区別して表示する。
2. Point / planar / camera の品質要約と失敗理由を review surface で確認可能にする。
3. 再 solve 時に manual correction を保持し、明示範囲だけを再計算する。
4. Null、Corner Pin、stabilization、camera layer への bake を command 境界で実行する。
5. 未解決・低信頼度フレームを黙って補間して bake しない。

## 非対象

- 新しいグローバル signal / slot。
- UI スレッド上の solve、暗黙 GPU readback、QImage を render hot path へ入れること。
- AI roto、3D scene reconstruction、外部トラッカーの模倣。

## フェーズ

### Phase 1 — 受入れ基準と状態監査

- 現行 Point Tracker の UI / 保存 / Undo / diagnostics を実素材で確認できるチェック表を作る。
- TrackFrame の confidence、failure reason、manual correction、accepted state の consumer を棚卸しする。

### Phase 2 — Review / correction bridge

- 失敗・低信頼度・手修正を明確に区別する review model を App 側に接続する。
- correction から限定 range の再 solve を行い、raw result と correction を分離して保存する。

### Phase 3 — Explicit bake

- bake target と対象範囲を明示する。
- 結果を一つの既存 command / undo 境界で property keyframe へ適用する。
- failure / rejected frame がある場合は診断を出し、ユーザーの明示選択なしに bake しない。

### Phase 4 — Planar / camera acceptance

- planar は homography / corner-pin、camera は calibrated pose / reprojection error を review してから bake する。
- representative footage で forward/backward、occlusion、manual correction、save/reload を確認する。

## 完了条件

- point / planar / camera の失敗状態と品質が bake 前に分かる。
- correction は re-solve と保存／再読込後も失われない。
- bake の Undo / Redo が結果全体を再現する。
- solver、UI、renderer の責務を混在させない。

## 関連

- `docs/technical/MOTION_TRACKING_CORE_PRO_ARCHITECTURE_2026-07-11.md`
- `docs/planned/MILESTONE_2D_POINT_TRACKER_2026-06-16.md`

## Update 2026-10-11

- Tracker ドックの問題フレーム移動に前／次の両方向を追加した。いずれも Playback Service が対象 Composition を所有し、追跡ジョブが停止中の場合のみ有効になる。
- Playbackが停止中またはスクラブ中は、現在フレームに最も近い結果フレーム番号・信頼度を表示し、明示的な失敗フレームと信頼度しきい値未満を区別する。再生中はLive playback表示に切り替え、フレーム更新ごとの品質文字列再構築を避ける。
- problem frameが残る場合、Position／Anchor／全ポイントNull／Corner Pin／Maskの各Bake前に問題数を示し、失敗フレームのキー欠落や低信頼度値の適用可能性を説明する確認を表示する。既定はCancel。
- 品質欄へ固定48点の信頼度推移グラフを追加した。横軸を結果時刻に揃え、欠落区間は線を切り、設定中の信頼度しきい値は破線で示す。再生中／解析中はサンプル収集を止める。
- 現状の品質レビューは問題フレームへの移動と集計値表示まで。raw result／manual correction／accepted-rejected state の区別、限定範囲の再 solve、失敗理由の個別レビュー、選択的 bake は引き続き未実装または未確認。

