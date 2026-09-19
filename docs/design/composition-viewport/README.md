# Composition Viewport concept

**最終更新:** 2026-09-19

**採用状況:** 2026-09-08 ユーザー採用済み。2026-09-09 に VP 周辺 UI の初回実装を反映。描画領域外のタブ状ヘッダー、上部ツールバー、下部の表示／再生コントロール、解像度・fps ステータスを対象とし、キャンバス内は変更していない。内蔵 imagegen を使用。

[モック画像](composition-viewport-dcc-concept-2026-09-08.png)

[上部クローム状態モック](composition-viewport-top-chrome-states-2026-09-12.png)

## 表示パス／比較ビュー検討モック（2026-09-19）

以下は Composition Viewport の表示確認ワークフローを比較検討するための
コンセプト画像であり、実装仕様の確定や既存ショートカットの変更を意味しない。

- [RGB／R／G／B／A チャンネル分離](composition-viewport-channel-isolation-mock-2026-09-19.png)
- [A/B 縦ワイプ比較](composition-viewport-ab-wipe-mock-2026-09-19.png)
- [A/B フレーム範囲スプリット比較](composition-viewport-ab-range-split-mock-2026-09-19.png)

共通して、下部 Viewer コントロールへ表示パス／チャンネル／比較モードを集約する案。
チャンネル案は `Beauty` と `RGB / R / G / B / A`、縦ワイプ案は
`A: Beauty` と `B: Color Grade`、範囲スプリット案は
`A: Frame 120` と `B: Frame 168` および `Range 120–168` を例示する。
キャンバス内の題材は比較状態を伝えるためのダミーであり、既存の描画内容、
ギズモ、選択表示、HUD、レンダリング経路を変更する根拠にはしない。

## フレームギズモ拡縮フィードバック検討モック（2026-09-19）

平面レイヤーをフレームギズモで拡縮している間だけ表示する、元サイズの
ゴーストと数値フィードバックの比較案。これはユーザーから明示された
キャンバス内オーバーレイの検討であり、既存ギズモの入力仕様変更は含まない。

- [コンパクト案：元枠ゴースト＋サイズ／倍率HUD](composition-viewport-scale-ghost-compact-2026-09-19.png)
- [寸法案：幅・高さ寸法線＋変化量HUD](composition-viewport-scale-dimensions-2026-09-19.png)
- [詳細案：変化領域＋XY倍率／アンカーHUD](composition-viewport-scale-hud-anchor-2026-09-19.png)

通常操作の第一候補はコンパクト案。元枠は低コントラストの破線、現在枠と
アクティブハンドルは既存の選択アクセント色、HUDはドラッグ点の外側へ置く。
寸法線は精密表示モードまたは修飾操作時だけ追加する案とし、常時表示による
キャンバス遮蔽を避ける。表示値は画面上の見かけ寸法ではなく、レイヤーの
評価後ピクセル寸法とX/Yスケールを区別して扱う必要がある。

**実装状況（2026-09-19）:** コンパクト案を基準に、既存の拡縮ドラッグ表示を
更新した。元枠は点滅しない低透明度の破線、HUDはアクティブハンドル外側へ配置し、
現在サイズ、X/Y倍率、幅・高さの差分を表示する。中央スケールハンドルおよび
複数選択の中央基準拡縮では `Anchor Center` を追加表示する。長い寸法線と
変化領域の塗りは常設せず、今回の実装対象外とした。

2026-09-12 の上部クローム状態モックは、Composition タブ、Viewport
ツールバーの ON/OFF 表現、Dock タブの閉じるボタンだけを対象とする。
選択状態はテーマのアクセント色による薄い面と下線で示し、閉じるボタンは
小サイズでも読める太さの X と明確なホバー領域を持つ。キャンバス内の表示や
操作仕様を変更する根拠にはしない。

2026-09-12 にユーザーが明示した View Navigator の選択肢として、Maya 型
Cube と Unity 風の Simple 表示を追加する。両者は同じ viewport orientation
状態、スナップ、ドラッグ軌道を使い、表示・ヒット領域だけを切り替える。
Navigator 内の `Cube` / `Simple` ボタンから選び、選択はアプリ設定に保存する。

