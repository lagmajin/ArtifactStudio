# Shapeギャップ更新 2026-09-10（導入すべき機能の全詳細）

**最終更新:** 2026-09-25

親調査 `docs/analysis/GAP_AE_NUKE_2026-08-01.md`（追補2026-09-10でシェイプ20%→60%）の詳細版。
コード読取のみ。ビルド・テスト・runtime検証は未実施。

AE基準: Contents > Group > Path/Fill/Stroke/Operator + Group Transform/Layer Transform、Trim/Repeater/Merge/Offset等のAddメニュー、Convert To Bezier Path、SVGからのCreate Shapes、Taper/Wave、Trim同時/個別、Repeater Composite順、式・Essential Graphics連携。

## 現状サマリ

- データ: `Artifact/include/Layer/ArtifactShapeLayer.ixx`(375行)に7種primitive、fill/gradient/stroke/dash/cap/join/align/taper、fillRule、`CustomPathVertex`、stack API、頂点KF API。`Artifact/src/Layer/ArtifactShapeLayer.cppm`(7572行)に`resolveShapeGeomDims`、`applyAnimatedOperatorParameters`、`createShapeOperator`10種、`toCoreShapeLayer`、`collisionOutlineLocalPoints`。Core `ArtifactCore/include/Shape/`8ファイルに`interpolate`、`MergePaths`等。
- Solo View: `LayerEditorShape*`分割済み。頂点/tangent/segment grammar、挿入、削除(ポリゴンのみ)、角丸/星ハンドル、operator context menu+Undo、tooltip、`ToolOptionsBar::setShapeOptions`連動。
- メインVP: custom path/polygonの頂点・tangent・segment overlay、ホバー/選択、角丸/星内径ハンドル、polygon挿入、削除/Ctrl+A/Escapeの選択grammar、一部operator HUD/Trim・primary handleを実装。右クリックのOpen/Close・Make Smooth/Corner操作も接続済み。ツールオプションの形状種別欄から新規作成形状（Rect/Ellipse/Star/Polygon/Line/Triangle/Square）を選択・保存できる。operator全種類の直接編集とruntime受入は残存。
- Property: `getLayerPropertyGroups:4450`にShape/Appearance/Parameters/Contents/Stack/Operatorあり。`shape.path.keyframes`は内部のみでGroup露出なし。

## F1. D-1 頂点/tangent/segment overlay移植（実装済み・受入残）

- 現状: `drawShapeVertexOverlay` と controller の hover/drag state 接続を追加済み。tangent円、頂点選択、開/閉線、Shift時のsegment挿入マーカーをGPU overlayで描画する。
- 残存: 2/3/N頂点、mask競合、上限値、座標逆変換のruntime受入。
- 対象: `ArtifactCompositionRenderOverlay.cppm`、`ArtifactCompositionRenderController.cppm`描画経路。
- 検証: 2/3/N頂点のホバー/選択、座標逆変換1px以内、mask競合なし。

## F2. D-2 Rect角丸/Star内径/Polygon挿入ハンドル（実装済み・受入残）

- 現状: main VPのmousePress/Move/ReleaseにRect/Square corner radius、Star inner radius、custom polygon vertex/segment insertを接続済み。各ドラッグは専用Undo commandを積む。
- 残存: プリセットUIとの統合、ズーム/回転を含む操作感、runtime受入。
- クランプ: Rect `0<=r<=min(w,h)/2`、Star `0.0〜2.0`、編集モード外・既存頂点ホバー時は挿入しない。

## F3. D-3 ShapeプリセットUI

- 現状: 形状種別欄は選択中Shapeの編集に加え、新規Shape作成時の既定形状としても使う。種別はQSettingsに保存し、Shapeツールのドラッグ作成時に反映する。7種を既存のprimitive layer経路で扱う。
- 残存: Lineは水平線primitiveのため、ドラッグ方向を持つ斜線作成ではない。作成中previewはprimitiveの輪郭ではなく矩形領域を表示する。
- 検証: ソース静的確認のみ。7種のドラッグ作成、保存、既存Shapeの種別変更、レイヤー選択時のmask動作はビルド・runtime未確認。

## F4. D-4 選択grammar完成（主要経路実装済み・受入残）

- 現状: main VPで`selectedShapePathVertices_`を保持し、Shift toggle/Ctrl add/plain replace、Ctrl+A、Escape、Delete/Backspace、path/polygonのUndoを接続済み。pending path中は編集削除を抑止する。
- 残存: marquee、multi-move、proportional、handle-only選択、runtime受入。
- 検証: 5頂点でShift複数/Ctrl追加/空クリック解除、Backspace削除→Undo、mask修飾子衝突なし。

## F5. D-5 operator HUD/ハンドル（部分実装・拡張残）

- 現状: operator HUD、Trim start/end/offset三角ハンドル、Offset/Pucker/Roundedのprimary handle、operator専用Undoを実装済み。Repeater等はHUD表示中心で、全operatorの直接編集ではない。
- 残存: Repeaterのcopies/offset/rotation/opacity直接編集、Wiggle/ZigZag/Wobble HUD、`evaluatePathAt`連動のruntime受入、`WigglePaths`常時キャッシュ回避のperf検証。
- 検証: Trimドラッグでクリップ、Repeater増減リアルタイム、Undo復元。

