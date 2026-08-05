> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md](MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md)

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

---

## Static audit follow-up (2026-07-25)

現行ソースを照合した時点の状態。ビルド・実機再生による確認はまだ行っていない。

| 順位 | 領域 | 現状 | 判定 |
|---:|---|---|---|
| 1 | Preview / Cache / Playback | `MediaPlaybackController` に再生・停止・シーク・ループ・FFmpeg/Media Foundation のバックエンドがある。階層キャッシュ／レンダーキャッシュの基盤もあるが、命中率・シーク復帰・フレーム落ちの実測と統合確認が不足。 | 部分実装／実行確認待ち |
| 2 | Mask / Blend / Track Matte | `LayerBlend`、GPU LayerBlend、MaskCutout、Track Matte のコア設計は存在する。matte 順序、premultiplied alpha、複数マスクの合成が実際のレンダー経路で一貫するかは未確認。 | 部分実装／正確性確認待ち |
| 3 | Text / Typography / CJK | Text Animator、shaping、GlyphAtlas、RTL の基盤はある。TextLayer への全 animator 伝搬、CJK fallback、box/point text の実運用は未確認。 | 部分実装／実行確認待ち |
| 4 | Keyframe / Graph Editor | Bezier 補間、curve editor、hold、roving の UI・翻訳・基礎処理は確認できる。speed/value graph の完全な編集、接線・roving の保存と再生整合性は未確認。 | 部分実装／実行確認待ち |
| 5 | Motion Blur / Time Remap | `TimeRemapProcessor` と frame blending／motion blur の処理がある。shutter angle/phase/sample count が統合レンダーで期待どおり効くかは未確認。 | 部分実装／統合確認待ち |
| 6 | Parent / Precompose | 親子階層と PreCompose の基盤は存在する。ネスト、transform 伝搬、循環・解除時の状態保持は未確認。 | 部分実装／実行確認待ち |
| 7 | Adjustment / Layer Styles | effects と adjustment 系の既存基盤はあるが、AE 相当の adjustment layer と drop shadow/glow/stroke の一貫した導線はこの監査では確定できない。 | 要追加監査 |
| 8 | Work Area / Ruler / Playhead | タイムラインの playhead/work area 操作は既存だが、各 widget の描画・状態同期の統一は未確認。 | 部分実装／UI確認待ち |
| 9 | Expression Engine | `Script.Expression` の value/parser/evaluator と利用例は存在する。プロパティ参照、math subset、編集 UI、評価の安全境界は未確認。 | 基盤あり／v1未完了 |
| 10 | Color Management / LUT | ColorGrading の LUT と `Color.OCIOConfig`（ACES/sRGB/Rec.709/Rec.2020）が存在する。preview/export の同一変換と実出力検証は未確認。 | 部分実装／出力確認待ち |
| 11 | Proxy / Render Queue | Render Queue の UI/service 基盤とメディア再生の複数 backend はある。proxy の編集導線、失敗時の再試行・復旧、実ファイル出力までの検証は未確認。 | 部分実装／実行確認待ち |
| 12 | Interop / Plugin / Expansion | 3D material、collaboration、各種 import/export の基盤は個別に存在するが、OFX/plugin boundary と project interchange の完成契約は確認できない。 | 未完了／要設計 |

### 次に着手する範囲

この backlog の優先順は妥当。次の実装候補は 1〜5 のうち、まず Preview/Cache/Playback の計測契約と、Mask/Blend/Track Matte の合成順テストを固めること。現時点ではソースの存在だけで「完了」とは扱わない。