## タブ責務

- `Composition Viewer` は Dock／パネルタブ。右クリックメニューは閉じる、
  フォーカス、分離、Dock 復帰、最大化などのワークスペース操作に限定する。
- `Comp1` はドキュメント／編集コンテキストタブ。未保存マークと保存確認を
  所有し、右クリックメニューはドキュメントの閉じ方や保存操作に限定する。
- 未保存マークを Dock タブへ表示せず、Dock 配置操作をドキュメントタブへ
  混在させない。
- 通常時の常設要素はタブ名と閉じるボタンだけとし、その他の操作は
  右クリックメニューへ収める。
- 現在の `Comp1` は単独の編集コンテキストであり、右クリックの
  「コンポジション表示を閉じる」は表示だけを閉じてプロジェクトを保持する。
  将来、独立した複数ドキュメントタブを導入する場合だけ、閉じる操作を
  dirty-state の保存確認へ接続する。

## 必須指示: VP 周辺 UI のみを参照する

この画像の採用を、Viewport 全体の再現・改修の許可と解釈してはならない。参照してよいのは、描画領域の外側にあるタブ、上部ツールバー、下部の表示／再生コントロール、ステータス行の配置・密度・配色・区切り方だけである。

- キャンバス内の文字、球、図形、背景、構図はダミーであり、再現・実装・初期コンテンツ化を禁止する。
- 選択枠、ハンドル、アンカー、変換軸、ギズモ、セーフガイド、描画領域内の HUD／方向表示は採用対象外。この画像を根拠に既存の描画・操作・形状・配色を変更してはならない。
- 既存の承認済みギズモ仕様と Viewport の描画・入力・レンダリング責務を維持する。変更にはユーザーから別途明示的な依頼が必要。
- 画像内のボタンやタブの存在は、新機能追加、ショートカット変更、ビュー構造変更の許可ではない。周辺 UI の実装では既存機能と責務に対応付け、倍率表示の重複など生成上の不整合をそのまま再現しない。

以下の生成プロンプトは制作履歴であり、上記の採用範囲を広げる実装指示ではない。画像・プロンプトと本指示が矛盾する場合は、本指示を優先する。

## Generation prompt

Use case: ui-mockup. Generate one high-fidelity desktop UI screenshot mockup for ArtifactStudio Composition Viewport, landscape 16:9, sharp legible English UI. Professional dense compositing DCC layout inspired by After Effects and Blender, original ArtifactStudio styling. Entire image a flat front-facing composition editor panel, no physical monitor and no perspective on UI. Charcoal #242629 panels and #191b1e viewport surround, off-white text, restrained amber #e4ad53 active controls and editable numbers. Tight squared controls, subtle separators, solid readable tool icons, no rounded dashboard cards or glowing sci-fi UI. Composition viewport occupies at least 78 percent of image area, no project browser, no effects inspector, no full timeline. Compact top tab row 'Composition 01' 'Layer Solo'; next toolbar grouped selection, move rotate scale, anchor, pen, shape, then Snapping, Guides and overlays. Canvas shows an appealing composed motion-design still: off-white rectangular 16:9 artboard on dark surrounding pasteboard, large navy lettering 'FORM / MOTION', a coral orange sphere and cobalt geometric shapes, subtle depth. One selected shape has a clean amber bounding rectangle, 8 compact handles, a visible anchor cross and red green transform axes, overlays confined to selected object. Faint title/action safe guides within artboard. Minimal top-left canvas label 'Active Camera', upper-right small orientation axis gizmo. Canvas unobstructed. Bottom single compact viewer control strip: '50%' zoom, 'Fit', 'Full' resolution, 'RGB', transparency grid, 'Active Camera', '1 View', timecode '00:00:12:08', frame-step and play controls. Small bottom status shows '1920 × 1080', '24 fps'. Carefully organized practical AE-like viewer chrome with Blender-style manipulation clarity. No large floating HUD or selection-count badge. Deliver only the mockup image.
