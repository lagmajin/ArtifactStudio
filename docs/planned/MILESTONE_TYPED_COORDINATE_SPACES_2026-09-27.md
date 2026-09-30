# 座標空間・単位の強型化マイルストーン

**最終更新:** 2026-09-30

**ステータス:** In Progress

## 目的

2D / 3D、UI、Viewport、Composition、Layer、World、Screen の座標や単位を、同じ数値ベクトルとして誤って演算・受け渡しできないようにする。異なる空間・単位の混在は警告ではなくコンパイルエラーにし、変換は名前のある明示関数だけに限定する。既存の描画・入力・レンダリングの挙動は維持する。

## 現状と関連契約

- `ArtifactCore/include/Math/Vec.ixx` の `vec2` / `vec3` は glm の別名であり、座標空間を表現しない。Qt 境界は明示変換を既に採用している。
- 3D ギズモでは `QVector3D` など同じ型が local/world の点と方向を表す。`docs/technical/GIZMO_2D_3D_CAMERA_COORDINATE_CONTRACT_2026-07-16.md` は、描画・ピッキング・ドラッグで同一の view/projection 組を使うことを要求する。
- `docs/done/COORDINATE_SYSTEMS.md` は主に2Dの Local → Composition → View → Screen を説明する。3D の World、カメラの View、NDC、Qt 論理ピクセル、物理ピクセルの詳細は型導入前に対象経路ごとに確定する。
- `docs/technical/HOT_PATH_RULES.md` に従い、ドラッグやフレーム描画の型変換で確保・同期・画像変換を増やさない。

## 型の契約

1. 空間タグ付きの `Point2/Point3` と `Vector2/Vector3` を値型として定義する。最初は実際に通る経路だけにタグを設け、名前だけの空間を増やさない。StrongTypeを新しい引数・戻り値・保持stateの境界に使い、同じ関数の中で局所的に使うQt/glm計算まで一律に包まない。
2. 同じ空間では `Point - Point = Vector`、`Point ± Vector = Point`、`Vector ± Vector = Vector` を許す。`Point + Point`、異空間同士の演算、2D と 3D の暗黙混合を許さない。
3. 距離、角度、正規化値などの単位を必要な API で型にする。少なくとも degrees/radians、Qt 論理 px/物理 px、composition px、world length を区別する。無次元の倍率は明示する。
4. `LayerLocal` のタグだけではレイヤー個体を識別できない。ローカル↔ワールド変換は対象レイヤーまたはその確定済み transform を引数に要求する。親子階層と transform の時点も呼び出し契約に含める。
5. 空間をまたぐ変換には `localToWorld`、`worldToClip`、`clipToViewport` など入出力空間を示す名前を付ける。逆行列が失敗し得る変換には失敗を表す結果を返す。暗黙コンストラクタや暗黙 `glm` / Qt 変換は設けない。1つの処理経路で同じ値を何度も包み直さず、意味が変わるAPI境界で変換する。
6. 位置の変換と方向の変換を分ける。方向には平行移動を適用せず、法線には必要な逆転置を適用する。行列は `From` / `To` の空間を型で表し、乗算順を型で制約する。
7. 格納・シリアライズ・シェーダー・Qt・glm との境界では、明示的な変換を使う。互換APIとの移行境界は対象経路ごとに認め、変換箇所を絞って追跡する。型付きの意味が必要な公開APIや共有stateでは生ベクトルに戻さないが、純粋な局所計算を全面的に型変換で覆わない。

### 適用範囲と変更の進め方

- 優先するのは、単位・空間の取り違えが実際に起こり得る関数間の受け渡し、共有state、変換結果の境界。安全性に寄与しない一時ラッパーや同じ関数内の反復変換は追加しない。
- 既存の `QPointF` / `QVector3D` / glm API は段階移行する。型付き入口と旧型入口を大量に二重公開せず、移行範囲をひとまとまりにして互換境界を明示する。
- 既存挙動を変えずに安全境界を作れる局所単位で進める。座標契約や精度が未確定なら、型の追加より先に調査し、その経路を未完了として記録する。
- 各段階では型付き境界、明示変換位置、数値精度、既存変換式を静的に照合する。コンパイルや実機確認が許可されていない間は、検証済みと扱わない。

## 段階

**進捗 (2026-09-29):** Phase 1 の最小型基盤を `Math.Vec` に追加した。既存のglm APIを一括置換せず、座標契約が明確な境界から段階移行する。今回、明示ソースマニフェストへ `Math.Vec` を登録。Projected Frame corner/interior/selection-frame hit-testの入力、投影corner/guide point配列、scale dragのstart/fixed/handle point、snap helperのpointer入力・戻り値と保持state、過去frame plane cornerとcenter hit-test入力を `ScreenPhysicalPoint2` に移した。複数選択の投影boundsにはdouble精度の `QRectF` を保持する `ScreenPhysicalBounds2` wrapperを使い、selection-frame hit-testとscale fixed-point helperでComposition rect等を誤って渡せないようにした。スケール倍率・履歴frame距離のdeltaは型付き物理点差分から計算する。旧QPointF geometry / draw / ray APIの境界でのみ明示変換し、操作・見た目の仕様は変更していない。型の実コンパイル確認と各演算の否定ケース確認は、ビルド／テスト未許可のため保留。
**進め方の調整 (2026-09-29):** 型安全の対象は座標の意味がAPI境界・共有stateで失われる箇所に絞り、同じ処理内部のQt/glm geometry演算を一律に包まない。明示変換は意味が変わる境界に集め、旧API互換は移行単位ごとの限定された入口・出口として扱う。これにより空間混同の拒否は維持しながら、日常的な実装での包み直しと重複APIを抑える。

### 制約緩和の管理ルール

型付けが日常実装の妨げになる場合も、異なる空間・単位を暗黙に混ぜる演算子や暗黙変換は追加しない。負担は、次の条件を満たす局所的な移行境界で減らす。

- 生のQt / glm / scalar APIとの境界に、空間または単位が名前から分かる明示変換を置き、変換箇所を共有 helper に集約する。
- 同一の意味を保つ単純な受け渡しは型付きのままとし、数値化は保存形式・Qt・GPU・既存scalar APIなど意味が変わる境界だけにする。
- 互換入口を残す場合は対象APIと撤去条件をマイルストーンに記録し、無関係な呼び出し元へ広げない。
- 例外を追加する提案には、解消する具体的な摩擦、型検査が失われる範囲、代替案、静的な拒否条件を併記する。公開APIの暗黙変換や異種型演算を許す変更は、このマイルストーンの通常実装では行わず、明示的な設計判断を要する。
- 座標・単位契約が未確定な場合は、扱いやすさのために単位を統合せず、根拠と次の確認項目をInsightに記録して該当経路を保留する。

