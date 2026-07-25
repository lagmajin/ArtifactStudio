# M-MOTION Motion Designer Feature Milestones (2026-06-02)

日付：2026-06-02
目的：モーションデザイナー視点で不足している高度な編集機能を段階的に実装するマイルストーン一覧。

---

## 優先順位

```
Priority 1（制作現場で毎日使う）
  1. M-MOTION-3: Expression loopOut Runtime (8-13h)
  2. M-MOTION-4: Pick-Whip UI (12-19h)
  3. M-MOTION-1: 2D Point Tracker (24-38h)

Priority 2（週に数回使う）
  4. M-MOTION-7: Time Remap Curve UI (8-12h)
  5. M-MOTION-2: Puppet Pin Deformation (28-42h)

Priority 3（差別化）
  6. M-MOTION-6: Auto-Trace / Vectorize (12-18h)
  7. M-MOTION-5: Mesh Warp / Liquify (20-30h)
```

合計見積: 112-172h

---

## 作成したマイルストーンファイル

1. `docs/planned/MILESTONE_2D_POINT_TRACKER_2026-06-02.md`
2. `docs/planned/MILESTONE_PUPPET_PIN_DEFORMATION_2026-06-02.md`
3. `docs/planned/MILESTONE_EXPRESSION_LOOPOUT_RUNTIME_2026-06-02.md`
4. `docs/planned/MILESTONE_PICK_WHIP_UI_2026-06-02.md`
5. `docs/planned/MILESTONE_MESH_WARP_LIQUIFY_2026-06-02.md`
6. `docs/planned/MILESTONE_AUTO_TRACE_2026-06-02.md`
7. `docs/planned/MILESTONE_TIME_REMAP_CURVE_UI_2026-06-02.md`

---

## おすすめ実行順序

1. **M-MOTION-3** (loopOut ランタイム) — 最小工数で最大インパクト。Copilot UI で提案して動かない loopOut が動くようになる
2. **M-MOTION-4** (Pick-Whip UI) — ドラッグでプロパティリンク、Null 活用の基本
3. **M-MOTION-7** (Time Remap カーブUI) — Curve Editor の本格活用
4. **M-MOTION-1** (2D Point Tracker) — もっとも需要の高い追跡機能
5. **M-MOTION-2** (Puppet Pin) — 有機的変形
6. **M-MOTION-6** (Auto-Trace) — ロゴのベクター化
7. **M-MOTION-5** (Mesh Warp/Liquify) — 高度な形状調整

---

## Static audit follow-up (2026-07-25)

この一覧のリンクと現行文書を照合したところ、一覧には現在存在しない `MILESTONE_PUPPET_PIN_DEFORMATION_2026-06-02.md` が残っている。また 2D Point Tracker は `MILESTONE_2D_POINT_TRACKER_2026-06-16.md` が追加されており、元の 2026-06-02 文書と foundation 文書の関係を明示しないと、実行順の参照先が曖昧になる。

個別状況は一覧から推測せず各正本で確認する。今回確認できた範囲では、Expression loopOut と Mesh Warp/Liquify は既に静的監査の追補があり、Time Remap Curve UI も 2026-07-25 の現状確認がある。残りの Pick-Whip、Auto-Trace、Puppet 系は個別文書の実装状況を別途監査する必要がある。

### Index maintenance status

- 優先順位と見積: 旧計画として保持
- 参照リンク: 要整理（存在しない Puppet Pin 文書、旧版 2D Tracker 文書）
- 完了判定: この一覧だけでは行わず、個別マイルストーンの status を正本とする
