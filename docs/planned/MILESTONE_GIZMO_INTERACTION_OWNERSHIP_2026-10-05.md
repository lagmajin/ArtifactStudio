# ギズモ操作所有者の統一

**最終更新:** 2026-10-05

**ステータス:** In Progress

## 2026-10-05 実装状況

Artifact の既存 `ArtifactCompositionRenderController.cppm` と `ArtifactTextGizmo.cppm` に実装。ビルド・テスト・実機の受入れは未実施であり、完了扱いにはしない。

- `resolveGizmoOwner` / `gizmoOwnerForLayer` を binding、投影フレーム press、通常 press、move、hover、cursor、共通 selection overlay に適用。Text / Content は camera の有効性で Transform3D へ切り替わらない。
- セッションが owner、primary layer / composition の weak reference、編集 frame、tool、gizmo mode、content edit mode、開始時の device pixel ratio と viewport→canvas mapping を保持。実 handle / Undo / 3D camera snapshot は既存ギズモと controller の snapshot に委譲し、二重所有しない。
- すべての既存 Transform3D press / modal 開始、Text / Content / legacy Transform2D 開始を登録。legacy 2D の press 候補は Transform2D と既存 Design workspace の互換経路に限定し、通常の3D操作で非表示ハンドルが入力を奪わない。move / release / cancel がそのセッションへ dispatch する。
- 再描画で2Dギズモを再バインドしない。selection event、composition / mode / content edit 切替、対象削除、frame / tool / lock / visibility の不一致で取消。group の既存 target snapshot を借用して secondary target の削除・lock も確認する。
- 終了 guard が再入、セッション解除、viewport の mouse capture 解放を扱う。debug assertion で二重ドラッグと owner / isDragging 不一致を検出する。既存の `gizmoDragActive_` は3D経路の補助状態として残し、Text / Content の重複フラグは撤去。
- Text の Offset / box resize / Rotate / AnchorPoint に `textSetAbsolutePosition` を適用。初期 Position を相対 track 値から分離し、rotation の initial offset も補正。静的 anchor の操作は不要なキーを作らない。
- expression が所有する編集チャンネルのドラッグを開始しない。Text ツールで disabled handle をクリックしても新規テキスト作成へ誤って流さない。
- box の既存 property surface は layer binding 時に準備し、drag 中に再構築しない。この初回準備は cold な selection/cache 構築境界であり、既存 property group / Undo の確保を再利用する。新規 container、毎フレームの collection copy、GPU resource、readback、診断文字列構築は追加していない。

混在選択は primary Text の既存単体編集を維持する。混在 group 全体へ Text を追加する仕様拡張は行っていない。実機の受入れ表、選択イベント経由の中断、Undo/Redo、親 transform / camera / HiDPI の検証が残る。新規 `.ixx`、module、CMake、signal/slot、backend 変更はない。

## 目的と根拠

テキストの投影フレーム press が 3D 操作を開始する一方、move はテキストを除外する不整合を解消し、同種の再発を防ぐ。[調査報告](../bugs/TEXT_GIZMO_INPUT_OWNERSHIP_INVESTIGATION_2026-10-05.md)を根拠とする。以下は設計契約であり、runtime 検証済みという意味ではない。

現行の `ArtifactCompositionRenderController.cppm` は、bind、draw、hover、press、move、release、cancel がそれぞれ layer type と camera state を見て経路を選ぶ。長期対策は、この判断を controller 内の純粋な所有者判定と、開始後の操作セッションに集約すること。

## 所有者と表現の分離

提案する値型 `GizmoRoute` は、操作所有者（None / Text / Content / Transform2D / Transform3D）、表示表現（投影フレーム等）、許可する操作、座標空間を区別して保持する。投影フレームと 3D 軸は同じ Transform3D 所有者の表示・hit の候補とする。カメラ行列の有効性だけで Text から Transform3D へ所有者を切り替えない。

判定入力は primary layer、既存 selection、tool、content edit mode、workspace、gizmo mode、可視性・lock、利用可能な camera。既存 selection を借用し、判定のための毎イベント collection コピーを避ける。判定関数は状態変更、setLayer、Undo、通知、GPU 操作を行わない。

単独テキストの所有者は既存設計どおり Text。Text ツールで既存 TextGizmo が hit すれば操作を開始し、hit しなければ既存の新規テキスト作成経路に渡す。Text box の resize と layer scale は別の編集であり、統一のために意味を変えない。

混在複数選択は既存の表示と変更対象を先に棚卸しする。primary の切替で暗黙に「単体編集」と「group transform」が切り替わらない契約が必要だが、全テキストを group transform 対象に追加する仕様拡張は別判断とする。棚卸し前に一律ルールを実装しない。

## セッションの契約

press で hit が成立したら、controller が `GizmoInteractionSession` を保持する。値型を優先し、既存 LayerID、座標型、時刻型、camera snapshot を使う。