この手続きにより、局所的なinteropの手間は下げる一方、呼び出し側が型制約を簡単に無効化できる状態にはしない。
**進捗 (2026-09-29, Phase 2):** 3Dギズモの`hitTest` / drag API入力を`WorldRay`（`WorldPoint3` origin + `WorldVector3` direction）に変更した。controllerの既存`Ray`からギズモmoduleの`toWorldRay()`で明示変換し、ギズモ内部は従来の`QVector3D`演算へ入口で戻す。near-plane原点、world方向、正規化処理、view/projection共有、交差計算式は維持。型の実コンパイルとcamera/orientation両モードの操作確認は未実施。`setTransform` positionの空間はparented layerでのglobal/local意味を確認してから次段階にする。
**進捗 (2026-09-29, Phase 2):** `CompositionRenderController::createPickingRay`の戻り値も`WorldRay`に変更。near/far unprojectionと正規化後の値を直接world point/directionへ詰め、gizmo操作へは包み直さず渡す。モデル／カメラ／過去frame用の既存Qt-vector交差関数へ出る箇所だけ`pickingLegacyRayFromWorldRay`で明示unwrapする。unprojection式、選択カメラ行列、ray値は維持。コンパイル・実操作確認は未実施。
**進捗 (2026-09-29, Phase 3):** `CompositionRenderController::layerAtViewportPos`の引数を`ScreenLogicalPoint2`へ変更。Qt mouse/context/drop位置からは`screenLogicalPointFromQPointF()`で明示変換し、controller内で`toQPointF()`後に従来どおりDPRを乗じて物理座標のhit-testを実行する。画面位置を直接使う外部call siteは全て型変換を要求する。関数内の変換式・交差優先順は維持。型コンパイルとDPI runtime確認は未実施。
**進捗 (2026-09-29, Phase 3):** Composition viewportの`panBy`入力を`ScreenLogicalVector2`、`viewportPan`戻り値を`ScreenPhysicalVector2`へ変更。mouse drag / wheel / momentumの既存Qt deltaは名前付きhelperでlogical vectorへ変換し、controller内で明示DPR変換してrendererへ渡す。従来どおりdeltaにDPRを1回掛け、有限値検査・履歴・pan式は維持。pan getterの呼び出し元は現状なし。コンパイル・DPR別操作確認は未実施。
**進捗 (2026-09-29, Phase 3):** Composition viewportのresize入口を`ScreenLogicalExtent2`に変更。widget/editorのlogical width/heightを名前付きfactoryで渡し、controller内で`ScreenPhysicalExtent2`へDPR変換してrendererへ設定する。resize比較閾値、swapchain順序、physical host stateは維持。コンパイル・複数DPR resize確認は未実施。
**進捗 (2026-09-29, Phase 3):** Work Cursorの2D canvas setter/getterを`CompositionPoint2`化。Composition中央への配置と選択layer centerから、Composition pointを明示的に渡す。内部QPointF保持との変換はcontroller境界へ限定し、finite fallback・visibility・spatial flagの挙動を維持。3D World position APIはscreen→world契約が未確定のため対象外。コンパイル・操作確認は未実施。
**進捗 (2026-09-29, Phase 3):** `zoomInAt` / `zoomOutAt` / `zoomAtFactor`、Box Zoom開始・更新、Tumble Pivot設定も`ScreenLogicalPoint2`入力へ変更。Qtイベントからの変換を名前付きhelperに統一し、画面論理pxをDPR適用前の境界で要求する。
**進捗 (2026-09-29, Phase 3):** `focusActiveCameraAtViewportPos` / `resetProjectedFrameHandleAt`も`ScreenLogicalPoint2`入力へ変更。いずれもQtイベント論理pxにDPRを掛けてpick用物理画面点を作るAPIで、ray生成・corner/interior判定の既存式は維持。
**進捗 (2026-09-29, Phase 3):** `isTransformGizmoHovered`も`ScreenLogicalPoint2`を要求し、construction hit-test用のDPR変換とText/2D gizmo APIへのlogical point引き渡しを境界内で明示した。hover対象・判定順は変更していない。
**進捗 (2026-09-29, Phase 3):** `adjustMagnifierScaleAt`のlogical point引数も`ScreenLogicalPoint2`に変更し、wheel event位置からの名前付き変換を追加。内部DPR変換とmagnifier bounds hit-testは維持。
**進捗 (2026-09-29, Phase 3):** `placeWorkCursorAtViewportPos`を`ScreenLogicalPoint2`入力にし、全Qt logical event / widget-center呼び出し元を明示変換。レンダラーのviewport寸法・panが物理pxであることに合わせ、`viewportToCanvas`前にDPRを適用する。DPR=1では同じ座標、HiDPI配置は修正となる。runtime確認は未実施。
**進捗 (2026-09-29, Phase 3):** Composition viewport内コマンド／コンテキスト／pie menuの配置API、ポインター追従・項目hit-test APIを`ScreenLogicalPoint2`に変更。controller入口で名前付き`toScreenPhysical(point, DPR)`を一度だけ適用し、保持位置とhit-testは`ScreenPhysicalPoint2`へ統一した。描画rectは既存どおり物理viewport寸法で作る。Qtイベント、WM経路、widget cursor位置のcall siteを型付き変換に移行。overlayフォントメトリクスと固定寸法のDPI scaling契約には未確定点が残るため幾何寸法は変更せず、コンパイル・高DPI runtime確認も未実施。
**進捗 (2026-09-29, Phase 3):** Interactive Render Region handle hit-test / drag開始・更新APIを`ScreenLogicalPoint2`化し、drag開始位置stateもlogical pointで保持。ドラッグdeltaはlogical vectorとして計算し、既存どおりzoomを一度適用する。handle hit-testだけは既存rect・panを物理viewport値として使うため、比較点と12 logical px hit radiusをDPR境界でphysical化した。ハンドルID・最小寸法・操作式は維持。コンパイル・DPR別操作確認は未実施。
**型基盤追記 (2026-09-29):** logical vector → physical vectorの明示DPR変換を追加。点位置変換と方向／delta変換を別overloadにし、point/vectorの意味を保つ。
**進捗 (2026-09-29, Phase 2):** `nudgeSelectedRigBoneRotation`のdelta引数を生`float`から`Units::Degrees`へ移行し、±15°呼び出し元も明示的なdegree値にした。レイヤーへ書き戻す既存float角度APIとの境界でのみ`.value`を取り出し、Undo・更新順・回転量は維持。コンパイル・操作確認は未実施。
**進捗 (2026-09-29, Phase 2):** `ArtifactCameraLayer::fov` / `setFov`とCreate Camera dialogのFOV取得値を`Units::Degrees`に変更。UI dialogからcamera setterへ度数型のまま渡し、property deserializationと焦点距離→FOV計算でも度数を明示する。投影・frustum描画・property/overlay表示など既存float境界では`.value`を取り出す。値範囲・fallback・manual FOV挙動は維持。Camera/Dialog moduleが`Math.Vec`をimportするため依存変更を含み、コンパイル・カメラUI確認は未実施。
**進捗 (2026-09-29, Phase 2):** camera focal-length getter/setterとCreate Camera dialog getterを`Units::Millimeters`にした。JSON/property restoreとUI dialogから型のまま受け渡し、焦点距離→FOV変換、表示、legacy renderer/property scalar境界でのみ値を取り出す。焦点距離の式・範囲・既定値は維持。単位の暗黙変換拒否static_assertを追加。Camera/DialogのMath.Vec依存と各利用経路のコンパイル・実機確認は未実施。
**進捗 (2026-09-29, Phase 2):** `Units::Meters`を追加し、`ArtifactCameraLayer::ipd` getter/setterと`StereoCamera::fromHmd`入力・保持値を型付け。Inspector/JSON scalarの境界だけで`.value`へ変換し、rendererではMetersのままStereoCameraへ渡す。eye offset計算は型付き値の数値を入口で取り出し、計算値は維持。Meters/Millimeters間の暗黙変換拒否static_assertを追加。`Core.Camera`にMath.Vecのmodule importが加わり、コンパイル・stereo経路確認は未実施。
**進捗 (2026-09-29, Phase 2):** `Units::Pixels`を追加し、Camera Layerのzoom/focus distance/ortho width・height/near・far clipのgetter/setter、Create Camera dialogのzoom/focus getter、`CameraDOFParameters`の距離値、`StereoCamera`のnear/far保持値と`fromHmd`引数を型付け。Inspector/JSONやprojection・DOF renderer scalar境界では`.value`またはPixels wrapperを明示し、px契約をcamera APIからprojection / stereo / DOFまで保持する。値範囲・投影式は変更していない。コンパイル・カメラ生成/property restore・DOF/stereo runtime確認は未実施。
**型契約補足 (2026-09-29):** `Meters` / `Millimeters`と生`float`の双方向暗黙変換も拒否する`static_assert`を追加。数値境界で`.value`または単位を付けたaggregate初期化が必要。
**型契約補足 (2026-09-29):** `Meters` / `Millimeters` / `Pixels` / `Degrees` / `Radians`の全異種ペアについて、双方向の暗黙変換拒否を`static_assert`で固定。型の追加・変更時に、制約が意図せず弱まった場合のコンパイル時ガードとする。
**座標不整合の修正:** Box Zoomは開始・更新位置をDPR適用後の物理pxで保持していたが、終了時の物理中心を論理px入力の`zoomAtFactor`へそのまま渡しており、HiDPIではアンカーにDPRが二重適用されていた。物理中心をDPRで論理pxへ戻して型付きAPIへ渡す。DPR=1では同じアンカー、DPR>1では選択領域の論理中心を正しく維持する見込み。式の静的照合のみで、runtime未確認。
**未解決の契約 (2026-09-29):** `position3D()`はlocal transform値を返し、`getGlobalTransform4x4()`は親を再帰合成する。gizmo同期ではglobal matrix由来のbasisと`position3D()`を併用し、hit-testはworld rayとgizmo positionを比較する。ドラッグ中のギズモ位置はUndo snapshot・制約計算・各レイヤーへの書き戻しにも使われるため、親付きlayerで空間混在の可能性がある一方、単純なworld位置への差し替えでは既存編集値が変わる。意図とruntime影響は未確認のため、今回の型導入では位置補正をせず、position APIを強型化する前に座標契約を確定する。

### Phase 0 — 経路と単位の棚卸し

- ギズモの描画、ピッキング、ドラッグに使う関数の引数・戻り値、行列の向き、単位、Qt 論理 px と物理 px の境界を表にする。
- 2D Composition の pan/zoom と 3D カメラの view/projection を別経路として記録する。
- 既存の座標仕様と実装に不一致があれば、実装を推測で変更せず、対象経路の契約を先に確定する。

### Phase 1 — 最小の型基盤

- 既存の数学型を一括置換せず、点・方向・空間・主要単位の小さな型を追加する。
- 型の演算可否と明示変換をコンパイル時に確認できる受け入れ条件を用意する。
- C++20 module の依存循環と登録範囲を確認し、新規 module より既存ファイルへの追加を優先する。
- 2026-09-29: `Math.Vec`内に非exportのコンパイル時契約を追加。同空間のpoint差・point+vector・vector加算、異空間point差の拒否、point+pointの拒否、degrees同士の演算、degrees/radians混合演算と暗黙変換の拒否を`static_assert`で固定。moduleをコンパイルして確認する工程は未実施（ビルド未許可）。

### Phase 2 — 3D ギズモ境界の移行

