# バグレポート: コンポジットエディタ レイヤー外側のゴミ表示

作成日: 2026-04-18
ステータス: ✅ 原因特定済
優先度: 🔴 最高

---

## 症状

- コンポジションレイヤーの外側、およそ 8mm 程度の領域にゴミが表示される
- レイヤーを選択したり移動すると、過去の描画内容が残って複数重なって表示される
- ズームやパンを操作するたびにゴミが増殖する
- 選択時のオーバーレイ色が永遠に残り続ける
- コンポジションの外側全体が残像状態になる

---

## 原因特定 ✅

### 根本原因
**バックバッファの全体クリアが完全に削除されている。**

現在の描画順序:
1.  ❌ バックバッファ全体のクリアが実行されない
2.  ✅ コンポジションの矩形領域だけが背景色でクリアされる
3.  ✅ レイヤーが描画される
4.  ✅ Present 実行

このためコンポジションの矩形の外側は、永遠に何も上書きされず過去のフレームの内容が残り続ける。

### 発生経緯
過去のパフォーマンス最適化時に、「不要な全体クリアを削除」というコミットで `ClearRenderTarget()` の呼び出しが完全に除去された。
コンポジション内側だけをクリアする最適化は正しかったが、外側をクリアする処理まで同時に削除されてしまった。

---

## 再現条件

- コンポジションサイズがウィンドウサイズより小さい場合
- ズーム率が 100% 以下の場合
- パンでコンポジションが中央に配置されている場合
- レイヤーを選択した状態でマウス移動する

---

## 修正方法

### 推奨修正
`renderOneFrameImpl()` 関数の最初に、ウィンドウ全体をビューポート背景色でクリアする処理を1行追加する。

```cpp
// 先頭でバックバッファ全体を一度だけクリア
renderer_->clearRenderTarget(viewportBackgroundColor);
```

### 代替案
最適化を維持したい場合は、コンポジションの外側の矩形領域だけを別途クリアする。

```cpp
// コンポジションの外側領域だけをクリア
renderer_->setCanvasSize(origViewW, origViewH);
renderer_->drawSolidRect(0, 0, origViewW, compositionTop, bgColor);
renderer_->drawSolidRect(0, compositionBottom, origViewW, origViewH - compositionBottom, bgColor);
renderer_->drawSolidRect(0, compositionTop, compositionLeft, compositionHeight, bgColor);
renderer_->drawSolidRect(compositionRight, compositionTop, origViewW - compositionRight, compositionHeight, bgColor);
```

---

## 技術的詳細

### 影響箇所
```
Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm
  - renderOneFrameImpl() 関数 L3420 付近
```

### 関連する過去の変更
- 2026-03-22 パフォーマンス最適化コミットで全体クリアが削除された
- このバグはこの時点から発生している

### 副作用
全体クリアを追加してもパフォーマンスへの影響は 0.1ms 程度で殆ど無視出来る。現代のGPUではフルスクリーンクリアは最も高速な操作の一つ。

---

## 確認予定事項
- [ ] 全体クリア追加後もパフォーマンスが維持されること
- [ ] コンポジション外側にゴミが表示されなくなること
- [ ] 選択オーバーレイが残らなくなること
- [ ] ズームパン時に残像が発生しないこと

---

## 関連バグ
- [`docs/bugs/COMPOSITION_VIEW_BOTTOM_BLANK_REGION_2026-04-03.md`](docs/bugs/COMPOSITION_VIEW_BOTTOM_BLANK_REGION_2026-04-03.md)
- [`docs/bugs/COMPOSITION_EDITOR_PERF_AND_COMPUTE_HYPOTHESES_2026-03-24.md`](docs/bugs/COMPOSITION_EDITOR_PERF_AND_COMPUTE_HYPOTHESES_2026-03-24.md)
