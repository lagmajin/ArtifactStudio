# After Effects Parity Execution Roadmap

> 2026-05-31

## Goal

After Effects parity を、`機能追加の羅列` ではなく `制作ソフトとして破綻しない順序` で前に進めるための実行ロードマップ。

## Working Principle

- 先に `state contract` を固める
- 次に `compositing correctness` を固める
- その後で `editor UX` を揃える
- 最後に `workflow / ecosystem` を伸ばす

## Phase 0 - Make The Core Truths Explicit

最初にやることは、画面の見せ方ではなく、内部の真実を 1 本化すること。

- preview / cache / playback が同じ state contract を読むようにする
- `requested / ready / failed` を曖昧に混ぜない
- track matte / alpha / blend の評価順を文書化する
- `Fill` / `100%` の座標系を `fit / cover` と `logical / physical` に分けて扱う

## Phase 1 - Fix Compositing And Stability First

制作中に壊れると信用が落ちる領域を先に固める。

- RAM preview / cache の状態遷移を安定化する
- track matte / alpha compositing の正確性を詰める
- blend mode coverage の穴を埋めるか、少なくとも不足理由を明示する
- playback / scrub / diagnostics が同じ情報を見るようにする

## Phase 2 - Make The Editor Feel Coherent

ユーザーが「使える」と感じるのは、細部の操作感が揃ったとき。

- graph editor / interpolation を timeline の自然な拡張にする
- text animator の UX を timeline と編集文法の両方で整える
- motion blur の UI 配線と quality policy を揃える
- parent / transform propagation の連鎖を明快にする

## Phase 3 - Close Standard Pro Gaps

標準的なプロ機能を、単体機能ではなく workflow として埋める。

- markers を導入する
- shape operators を補完する
- precompose workflow を実務の流れに合わせる
- layer styles を追加する
- time remap / frame blend を整理する
- expression engine の AE 互換 gap を縮める

## Phase 4 - Long-Tail Ecosystem

業務導線や拡張性は最後にまとめて伸ばす。

- effects をカテゴリ単位で増やす
- OCIO / ACES の pipeline を整える
- plugin SDK / OFX / AEX compatibility の方針を固める
- mogrt-like templates の必要性を判断する
- Python API coverage を production use まで広げる

## Practical Milestones

1. `Preview / Cache / Playback` の state contract を確定する
2. `Track Matte / Alpha / Blend` の順序と例外を見える化する
3. `Fill / 100%` の UI contract を fit / cover と分離する
4. `Graph Editor / Text Animator` を timeline UX に接続する
5. `Precompose / Parent / Motion Blur` を workflow として揃える

## What To Avoid

- 機能数だけを増やして「AE っぽくなった」と誤認すること
- UI 表示の名前と実際の座標系を混同すること
- `feature missing` と `workflow incohesion` を同じ箱で扱うこと

## Done Criteria

このロードマップが進んだと言えるのは、以下が揃ったとき。

- preview / cache / playback の契約が一貫している
- compositing の評価結果が予測可能
- editor UX が媒体サイズや DPI に依存して崩れにくい
- timeline / text / transform / motion blur の関係が見通しよくなっている

## Reference

- 総括: [AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md](AFTER_EFFECTS_PARITY_MASTER_SUMMARY_2026-05-30.md)
- 入口図: [AFTER_EFFECTS_PARITY_DOCUMENT_MAP_2026-05-30.md](AFTER_EFFECTS_PARITY_DOCUMENT_MAP_2026-05-30.md)
- 再開点: [AFTER_EFFECTS_PARITY_HANDOFF_2026-05-30.md](AFTER_EFFECTS_PARITY_HANDOFF_2026-05-30.md)
- チェック表: [AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md](AFTER_EFFECTS_PARITY_CHECKLIST_2026-05-30.md)
- 未解決論点: [AFTER_EFFECTS_PARITY_OPEN_QUESTIONS_2026-05-30.md](AFTER_EFFECTS_PARITY_OPEN_QUESTIONS_2026-05-30.md)