- 描画・ピッキング・ドラッグで共有するカメラ入力、ray、ヒット結果、操作 delta の関数境界を強型化する。
- 同じフレームの view/projection 組を使用する既存契約を維持する。2D ギズモ、HUD、ショートカット、キャンバス内の見た目は変更しない。
- 各経路で生ベクトルへの変換箇所を明示し、異空間の値がそのまま渡せないことを確認する。
- 2026-09-29: 3Dギズモ公開のhit-test / begin / constrain / update drag入力を`WorldRay`に変更。controllerではmoduleの`toWorldRay()`で既存rayを型付きに変換し、内部実装はAPI入口で一度legacy `Ray`へ変換する。既存のnear/far unprojectionとview/projection経路は維持。全呼び出し元、module依存、コンパイル、実機確認は未検証。
- 2026-09-29: `Math.Vec`に相互変換関数付きの`Units::Degrees` / `Units::Radians`を追加。3D gizmoのrotation ring start angleとdegrees snap increment / deltaを度数型にし、`atan2`出力は名前付き`toDegrees()`を通す。`Artifact3DGizmo::setTransform`のEuler回転入力と`rotation()`戻り値も`EulerDegrees3`にし、controllerのlegacy `QVector3D` state / layer API境界では名前付き変換を使う。Qt quaternion境界ではdegreesを明示的に取り出す。コンパイルと角度 parity確認は未実施。
- 2026-09-29: `Artifact3DGizmo::setLocalBasis` / `setViewBasis`の軸入力と`dragAxisDirection()`の戻り値を`WorldVector3`にした。global transform / inverse viewから得たQt軸を呼び出し境界で明示変換し、gizmo内部では既存のQt vector計算へ一度戻す。local/view basisがworld座標で表現される契約をAPIコメントに記載。コンパイルと描画・hit-test parity確認は未実施。
- 2026-09-29: `createPickingRay`の戻り値を`WorldRay`にし、物理画面点からworld空間へのunprojection境界で型付けした。gizmo callsiteでは従来の`Ray`→`WorldRay`包み直しを除去し、既存Qt交差計算へ渡す場合だけ`pickingLegacyRayFromWorldRay`で明示変換する。値・カメラ行列・正規化式は維持。コンパイルとruntime確認は未実施。
- 2026-09-29: `position3D()`がlocal transform値、`getGlobalTransform4x4()`が親合成後のglobal transformであることを実装で確認。gizmo setupはglobal basisとlocal positionを併用し、hit-testはrayとgizmo positionを交差判定する。parented layerへのruntime影響は未検証。意図を確認するまでposition APIを型変更・補正しない。
- 2026-09-29: ドラッグ書き戻しまで静的追跡。`handleMouseMove`はgizmo position差分を軸制約・snapshot計算に利用し、単一／複数選択の編集経路でTransform値へ反映するため、同期位置だけをworld originに差し替えるとドラッグ値の意味も変わる。型境界の整備は継続するが、この未確定契約をまたぐ位置の型付け・変換追加は保留。

**進捗 (2026-09-29, Phase 2):** `Artifact3DGizmo::setScale` / `scale`を無次元の`Units::Scale3`へ変更。controllerの既存QVector3D計算・Undo state・overlay表示との境界だけで名前付き相互変換を使い、位置ベクトルをscale APIへ渡せないようにした。scale値、符号、クランプ、操作式は維持。コンパイル・scale操作/Undo確認は未実施。

**進捗 (2026-09-30, Phase 2):** `ArtifactAbstractLayer::position3D()` / `setPosition3D()`を親変換前の authored transform値として`LayerLocalPoint3`に変更。controllerのgizmo/Undo用`QVector3D` snapshotでは名前付き変換を使い、layer位置の書き込みでは型付きaggregateを渡す。数値・アニメーション時刻・保存形式は変更していない。これはlocal/world gizmo pivotの不整合を修正するものではなく、座標契約をAPI型で露出する段階。モジュールコンパイル、親付きlayerでのgizmo描画・hit-test・drag/Undo確認は未実施。

**進捗 (2026-09-30, Phase 2):** `Artifact3DGizmo::setTransform` / `position` と内部controller API境界のpositionを`WorldPoint3`化。3D model layerのgizmo同期pivotは親を含むglobal transformのoriginから得るようにし、親付きlayerの通常moveではworld gizmo positionを親global inverseでauthored local positionへ戻す。親inverseが特異な場合はlayer位置を据え置く。既存gizmo geometryのQt演算は内部境界に維持し、drag式・表示・undo snapshotの数値型は全面置換していない。local/worldを同じ`QVector3D`のまま混在させる旧gizmo同期経路のうち単一3D layerを修正。複数選択pivotと projected-frame経路のruntime・親付きdrag/undo・module compileは未確認。

**進捗 (2026-09-30, Phase 2):** `GizmoGroupLayerState::worldAnchor`、controllerのprojected-frame start anchor / group pivot stateも`WorldPoint3`に変更。world point同士の差を`WorldVector3`としてから既存Qt group geometryへ明示変換し、world translation vectorを足して新world anchorを構築。各親のinverseで戻した位置は`LayerLocalPoint3`を通して旧Undo/property snapshotへ渡す。保存・runtime式の値は維持する意図だが、型整合のコンパイル・複数選択group drag/Undo・projected-frame操作は未確認。

**進捗 (2026-09-30, Phase 2):** projected-frame開始軸と複数選択gizmo basisの共有stateを`WorldVector3`に変更。global transform / inverse viewから得たQt方向をstateへ格納する境界で型付けし、snap制約・group scaling/rotationの既存Qt演算直前だけ明示変換する。`setLocalBasis`へはstateの型付き方向を直接渡す。軸正規化・basis・操作式は変更していない。module compileとShift-constrained move、combined projected group transformのruntime確認は未実施。

**進捗 (2026-09-30, Phase 2):** `GizmoTransformSnapshot::position`をauthored値の`LayerLocalPoint3`に変更し、visual gizmoの開始pivotは別の`WorldPoint3` stateへ分離。Undo/property snapshotのposition比較・保存反映・keyframe値はlocal pointのまま扱う。通常moveのworld deltaは親global inverseの`mapVector`でlocal vectorに変換し、`LayerLocalPoint3 + LayerLocalVector3`でlocal positionを更新する。group / projected-frameのinverse結果も型付きlocal pointとして保存する。関数型・座標契約は静的に照合したが、コンパイル・親あり/なしのmove、snap、Undo/redo parityは未確認。
**進捗 (2026-09-29, Phase 2):** `Artifact3DGizmo::setBoundingBox`のmin/maxを`LayerLocalPoint3`に変更。選択model mesh boundsとlayer local bounds fallbackをnamed conversionで渡し、gizmo入口で既存QVector3D geometryへ変換する。finite/order検査、bounds保持、drag計算は維持。Boundsは現在のtarget layerに属し、layer identity自体はAPI型で符号化していない。コンパイル・model/parent transform時のbounds parity確認は未実施。

**進捗 (2026-09-30, Phase 2):** Modal 3D gizmo numeric inputを操作別に型分離。Moveは`WorldLength`、Rotateは`Degrees`、Scaleは`ScaleFactor`で受け、controller/editorの入力分岐とgizmo setterでmodeも照合する。Frame dimension比率も`ScaleFactor`で渡す。既存の数値式と表示は維持。コンパイル / keyboard modal操作 / Undo未確認。

**進捗 (2026-09-30, Phase 3):** `CompositionRenderController::handleMouseMove`の公開境界を`ScreenLogicalPoint2`に変更し、editor/widgetのQtイベントとcontroller内の論理座標再入呼び出しを全て`screenLogicalPointFromQPointF()`経由に統一。controller内部で物理pxへ変換するDPR適用は従来どおり1回。gizmo内部の物理座標処理、描画・入力挙動は変更していない。型コンパイルと複数DPRでのドラッグ確認は未実施。

**進捗 (2026-09-30, Phase 3):** modal gizmo開始・軸拘束・数値入力・Frame Size badge開始APIも`ScreenLogicalPoint2`を要求するよう変更。Frame Sizeの入力値は`Units::Pixels`にし、ScaleFactor算出時にのみ数値化する。EditorのQt cursor/event位置は入口で明示変換し、controller内のDPR変換は従来どおり1回。入力値・Undo経路・画面表示は維持。コンパイル、modal操作、高DPI、Undo確認は未実施。

**進捗 (2026-09-30, Phase 3):** custom shape param/polygon/operator/path vertex drag・hover APIを`ScreenPhysicalPoint2`化。controllerのmouse event入口で既存logical→physical変換済みpointを渡し、各API内部では既存QPointF geometry演算へ明示変換する。Puppet pin wheel hit-testもScreenPhysicalPoint2を要求し、EditorでQt logical位置をwidget DPRで一度変換する。判定半径・drag式・挙動は維持。コンパイル、DPR別のshape edit・pin wheel確認は未実施。

**進捗 (2026-09-30, Phase 3):** `editTextAtViewport`を`ScreenLogicalPoint2`入力に変更し、Qt mouse eventから明示変換。controller内でrendererへ渡す前にDPRを一度適用して物理viewport位置にし、選択中／未選択text layerの両hit-testで共用する。既存のcanvas→layer判定・選択・編集dialog経路を維持。HiDPIでのクリック位置は修正となる見込みだが、実機確認は未実施。

**進捗 (2026-09-30, Phase 3):** `cursorShapeForViewportPos`を`ScreenLogicalPoint2`入力にし、EditorのQt位置から明示変換。controller内でcamera POI handle比較用に`toScreenPhysical()`を使う。gizmo cursor APIへ渡す座標計算とcursor選択順は維持。コンパイル・複数DPRのcursor確認は未実施。

**進捗 (2026-09-30, Phase 3):** `createTextLayerAtCanvas`入力とText toolのstart/current canvas stateを`CompositionPoint2`化。`viewportToCanvas`結果から型付き値を保持し、点差分はQt描画deltaを作る箇所、矩形はoutline描画箇所、layer transform保存は既存API境界でのみQPointF/scalarへ明示変換する。点／box textの配置式、box size、選択更新は維持。コンパイル・text layer生成確認は未実施。