## F6. D-6 open/closed・smooth・corner/bezier

- 現状: custom path頂点の右クリックメニューにMake Smooth/CornerとOpen/Close Pathがあり、pending path中はhover頂点がないため無効。controller操作は既存のpath編集command経路を使用する。
- 検証: ソース静的確認のみ。開閉後の評価結果、smooth切替、Undo/Redoとruntime操作は未確認。

## F7. 1レイヤー複数シェイプ/グループ・Contents

- 現状: `ShapeStackNodeType{Path/Fill/Stroke/Operator}`・`shape.contents.*`・`shape.stack.*`あり。AEのContents>Group>Transform・Group Blend・Material相当なし。E判定は「最大・設計レビュー推奨」。
- 導入: Group{paths/fills/strokes/operators+Transform+blend+可視+名前}モデルへ移行。`toCoreShapeLayer()`を雛形に`ShapeGroup`接続。Group Transform→Layer Transform順評価、anchor先行設定、垂直順序の結果決定、collapse/命名整理。
- 検証: 共有operatorグループ vs 独立アニメの分離、stack順変更の見た目一致、保存復元。

## F8. Merge Paths本物化

- 現状: `AeOperators.ixx:147-231 MergePaths{Add/Subtract/Intersect/Difference/Merge}`は`united/subtracted/intersected/XOR/addPath+simplified`実装（QPainterPath依存の見栄）。Phase C設計メモはClipper2相当/even-odd booleanを検討。
- 導入: `ShapeOperatorType::Merge`新設済み前提で、triangulate前提の領域演算かClipper2相当へ置換。`ShapePath`単位apply。アプリ側は既存operator UI枠追加のみ。
- 検証: 自己交差パス・Inside/Outside輪郭の破綻確認、計算コスト計測（Mergeは高コストのため多用時注意表示）。

## F9. SVG入出力の完成

- 現状出力: Phase A実装済み。`<defs>` gradient、Inside/Outside輪郭化。制限=taper均一幅近似、conic線形近似、repeating/mirroredはspreadMethod。
- 不足入力: AEのDirect SVG Import→Create Shapes from Vector Layer相当なし。`fromPainterPath`・`triangulate`・OpenCV基盤はあり。
- 導入: SVG取込→Group/Path/Fill/Stroke/gradient/clip保持でShape化。画像→ベクター化(marching squares/輪郭トレース→`ShapePath`)は`IDEAS:5`として中難度・高インパクト。
- 検証: ブラウザ/Illustrator表示確認、gradient見栄差、モジュール再スキャン影響。

## F10. fill拡充（マルチストップ・noise・pattern）

- 現状確認（2026-09-25）: Shapeの多段階stopモデル、Shape/Core変換、Shape JSONの`fillGradStops`保存・復元、空配列時の旧2色評価、Property経由の更新は実装済み。Property EditorはJSON文字列直接編集から、stop一覧UI（追加・削除、位置、FloatColorPickerによる色編集）へ置換した。stop上限32、位置/色の範囲補正、同位置stopの入力順保持を実装した。旧JSONの読み込み形式と保存キーは維持。モデル側と編集UI側のJSON読み込みでは16Ki文字上限を設け、不正な非オブジェクト／空オブジェクトstop要素を読み飛ばす。legacy stop propertyとShape Contents各Fillのstop propertyをanimatable channelにし、既存keyframe／Undo経路でランプ全体を記録・復元する。停止点プロパティのkeyframeはShape専用JSONにも保存・復元し、再読込後も補間区間とInterpolationTypeを保持する。両端キーのstop数が一致する区間は位置/RGBAをキーのInterpolationType（Bezier、Catmull-Rom、Hermiteを含む）で評価し、トポロジーが異なる区間は破綻を避けてHoldする。ロービングキーも既存String channelに合わせてHoldへフォールバックする。Property Coreは現在区間の共有値スナップショットを返し、Shapeのフレーム評価でキー配列全体をコピーしない。legacy/Contents両方のGPU描画、legacyとContentsのQImage経路、Core変換、legacy Core SVG export、Contents SVG exportで評価時刻のstop列を参照する。GPU paint itemはstop列をspanで借用し、stop-vector複製を避ける。キーJSONは区間端点が変わった時だけ再パースし、ランプ補間結果はstop数上限32の事前確保領域で評価する。
- JSON受入: Property JSON文字列、legacyの`fillGradStops`配列、Contentsの`gradStops`配列は共通の32-stop parser/normalizerを通し、静的JSONも上限を超えるvector確保をしない。編集UI parserは入力16Ki文字で上限を設ける。
- 未完了: Planeとの共通編集・評価経路、GPU/ソフト描画の視覚比較とruntime受入。noise/pattern fillも未実装。したがってF10全体は部分実装。
- 次段階: Planeとのstop編集・評価共有、`ProceduralTexture`（7種noise+GPU）の露出、pattern/checker/brick/hex/dot/stripe/halftone/scanlineのGenerator化。NoiseはGPU優先でCore bridgeを追加する。パレット自動配色・深度パララックス・ピクセルソートは`IDEAS:6,7,9`の任意拡張。
- 検証待ち: 旧JSON再読込互換、新stopキー保存往復の実行確認、無KF時ゼロコストearly-return、GPU/ソフト視覚比較。既存の`ArtifactTestShapePath`には停止点／stop-keyframeのassertがなく、この回帰確認は未カバー。ビルド・テスト・runtime検証は未実施。