- セッションは所有者、対象 layer / selection revision、編集時刻、handle、開始時の座標変換を保持する。
- move / release / cancel は開始済みセッションの所有者へ渡す。move のたびに現在選択から所有者を再判定しない。
- 再描画・hover はセッションを解除せず、開始済み所有者と整合した表示を行う。
- 選択・tool・composition・編集時刻の変更、対象削除、lock 変更等は、既存契約を確認したうえで明示的な cancel/commit 境界にする。通常のドラッグ自身による geometry revision 更新は中断理由にしない。
- cancel は開始状態へ戻し、commit は一度だけ Undo を記録する。mouse capture と interaction finish を必ず同じ終了経路で解放する。
- 複数ギズモの `isDragging()` が同時に true になる状態を debug invariant として検出する。セッションと実ギズモの状態が食い違う場合も検出する。

入力用セッションは geometry 評価や Undo の第二所有者にはしない。各ギズモの既存変換・Undo 実装を呼ぶ薄い経路として導入し、状態を移管する場合は旧状態を同時に撤去する。

## 座標と変換値

Qt logical → physical は controller の入口で一度だけ変換する。Text の canvas 操作、投影フレームの camera 操作を同じ座標として扱わず、draw と hit は同じ mapping を使う。ドラッグに使用する mapping は開始時に確定する。

TextGizmo 内の Position 更新は、既存 `.cppm` 内の明示 helper に寄せる候補とする。外部からは絶対 Position を受け、静的 transform と keyframed transform の相対値への変換、property 同期、dirty 通知を揃える。Offset / box resize / Rotate / AnchorPoint 間で処理が異なる残存リスクを解消する。expression、spatial tangent、キー補間の扱いは既存の正規 authoring 経路と比較し、暗黙に上書きしない。

## 移行手順

1. **不具合を閉じる:** 投影フレーム press のテキスト除外を揃える。単独テキストの Selection / Text 操作と混在選択への影響を確認する。
2. **判定を集約:** 既存 `.cppm` 内に route 判定を導入し、bind / draw / hover / press を移行する。利用側に layer type の独立した除外条件を残さず、所有者単位で dispatch する。行列や実際の hit geometry の計算まで判定関数へ詰め込まない。
3. **開始後を固定:** セッションを導入し、move / release / cancel / capture / drag-active query を統一する。移行期間の並行した開始経路をなくす。
4. **Text authoring を整合:** Position 補償の helper 化とキー・Undo の保存を確認する。入力所有者修正とは別差分で扱う。
5. **再発検出を追加:** 所有者判定の表形式チェック、セッション状態遷移、実 viewport 操作の回帰確認を整える。

既存ファイルで完結できる範囲を先行する。ProjectedFrameGizmo のクラス分離は[既存計画](MILESTONE_PROJECTED_FRAME_GIZMO_2026-09-21.md)と整合させ、今回の所有者統一の前提にしない。

## 受入条件

| 確認 | 条件 |
|---|---|
| 単独テキスト | Selection / Text で本体移動・box resize・回転・anchor 操作が開始から終了まで Text に届く |
| 入力と表示 | draw / hover / press の対象と handle が一致し、非表示ギズモが hit を奪わない |
| 状態遷移 | idle → drag → commit/cancel。再描画で drag が消えず、対象変更・削除で dangling session が残らない |
| Undo | 1 gesture が1操作。cancel は値とキーを戻し、Undo/Redo 後も表示と property が一致する |
| 非テキスト | image / solid / shape / 3D と content edit の正規操作経路を維持する |
| 複数選択 | 同種・混在・親子 selection、primary 切替で、表示枠と変更対象が一致する |
| 座標 | zoom / pan / HiDPI / parent transform / viewport orientation で hit と drag が一致する |
| authoring | 静的・keyframed・初期 Position 非ゼロ・補間ありで Position 補償が二重加算されない |
| hot path | route 判定で collection コピー・文字列構築・GPU resource 作成・readback を追加しない |

判定のチェックだけでは表示上の hit 一致を証明できない。実 viewport の回帰確認を受入れに含める。診断は起動時確定フラグで有効化し、イベント境界だけで遅延評価する。新規の共有フラグが必要なら StartupFlags 契約を使用する。

## 範囲と未確認事項

実装候補は Artifact の controller / TextGizmo の既存実装ファイル。ArtifactCore / ArtifactWidgets / DiligentEngine の変更は前提にしない。新規 signal/slot、QtCSS、QImage/QPainter 合成、標準 container、新規 module を必要条件にしない。

対象は Diligent より上の共通入力経路。D3D12/Vulkan の backend、GPU resource lifetime、同期を変更する計画ではない。実行中 backend、実機再現、混在 selection の契約、expression 付き補償は未確認。

この文書作成は実装完了を示さない。ビルド・テスト・CMake・実機確認は AGENTS.md に従いユーザーの明示指示後に実施する。
