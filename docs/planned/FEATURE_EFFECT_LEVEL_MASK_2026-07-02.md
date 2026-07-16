# エフェクト単位の個別マスク設定

**作成日:** 2026-07-02
**状態:** 実装済み・実機未確認

---

## 目的

現在はレイヤー全体に対するマスクのみサポートされているが、特定のエフェクトに対して個別にマスクを設定できるようにする。

AE における「エフェクトコントロールパネルからマスクを割り当てる」操作に相当する。

---

## ユースケース

1. **DropShadow を特定の領域だけに適用したい**
   - レイヤー全体のマスクとは別に、影だけを部分的にカットしたい

2. **Blur エフェクトを領域限定でかけたい**
   - レイヤーは全面表示だが、ぼかし効果だけ特定マスク範囲に制限

3. **複数エフェクトに異なるマスクを割り当てたい**
   - エフェクトAはマスクα、エフェクトBはマスクβ、のように

---

## 設計案

### Phase 1: Effect へのマスク参照追加

各エフェクトがオプションでマスクの参照を持てるようにする。

```cpp
class ArtifactAbstractEffect {
    // ... 既存 ...
    
    // 新規: エフェクト単位のマスクリスト
    std::vector<LayerMask> effectMasks_;
    
    bool hasEffectMasks() const;
    void addEffectMask(const LayerMask& mask);
    void removeEffectMask(int index);
    void clearEffectMasks();
    LayerMask effectMask(int index) const;
    int effectMaskCount() const;
};
```

### Phase 2: パイプラインへの組み込み

`buildRasterizedSurfaceBuffer()` または `applyConfigured()` 内で、エフェクト適用後に個別マスクを適用する。

処理順序（目標）:
```
1. Layer base surface 生成
2. → レイヤーマスク適用
3. → Rasterizer エフェクト + エフェクト個別マスク
     各エフェクトごとに:
       a. エフェクト適用
       b. エフェクトに割り当てられたマスクがあれば適用
4. → 最終合成
```

### Phase 3: UI

- Inspector / PropertyWidget 上の各エフェクトにマスクピッカー UI を追加
- マスク選択は既存のマスク（レイヤーに定義済み）から選択する方式
- ドラッグ&ドロップでの割り当ても可能にする（将来）

---

## 非目標

- エフェクト内部のパラメータをマスクでドライブする（AE の「マスクによるエフェクトコントロール」は別機能）
- マスクのマスク（入れ子）
- リアルタイムマスク編集プレビュー（初版では簡易表示でよい）

---

## 参考

- 現在のレイヤーマスク実装: `Artifact/src/Mask/LayerMask.cppm`
- 現在のエフェクトパイプライン: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` `buildRasterizedSurfaceBuffer()`
- エフェクト抽象: `Artifact/include/Effects/ArtifactAbstractEffect.ixx`

## 実装状況メモ (2026-07-13)

- `ArtifactAbstractEffect` 側に primary mask image と追加 effect mask images の保持が入った。
- `ArtifactPresetManager` で effect mask image を preset 往復できるようになった。
- `ArtifactTextLayer` の Source Text keyframe 露出と合わせ、マスク系の保存/復元の土台は前進している。
- Inspectorのeffect context menuから、現在layerまたは選択layerの既存maskをeffect mask imageへ変換できる。
- 置換 / 追加 / clearと`SetEffectMaskImagesCommand`によるUndoに対応。
- effect listへmask数とtooltipを表示し、割り当て状態を確認できる。
- primary / additional mask imageはeffect適用時に合成され、preset round-tripにも含まれる。
- build / testはユーザー方針により実施していないため、実機確認だけ未完了。