## F11. stroke表現完成（Taper/Wave・Trim同時/個別・Repeater順）

- 現状: taper start/end・gradient stroke・配置あり。AEのTaper Length/Start-End Ease・Wave Amount/Phase・`Trim Multiple Shapes`(Simultaneously/Individually)・Repeater Composite順・Offset回転なし。
- 導入: Taper長さ/ease、Wave量/位相、Trim同時/個別enum、Repeater{copies/offset/order/Transform}完全化、Offset均等膨張のstroke幅保持。式(`360/copies`自動分配等)の下地としてSlider/Angle/Checkbox連携を見込む。
- 検証: draw-on/reveal定番動作、staggerタイミング、放射バースト12コピー30°等のprocedural rig再現。

## F12. 変換・選択・path相互運用

- 現状: Solo Viewに空クリック追記・`Key_F` ToggleClosed・`Key_A`全選択・`Key_E` Extrudeあり。`Convert To Editable Path`(primitive焼付け+bounds/anchor/gradient/stroke維持+JSON互換)はM-LE-3 Phase4の本体で残存。marquee/additive/multi-translate・proportional・split・broken/mirrored切替・handle only選択・reset/flatten/alignは未整理。polygon削除のみでpath削除・複製(duplicate)はcontext menu/未確認混在。
- 導入: Convert(one-way明示+可能ならrevert/reset)、marquee/additive/multi-move、proportional、split、smooth/corner/broken+mirror/independent、handle可視化分離。main VPのpath/polygon頂点削除とUndoは実装済み。Mask↔Shape双方向変換は実装済み(action+Undo+transform保持)のためVPメニュー到達性のみ整備。`shape.type`のproperty enum寄りを構造操作へ分離。
- 検証: M-LE-3 validation checklist 7項目（入室・追加/移動/削除/複数・insert・tangent視認・convert・stack並替・保存復元）。

## F13. パス・operatorアニメと式

- 現状: 頂点KF線形補間（topology不一致snap）、operator Float化（Repeater String anchor/position/scale含む拡張がP1残存）、`shape.path.keyframes`のtimeline表示未統合、Timeline `U`・graph tangent(Roving/Hold)操作性残存、pick-whip・AE stdlib拡充残存。
- 導入: パスモーフィングUI（星→ハート→歯車、`ShapePath::interpolate`接続が費用対効果最大）+頂点数/順序ガイド、Group Transform切分け rig（path KFは形状変化のみ）、operator HUDのKF打鍵・timeline/GUI表示、Trim/Offset/幅/Group transformの式・Slider一括駆動・loop・wiggle restraint、text/path binding・レイヤー沿配置（`pointAtPercent/tangent/normal/sampleEquidistant`活用）は中〜高難度の拡張。
- 検証: width KFのGPU/互換両追従、wiggle併用非影響、KF有り再構築コスト計測。

## F14. runtime受入・perf・互換

- 未検証: pixel parity、保存/再読込往復(gtest固定)、GPU/Software/Queue 3経路一致、softブレンド縮退(Hue/Sat/Color/Lumi→SourceOver差)、無効ログの`QString::arg`先行評価禁止・category遅延・flush境界、hotpathの値型/固定容量/作業領域/プール原則。
- 導入: KF不在early-return維持、頂点KF中はnative geometry bypass+surface cacheへframe混入済み方針の実測、旧プロジェクト再読込、TimeRemap無効時非変化、`WigglePaths`無条件trueの条件化、座標・z-order・上限フェイルセーフの受入表（2D_SHAPE validation + Phase D完了条件の同時操作回帰）。
- 順序推奨: D-1→D-2→D-4→D-5→D-3→D-6、1機能ずつ上げ、各Phaseでビルド確認（AGENTS.md制約）。Eは最後。

## 関連

- `docs/done/MILESTONE_LAYER_EDIT_2026-04-25.md`、`docs/done/MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md`、`docs/planned/MILESTONE_SHAPE_SVG_EXPORT_AND_KEYFRAME_VERIFY_2026-08-22.md`(Phase D-1〜D-6)、`docs/planned/MILESTONE_LAYER_FEATURE_EXPANSION_GAPS_2026-08-21.md`(P1)、`docs/planned/MILESTONE_SHAPE_PATH_CORE_IMPLEMENTATION_2026-04-16.md`、`docs/shared/IDEAS_PLANE_SHAPE_IMAGE_CREATIVE_2026-08-18.md`、`docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md:328-329`。
