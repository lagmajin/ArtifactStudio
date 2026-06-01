# After Effects Parity P0 Checklist

> 2026-05-31

## Preview / Cache / Playback

- [ ] `requested / ready / failed` が同じ state contract で扱われている
- [ ] cache hit と final image readiness が分離されている
- [ ] playback / scrub / diagnostics が同じ状態を見る
- [ ] fallback policy が UI 側の見え方だけで隠されていない

## Track Matte / Alpha / Blend

- [ ] track matte の評価順が文書化されている
- [ ] alpha / premultiplied alpha の境界が明確
- [ ] mask / blend の組み合わせで破綻しない
- [ ] blend mode の不足が理由付きで追える

## Viewport Contract

- [ ] `Fill` が `fit` か `cover` かを明示できる
- [ ] `100%` が logical pixel か physical pixel かを明示できる
- [ ] high DPI で zoom/pan の基準がぶれない
- [ ] resize debounce と initial fit の競合が説明できる

## P0 Exit Criteria

- [ ] 画面に出ている状態と内部状態が食い違わない
- [ ] compositing の結果を再現可能に説明できる
- [ ] `Fill / 100%` の見え方を仕様として語れる
- [ ] P0 の不具合を「壊れた場所」まで辿れる

## Notes

- これは実装メモであって、完了判定表ではない
- 実装の優先順位は [AFTER_EFFECTS_PARITY_P0_STARTER_2026-05-31.md](AFTER_EFFECTS_PARITY_P0_STARTER_2026-05-31.md) と [AFTER_EFFECTS_PARITY_EXECUTION_ROADMAP_2026-05-31.md](AFTER_EFFECTS_PARITY_EXECUTION_ROADMAP_2026-05-31.md) を基準にする
- 詳細比較は [AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md](AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md)

