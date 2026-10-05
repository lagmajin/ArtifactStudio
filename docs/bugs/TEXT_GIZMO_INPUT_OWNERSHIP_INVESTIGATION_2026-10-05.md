# テキストレイヤーのギズモが動かない問題の調査

**最終更新:** 2026-10-05

**状態:** 静的調査後に所有者統一と操作セッションを実装。実機再現・ビルド・テスト未実施。

以下の行番号・経路は修正前ソースの調査記録。2026-10-05 に Artifact の controller / TextGizmo を変更した。実装状況と受入れ残項目は[所有者統一計画](../planned/MILESTONE_GIZMO_INTERACTION_OWNERSHIP_2026-10-05.md)を参照する。実装修正をもって実機原因確定とは扱わない。

## 結論

現行ソースに、テキストの押下を 3D ギズモが先取りする一方、移動処理はテキストを除外する不整合がある。報告された「ギズモが動かない」を直接説明する最有力原因。実行中のバイナリとの一致と、実機でどの hit が成立するかは未確認。

## 根拠と経路

対象: `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。

1. `drawViewportOverlayPass` はテキストに TextGizmo を描画し（50363–50390）、3D ギズモ表示からテキストを除外する（50446）。
2. `handleMousePress` の投影フレーム分岐（29224–29228）は、`selectedGroupUsesProjectedFrame || viewportOrientationMatricesValid_` で入る。**`!layerUsesTextGizmo(selectedLayer)` がない。**
3. orientation camera の生成は無条件ブロック（40763 以降）であり、正常な描画では `viewportOrientationMatricesValid_ = true` になる（40823）。投影フレーム対象ではない単独のテキストも、この条件に入れる。実 3D カメラを追加しなくても成立する。
4. フレーム角・辺の helper（6786）、内部の helper（6855）にもテキスト除外はない。テキストの local bounds と global 4x4 transform を使って hit し得る。
5. フレーム handle / 3D axis / 内部 hit のいずれかが成立すると、`gizmo3D_->beginDrag` の後に return する。内部 hit は 29551–29584、handle は 29536 付近。TextGizmo の押下処理（29679–29693）には到達しない。
6. `handleMouseMove` の 3D 操作ブロックは `sel3DLayer && !layerUsesTextGizmo(sel3DLayer)`（31822）なので、開始したテキストの 3D ドラッグを更新しない。
7. 後段の TextGizmo 更新（32508）は `textGizmo_->isDragging()` を必要とする。しかし press が先取りされたので false。どちらの操作経路からもレイヤーは更新されない。

ホバーカーソル側の投影フレーム分岐にはテキスト除外がある（39260）。「カーソルは TextGizmo の操作を示すが、押すと別経路が取る」という不一致もある。

`git blame` では該当 press 条件と move 条件は `8d43670ba`（2026-09-30）に属する。ただし、このコミットが不具合の初回導入だったことまで履歴比較・実機では確認していない。

## ツールによる差

- Selection / Move / Scale / Rotate 等: 投影フレーム分岐による先取り候補。
- Text: 27774–27796 の早い分岐が選択中 TextGizmo に先に渡す。TextGizmo に hit すれば今回の先取り経路を回避する。
- AnchorPoint: 問題の投影フレーム分岐の条件で除外される。
- Pen: 同分岐と後段の通常ギズモ操作から除外される。

よって、Text ツールに替えると動くかどうかは、修正前の切り分けに有効。回転済み・傾斜ビューでは投影フレームと TextGizmo の判定領域が一致しないため、操作点によって再現性が変わる可能性がある。

## 確認した既存対策

- `TextGizmo::setLayer` は同じ layer を再設定した際に早期 return する（353–359）。再描画のたびにドラッグ状態が消える旧問題の対策は現行コードにある。
- TextGizmo press 前に `setLayer` を行う経路がある。未バインドだけでは今回の現行ソースを説明できない。
- TextGizmo move は base composite を invalidate し、render dirty を立てる（32510–32516）。TextGizmo が正しく開始されれば更新要求は存在する。
- Offset は静的 Position とアニメーションの相対 Position を分けて扱い（1258–1270）、cached property も同期する。単純移動の旧 absolute/relative 誤用対策は入っている。
- `setDirty(Transform)` は geometry revision を増やす。bounds/global transform キャッシュの revision 更新経路はある。
- Qt logical pointer → physical pointer の変換は press / move とも controller で実施する。今回の主因として DPR の二重変換を示す根拠は得られていない。

## 別の残存リスク

`ArtifactTextGizmo.cppm` の Rotate（1108）と AnchorPoint（1199）の Position 補償は、Position キーがある場合に絶対座標 `newPosX/Y` をそのまま `setPosition` に渡す。一方、Offset とボックスサイズ変更は initial Position を差し引く。

`ArtifactCore/src/Animation/AnimatableTransform3D.cppm:542` の `setPosition` は initial + 入力値を現在値にする。この契約上の不一致は確認できる。ただし cached property が絶対値で同期されるため、表示上いつ問題化するかは未検証。今回の「押しても動かない」とは分けて扱う。

## 最小修正案

まず controller の投影フレーム press 条件へ、hover / 通常 3D press / 3D move / draw と同じテキスト除外を入れ、単独テキストの操作所有者を TextGizmo に統一する。描画形状・承認済みギズモ仕様・Diligent backend・GPU resource lifetime を変更する必要はない。

混在複数選択では primary がテキストでも group に投影フレーム対象が含まれるため、テキスト primary を常に TextGizmo へ渡す現行方針と、group transform の期待動作を確認する。混在 group 全体の仕様変更はこの調査に含めない。

## 実機確認手順（未実施）

1. 正面の通常ビュー、静的な単独テキスト、Selection ツールで本体と角・辺をドラッグする。
2. press の投影フレーム分岐で停止し、`viewportOrientationMatricesValid_`、`frameHandle`、`frameInteriorHit`、`priorityAxis` を確認する。
3. TextGizmo の `handleMousePress` が呼ばれないこと、3D `isDragging()` だけが true になることを確認する。
4. move の 31822 で除外され、TextGizmo move の 32508 にも入らないことを確認する。
5. Text ツールで同じ操作を比較する。
6. 修正後は本体移動、ボックスサイズ変更、回転、アンカー、Esc/右クリック取消、Undo/Redo を確認する。ズーム・pan・HiDPI・親 transform・アニメーションあり・混在選択も対象とする。

調査は共通入力経路が対象。実行中 backend は未確認であり、D3D12/Vulkan 固有の不具合を示す証拠はない。ビルド・テスト・実機確認には AGENTS.md に従いユーザーの明示指示が必要。
