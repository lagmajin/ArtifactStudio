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