# MILESTONE — Composition Editor Rich HUD (2026-06-16)

## Background

`ArtifactCompositionEditor` のビューポート HUD は、長らく
`drawViewportInfoOverlay()` の 1〜2 行表示だけだった。`setInfoOverlayText()` を
介したエフェメラルな通知（Smart Import Placement など）しか乗せられず、

- 選択レイヤーの Transform / Opacity / Blend が見えない
- タイムコードがタイムラインに focus を移さないと読めない
- ズーム率とパン座標がすぐ分からない
- ツール切り替え後の "今何のツールが有効か" のフィードバックが地味

…と、操作中に必要な情報が毎回ペインを切り替える必要があり、編集のリズムが
悪くなっていた。本マイルストーンは HUD を「常時表示のステータスボード」に
拡張し、視線の移動量を減らす。

## Goals

1. 選択レイヤー / Playhead / Viewport / Tool を 1 つの K/V ブロックで常時表示
2. 既存 `setInfoOverlayText()` のエフェメラル通知は**温存**（上書きしない）
3. Editor の selection / playback / ツール変更に**追従**して再描画
4. K/V 行の追加・拡張が将来 1 ファイルで済むよう、setter は単純な
   `QList<QPair<QString, QString>>` で受ける

## Implementation

### 1. Render Controller に K/V バッファを追加

`ArtifactCompositionRenderController.Impl` に以下を追加:

```cpp
QList<QPair<QString, QString>> infoOverlayKeyValues_;
```

公開 API:

```cpp
void setInfoOverlayKeyValues(const QList<QPair<QString, QString>>& rows);
void clearInfoOverlayKeyValues();
```

setter は「値が変わった時のみ overlay composite を invalidate + render dirty」
の最適化付きで実装。`setInfoOverlayText()` と独立して使える。

### 2. Overlay 描画の拡張

`ArtifactCompositionRenderOverlay.cppm` に新規関数を追加:

```cpp
void drawViewportInfoOverlayWithKeyValues(
    ArtifactIRenderer* renderer, int overlayW, int overlayH,
    const QString& title, const QString& detail,
    const QList<QPair<QString, QString>>& keyValues,
    const QSize* restoreCanvasSize);
```

内部で共有の `drawViewportInfoOverlayWithKeyValuesImpl()` を呼び、key
が空のときは従来の 2 行レイアウトにフォールバックする。Label は `92%` の
やや小さいフォント、Value は元のサイズ + 高コントラスト白。空白 key
(`{ "", "" }`) はセクション区切りとして半行分の余白を空ける。

### 3. Render Controller の dispatch

`CompositionRenderController::Impl::drawViewportGhostOverlay()` 内の
info overlay 描画を、K/V バッファが空かどうかで分岐させる:

```cpp
if (infoOverlayKeyValues_.isEmpty()) {
  ::Artifact::drawViewportInfoOverlay(...);
} else {
  ::Artifact::drawViewportInfoOverlayWithKeyValues(
      ..., infoOverlayKeyValues_, &restoreCanvasSize);
}
```

これでエフェメラル通知だけが欲しいパスは従来通り動く。

### 4. Editor 側の HUD collector

`ArtifactCompositionEditor::Impl` に `refreshViewportHud()` を追加。以下の
ブロックを上から順に `QList<QPair<QString, QString>>` に積む:

- **選択ブロック**: 名前 + 選択件数 / 位置 (X, Y) / スケール (一様なら
  "N%"、違ってれば "Nx% / Ny%") / 不透明度 (%)
- **Playhead ブロック**: タイムコード (HH:MM:SS:FF) / フレーム (current /
  total + fps) / ワークエリア (start - end、あれば)
- **Viewport ブロック**: ズーム (%) / パン (X, Y)
- **Tool ブロック**: 現在のツールラベル

各ブロック間は `{ "", "" }` 1 行で区切る。

### 5. 駆動元

`ArtifactCompositionEditor::event()` 内の `SelectionSync` ハンドラで
`refreshViewportHud()` を呼ぶ。selection / playback / composition 切替
すべて既存の `queueSelectionSync` 経路を通るため、追加の subscription は
最小限。

加えて `ArtifactPlaybackService::frameChanged` signal を Editor 構築時に
接続し、スクラブ中も HUD が追従するように。

## Files touched

- `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm`
- `ArtifactStudio/docs/done/MILESTONE_COMPOSITION_EDITOR_RICH_HUD_2026-06-16.md` (this file)

## Verification

ビルドは手元では未実施（taste: 「ビルド/テストはユーザーが明示的に依頼
した場合のみ」）。実装側はリポジトリ全体で次を確認できる:

- `setInfoOverlayKeyValues({})` 直後は従来 2 行レイアウトに戻ること
- selection 切替で HUD が再描画され、新しいレイヤーの値が出ること
- スクラブ中にタイムコードが追従すること
- ツール切替でツール行が更新されること
- zoom/pan 操作後にそれらが反映されること

## Follow-ups (not in this milestone)

- レイヤー transform を矢印キーで微調整するモード（候補: `Alt+矢印`）
- キーフレームが打たれているフレームにいる時、その数とキーをハイライト表示
- グループ / プリコンポ時の "親 → 子" 階層を K/V 末尾に
- スマートガイド状態 (Snap to Grid / Guides / Active Guide) の常時表示
- HUD の透明度 / 配置を Preferences で切替可能に
