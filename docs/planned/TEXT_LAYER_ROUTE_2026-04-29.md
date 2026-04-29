# Text Layer Route Note - 2026-04-29

## 現状

`ArtifactTextLayer` は描画・保存・プロパティ公開・Animator の入口まで持っている。

## いま進めている整理

- `text.layoutMode` を追加し、`Point` と `Box` を明示
- `maxWidth` / `boxHeight` は Box レイアウト時に意味を持つように寄せる
- リサイズ badge でも `Point` / `Box` の違いを見えるようにする
- `Animator` は Core の `TextAnimatorEngine` に合成を寄せる
- 日本語など CJK の静的 text は簡易 GPU 経路を避け、Qt 文書経路へ逃がす
- `GlyphLayout` も CJK では `QTextLayout` ベースの shaping を使い、アニメ付き text の位置ズレを減らす

## まだ大きい課題

- Animator の Core 化
- 日本語 shaping と backend 安定化
- point text / box text の UI 上の見せ方の最終確定

## 次の順番

1. `text.layoutMode` を UI で触りやすくする
2. Animator の selector / range の責務を Core 側へ寄せる
3. 日本語と複数行 text の描画差分を詰める
