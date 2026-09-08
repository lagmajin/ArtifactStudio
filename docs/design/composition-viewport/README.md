# Composition Viewport concept

**最終更新:** 2026-09-08

**採用状況:** 2026-09-08 ユーザー採用済み。ただし採用範囲は VP 周辺の UI のみ。実装は未着手。内蔵 imagegen を使用。

[モック画像](composition-viewport-dcc-concept-2026-09-08.png)

## 必須指示: VP 周辺 UI のみを参照する

この画像の採用を、Viewport 全体の再現・改修の許可と解釈してはならない。参照してよいのは、描画領域の外側にあるタブ、上部ツールバー、下部の表示／再生コントロール、ステータス行の配置・密度・配色・区切り方だけである。

- キャンバス内の文字、球、図形、背景、構図はダミーであり、再現・実装・初期コンテンツ化を禁止する。
- 選択枠、ハンドル、アンカー、変換軸、ギズモ、セーフガイド、描画領域内の HUD／方向表示は採用対象外。この画像を根拠に既存の描画・操作・形状・配色を変更してはならない。
- 既存の承認済みギズモ仕様と Viewport の描画・入力・レンダリング責務を維持する。変更にはユーザーから別途明示的な依頼が必要。
- 画像内のボタンやタブの存在は、新機能追加、ショートカット変更、ビュー構造変更の許可ではない。周辺 UI の実装では既存機能と責務に対応付け、倍率表示の重複など生成上の不整合をそのまま再現しない。

以下の生成プロンプトは制作履歴であり、上記の採用範囲を広げる実装指示ではない。画像・プロンプトと本指示が矛盾する場合は、本指示を優先する。

## Generation prompt

Use case: ui-mockup. Generate one high-fidelity desktop UI screenshot mockup for ArtifactStudio Composition Viewport, landscape 16:9, sharp legible English UI. Professional dense compositing DCC layout inspired by After Effects and Blender, original ArtifactStudio styling. Entire image a flat front-facing composition editor panel, no physical monitor and no perspective on UI. Charcoal #242629 panels and #191b1e viewport surround, off-white text, restrained amber #e4ad53 active controls and editable numbers. Tight squared controls, subtle separators, solid readable tool icons, no rounded dashboard cards or glowing sci-fi UI. Composition viewport occupies at least 78 percent of image area, no project browser, no effects inspector, no full timeline. Compact top tab row 'Composition 01' 'Layer Solo'; next toolbar grouped selection, move rotate scale, anchor, pen, shape, then Snapping, Guides and overlays. Canvas shows an appealing composed motion-design still: off-white rectangular 16:9 artboard on dark surrounding pasteboard, large navy lettering 'FORM / MOTION', a coral orange sphere and cobalt geometric shapes, subtle depth. One selected shape has a clean amber bounding rectangle, 8 compact handles, a visible anchor cross and red green transform axes, overlays confined to selected object. Faint title/action safe guides within artboard. Minimal top-left canvas label 'Active Camera', upper-right small orientation axis gizmo. Canvas unobstructed. Bottom single compact viewer control strip: '50%' zoom, 'Fit', 'Full' resolution, 'RGB', transparency grid, 'Active Camera', '1 View', timecode '00:00:12:08', frame-step and play controls. Small bottom status shows '1920 × 1080', '24 fps'. Carefully organized practical AE-like viewer chrome with Blender-style manipulation clarity. No large floating HUD or selection-count badge. Deliver only the mockup image.
