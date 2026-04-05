# 平面レイヤー描画順序逆転問題 (2026-04-03)

## 症状

コンポジションに平面レイヤー（Solid/Image等）を2つ以上配置した際、タイムラインのレイヤーリストで上に表示されているレイヤーが、コンポジションビューでは**奥**（下）に描画され、**リストの順序と描画順序が逆転しているように見える**。

例：
- タイムライン表示順（上から）: Layer A（最上）, Layer B（その下）
- 実際の描画順: Layer B → Layer A
- 結果: Layer B が Layer A の後ろに隠れる（期待と逆）

## 調査経緯

### 1. 描画側の順序決定

`ArtifactPreviewCompositionPipeline::render()` 内（`CompositionRenderer.cppm` 参照）で、以下のようにレイヤーを取得・描画している：

```cpp
const auto layers = composition_->allLayer();
for (const auto& layer : layers) {
    if (!layer || !layer->isVisible()) continue;
    // ... 描画処理
}
```

つまり、`composition_->allLayer()` が返す `QVector<ArtifactAbstractLayerPtr>` の**インデックス 0 から順に描画**している（後で描画したものが手前に来る）。

### 2. `allLayer()` の戻り順

`ArtifactAbstractComposition::Impl::allLayer()`（行 213-216）は単に `layerMultiIndex_.all()` を返す。

`layerMultiIndex_` は以下の型：
```cpp
using MultiIndexLayerContainer = MultiIndexContainer<ArtifactAbstractLayerPtr, LayerID>;
```

この `MultiIndexContainer` の `all()` が**何种順序でベクトルを返すか**が鍵になる。

### 3. レイヤー追加メソッドの挙動

`appendLayerTop()` と `appendLayerBottom()` の実装：

- **`appendLayerTop`**（行 124）: `layerMultiIndex_.add(layer, id, type);`
- **`appendLayerBottom`**（行 236）: `layerMultiIndex_.insertAt(0, layer, ...);`

`appendLayerBottom` が **インデックス 0 に挿入**していることから、`insertAt(0)` は先頭（最奥）に置く操作と解釈できる。すると `add()` は末尾（最前面）に追加するはず。

したがって、期待される順序は：
- インデックス 0 = 最奥レイヤー
- インデックス最大 = 最前面レイヤー

描画ループがインデックス 0 から描画するので、**奥から手前へ**の正しい重なり順になる。

### 4. タイムライン側のレイヤーリスト順序

`ArtifactLayerPanelWidget` は `composition_->allLayer()` と同様のコンテナからレイヤーを取得し、`QTreeView` にモデルとして提供している。このとき、**UI 上ではインデックス 0 がリストの最上部**に表示される。

つまり：
- UI リスト上: `allLayer()[0]` が最上
- 描画順: `allLayer()[0]` が最初（最奥）

この結果、**UI で上にあるレイヤーほど奥に描画される**ため、ユーザーにとっては「順番が逆に見える」。

## 根本原因

`layerMultiIndex_` の `all()` が返す順序は **追加順**（`insertAt` での位置指定を反映）である可能性が高いが、UI のレイヤーパネルではその順序をそのままリストの上→下として表示している。

しかし、コンポジションソフトでは一般的に：
- **タイムラインのレイヤーリスト**: 上 = 手前（front）
- **描画順序**: 上から下へ = 奥から手前

将这个順序を一致させるには、以下のいずれかが必要：

1. `allLayer()` が返す順序を「手前から奥」にする（インデックス 0 が最前面）
2. 描画ループを**逆順**（`layers.rbegin()` から）に回す
3. UI のレイヤーパネルが表示順を逆にする

## 推奨修正

### 修正案 A: 描画ループを逆順に変更 (最小影響)

`ArtifactPreviewCompositionPipeline.cppm` の描画ループを：

```cpp
const auto layers = composition_->allLayer();
// ...
for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
    const auto& layer = *it;
    // 描画処理
}
```

に変更する。これにより、`allLayer()` の最後の要素（最前面）が最初に描画され、正しい重なり順になる。

### 修正案 B: `allLayer()` の順序を逆にする (根本)

`ArtifactAbstractComposition::Impl::allLayer()` を：

```cpp
QVector<ArtifactAbstractLayerPtr> ArtifactAbstractComposition::Impl::allLayer() const
{
    auto v = layerMultiIndex_.all();
    std::reverse(v.begin(), v.end());
    return v;
}
```

と変更。ただし、既存の他のUI（レイヤーパネル等）が `allLayer()` を表示に使っている場合、それらも逆順になってしまう可能性がある。UI側も逆順にすれば良いが、影響範囲が広い。

### 修正案 C: UI 側のモデルが明示的に逆順にする (分離)

`ArtifactLayerPanelWidget` が `allLayer()` を取得後、`std::reverse()` して表示する。これなら描画順には影響しないが、UI と描画順が一致する。

**最も安全なのは修正案 A**。描画側だけを変更すれば、UI 表示と描画順が整合する。

## 再現手順

1. 新しいコンポジションを作成
2. 平面レイヤーを2つ追加（例: 赤色と青色）
3. タイムラインのレイヤーパネルで、順序を上: 青, 下: 赤 にする（青が手前）
4. コンポジションビューで確認: 赤が青の上に描画される（期待と逆）
5. `appendLayerTop` / `appendLayerBottom` の違いでテスト: `appendLayerTop` で追加したレイヤーが最前面になるはずが、UI ではリストの最下部に追加されるので混乱

## 関連ファイル

- `Artifact/src/Preview/ArtifactPreviewCompositionPipeline.cppm` (描画ループ)
- `Artifact/src/Composition/ArtifactAbstractComposition.cppm` (`allLayer()` 実装)
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` (UI 側のモデル)

## 注記

`MultiIndexContainer` は `insertAt(0)` での先頭挿入をサポートしていることから、コンテナ内部はインデックス 0 = 最前列という自然な順序で管理されている可能性が高い。その場合、`allLayer()` が返す順序はそのままコンテナ順（0 最前列, N 最奥列）だが、描画が 0 から（最前列から）始まると手前→奥の順になってしまう。したがって描画ループを逆順にするのが正しい。

## 調査日

2026-04-03
