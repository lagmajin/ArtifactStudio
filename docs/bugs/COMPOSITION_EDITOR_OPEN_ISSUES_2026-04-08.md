# コンポジションエディタ Open Issues まとめ (2026-04-08)

コンポジションエディタ周辺で、いま挙がっている不具合を 3 件に整理したメモ。

対象の主戦場は `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` と
`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。

---

## Issue 1: コンポジションエディタの背景色が変わらない

### 症状

コンポジションの background color を変更しても、エディタ上の見た目が更新されない。

### 観点

- 設定値の変更が render controller に伝わっていない
- 既に描いた背景が再描画されていない
- background color が state として更新されているが、描画パスに反映されていない

### 優先度

- 中

### 関連候補

- `ArtifactCompositionRenderController.cppm`
- `ArtifactCompositionEditor.cppm`
- `docs/bugs/COMPOSITION_FILL_NOT_VISIBLE_2026-04-05.md`
- `docs/bugs/COMPOSITION_EDITOR_RENDER_PIPELINE_BUGS_2026-04-04.md`

### 備考

Issue 3 と描画経路が重なる可能性がある。  
ただしこちらは「色が変わるべきタイミングで state が更新されていない」側を疑う。

---

## Issue 2: コンポジションエディタの初期化が重い

### 症状

コンポジションエディタを開くときの初期化が重く、表示開始まで時間がかかる。

### 観点

- controller 初期化時に重い setup が走っている
- renderer / pipeline / viewport / preview cache の初期化がまとまっている
- show 時に必要以上の再構築が入っている

### 優先度

- 高

### 関連候補

- `ArtifactCompositionEditor.cppm`
- `ArtifactCompositionRenderController.cppm`
- `docs/bugs/COMPOSITION_EDITOR_PERF_AND_COMPUTE_HYPOTHESES_2026-03-24.md`
- `docs/bugs/MULTI_VIEW_RENDER_CONCURRENCY_HYPOTHESES_2026-03-24.md`
- `docs/technical/RENDER_SYSTEM_AUDIT_2026-03-27.md`

### 備考

この issue は「開くのが遅い」体感に直結する。  
背景色や描画の不具合よりも、まず操作開始までの待ちを短くしたいケースで優先度が上がる。

---

## Issue 3: コンポジットエディタの背景色が描画されない

### 症状

background color を設定しても、背景の矩形自体が画面に出ない。

### 観点

- draw 順序の問題
- viewport / clear / blit の順序問題
- GPU パスと fallback パスで描画条件が揃っていない

### 優先度

- 高

### 関連候補

- `ArtifactCompositionRenderController.cppm`
- `docs/bugs/COMPOSITION_EDITOR_RENDER_PIPELINE_BUGS_2026-04-04.md`
- `docs/bugs/COMPOSITION_FILL_NOT_VISIBLE_2026-04-05.md`
- `docs/bugs/BUG_INVESTIGATION_COMPOSITION_VIEW_DILIGENT.md`

### 備考

Issue 1 と見た目が似るが、こちらは「色は合っていても描画されていない」側。  
背景色の state が変わらない問題とは切り分ける。

---

## ざっくり優先順

1. Issue 3: 背景色が描画されない
2. Issue 2: 初期化が重い
3. Issue 1: 背景色が変わらない

---

## まとめ

- Issue 1 と Issue 3 は、見た目は近いが根っこが違う可能性がある
- Issue 2 は別軸の UX 問題として扱った方がよい
- 3 件とも共通の担当領域は `CompositionEditor` / `CompositionRenderController`

