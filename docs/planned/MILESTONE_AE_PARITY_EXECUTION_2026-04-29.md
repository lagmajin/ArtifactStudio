# MILESTONE: AE Parity Execution Plan - 2026-04-29

`MILESTONE_AE_PARITY_BACKLOG_2026-04-29.md` を実行用に短く切った版。
ここでは「何を先にやるか」だけを固定する。

---

## 今週やる

### 1. Preview / Cache / Playback Stability
- 軽いシーンでも引っかかる再生の解消
- フレームキャッシュ命中率の改善
- シーク / 停止 / 再開の体感を安定化
- `WorkAreaControl` の playhead 位置を他の timeline widget と同じ frame 基準へ揃える
- `TrackPainterView` / `ScrubBar` / wheel seek の上限を `duration-1` に統一する

### 2. Mask / Blend / Track Matte / Compositing Correctness
- mask と blend の同時利用で塗りつぶしになる系の崩れを潰す
- track matte の順序と alpha 取り扱いを揃える
- premultiplied alpha の破綻を止める

### 3. Text Animator / Typography / CJK Shaping
- `range selector` の責務を固める
- `Point` / `Box` の UI と内部モードを一致させる
- 日本語 shaping と animated text のズレを減らす

---

## 来月やる

### 4. Keyframe Interpolation / Graph Editor
- value graph / speed graph
- easy ease / bezier handle
- hold / roving / tangent の整理

### 5. Motion Blur / Time Remap
- shutter angle / phase / sample count
- frame blending
- time remap の見え方と安定性

### 6. Parent / Child / Precompose
- 親子伝搬
- プリコンポーズ
- transform 継承の安定化

### 7. Adjustment Layer / Layer Styles
- adjustment layer
- drop shadow / glow / stroke
- 合成の実務感を上げる

---

## 後回し

### 8. Work Area / Ruler / Playhead Visual Consistency
- timeline 周辺の見た目統一
- playhead / ruler / work area の整合

### 9. Expression Engine v1
- 安全な subset での式評価
- プロパティ参照

### 10. Color Management / Gamma / LUT
- preview と export の見え方一致
- 色空間の明示

### 11. Proxy / Render Queue / Export Robustness
- proxy 導線
- render queue の安定
- 書き出し失敗時の戻し方

### 12. Interop / Plugin / Advanced Expansion
- OFX / plugin boundary
- project interchange
- 3D material / collaboration / advanced effect

---

## 判断基準

- まず「再生が信用できる」こと
- 次に「合成が破綻しない」こと
- その次に「text が崩れない」こと
- 見た目の追加より先に、制作の土台を固める

---

## 参照

- [`docs/planned/MILESTONE_AE_PARITY_BACKLOG_2026-04-29.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AE_PARITY_BACKLOG_2026-04-29.md)
- [`docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-04-08.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-04-08.md)
- [`docs/planned/MILESTONE_AE1_0_PRIORITY_AND_ROADMAP_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AE1_0_PRIORITY_AND_ROADMAP_2026-04-20.md)