**進捗 (2026-09-30, Phase 3):** `snapCanvasToGrid`の入出力を`CompositionPoint2`に変更。Pen vertex入力とWidget側のdrag Composition位置を明示変換し、QPointF grid geometry境界で明示unwrapする。Cartesian / polar / isometricの既存丸め式・spacing・ガイド判定を維持。コンパイル・各grid modeの操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Tumble PivotのComposition canvas位置保持stateとgetterを`CompositionPoint2`化。Viewport→canvas setterの変換結果を型付きで保持し、view matrix targetだけ明示的にQPointFへ変換、marker描画ではtyped x/yを使用する。座標・履歴・marker挙動は維持。コンパイル・spatial orientation操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Rectangle / Ellipse / Shape toolのdrag開始・現在canvas point stateを`CompositionPoint2`化。Viewport hit位置からcanvas変換後に型付きで保持し、modifier drag計算は既存QPointF geometry内、Mask / Shape生成やpreview rectangleでは専用`dragRectFromPoints(CompositionPoint2, CompositionPoint2, ...)`境界で明示unwrapする。Alt中心描画・Shift比率制約・mask/shape生成式は維持。コンパイル・tool別操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Mask rubber-band selectionのstart/current Composition canvas stateとmask snap preview位置を`CompositionPoint2`化。`viewportToCanvas`結果を型付きで保持し、threshold delta / selection QRectF / crosshair描画などQt geometry境界のみ明示変換する。選択範囲・snap候補・threshold計算は維持。コンパイル・mask vertex selection操作は未確認。

**進捗 (2026-09-30, Phase 3):** Color Sampler overlayのcanvas位置stateを`CompositionPoint2`化。`viewportToCanvas`出力と同じfloat精度で保持し、既存の色／pixel／layer比較、0.5 canvas-unit変化閾値、表示書式は維持。コンパイル・sampler表示確認は未実施。

**進捗 (2026-09-30, Phase 3):** Brush cursorとClone Stamp source/start/lastのcanvas stateを`CompositionPoint2`化。viewport→canvas値を型付きで保持し、PaintLayerのlegacy QPointF API・clone aligned delta計算だけ明示変換する。cursor/source marker描画はtyped値を使い、clone spacing閾値と描画位置を維持。コンパイル・ブラシ／clone操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Motion Sketch last-sampleとPuppet pin drag-startのcanvas位置stateを`CompositionPoint2`化。`viewportToCanvas`から型付き保存し、MotionSketchの既存QPointF APIとPuppet pin delta演算でのみ明示変換する。sample順・Shift軸拘束・drag位置は変更していない。コンパイル・sketch/pin操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** RenderWidgetのlayer drag開始Composition point stateを`CompositionPoint2`化。`viewportToCanvas`から直接型付き保持し、shift拘束・center snap・grid snapを含む既存QPointF delta計算との境界で明示変換する。snap式・drag mode・transform適用値は維持。コンパイル・移動／snap操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Motion Path drag開始APIを`CompositionPoint2`入力に変更。Composition位置は親layer逆変換を適用する一時入力に限定し、共有stateには変換後のlocal startだけを従来どおりQPointF精度で保持する。canvas/local startの意味を混同しないようにしつつ、親逆変換とdrag挙動を維持。コンパイル・motion path drag確認は未実施。

**進捗 (2026-09-30, Phase 3):** RenderWidgetのSmooth Zoom canvas anchor stateを`CompositionPoint2`、viewport anchorを`ScreenLogicalPoint2`で保持。canvas anchorは`viewportToCanvas`／Composition centerから型付きで作る。zoom補間・panアンカー式は維持。コンパイル・smooth zoom確認は未実施。

**進捗 (2026-09-30, Phase 3):** Composition Render Widgetのpan momentum velocityを`ScreenLogicalVector2`（論理px/ms）に変更。mouse deltaから名前付きfactory経由で作り、経過msで除算・clamp・decayをtyped vectorで行う。独立widgetのrendererはlogical `width()/height()`をviewport sizeに設定し、`ViewportTransformer`もDPR変換せず同じviewport座標で変換するため、drag／momentumのpan deltaも論理pxのままrendererへ渡す。別のCompositionRenderControllerがrendererへ物理pxを渡す経路とは契約が異なる。コンパイル・DPR別操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Composition Render Widgetの`viewportToCanvas`を通るhit-test、particle操作、layer drag開始／更新、smooth zoom anchor、zoom marquee cornerの入力を`ScreenLogicalPoint2`へ明示変換し、`compositionPointAtLogicalViewport()`で`CompositionPoint2`を返す。独立widgetのviewport transformが論理px設定であることを確認し、DPRを掛けずに既存の式へ入力する。Qt geometryが必要なhit-area境界だけQPointFへ明示変換する。コンパイル・HiDPI操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Composition Render WidgetのSmooth Zoom viewport入力を`ScreenLogicalPoint2`、倍率とアニメーション開始／目標zoom stateを既存`Units::ScaleFactor`へ変更。Qt wheel／tool event・widget中心から論理pointへ明示変換し、wheel／tool倍率・box zoom targetは生成時に明示wrapする。clampと補間は`.value`で既存float計算を維持し、renderer `setZoom(float)`境界でのみunwrapする。倍率範囲・イージング・操作量は変更していない。コンパイル・zoom操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Composition Render Widgetのrotation drag開始角とsnap設定stateを`Units::Degrees`で保持。renderer / 公開float設定APIとの境界だけ`.value`で数値化し、角度差・近中心fallback・modifier snap・範囲clampを既存degree計算のまま維持する。コンパイル・回転drag／snap操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Composition Render Widgetの公開`rotateCanvas`、rotation snap setter/getterも`Units::Degrees`で統一。この3 APIはrepository内に外部call siteがないことを検索確認した。module interfaceに`Math.Vec`をimportし、renderer `setRotation(float)`・有限値検査とclampの境界だけdegree値を数値化する。呼び出し側の明示単位指定が必要になる。module compileは未確認。

**進捗 (2026-09-30, Phase 3):** CompositionRenderControllerの`zoomAtFactor`倍率引数を`Units::ScaleFactor`へ変更。zoom in/out、Box Zoom target/current比、両Editor wheel経路で倍率を明示構築し、controller内の有限値検査・target clamp・renderer連携の式は維持する。viewport位置と無次元倍率を引数型で識別する。コンパイル・zoom操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** `rotateSelectedMaskVertices`に`LayerLocalPoint2`中心と`Units::Degrees`角度を要求する。描画・hit-testでmask vertexをlayer global transformに通すコードを根拠に、mask path格納座標はlayer localと判定した。degree→radian式および回転演算は維持する。repository内にこのAPIの外部call siteはない。module compile・mask回転操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** 2軸dimensionless multiplier用の`Units::Scale2`を追加し、ScaleFactor / Scale3 / CompositionVector2 / LayerLocalPoint2との暗黙混同をstatic_assertで拒否する。`scaleSelectedMaskVertices`を`LayerLocalPoint2 + Scale2`入力にし、既存の有限値検査・各vertex/tangentのscale式を維持する。外部call siteはrepository内になし。module compile・mask scale操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Mask feather/expansionの変更量を`Units::LayerLocalLength`とし、単体・複数選択APIおよびEditorの±5操作を型付き化。Mask path vertexがlocal座標で保存され、feather handleがlocal normalへ`path.feather()`を加える実装を根拠に長さ単位を確定した。値の加算・下限・Undo処理は維持。コンパイル・操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Mask opacityの増減引数をsigned normalized `Units::OpacityDelta`に変更し、single/selected API、context menuと角括弧キー操作の全call siteを型付き化。opacityの既存0..1 clampとundo/publication経路を維持。ScaleFactorやLayerLocalLength・Degrees・floatとの暗黙変換拒否をstatic_assertで追加。コンパイル・UI操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Pen mask feather handleとfeather/expansion segment dragの開始値・Y方向deltaを`Units::LayerLocalLength`で保持／計算。これらは逆global transform後の`localPos`と`MaskPath`属性から生成し、既存の`MaskPath` float setter境界でのみunwrapする。投影・tangent fallback・clamp・drag操作式は維持。コンパイル・mask drag確認は未実施。

**進捗 (2026-09-30, Phase 3):** `MaskPath`のuniform／horizontal／vertical／inner／outer featherとexpansionのgetter/setterを`Units::LayerLocalLength`へ統一。公開interfaceは型を利用側へ公開するため`export import Math.Vec`を追加。描画、property、Undo、project JSON/preset、Roto bridge、menu、Controllerの全呼び出しを型付き値または明示float境界へ移した。`MaskPathKeyframeSnapshot`、プロジェクト永続形式、RotoMask互換値はfloatのまま維持し、往復境界で明示変換する。既存clamp・保存値・補間値は同じfloat値。モジュール依存／コンパイル・保存読込・操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** `Core.Camera`の距離getter/setter、pan/dolly量、frame-all半径、sphere-fit半径を`Units::WorldLength`へ変更し、WorldLength同士の加減算とfloat係数による積商を追加。orbit/yaw/pitch/FOV APIは`Units::Degrees`、rad getterは`Units::Radians`、near/far projection clipは`Units::Pixels`に変更した。clamp・投影式・演算値は維持し、当該APIの外部repository call siteは見つからなかった。位置・targetの`float3`とdimensionless aspect ratio APIは今回対象外。モジュール依存／コンパイル・カメラ操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** `ArtifactLightLayer`のspot cone angle/featherとGOBO rotation、`CreateLightLayerDialog`の角度getter、`Core.Light`のspot cutoff/spot angle/GOBO rotation APIを`Units::Degrees`に変更。ダイアログからLayer・Core Light contract・MeshRendererの三角関数／shader入力境界までdegree型を保ち、JSON/QVariantとshader数値境界でのみ明示的に数値化する。クランプ・保存値・半角計算・描画計算は維持し、`Math.Vec` module importを追加。モジュール依存／コンパイル・UIと描画確認は未実施。

