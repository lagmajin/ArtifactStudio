# MILESTONE: AE Parity Backlog - 2026-04-29

ArtifactStudio を After Effects 風アプリとして見たときの、実装優先順 backlog。
この文書は「機能一覧」ではなく、「今の状態から何を先に埋めると AE らしくなるか」を並べたもの。

---

## 前提

- すでに UI の骨格、レイヤー基盤、タイムライン、GPU ルートはかなりある
- なので不足は「見た目の派手さ」より「制作体験の核」に寄っている
- まずは壊れやすい再生 / 合成 / text を固めるのが近道

---

## 今すぐ埋める順

### 1. Preview / Cache / Playback Stability
- 軽いシーンでも引っかからない再生
- フレームキャッシュの命中率を上げる
- プレビュー開始 / 停止 / シークの体感を安定させる
- 既存の `playhead` / `work area` / preview pipeline の揺れを潰す

### 2. Mask / Blend / Track Matte / Compositing Correctness
- mask と layer blend の組み合わせで破綻しないこと
- track matte を AE っぽく扱えること
- alpha / premultiplied alpha / matte order の一貫性
- 見た目の「単一色塗りつぶし」系バグを先に潰す

### 3. Text Animator / Typography / CJK Shaping
- `range selector` の本格化
- per-character / per-word / per-line の分離
- 日本語 shaping と fallback font の安定
- box text / point text の明確化
- AE っぽさを最も感じやすい領域の一つ

### 4. Keyframe Interpolation / Graph Editor
- speed graph / value graph
- easy ease / bezier handle
- roving / hold / tangent 編集
- 線形補間だけの状態から抜ける

### 5. Motion Blur / Time Remap
- shutter angle / phase / sample count
- frame blending
- time remap の見え方
- アニメーションの品質を底上げする

### 6. Parent / Child / Precompose
- 親子伝搬
- プリコンポーズ
- parenting と transform の一貫性
- 大きめの制作に入るための前提

### 7. Adjustment Layer / Layer Styles
- adjustment layer
- drop shadow / glow / stroke
- 合成の実務感を上げる

### 8. Work Area / Ruler / Playhead Visual Consistency
- work area と ruler の見た目統一
- playhead の一貫した描画
- timeline 周辺の widget 境界の見え方を揃える
- 操作感の違和感を減らす

### 9. Expression Engine v1
- プロパティ参照
- 基本 math 関数
- まずは安全な subset で入れる

### 10. Color Management / Gamma / LUT
- preview と export の見え方一致
- 色空間の明示
- 制作物の信用を上げる

### 11. Proxy / Render Queue / Export Robustness
- proxy 導線
- render queue の安定
- 書き出し失敗時の戻し方

### 12. Interop / Plugin / Advanced Expansion
- OFX / plugin boundary
- project interchange
- 3D material / advanced effect / collaboration
- 最後に広げる層

---

## 先に直すべき順

1. Preview / Cache / Playback Stability
2. Mask / Blend / Track Matte / Compositing Correctness
3. Text Animator / Typography / CJK Shaping
4. Keyframe Interpolation / Graph Editor
5. Motion Blur / Time Remap

---

## この順にした理由

- AE っぽさは、派手な機能より「再生が信用できる」「合成が破綻しない」「テキストが崩れない」で決まる
- 逆にここが不安定だと、他の機能を足しても制作ソフトとしての印象が弱い
- 今の ArtifactStudio は土台が強いので、最優先は土台の信用度を上げること

---

## 参照

- [`docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md)
- [`docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md)
- [`docs/planned/MILESTONE_AE1_0_PRIORITY_AND_ROADMAP_2026-04-20.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AE1_0_PRIORITY_AND_ROADMAP_2026-04-20.md)
- [`docs/planned/TEXT_LAYER_ROUTE_2026-04-29.md`](X:/Dev/ArtifactStudio/docs/planned/TEXT_LAYER_ROUTE_2026-04-29.md)
