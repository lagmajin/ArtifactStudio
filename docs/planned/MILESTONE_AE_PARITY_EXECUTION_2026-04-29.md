> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md](MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md)

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
- [`docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md)
- [`docs/planned/MILESTONE_AE1_0_PRIORITY_AND_ROADMAP_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AE1_0_PRIORITY_AND_ROADMAP_2026-04-20.md)

---

## Static audit follow-up (2026-07-25)

実行計画の各項目を現行ソースと照合した。ビルド・テスト・実機操作は未実施。

| 順位 | 確認結果 | 残課題 / 判定 |
|---:|---|---|
| 1 | `ArtifactPlaybackService`/`ArtifactPlaybackEngine`、`ArtifactFrameCache`、GPU cache、composition playback の基盤は存在する。 | WorkAreaControl/TrackPainterView/ScrubBar の frame 上限統一と、requested/ready/failed の実測は未確認。部分実装。 |
| 2 | `MaskPath`/`LayerMask`、Roto/PathMorph、MaskCutout/Rasterizer、LayerBlend の基盤は存在する。 | mask+blend の塗りつぶし、matte order、premultiplied alpha の実行経路確認が必要。部分実装。 |
| 3 | `TextAnimator`、shaping、GlyphLayout/Atlas、`ArtifactTextLayer` と text property editor が存在する。 | range selector、Point/Box の内部・UI一致、CJK animated text の実機確認が必要。部分実装。 |
| 4 | keyframe editing/easing、timeline model、curve editor が存在する。 | value/speed graph の完全な切替、Bezier handle、hold/roving/tangent の確認待ち。部分実装。 |
| 5 | `ArtifactMotionBlur`、Core MotionBlur、`TimeRemapProcessor`、`FrameBlendEffect` が存在する。 | shutter/phase/sample policy と preview/render 一致を確認する必要がある。部分実装。 |
| 6 | `PreCompose` と親子・transform 関連の基盤が存在する。 | nested precompose と transform 継承の実行確認待ち。部分実装。 |
| 7 | Adjustment layer テストと複数の effect 実装が存在する。 | Drop Shadow/Glow/Stroke を含む layer-style の統合導線は未確認。部分実装。 |
| 8 | Playback と work-area の周辺基盤は存在する。 | ruler/playhead/work-area の widget 間の見た目・frame 契約は未確認。後段。 |
| 9 | Expression parser/evaluator/value の基盤が存在する。 | 安全 subset、property reference、debug/inspection の実行確認待ち。基盤あり。 |
| 10 | OCIO/Color management、LUT、grading の基盤が存在する。 | preview/export の見え方一致と gamma/LUT の出力確認待ち。部分実装。 |
| 11 | RenderQueue service/widget/preset、ProxyService が存在する。 | proxy導線、失敗時復旧、実ファイル export の E2E は未確認。部分実装。 |
| 12 | import/export、3D、collaboration 等の周辺実装は存在する。 | OFX/plugin boundary と高度な interop は未完了。後回し。 |

### 次の実装・確認単位

まず 1 の playback/cache 状態契約と frame 境界を確認し、次に 2 の合成順・alpha 契約、3 の Text Animator 編集導線へ進む。この順序を維持する。