**進捗 (2026-09-30, Phase 3):** `Core.Light`のposition/direction getter/setterとdirectional/point/spot factoryを`WorldPoint3`／`WorldVector3`に変更。Controllerではglobal transformのorigin mapをWorld point、`mapVector`と正規化後の値をWorld vectorへ名前付きQt変換し、MeshRenderer内部ではtyped x/y/zをGPU定数へ格納する。位置と方向の取り違えをAPI境界で拒否し、正規化式・行列経路は維持する。呼び出し元はrenderer ControllerとCore factory内に限定。モジュール依存／コンパイル・ライト描画確認は未実施。

**進捗 (2026-09-30, Phase 3):** `DepthOfFieldSettings`のfocus/near/far distanceと最大CoC blur radiusを`Units::Pixels`、focal lengthを`Units::Millimeters`へ変更。CameraLayer由来のfocus/clip/lens値を型付きで渡し、fallback値も単位を明示。DOF shader parameter bufferへ詰めるときのみfloatへ数値化し、clamp・thin-lens式・pixel blur radius計算は維持する。`Math.Vec` module importを追加。モジュール依存／コンパイル・DOF表示確認は未実施。

**進捗 (2026-09-30, Phase 3):** CameraLayerのapertureと`CameraDOFParameters::apertureSize`、`DepthOfFieldSettings::fStop`を専用`Units::FStop`にし、DOF `cocScale`を`Units::ScaleFactor`へ変更。UI/property/JSONからLayerへ入る境界、正規化CoC倍率、GPU parameter buffer境界を明示し、f-stopの0 sentinel・blur scale・DOF式を維持する。`FStop`はScaleFactor、Pixels、Millimeters、floatとの暗黙変換をstatic_assertで拒否する。モジュール依存／コンパイル・UI/DOF操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Camera Point of Interest (POI) の座標契約を追跡した。Layer APIは未型付け`QVector3D`、propertyはpx表記だが、3D handleはprojection／world picking rayへ直接渡され、POI有効時の姿勢計算はcameraのlocal positionと保存POIを直接比較している。親付きcameraのglobal originとの差やview/picking座標の意味が揃っている根拠がなく、単純な`WorldPoint3`移行は既存の不整合を型で隠すため見送った。型移行前にPOI authored-spaceとparent transform契約の確定が必要。実装挙動は変更していない。

**進捗 (2026-09-30, Phase 3):** `applyProjectedFrameAnchorDelta`の公開引数を、3D gizmo APIのコメントが保証する`WorldVector3`へ変更。唯一の呼び出し元でgizmo legacy deltaを名前付きWorldVector3変換し、Controller内ではQTransformの2D compatibility計算へ入る直前に明示的にQVector3Dへ戻す。ゼロ判定・逆world transform・anchor／position補償式は従来値を維持し、WorldPoint3やlocal offsetの誤入力をAPI境界で拒否する。ビルド・アンカードラッグ確認は未実施。

**進捗 (2026-09-30, Phase 3):** `Artifact3DGizmo::anchorDragDelta()`の戻り値も`WorldVector3`にし、Gizmo内の既存QVector3D差分計算から戻る箇所に名前付き変換を配置。Controller→delta取得→anchor適用までWorldVector3を保持し、前項の引数呼び出しで重複変換を除いた。ギズモの内部演算とdelta値は変更していない。モジュール依存／コンパイル・アンカードラッグ確認は未実施。

**進捗 (2026-09-30, Phase 3):** `Artifact3DGizmo::Impl`のdrag-start ray stateを`WorldRay`に変更し、軸拘束切替時にlegacy `Ray`からWorldRayへ戻していた変換を除去。drag開始ではすでに型付きで受け取るWorldRayを直接保存し、既存の幾何演算へ入る地点のみlegacy `Ray`へ明示変換する。軌跡・演算値は変更していない。モジュールコンパイル・軸切替drag確認は未実施。

**進捗 (2026-09-30, Phase 3):** `WorkCursorState`の平面位置と空間位置を`CompositionPoint2`／`WorldPoint3`の別フィールドに分離し、rotation stateも`EulerDegrees3`へ変更。`setWorkCursorWorldPosition`にWorldPoint3を要求して呼び出し側に明示構築させ、3D overlayはworldPositionのみを読み出す。2D canvas setterはcanvasPositionのみを更新する。Viewport配置が従来どおり数値をz=0 planeへ置く挙動は維持し、その平面写像を呼び出しコメントとWorldPoint3構築に明示した。API/module compile・cursor配置と描画確認は未実施。

**進捗 (2026-09-30, Phase 3):** `WorkCursorState.canvasPosition`と重複していたControllerの生`QPointF workCursorCanvasPos_`を撤去。2D cursor setter・getter・state equalityはCompositionPoint2を直接使用し、legacy viewport overlayを呼ぶ描画境界でのみ`toQPointF`する。world cursor更新がcanvas stateを上書きする旧重複挙動も除き、両spaceのstateを独立させた。ビルド・2D/3D cursor overlay確認は未実施。

**進捗 (2026-09-30, Phase 3):** `setSelected3DTransform`のController APIを`LayerLocalPoint3`、`EulerDegrees3`、`Scale3`入力へ変更。実装が選択3D layerの`position3D()` authored値をそのままsnapshot／transformへ書き戻すため位置をlayer-localとして型付けし、Propertyダイアログとtransform clipboardのlegacy QVector3D境界に名前付き変換を追加。finite validation、scale min clamp、Undo/keyframe適用経路は維持。module compile・手入力／copy-paste／Undo確認は未実施。

**進捗 (2026-09-30, Phase 3):** `CameraFrustumVisual`のcamera positionとnear/far cornersを`WorldPoint3`、aspect ratioを`ScaleFactor`、camera zoomを`Pixels`に変更。camera view inverse／clip unprojectionが生成するworld pointを保持stateまで型付けし、frustum overlay rendererへ渡す点のみscalar `float3` に明示展開する。near-plane中心の平均はWorldPoint3座標成分から数値を変えずに算出。UI表示・描画値は不変。module compile・frustum overlay確認は未実施。

**進捗 (2026-09-30, Phase 3):** 3D model ray intersectionの最近接hit距離出力と選択／focus handlerのnearest stateを`WorldLength`へ変更。normalized world-rayとtriangle `t` の関係を維持し、三角形内部では既存float geometry、model intersection境界からtyped lengthを返す。Focus propertyは既存`Pixels`契約だがcamera world-unit換算根拠が未確認のため、従来どおりの数値写像を明示コメント付きで残した。ビルド・3D selection／click-focus確認とcamera world-unit契約は未確認。

**進捗 (2026-09-30, Phase 3):** model/triangle picking helperとnearest-layer検索の入力を`WorldRay`へ変更し、screen physical point→normalized world ray→selection/focus queryまで型を保持。triangle頂点をWorldPoint3としてhelperへ渡し、Möller–Trumboreの既存Qt arithmeticへ入る関数入口だけ明示変換する。交差距離は`WorldLength`のまま比較・返却し、`createPickingRay`の正規化directionが`t`をworld lengthにする前提をコードコメントに明記。既存三角形演算・focus pixel numeric bridgeは変更なし。ビルド・選択／フォーカス操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Möller–Trumbore triangle intersection内のedge、cross product、ray-origin offset、dot productを`WorldVector3`演算へ移行し、world ray/triangle点からQt `QVector3D`への往復を除去。barycentric scalarとepsilon、判定順序、t式は維持し、返却distanceは引き続き`WorldLength`。ビルド・triangle picking parity確認は未実施。

**契約修正 (2026-09-30):** 上記2件のtriangle point / internal vector強型化は取り消した。実装を追うとtriangle頂点はlayer自身の`snapshotAt()`だけで作るmodel matrixから出る一方、`WorldRay`はviewport camera由来であり、親付きlayerの座標空間一致が保証されない。誤った`WorldPoint3`タグを残さず、Möller–Trumboreの既存QVector3D演算へ戻した。WorldRay関数入口とWorldLength hit距離は維持するが、model intersectionの親transform契約は別途解決・検証が必要。

**進捗 (2026-09-30, Phase 3):** `Artifact3DGizmo::Impl::anchorDragDelta`共有stateを`WorldVector3`化。Qt ray-plane hit resultを代入する地点のみ名前付き変換し、resetは型付きzero値、公開getterはstateを直接返す。world-space offsetの意味は既存APIコメントに基づき、hit/drag式は変更していない。コンパイル・anchor drag確認は未実施。

**進捗 (2026-09-30, Phase 3):** `textSessionCaretPositionAt`公開APIを`ScreenLogicalPoint2`入力へ変更。実装では既存どおりDPRを掛けてphysical viewport座標からcanvas→layer localへ写像する。repository内call siteはなく、Qt座標境界をStrongTypeで限定した。module compile・text caret hit確認は未実施。

