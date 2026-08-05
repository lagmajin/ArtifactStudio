> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_CURVE_EDITOR_DCC_IMPROVEMENTS_2026-07-22.md](MILESTONE_CURVE_EDITOR_DCC_IMPROVEMENTS_2026-07-22.md)

# マイルストーン: カーブ / グラフエディタ 機能監査 (2026-07-04)

> AE Graph Editor / Blender Graph Editor / Maya Graph Editor 比較。

## 🔴 P0: 基本編集

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Value Graph 表示** | AE | ✅ |
| **Speed Graph 表示** | AE | ⚠️ 表示とサンプルは接続済み、編集は未対応 |
| **キーフレーム選択/移動/削除** | AE/Blender | ⚠️ |
| **ベジェハンドル編集** | AE/Blender | ⚠️ 部分的 |
| **ハンドルタイプ切替（Free/Aligned/Vector/Auto/AutoClamped）** | Blender | ❌ |
| **キーフレーム値の直接入力** | AE | ❌ |

---

## 🟡 P1: 表示・ナビゲーション

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Proportional Editing（減衰編集）** | Blender | ❌ |
| **Pivot Point 切替** | Blender | ❌ |
| **Auto-Snap（フレーム/秒/マーカー）** | Blender | ❌ |
| **Normalize Curves（0-1 正規化）** | Blender | ❌ |
| **Summary Row（全カーブ合成）** | Blender Dope Sheet | ❌ |
| **Only Selected Keyframes フィルタ** | Blender | ❌ |
| **Channel Grouping** | Blender/Houdini | ❌ |

---

## 🟡 P1: カーブ加工

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Clean Keyframes（閾値間引き）** | Blender | ❌ |
| **Decimate Curves（大幅間引き）** | Blender | ❌ |
| **Smooth Curves（平滑化）** | Blender | ❌ |
| **Bake Curves（ベイク）** | Blender | ❌ |
| **Euler Filter（ジンバルロック回避）** | Blender/Maya | ❌ |
| **Curve Mirror（時間/値反転）** | Blender | ❌ |
| **Easy Ease 一発適用** | AE F9 | ⚠️ |
| **Elastic/Bounce プリセット** | Motion 4 | ❌ |

---

## 実装状況メモ (2026-07-07)

- `ArtifactCurveEditorWidget` 側には `sampleSpeedGraph()` があり、Timeline 側も read-only の Speed Graph 表示に接続されている
- そのため「未接続」よりは、**表示はあるが編集はまだ足りない** と表現する方が現在の実態に近い
- 今後の中心は、Speed Graph の編集導線、ハンドル操作、そして Blender 風の補助機能の段階的追加