**進捗 (2026-09-30, Phase 3):** Interactive Render Regionの公開設定／取得値を新しい`CompositionBounds2`（typed minimum/maximum points）に変更。呼び出し側のcanvas `QRectF`境界とprivate controllerのlegacy `QRectF`状態／描画・hit-test間だけで明示変換し、正規化・2 px minimum・drag値は維持する。内部IRR rectangleがcanvas coordinatesであることはcreation式・zoom/pan hit-test・canvas drawingで確認。module compile・IRR操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Image source cropのController APIに`SourcePixelPoint2` / `SourcePixelBounds2`型付きoverloadを追加し、既存`QRectF` APIは名前付き変換を通す互換入口とした。座標契約は公開コメントと`sourceSize()`基準のcrop gizmo計算からsource pixelsと確認。保存状態は従来どおり`QRectF`、Undo・Property・描画処理も未変更。repository内に外部呼び出し元がなく、新overloadの移行先はない。module compile未確認。

**進捗 (2026-09-30, Phase 3):** 共通`Coordinates::Bounds2<Space>`を追加し、既存のProjected Selection Frame boundsを`ScreenPhysicalBounds2` aliasへ移行。点型のminimum/maximumとし、従来のinvalidフラグは正のwidth/height判定へ置換。selection-scale/hit-test・snap・zoom handlerの有効判定箇所を照合して同じゼロ寸法拒否を維持。さらに`setDropGhostPreview`の公開入口を物理screen bounds化し、Editorとcleanupのviewport rectangleから明示構築して描画直前に旧`QRectF`へ変換する。fallback値・overlay描画・ズーム/DPR計算式は変更なし。実コンパイル・selection bounds parity・drop preview確認は未実施。

**精度調整 (2026-09-30):** `Bounds2<Space>`のpoint storageをfloatの`Point2`からdoubleの`BoundsPoint2`へ変更。`QRectF`ベースのComposition IRR、Projected Selection Frame、cleanup/drop-preview boundsは従来double値を維持し、source cropの型付き互換入口もdoubleを保つ。IRR getterもfloat化する`compositionPointFromQPointF`を通さずdouble boundsを直接返す。表示座標・pixel値の精度が型導入で低下しないようにする修正。module compile・数値parity未確認。

**型契約補足 (2026-09-30):** `WorldPoint3`と`LayerLocalPoint3`間の減算・加算が成立しないことを`Math.Vec`のcompile-time contractに追加。同じ次元でも異なる空間の点を演算できない既存ルールを3Dでも固定する。module compile未確認。

**型契約補足 (2026-09-30):** 3D `WorldVector3`と`LayerLocalVector3`（`Vector3<LayerLocalSpace>`）間の加算・減算拒否もcompile-time contractへ追加。点に加えて方向／差分ベクトルでも空間混合を拒否する。module compile未確認。

**型契約補足 (2026-09-30):** `WorldPoint3`へ`LayerLocalVector3`を加算・減算できないこともcompile-time contractで固定。point-vector演算でも異空間の混合を直接検査する。module compile未確認。

**進捗 (2026-09-30, Phase 3):** Interactive Render RegionのEditor呼び出し元でも、`QRectF`の角をfloat `CompositionPoint2`へ落としてから渡していた変換を除去し、double bounds pointを直接構築。合成bounds全体で元のqreal精度を維持する。実コンパイル・IRR数値parity未確認。

**進捗 (2026-09-30, Phase 3):** `ContentGizmo`のimage crop drag stateを`SourcePixelBounds2` / `SourcePixelExtent2`へ変更。press時のsource crop rectとsource image寸法をdouble精度のsource-pixel型で保持し、既存crop/Undo `QRectF` API、layer-local deltaからsource pixel deltaへのscale式、fallback full-source crop、release/cancel復元を名前付き境界変換で接続。source rect / source sizeの値域をlocalBoundsおよびcanvas deltaと共有state上で区別する。実コンパイル・crop handleのdrag/undo/cancel確認は未実施。

**進捗 (2026-09-30, Phase 2):** `Core.Camera`の内部yaw/pitch/FOV stateを`Degrees`、orbit distanceを`WorldLength`、near/far clipを既存公開契約どおり`Pixels`で保持。setter clamp・初期値・preset・orbit/dolly/fit計算は数値を維持し、三角関数／glm projection境界でだけ`.value`を読む。getterは内部StrongTypeをそのまま返す。モジュールコンパイルとorbit/pan/dolly/projection parity確認は未実施。

**進捗 (2026-09-30, Phase 2):** 同じ`Core.Camera`のeye/targetを`WorldPoint3`、up/forward/rightを`WorldVector3`に変更し、`lookAt` / `fitToSphere` と取得APIおよび保持stateへ座標空間を通した。軌道位置は`WorldPoint3 + WorldVector3`として構築し、glm view matrix境界だけ数値成分を展開する。repo内にこれらの公開メソッドを直接呼ぶ旧型call siteがないことを確認。コンパイル・view matrix parity確認は未実施。

**契約調査 (2026-09-30):** Camera Layerのnear/far clipはInspectorで`px`表示され、永続値とStereoCamera APIもPixelsだが、`projectionMatrix`ではQt perspective/orthoのcamera-space depth引数として使用され、frustum guideも距離値として扱う。現状コードだけではscene/camera coordinateとproperty pxの数値スケール契約を確定できないため、既存Pixels型・数値を維持し、WorldLengthへの型名変更は保留する。Insight.mdの既存camera距離項目を更新。設計資料／scene期待値の確認が必要。

**進捗 (2026-09-30, Phase 3):** Rectangle/Disk Area Lightの幅・高さをCreate Light dialogから`ArtifactLightLayer`と`Core.Light`のgetter/setter・保持stateまで`Units::Pixels`化。Dialogのpx suffixとInspectorのpx unit、Layerのshape geometry、Core LightのGPU area-size入力を根拠に単位を確定した。JSON/property/renderer constant-buffer境界だけ`.value`へ明示し、最小値1 px、既定値100 px、保存値、overlayとGPU数値は維持。ArtifactCore module依存・コンパイル、create/edit/restore/render確認は未実施。

**進捗 (2026-09-30, Phase 3):** Point/Spot/Area attenuation rangeをDialog getter、Layer API・state、Core LightのsetRangeとmakePoint/makeSpot range引数まで`Units::Pixels`化。point/spot dialog suffixとInspector unitはpxで、Layer gizmoのrange ringとCore attenuation係数へ同じ数値距離として伝播する。JSON/property/attenuation calculation境界でのみ`.value`を読み、1 px下限・range既定値と計算式は維持。Spotの独立cone-length floatは既存APIを保ち、Core attenuation setterへ渡す地点でPixelsを明示構築する。module compile・light creation/restore/attenuation/render確認は未実施。

**進捗 (2026-09-30, Phase 3):** Spot `coneLength` をInspectorのpx契約に沿って`ArtifactLightLayer` APIと保持stateで`Units::Pixels`化。cone/frustum guide geometry、attenuation rangeとの比較、glow fallbackはいずれも既存float演算へ入る局所境界で`.value`を読む。JSON/property境界を明示し、最小1 px・既定300 px・描画/減衰値は変更なし。Controller→Core Lightの減衰距離渡しもPixelsのまま連結。module compile・cone edit/restore/render確認は未実施。

**進捗 (2026-09-30, Phase 3):** Light LayerのShadow Radius APIと保存stateをInspectorのpx表示に沿って`Units::Pixels`化。overlay ring geometryとJSON/property境界は`.value`を明示し、Core Lightのshader-facing無次元 softnessへ渡す箇所だけ既存どおり10で除算する。finite fallback、0–20 clamp、既定10、shadow softnessの値は維持。module compile・property/overlay/shadow render確認は未実施。

**進捗 (2026-09-30, Phase 3):** `ShadowHighlightEffect`のshadow/highlight radius getter/setter、所有state、CPU impl stateを`Units::Pixels`化し、effect property QVariant境界とpixel loop計算直前に`.value`を使用する。明示 `px` API契約に合わせ、30 px fallbackと有限・非負値処理を維持。現状のradius数値はCPU効果で0かどうかしか参照されない点をInsight.mdへ記録し、画像処理式の変更は含めない。モジュールimport・コンパイル・effect runtime確認は未実施。

**進捗 (2026-09-30, Phase 3):** 新unit `Units::Percent`を追加し、percentage pointsの100=normalized 1.0契約とPixels/ScaleFactor/Degrees/Meters/Millimeters/WorldLength/OpacityDelta/float間の暗黙変換拒否をstatic_assertで固定。Light Layerのintensity API・stateとCreate Light dialog getterをPercent化し、preview brightness/glow、JSON/property、render-queue境界で明示的に数値化。3D Core Lightへ渡すときだけ従来どおり100で除算して無次元floatとし、初期値・clamp/計算式・保存数値を維持。module compile・light intensity UI/JSON/render確認は未実施。

**進捗 (2026-09-30, Phase 3):** `ArtifactLightLayer`の既型付きcone-angle/cone-feather APIに加えて、内部stateを`Units::Degrees`に変更。frustum geometry、radian計算境界、JSON/Property境界だけ数値化し、既存 clamp、feather差分、描画式、保存値を維持。モジュールコンパイル・Inspector編集・frustum表示確認は未実施。

**進捗 (2026-09-30, Phase 3):** `ArtifactCameraLayer::Impl`のzoom/focus/near/far/ortho sizeをPixels、apertureをFStop、FOVをDegrees、IPDをMetersで保持。既存typed getter/setter、DOF settings、projection、Inspector、JSON/restore境界を再接続し、数値式・fallback・clamp・永続キーと値は維持。near/farのpx対camera-depthスケール契約は未解決のまま数値や型を変更していない。コンパイル、projection/DOF parity、property/JSON round-tripは未確認。

**進捗 (2026-09-30, Phase 3):** Camera LayerのBlur AmountをDialog getter、Layer getter/setter・保持stateまで`Units::Percent`化。camera overlay、DOF blur scale、motion-blur shutter-angle換算、Property/JSONでは既存どおり100で割るか数値化し、初期値100%、0–100 clamp、shutter式は維持。コンパイル・camera blur UI/render確認は未実施。

**進捗 (2026-09-30, Phase 3):** Create Camera dialogのaperture getterも`Units::FStop`を返すように変更し、EditorとLayer Menuのdialog→Camera Layer経路を同型で接続。combo text parse、invalid/no-combo fallbackのf/4値は維持し、FStopへの再wrapを除去。モジュールコンパイル・camera creationでのDOF確認は未実施。

**進捗 (2026-09-30, Phase 3):** `Artifact3DGizmo`のbounding box min/max保持stateを`LayerLocalPoint3`へ変更。既存の型付きsetter入口で有限値・正寸法を検査してtyped pointを保持し、bounding-box geometry helper入口でのみ`QVector3D`へ変換する。scale drag中のlocal extent計算は一度だけ明示変換した値から行う。boundsの座標契約はAPIコメントおよびControllerがmesh/localBounds値を渡す経路に基づく。幾何式・handle配置・Undo経路は変更していない。モジュールコンパイル・box handle hit/drag確認は未実施。

**進捗 (2026-09-30, Phase 3):** `Artifact3DGizmo::Impl`のlocal/view basis軸6本も`WorldVector3`として保持。既存public setterが要求する「worldで表したlocal object axes」「view-camera axes」の契約をstateまで保ち、basis生成helperの境界でだけQt vectorへ変換する。入力正規化・fallback軸・basis選択条件は維持。コンパイル・local/view/world gizmo操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Projected Frame dragの`projectedFrameCorrectedLocalPosition_`共有stateを`LayerLocalPoint3`化。press時・frame scale時の元local位置は`GizmoTransformSnapshot`から型付きで直接保持し、scale / rotate / moveのwrite-backで不要だったQVector3D往復変換を除去した。parent inverseを通るgizmo world pointだけは名前付き変換でLayerLocalPoint3にする。行列・境界処理とドラッグ式は維持。コンパイル・parented/unparentedでの操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Projected Frame anchor dragのstart/current anchorおよび補償position stateを`LayerLocalPoint3`に変更。開始anchorは`transform3D()`、開始positionは`position3D()`から取得され、いずれもauthored local値であることを確認。ドラッグ中のQTransform／QVector演算は既存のまま保ち、typed pointから演算値へ入る箇所と旧QVector3D Undo command境界だけ明示変換する。更新式・Undo/reject時rollbackは維持。コンパイル・親付きanchor drag／Undo確認は未実施。

**進捗 (2026-09-30, Phase 3):** 複数レイヤーgizmoの開始rotation / scale stateを既存`EulerDegrees3` / `Scale3`で保持。`Artifact3DGizmo::rotation()` / `scale()`が既に返す型をそのままcaptureし、group drag計算で既存QVector演算へ入る箇所だけ明示変換する。相対回転・倍率式、初期値、操作値は維持。コンパイル・複数選択gizmo操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** 単一レイヤー／modal gizmoの視覚snapshotも`EulerDegrees3` / `Scale3`へ変更。gizmo getterから直接captureし、HUD・drag constrain・relative transform等で既存QVector計算へ入る箇所に単位明示変換を配置。Undo側のauthored layer snapshotとworld pivotの分離を維持し、回転・倍率値や表示式は変更していない。コンパイル・modal/regular操作・HUD表示確認は未実施。

**進捗 (2026-09-30, Phase 3):** group gizmoとProjected Frame rotate dragの開始値との差分も`EulerDegrees3` / `Degrees`上で計算する。Qt quaternion / legacy layer rotation計算に入る直前だけQVector3Dまたはscalar degreeへ明示変換し、差分式・閾値・回転適用値は維持。コンパイル・回転drag parity確認は未実施。

**進捗 (2026-09-30, Phase 3):** group gizmoの現在scale、開始scale、relative scale ratioを`Scale3`のまま計算・保持する。軸別倍率は引き続き同じscalar除算とsigned near-zero fallbackを用い、translation geometryとrotation snapshotへの適用境界にのみ必要なQt型を残す。式・clamp・操作結果は意図的に変更していない。コンパイル・複数選択scale操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** gizmo HUDのposition / translation delta / rotation / scale / scale ratioを、それぞれ`WorldPoint3` / `WorldVector3` / `EulerDegrees3` / `Scale3` / `Scale3`のまま文字列化直前まで保持する。HUD計算中のQVector3D変換をなくし、従来の式・書式・表示値を維持。コンパイル・HUD runtime確認は未実施。

**進捗 (2026-09-30, Phase 3):** Projected Frame edge/corner Shift constraintの比率を`ScaleFactor`として計算し、scale stateも`Scale3`のまま更新する。従来の正のepsilon denominator、finite guard、cornerで大きい変化側を選ぶ規則を維持し、比率の適用境界で`.value`を使う。操作式・値は不変。コンパイル・modifier操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Projected Frame minimum-dimension clampでもgizmo現在・clamped scaleを`Scale3`として扱い、そのまま型付きsetterへ戻す。local-bounds由来の下限計算、符号保持、XYZ比較の条件は維持し、Qt vectorへのunwrap/rewrapを除いた。コンパイル・corner/edge clamp確認は未実施。

**進捗 (2026-09-30, Phase 3):** `GizmoTransformSnapshot`の回転・倍率stateを`EulerDegrees3` / `Scale3`に変更。keyframe capture、live property synchronization、group／single／Projected Frame操作、change判定、Undo restoreまで型を保持し、Qt/legacy float境界ではDegreesの`.value`または既存float APIを明示的に使う。差分しきい値、exact equality、scale値、回転値は維持。コンパイル・Undo/keyframe round-trip・各gizmo操作確認は未実施。

### Phase 3 — 2D / UI 境界への拡張

- Layer local、Composition、Viewport、UI 論理 px、Screen 物理 px の変換を対象ごとに移行する。
- 既存 API の互換境界は限定的に残し、新規 API では空間・単位を必須にする。移行済み経路から生の座標 API を段階的に閉じる。
- 2026-09-29: Projected Frameのcorner/interior/selection-frame hit-testとscale/snap経路で物理viewport点を `ScreenPhysicalPoint2` に移行。layer / 過去frame projected cornersとresize guide helperも型付き物理点で受け渡し、point差分は物理pxのvectorとして計算する。Qt clipping / drawing / ray helperへ出る境界で明示変換を行う。コンパイル・高DPI操作確認は未実施。
- 2026-09-29: `CompositionRenderController::createPickingRay`の入力を`ScreenPhysicalPoint2`に変更。イベント論理座標をDPR変換した後、全呼び出し元で名前付き`screenPhysicalPoint()`を通す。unprojection式、DPR適用箇所、Ray結果は維持。コンパイル・高DPI確認は未実施。
- 2026-09-29: 複数選択Projected Frame boundsを、内部double値を変えない `ScreenPhysicalBounds2` に包む。selection-frame hit-testとscale fixed-point APIに物理画面boundsを要求させ、既存 `QRectF` geometryは境界内で利用する。コンパイル・bounds parity確認は未実施。
- 2026-09-29: 過去frameの固定3D plane corner投影値とcenter hit-test入力も `ScreenPhysicalPoint2` にし、corner平均とpointer差分をtyped physical座標上で計算する。Qt clipping / ray picking境界は既存表現を保つ。コンパイル・motion path ghostの高DPI確認は未実施。
- 2026-09-29: `layerAtViewportPos`に`ScreenLogicalPoint2`入力を要求し、Qtイベント／drop位置からの明示変換を全call siteへ追加。controller内部のlogical→physical DPR変換は変更していない。コンパイルと高DPI確認は未実施。
- 2026-09-29: navigation zoom、Box Zoom開始／更新、Tumble Pivot APIを`ScreenLogicalPoint2`化し、イベント位置の型変換を明示。Box Zoom終了時に物理pxの中心を論理pxへ戻してから`zoomAtFactor`に渡すよう修正し、DPR二重適用を除いた。HiDPI操作確認は未実施。
- 2026-09-30: `ArtifactLayerEditorWidget::panBy`を`ScreenPhysicalVector2`、`zoomAroundPoint`を`ScreenPhysicalPoint2`入力に変更。mouse deltaとwheel event位置はQt論理座標からwidget DPRを一度掛けて物理pxへ変換する。renderer viewportが`layerEditorPhysicalViewportSize()`で物理px設定されることを確認。DPR=1の値は維持し、DPR>1ではrenderer viewport座標に合わせてdelta/anchorを物理px化する。コンパイル・複数DPI操作確認は未実施。
- 2026-09-30: `ArtifactLayerEditorWidget::setPan`も`ScreenPhysicalVector2`入力に変更。renderer pan stateと同じ物理viewport座標を要求し、既存float renderer境界を維持。呼び出し元はrepository内になし。コンパイル・DPI別確認は未実施。
- 2026-09-30: `LayerEditorFrameViewState`の退避zoomを`Units::ScaleFactor`、panを`ScreenPhysicalVector2`で保持。Renderer get/set pan・zoomのscalar境界だけでunwrap/wrapし、frame-view save/restore値は変更しない。interface module依存・コンパイル・frame render中の状態復元確認は未実施。
- 2026-09-30: Layer Editorの共有`zoomLevel_`とkey/chrome controller pointerを`Units::ScaleFactor`化。renderer getter/setterとzoom操作APIのfloat境界だけで明示的にwrap/unwrapする。Widgetの`zoomAroundPoint`も物理point + ScaleFactorを受ける。key zoom centerは`ScreenPhysicalPoint2`、Viewport Chrome interaction stateはlogical position / physical center / DPR factorの型を分け、logical→physical変換をChrome hit-testに集約。倍率計算・clamp・操作量は維持し、keyboard zoom中心はviewportの物理px契約に合わせた。module compile・key/chrome操作・DPI別zoom確認は未実施。
- 2026-09-30: Viewport Chromeの描画・hit-test stateと`layerEditorChromeControlAt`もviewport dimensionsを`ScreenPhysicalExtent2`で受ける。widget physical-size helperからの変換をnamed helperに置き、QRectF hit-test内部では既存寸法を維持する。コンパイル・各DPIでのChrome表示/hit-test確認は未実施。
- 2026-09-30: `layerEditorChromeControlAt`の位置入力も`ScreenPhysicalPoint2`に変更し、physical point→QPointF変換をgeometry helper内部に閉じる。Chrome interaction controllerから生Qt pointを渡せなくし、hit領域・優先順位は維持。コンパイル・各DPI hit-test確認は未実施。
- 2026-09-30: composition/physical viewport extent間の暗黙変換拒否static_assertを追加。異なる座標系のextentを互換サイズとして扱えない契約を固定。module compile未確認。
- 2026-09-30: Layer Editorのframe-view backup APIとframe-background描画stateも`ScreenPhysicalExtent2`を受ける。Qt `QSize`をrenderer座標へ渡すboundaryをwidget側のphysical extent helperに限定し、viewport dimensionsは従来値を保つ。module compile・frame/background描画確認は未実施。
- 2026-09-30: `Math.Vec`に`CompositionExtent2` aliasを追加し、Viewport Chromeの`restoreCanvasSize`もcomposition extentとして保持する。composition設定`QSize`からtyped stateへ変換し、renderer canvas-size scalar境界だけで数値を渡す。reset canvas geometryは維持。モジュールコンパイル・reset action/runtime確認は未実施。
- 2026-09-30: Transform HUDもphysical viewport extentとcomposition restore extentを型付き引数として受ける。renderer scalar変換は描画関数境界に限定し、HUD位置・restore canvas値は維持。module compile・HUD表示とreset操作確認は未実施。

## 完了条件

- 異空間の加減算、点同士の加算、degrees/radians と論理/物理 px の暗黙混合、誤った向きの行列適用がコンパイル不可である。
- 3D ギズモの描画・ピッキング・ドラッグの関数境界が型付きで、同じカメラ組を使う。
- 既存のアクティブカメラ／Viewport orientation、親子 transform、DPI 倍率、非可逆 transform の扱いを確認する。
- ビルド・テスト・CMake・実機確認は、ユーザーから明示指示を受けた場合だけ実施し、未実施の検証は完了と記さない。

## 変更境界

最初の実装対象は座標契約と関数引数・戻り値に限定する。レンダリング方式、ギズモ仕様、UI、入力操作の機能変更を含めない。`Artifact` / `ArtifactCore` は子リポジトリなので、その編集にはユーザーの明示指示を要する。

**進捗 (2026-09-30, Phase 3):** Layer Editor press/move controller stateを`ScreenPhysicalPoint2`化し、Qt eventの`ScreenLogicalPoint2`からwidget DPRで物理点へ変換して渡す。Viewport Chromeには論理点、panの共有開始位置と`lastMousePos_`も`ScreenLogicalPoint2`で保持し、deltaを`ScreenLogicalVector2`として計算してrenderer境界で物理化。shape/mask press・move・double-click・rubber-bandの入力も共通helperでScreenPhysicalPoint2→CompositionPoint2に写像する。renderer viewportが物理px設定であること、およびTransformGizmoのcanvas hit-testと固定viewport hit rectangleが同じrenderer viewport座標を使うことを確認。保存・描画式は変更なし。コンパイル・複数DPIでの操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** `GizmoTransformSnapshot`の回転と倍率を`EulerDegrees3` / `Scale3`化し、keyframe capture、property同期、単体・複数ギズモ、Projected Frame、差分判定、Undo復元まで型を維持する。比較閾値と数値式は維持し、従来float transform APIとの境界でのみdegree値を取り出す。コンパイル・keyframe/drag/Undo実機確認は未実施。

**進捗 (2026-09-30, Phase 3):** `AnchorPointUndoCommand`のanchor / position引数とUndo保持stateを`LayerLocalPoint3`化。typed drag経路はそのまま渡し、2つの既存Qt-vector操作経路のみcommand境界で名前付き変換を行う。Undo適用ではlegacy transform scalar APIへtyped pointのcomponentを渡し、検証閾値・更新順は維持。コンパイル・anchor reset/drag Undo確認は未実施。

**型契約検査 (2026-09-30):** `Math.Vec`のcompile-time拒否条件にComposition 2D pointとLayerLocal 2D / World 3D point/vectorの異種次元・空間演算、およびComposition / SourcePixel / ScreenPhysical extent間の暗黙変換拒否を追加。既存の型定義・実行時挙動は変更していない。ビルド未許可のため静的assertのコンパイラ評価は未確認。

**進捗 (2026-09-30, Phase 2):** Camera POIの保存API/state/Undo値を`LayerParentPoint3`化し、parent-localとWorldRayの空間を区別。`pointOfInterestWorld()` / `setPointOfInterestWorld()`で親global transformを明示適用し、Two-node cameraのeyeとtarget、POI overlay投影、ray-plane dragをworld spaceで揃える。world交差点を保存する際は親transform逆行列でLayerParentPoint3へ戻し、非可逆時は更新を拒否。JSONキーと保存値は従来の親local値を維持。compile・parented/unparented camera・POI drag/undo/cancel確認は未実施。

**進捗 (2026-09-30, Phase 3):** Box Zoomの開始・現在viewport位置stateをScreenPhysicalPoint2化。logical inputからnamed DPR変換でphysical pointを作り、差分は同空間のvectorとしてhit threshold判定に使う。既存Qt geometry helper直前とoverlay描画境界だけQPointFに変換し、fit centerはScreenPhysical→ScreenLogicalを明示してzoom anchorへ渡す。DPR適用回数、閾値、rect/fitting式は維持。コンパイル・DPI別Box Zoom確認は未実施。

**進捗 (2026-09-30, Phase 3):** Rubber Band選択の開始・現在viewport位置stateをScreenPhysicalPoint2化。Qt press/move位置は既存DPR変換後に名前付きphysical helperでstateへ保存し、viewport→canvas geometry helperの境界だけQPointFへunwrapする。選択矩形・入力処理は維持し、別経路のLasso point列は今回の対象外。コンパイル・DPI別選択確認は未実施。

**進捗 (2026-09-30, Phase 3):** Lassoのviewport point列をQVector<ScreenPhysicalPoint2>へ変更。press/move時点でphysical screen pointを保存し、3 physical pxの点追加距離判定はQt QLineF境界へ明示変換して維持する。renderer viewport→canvas変換にはtyped componentを直接渡し、選択polygon・描画座標は変更なし。コンパイル・DPI別Lasso確認は未実施。

**進捗 (2026-09-30, Phase 3):** Shape Vertex Marqueeの開始・現在viewport位置stateをScreenPhysicalPoint2化。press/moveは既存DPR適用後の物理位置をnamed helper経由で保持し、viewport→canvas rect helperだけQPointF境界にする。marqueeの選択・表示式は維持。コンパイル・DPI別vertex選択確認は未実施。

**進捗 (2026-09-30, Phase 3):** Rig Weight stroke last pointとRig Select control/bone drag開始点をLayerLocalPoint2に変更。値はscreen physical→canvas→layer global inverse transformで得る2D layer-local座標であることを確認。Math.VecにQPointFとのnamed conversionを追加し、QTransform/QLineF/QPointF演算・overlay境界でだけ明示変換する。weight falloff、drag delta、angle計算式は維持。コンパイル・rig操作確認は未実施。

**進捗 (2026-09-30, Phase 3):** Brushのrelease viewport位置、context/pie menu anchor位置、Magnifier cursor位置をScreenPhysicalPoint2として保持。menu APIのlogical→physical変換結果とQt mouse位置を型付きstateへ格納し、renderer / overlayでcomponentを読む。動作・位置計算は維持。参照先のなかった重複pieMenuMousePos_ stateと代入を削除。コンパイル・DPI別brush/menu/magnifier確認は未実施。

**進捗 (2026-09-30, Phase 3):** Mask vertex drag開始点、handle drag開始点、feather/expansion drag開始位置をLayerLocalPoint2化。これらは対象layerのglobal transform逆変換後に取得されることをcall pathで確認。既存QPointF geometry計算へ出る直前だけnamed conversionを使用し、constrained vertex、feather距離、expansion/feather delta式は維持。コンパイル・mask操作確認は未実施。

**型基盤・進捗 (2026-09-30, Phase 3):** LayerParentPoint2 / LayerParentVector2を追加し、CompositionPoint2との暗黙混合拒否をstatic assertionで固定。Motion Path dragの公開helper入力をCompositionPoint2、開始／更新positionをLayerParentPoint2、差分をLayerParentVector2にする。親付きlayerはparent global inverse、root layerはroot-parent frameとしてComposition値を明示写像し、Qt geometry境界でのみunwrapする。既存transform key式・group操作を維持。コンパイル・親子motion-path drag確認は未実施。
