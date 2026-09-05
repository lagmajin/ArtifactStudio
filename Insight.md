**最終更新:** 2026-09-05

# Insight Register

## 2026-09-05 — Box2D接触イベントはstep内で正規化する

- **関連:** `ArtifactCore/src/Physics/Physics2D.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`。
- **事実:** Box2D 3.1.1のbegin/end/hit配列はworld step後の一時データで、shape削除後のend eventには無効shapeが含まれ得る。接触／hit eventはshapeごとに既定OFFである。
- **対応:** レイヤーbodyとfloor shapeでcontact/hitを有効化し、step直後に`PhysicsContactEvent`へコピーする。compositionが対象レイヤーへ配布し、レイヤーはstep単位のbegin/end/hit数、継続接触数、最大接近速度、直近hit情報を保持する。保存・Undo・新規signalは追加しない。
- **価値／懸念:** 将来の衝突particle／sound／break判定は同一の正規化済みデータを利用できる。shapeが削除されたend eventは安全のため解決不能なら破棄するので、その稀な経路ではactive数はworld resetまで残り得る。
- **次に確認:** 床・Dynamic/Static/Kinematicの組合せ、複数同時接触、body削除直後、hit speed閾値、将来の視覚／音反応のrate limit。ビルド・実機確認は未実行。

## 2026-09-05 — 再生中の物理ドラッグは保存済みJointと分離する

- **関連:** `ArtifactCore/src/Physics/Physics2D.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`。
- **事実:** Box2D 3.1.1のMouse Jointは静的bodyとDynamic bodyの間で、world targetを追従させるランタイム拘束である。既存の`layerJoints`はComponentsから復元される保存済みconstraintの寿命を管理する。
- **対応:** `mouseJoints`を別のowner管理にし、再生中のSelectionツールで選択済みDynamic bodyだけに作成する。停止・release・body破棄で除去され、authoring transform、Components JSON、Undo履歴を変更しない。
- **価値／懸念:** 通常のVP Transformと競合せず、Spring/Rope/Sliderを維持したまま直接演技を調整できる。一方、現在は選択済みレイヤー全体を掴むため、collision shapeの厳密なポインタhit testやドラッグ強度のUI調整は未実装。
- **次に確認:** 再生中のbody質量ごとの追従感、ウィンドウ外release、再生停止・layer削除中のjoint除去、既存joint併用時の安定性。ビルド・実機確認は未実行。

## 2026-09-05 — 次の物理機能は「固定／操作可能なbody」と「スライド拘束」が最短

- **関連:** `ArtifactCore/include/Physics/2D/Physics2D.ixx`、`ArtifactCore/src/Physics/Physics2D.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`。
- **事実:** 2D剛体はcomposition共有world、owner別jointの破棄、fixed-step更新まで接続済み。現行ラッパーが露出するjointはDistance/Revoluteのみだが、導入済みBox2D 3.1.1にはPrismatic、Mouse、Motor、Weld等のAPIがある。bodyにはsleep、CCD、damping、gravity scaleも既にある。LiquidSolver2Dはcontainer、opening、spill、checkpoint、collision layerへの衝突、surface snapshotまで存在し、SoftBodyはsnapshot、wind、collider、tear基盤を持つ。
- **判断:** 次の小さく実用的な追加は、Static/Kinematic/Dynamic body modeとViewportのMouse Joint操作、その次にPrismatic（軸・移動範囲・motor）である。新エンジンを導入せず、既存Box2DのCore ownershipとComponents面を維持できる。
- **価値／懸念:** kinematic targetは動く親・衝突壁・接続先の明示に使え、mouse jointは再生中の物理演出を直接調整できる。joint追加はanchor座標・Undo・seek再生成・joint別寿命管理を共有する必要がある。MPM、Fluid、SoftBody、PyroのGPU化はDiligent/CPU parityとsnapshot検証を伴うため別規模。
- **次に確認:** body typeのJSON／Components UI、編集時にkinematicへ安全にtransform同期する経路、mouse dragと既存VP transform toolの入力競合、Prismatic jointのlocal axis／limit／motorの最小契約。実機・ビルド未実行。

## 2026-09-05 — 2D body種別／Slider／破断を既存Box2Dへ追加

- **関連:** `ArtifactCore/include/Physics/2D/Physics2D.ixx`、`ArtifactCore/src/Physics/Physics2D.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`。
- **対応:** CollisionにBody Type（Dynamic/Static/Kinematic）、JointにSlider（Prismatic: axis/limit/motor）とBreak Forceを追加。Kinematic/Staticは各fixed step前に編集Transformから位置／角度を同期する。Box2Dのconstraint forceがBreak Forceに達するとownerのjointだけを破棄し、runtime broken stateで再生成を抑止する。
- **寿命:** 閾値・body/joint設定はComponents JSONへ保存する。broken stateは保存せず、seek/reset／joint設定編集でfalseへ戻す。新規signalは追加せず、既存のLayerDirty/Components descriptor更新を使う。
- **次に確認:** Sliderのローカル軸・limitの視覚的な向き、kinematic bodyのアニメーション速度、破断境界値、Dynamic/Static/Kinematicの複数body衝突、Undo/Redo後の再接続。ビルド・実機未実行。

## 2026-09-05 — レイヤー間Spring／Ropeと接続点

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Composition/ArtifactAbstractComposition.cppm`、`ArtifactCore/src/Physics/Physics2D.cppm`。
- **事実:** 既存Springはhertz/dampingだけを指定し、Box2DのenableSpringを設定していなかった。joint専用layer worldは接続先をstatic proxyで追従し、Composition共有worldとは分離される。
- **対応:** Spring=3を有効化、Rope=4はenableSpring=true/hertz=0/enableLimit=trueで最大長のみ拘束する。owner body中心／target layer中心からのlocal X/Y offsetをComponents・JSON・descriptor・joint生成に接続。名前は一意な場合だけIDへ解決して保存する。床proxy(-3)をprimary bodyに選ばないよう修正。
- **根拠:** 既存依存Box2D 3.1.1、公式 `src/distance_joint.c`（MIT、 https://github.com/erincatto/box2d/blob/v3.1.1/src/distance_joint.c ）とインストール済み `types.h`。既存API利用であり外部コードの複製なし。CPU物理のみ、Diligent/D3D12/Vulkan資源・同期に変更なし。
- **双方向対応:** Collision / Joint有効の2D layerをcomposition共有worldへ統合し実body同士を接続。CoreのNamedVectorでowner別jointを所有し、body破棄時のBox2Dによるjoint破棄に登録情報を追従させる。固定targetはowner別proxyを維持する。
- **確認した問題:** bodyの位置をレイヤーpositionへ直接戻していたほか、Box2Dのradianを表示のdegreeへ直接代入していた。初期transform＋body差分で合成し、初期値を含むsnapshotを使用。PlaybackServiceの通常再生はgoToFrameを使うため、setFramePositionだけを直してもfixed-stepに到達しない。両入口のclockを統合した。
- **制限／次の確認:** 逆行／大きなseekは移動先編集値から再生成し、rigid snapshot完全復元は未実装。非一様scale親によるshearとcolliderの一致、layout/modifierとの併用、ネストcompositionの時間サンプリングは未検証。必要なら将来rigid snapshotとauthoring revisionによる無効化を分離する。今回ビルド・テスト・実機操作は未実行。双方への反作用、三者連鎖、片側無効化・削除、ばね振動、ロープ最大長、開始フレーム復帰、Undo・保存復元を次に確認する。

## 2026-09-05 — Construction Layerの描画と吸着を接続

- **関連:** `ArtifactConstructionLayer.cppm`、`ArtifactCompositionRenderController.cppm`、`ArtifactSmartGuidesManager.cppm`。
- **事実:** itemsは保存だけでdrawが参照していなかった。Smart GuidesはGuideSetの座標を親子transformなしで利用し、VPのprojected-frame候補はconstructionの内容を使っていなかった。最終出力包含ONでもguide除外条件と競合し得た。
- **対応:** Line/Circle/Annotationの描画・Inspector値編集と共通のlocal snap pointsを追加。VPとSmart Guides双方でtransformを適用する。包含ON時のguideフラグを整合させた。
- **価値／懸念:** スナップと表示が同じconstruction設定を使える。Inspector項目の追加Undoは非表示状態と値を復元し、配列内の無効項目は残す。既存APIからitemsを並べ替えるとordinalプロパティの対応が変わるため、将来の並べ替えUIにはIDベースのUndoが必要（未実装）。
- **次の確認:** ビルド・実機操作は未実行。保存復元、親子変換、編集Undo、出力ON/OFFを確認する。追加専用UI・寸法線・VP個別制御点編集は別作業。
- **VP編集追補:** 個別制御点ドラッグを実装。表示・pickは同じlocal handle座標を使い、press時のカメラと逆world transformでlocal z=0に交差させる。端点／平行移動／半径は開始時スナップショットから計算し、releaseで単一Undo、Esc／右クリックで復元する。Undo時に既存LayerChangedEventを再利用し、内容更新にはSource dirtyを使う。新規作図・文字入力はInspectorに残す。ビルド・実機操作は未実行。

## 2026-09-05 — VPフレーム吸着の残課題

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` の `snapProjectedFramePointer` とmouseMoveの呼出し。
- **事実:** コンポジションと他レイヤーの端・中央を候補化する。呼出しはFrontに限定され、ローカル軸移動はaxisMoveSnap対象外。候補は各pointer更新で全レイヤーのboundsを投影して再生成し、各軸の最短距離で選ぶ。
- **未検証の仮説:** 多数レイヤーでは候補再生成が入力負荷に寄与し、近接ガイド間では吸着先が切り替わりやすい可能性がある。平面1枚時の引っかかりの原因とは断定しない。
- **価値／次の確認:** ドラッグ中の候補計算時間を確認し、必要なら候補キャッシュと吸着の保持・解除閾値を導入する。今回は調査のみ。
- **実装追補:** ユーザー指定の1〜3に対応。吸着10 logical px／解除16 logical pxの保持帯を導入。候補配列を既存Core.Arrayへ移し、カメラ・viewport・選択・composition変更時と250ms間隔で再生成、ソートして二分探索する。press/modal開始/終了でキャッシュを無効化し、Alt/OFFで保持を解除する。Front限定を外し、ドラッグrayと同じカメラ行列で画面上のboundsへ吸着する。World/Local/ViewのX/Y/Z移動はギズモ内部のdragAxisDirectionを参照し、一つの画面ガイドへ軸方向の補正を行う。GPU資源・Diligent backend・同期経路は変更しない。
- **未検証／制限:** ビルド・テスト・実機操作は未実行。透視投影下の吸着は画面上の整列であり、3D面・頂点への吸着ではない。候補の外部変更は最大250msの更新遅延がある。透視ビュー・親付きローカル軸・Shift精密操作・Ctrl量子化の組合せで、ガイドと確定結果の一致を次に確認する。負荷軽減量は未計測。

## 2026-09-05 — Native Dock のQADS相当操作

- **関連:** `Artifact/include/Widgets/ArtifactNativeDockSurface.ixx`。
- **事実:** Native DockはQADSを実行時に生成せず、Qtの `QTabWidget` / `QSplitter` と所有する `QDialog` によるbackend-neutral surfaceである。既存実装は他タブ面へのdropは持つが、タブ順の永続化、タブを外へドラッグして分離、フローティング状態からの再ドックを一貫して扱っていなかった。
- **対応:** タブ面の表示順をportable layoutへ保存・復元し、同一面のdropを順序変更として扱う。受け取り先のないtab dragは所有panelを保持したままフローティング化し、浮動ウィンドウのDock backボタンで元のareaへ戻す。復元時にもembedded/floating間を変換する。closeは非破壊の非表示を維持する。ドラッグ中は、候補tabまたはdock areaに半透明のアクセント色プレビューをowner-drawで表示する。tab面とtab barの双方でdock MIMEを受け、既存tab上はtab化、空白部はその領域へ追加する。tabのダブルクリックも同じ安全なフローティング経路へ接続した。
- **未検証:** 実機でのtab drag、外部dropのcancelと分離の境界、Dock back、ドロッププレビュー、再起動後の順序／geometry、各既存panel（viewportを含む）のfloating再親子化とサイズ更新。Build/testは未実行。

## 2026-09-05 — App Debuggerの自動更新とVP入力停止の候補

- **関連:** `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm` のtimerEvent/refresh、`Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` のframeDebugSnapshot。
- **事実:** App Debuggerは250msのtimerでrefreshし、controllerのsnapshotを取得する。snapshotにはGPU画像のreadback、最終effect処理、preview差分画像作成が含まれる。refresh入口は再入ガードのみで非表示チェックがない。VPの定期描画はrenderOneFrameを通らずrenderOneFrameImplを直接呼ぶため、追加したイベント6/8はその経路を計測していない。
- **未検証の仮説:** 診断UIの周期的な更新がUIスレッドを占有し、パン入力の約397msの空白や描画開始間隔の約570ms〜1.09秒の空白へ寄与している可能性。実際に当該widgetが生成済みか、更新の実測時間は未確認。
- **価値／次の確認:** 診断機能自身の観測負荷を分離する。App Debuggerを生成しない起動で比較し、必要ならrefresh前後とtickキュー投入/受信、renderOneFrameImpl全体を計測する。修正は今回行っていない。
- **追加確認・対応:** Native DockのaddLazyDockedWidgetFloatingはfactoryを即時呼び出すため、App Debuggerは未表示でも生成されていた。ユーザー依頼によりconstructorでのtimer開始を削除、非表示refreshを抑止、show/hideでtimerを開始/停止。初期登録と保存レイアウト復元後は非表示へ固定した。Native Dockのタブに標準closeアイコンを持つボタンを追加し、既存eventFilterからsetDockVisibleへ渡す（新しいsignal/slot接続なし）。タブの非表示・再表示はsetTabVisibleで保持し、内容widgetは破棄しない。ビルド・非表示時の負荷・全タブを閉じた後のメニュー再表示・キーボードSpace操作は実機未検証。

## 2026-09-04 — 物体検出バックエンドの共通契約

- **関連:** `ArtifactCore/include/AI/ObjectDetector.ixx`。
- **確認できた事実:** 既存の物体検出は具体クラスしかなく、ONNX等の実検出器を同じ呼び出し側へ接続する抽象契約がなかった。`IObjectDetector` に ready、detect、error 状態を定義し、既存検出器を適合させた。
- **価値／懸念:** 将来の実モデルを App API の変更なしに差し替えられる。現行の輝度ベース検出はフォールバックであり、物体認識の品質を保証しない。
- **次に確認すべきこと:** 実ONNX検出モデルのラベル・矩形・NMS出力をこの契約へ正規化する。

## 2026-09-04 — 連番マスクの変化診断

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** 連番のマスクを正規化座標で比較し、平均差分、最大差分、大きく変化した面積率を返す診断APIを追加した。
- **価値／懸念:** App側は急なマット変化を検出して再推論や手動確認を促せる。これは動き補償を行わないため、被写体が移動する連番ではRoto Brush伝播後の比較を前提とする。
- **次に確認すべきこと:** 実連番で警告閾値と、再推論・安定化のUI方針を決める。

## 2026-09-04 — セグメンテーションマスクの非破壊プレビュー

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** `DepthMap` は正規化された単一チャンネル値を保持する。これを直接 `ImageF32x4_RGBA` のグレースケール画像として生成するプレビュー API を追加した。
- **価値／懸念:** App側は元画像やalphaを変更せず、推論・Roto Brush・手動補正のマスクを共通表示できる。GPUプレビューとの最終的な見え方の一致は実機確認が必要。
- **次に確認すべきこと:** App の既存マスク表示導線へ接続し、比較表示と反転表示を確認する。

## 2026-09-04 — ONNX セグメンテーション設定契約のテンプレート化

- **関連:** `ArtifactCore/docs/ONNX_IMAGE_SEGMENTATION_CONFIG.md`。
- **確認できた事実:** モデル固有の入力サイズ・正規化・色順・出力選択は `loadOptionsFromJson()` で外部化されている。設定ファイルの最小テンプレートと許可値を文書化した。
- **価値／懸念:** モデル導入時にコード変更ではなくモデル配布物だけで契約を更新できる。テンプレート値は特定モデルの推奨値ではないため、実モデル仕様との照合が必須。
- **次に確認すべきこと:** 最初の採用モデルについて、モデル／設定／ライセンス情報を同じ配布単位にまとめる。

## 2026-09-04 — 一括セグメンテーション後処理へのクリーンアップ統合

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** batch API は `SegmentationMaskRefinementOptions` を通じて後処理を実行する。穴埋めと小領域除去も同設定に統合し、モデル推論後の各フレームへ一貫して適用できるようにした。
- **価値／懸念:** 単一画像と連番バッチでマット整形の条件がずれない。既定では両方無効であり、形状を変える処理は明示設定時だけ適用される。
- **次に確認すべきこと:** App側で素材カテゴリに応じたプリセットを設けるか検討する。

## 2026-09-04 — セグメンテーションマスクの小領域除去

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** 推論マスクには孤立した小さな前景島が現れる。閾値以上の連結成分を走査し、指定面積以下だけを透明化する処理を追加した。
- **価値／懸念:** 背景上の小さな誤検出を、モデル変更なしにプレビュー段階で抑えられる。小物や細部まで消す可能性があるため、既定値は無効で明示的な面積指定を必要とする。
- **次に確認すべきこと:** 人物の髪・アクセサリー、製品写真で妥当な面積範囲を確認する。

## 2026-09-04 — セグメンテーションマスクの穴埋め

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** AIマットには前景内の小さな背景穴が発生する。外周へ到達しない背景連結成分だけを検出し、面積上限を指定可能な穴埋め処理を追加した。
- **価値／懸念:** 人物・製品の内側に残る小穴をモデル非依存で整えられる。細いリング状オブジェクトの内側を消してしまうため、面積上限とプレビューでの確認が必要。
- **次に確認すべきこと:** 文字、メガネ、穴のある製品素材で既定の面積上限を決める。

## 2026-09-04 — ONNX セグメンテーションの入力色順設定

- **関連:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。
- **確認できた事実:** ONNX の画像モデルには RGB だけでなく BGR の入力テンソルを前提とするものがある。`inputColorOrder` を JSON 設定および型付き option に追加し、色順に対応する mean/stddev も正しい色成分へ適用する。
- **価値／懸念:** モデル固有の色順のためだけに変換ノードを増やさず、U²-Net系などの導入候補を設定で試せる。実モデルでの色順・正規化仕様の確認は未実施。
- **次に確認すべきこと:** 導入する実モデルの preprocessing 定義を JSON と照合する。

## 2026-09-04 — 物体検出から共通マットへの接続

- **関連:** `ArtifactCore/include/AI/ObjectDetector.ixx`、`ArtifactCore/src/AI/ObjectDetector.cppm`。
- **確認できた事実:** 検出結果はラベル、信頼度、矩形を持つが、既存コードにはマスク処理へ渡す経路がなかった。検出矩形をsoft edge対応の`DepthMap`へ rasterize するAPIを追加した。
- **価値／懸念:** 将来のYOLO等の実検出器でも、矩形を初期選択・保護領域・Roto Brushの開始マットとして共通利用できる。矩形は物体輪郭ではないため、最終切り抜きにはセグメンテーションとの合成が必要。
- **次に確認すべきこと:** 実検出モデルを導入後、複数検出のラベル選択とセグメンテーション初期化のUI導線を設計する。

## 2026-09-04 — AI マスクの切り抜き・背景置換を共通 CPU 経路に集約

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** セグメンテーション結果は `DepthMap` の正規化マスクとして扱える。切り抜き、自動前景クロップ、単色背景、背景画像の合成を `ImageF32x4_RGBA` と `FloatRGBA` の直接操作で追加し、Qt 合成・`QImage` 変換を経由しない。非AIの輝度フォールバックも既存の透明領域を前景として復活させないよう、入力alphaを既定で尊重する。
- **価値／懸念:** ONNX、Roto Brush、将来のモデルが同じ出力処理を共有できる。背景画像はバイリニアでサンプルし、異解像度の置換で段差を作らない。将来は GPU 経路で同じ straight-alpha 契約を維持する必要がある（未検証）。
- **次に確認すべきこと:** GPU 合成経路へ接続する際、straight-alpha の契約を保持してプレビューと書き出しの結果を一致させる。

## 2026-09-04 — セグメンテーション境界の色かぶり補正

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** 前景マスクの半透明境界では、グリーン／ブルースクリーンの色が残る。mask coverage が中間値の画素だけに green/blue の過剰成分を抑える処理を追加した。
- **価値／懸念:** 切り抜きの縁をモデル非依存で改善できる。人物固有の緑／青を過度に変えないよう、完全不透明領域には適用しない。強さと境界幅は実素材で調整が必要。
- **次に確認すべきこと:** 緑髪・青い衣装を含む素材で、補正量とエッジ幅の既定値を決める。

## 2026-09-04 — 高解像度向けマスク後処理の分離パス化

- **関連:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。
- **確認できた事実:** foreground の expand／contract は矩形要素の max/min 演算、feather は矩形 box blur として実装されている。どちらも水平・垂直の二段に分離しても edge-clamp を含む結果は同じになる。
- **価値／懸念:** 半径に対する計算量を二次から線形へ下げ、4K素材のマスク調整を実用的にする。CPU処理のままなので、GPU経路が整った段階で置き換え候補として確認する。
- **次に確認すべきこと:** 実素材で既存 GPU 経路とのエッジ見え方とCPU処理時間を確認する。

未着手の設計判断、現在の優先方針に直結する実装候補、実機検証待ちだけを記録する。実装済みの詳細履歴と過去の調査は [Insight Archive (through 2026-09-01)](docs/analysis/INSIGHT_ARCHIVE_2026-09-01.md) を参照。

## 現在の優先検証

### 静止画・連番画像 — GPU cache と実素材の再生／出力確認

- **関連:** `Artifact/src/Layer/ArtifactImageLayer.cppm`、`Artifact/src/Render/GPUTextureCacheManager.cppm`。
- **状態:** 実装済み、runtime未検証。
- **確認すること:** 4K連番、欠番、Time Remap、再リンク、Preview／Render Queueでフレーム・GPUメモリ・出力が一致すること。

### Shape — Path keyframe／Merge Paths／SVG出力の実機確認

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`、`Artifact/include/Layer/ArtifactShapeLayer.ixx`、`Artifact/src/Render/ArtifactRenderQueueService.cppm`。
- **状態:** GPU／互換フォールバック／boundsの同期は実装済み。SVGのグラデーション、stroke taper／alignは未対応。
- **確認すること:** 複数subpathと各Merge Paths mode、Path keyframe、GPU／出力SVGの一致。

### Shape — 複数コンテンツ／GPUベクター描画の実機確認（2026-09-03）

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`（`ShapeContent`、`paintGpuPaintItems`、`resolveContentVisPaths`、`renderContentsToImage`）、`Artifact/include/Layer/ArtifactShapeLayer.ixx`。
- **状態:** 実装済み、ビルド・runtime未検証（ビルドは指示待ちのため未実行）。
- **内容:** 1レイヤー複数パス（形状＋塗り＋線＋表示＋結合モード、空＝従来動作）。グラデーションは三角形重心サンプリング、Inside／Outsideはハーフオフセット＋中央線、テーパー／勾配線はセグメント分割でGPU描画し、viewportのQImageスプライト分岐を撤去。結合はCPU側QPainterPath真偽値演算で解決し、スタイルは保持。物理グリッド・3Dカード高速パス・オペレータキー評価は従来のまま。
- **確認すること:** 既存単一シェイプの見た目不変（solid高速パス・operator分岐は温存）、グラデーション／align／taperのGPU描画、Subtract／Intersect／Differenceの穴・境界線、連番・サムネイル・SVG出力、Repeater大量複製時の負荷。
- **既知の近似（未検証の仮説ではない仕様）:** テーパー線の結合部は Butt 重ね、Roundキャップは矩形延長近似、dash＋taper併用はdash優先、コンテンツのパス頂点アニメは未対応（静的）。

### Shape — 沿路グラデーション線／ダッシュオフセット（2026-09-03）

- **関連:** `Artifact/include/Render/ArtifactIRenderer.ixx`（`PolylineStyle`）、`Artifact/src/Render/ArtifactIRenderer.cppm`（`drawStyledPolyline`）、`Artifact/src/Layer/ArtifactShapeLayer.cppm`。
- **状態:** 実装済み、ビルド・runtime未検証。
- **内容:** `PolylineStyle`に`gradientEnabled/gradientStart/gradientEnd/dashOffset`を追加。`drawStyledPolyline`は累積長パラメータでセグメント・ダッシュ・結合・キャップを沿路補間色で描画し、dash位相は`dashOffset`の剰余で解決。レイヤー側は`shape.dashOffset`（アニメ可）＋コンテンツ別`dashOffset`、勾配のみの線は taper 分割器ではなく`drawStyledPolyline`経由に変更（結合・キャップ・dashと合成可）。QImage互換・SVG出力（`dashOffset`のみ）・保存も配線。
- **確認すること:** 既存実線の見た目不変（新フィールド既定で旧経路と同一）、勾配＋dash＋round結合の合成、負offset・巨大offset、マーチングアンツのキーフレーム補間。
- **既知の近似:** taper＋dash併用はdash優先でtaper無効、勾配サンプリングは線形補間。

### Shape — SVG相互運用（取込・書出）（2026-09-03）

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`（`SvgImport`、`shapeContentsToSvg`、`parseShapeContentsFromSvg`、`addShapeContentsFromSvg`、`importSvgFileContents`）。
- **状態:** 実装済み、ビルド・runtime未検証。
- **内容:** 書出は結合解決済みパス＋塗り／線／dash／fill-ruleを`<path>`＋`linear/radialGradient` defsで出力（taper線→通常線、conical→単色、勾配線→中間色に縮退）。取込は`path(d全命令・Aはベジェ化)`・rect（角丸可）・circle・ellipse・polygon・polyline・line＋線形／円形グラデーション（前方参照可）＋transform bake＋継承スタイルを編集可能コンテンツ化（座標はbounds正規化、256件cap、64MB cap）。`ClipboardManager`は未変更で、受渡し自体は素のSVGテキストを呼出側に委譲。
- **確認すること:** Illustrator／Figma出力SVGの往復、userSpace勾配・奇数dash・相対命令・指数表記の数値、空・不正SVG（0件／-1）、既存JSON互換。
- **既知の近似:** 複数subpathは1要素に統合（線描画で連結線が出る）、3 stops以上は両端のみ、gradientTransform・非等方scale下の線幅・group fill-opacity継承は近似、strokeのurl()は勾配線として解決（fillのみ前方参照対応だった点をstore側で統一）。

### Shape — コンテンツ編集サポート（2026-09-03）

- **関連:** `Artifact/src/Layer/ArtifactShapeLayer.cppm`（`activeContentIndex_`、`ShapeContentProxy`、`duplicateShapeContent`、`moveShapeContent`、`insertShapeContent`、`swapShapeContents`）、`Artifact/include/Layer/ArtifactShapeLayer.ixx`。
- **状態:** 実装済み、ビルド・runtime未検証。
- **内容:** `activeContentIndex_`（-1 = レガシーモード）と`ShapeContentProxy`（`ArtifactShapeLayer*` + index）を導入。Proxyは`name`/`visible`/`opacity`/`merge`/`fill`/`stroke`/`geometry`/`duplicate`を`setShapeContentAt`経由で直接編集し、PropertyEditorは`shape.activeContentIndex`で操作対象を切り替える。複製（挿入位置にコピー）、挿入、`move`、`swap` APIを追加。`shape.content.<i>.type/width/height/cornerRadius/starPoints/starInnerRadius/polygonSides/fillRule` を `setLayerPropertyValue` で直接編集可能に拡張。JSONシリアライズに`activeContentIndex`を含む。
- **確認すること:** Proxyのスワイプ（他のインデックス参照）、move/swap後のbounds・visPaths再構築、JSON往復、PropertyEditorでのアクティブコンテンツ切り替え時の描画反映、contentジオメトリ編集時の再構築・保存。

### 2.5D — 局所DOF／motion blurの品質と負荷

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`Artifact/src/Layer/Artifact{Image,Shape,Text}Layer.cppm`、`Artifact/src/Render/PrimitiveRenderer2D.cppm`。
- **状態:** 実装済み、runtime未検証。
- **確認すること:** Image／Shape／Textで深度・focus・shutterを変え、alpha順、極端なblur、再生時のGPU負荷、書き出し結果を確認する。

### Gobo runtime texture — 将来接続の安定ID境界

- **関連:** `ArtifactCore/include/Graphics/MeshRenderer.ixx`、`ArtifactCore/src/Graphics/MeshRenderer.cppm`。
- **状態:** ファイルGoboを置換できるruntime SRV入力は実装済み。Image Layerとの接続・UI・保存は未実装。
- **判断待ち:** 接続を始める時点で、packed scene-light slotではなく安定したLight IDとresource revisionを対応付ける。

## 現在の設計判断

### Semantic Debugger — 外部 `ArtifactDebugger.exe` を正規UI境界とする

- **関連:** `docs/planned/MILESTONE_EXTERNAL_SEMANTIC_DEBUGGER_2026-09-02.md`、既存のMCP／Trace／Shared Memory IPC診断基盤。
- **状態:** 未着手。設計判断をマイルストーン化。
- **判断:** ArtifactStudio本体には低コストの `ArtifactDebugRuntime`（semantic identity、mutation provenance、frame snapshot、safe-point制御）だけを置き、意味表示・原因解析・timeline・semantic breakpointのUIは外部プロセスへ分離する。既存MCPはAI専用に作り直さず、Debuggerとheadless harnessが共有するread／control protocolの基盤として再利用する。
- **価値／懸念:** 本体のQt／レンダリング状態をデバッガUIから隔離し、VS native debuggerとの併用、実行中Attach、Debugger単独更新を可能にする。一方、protocol version、履歴欠落の「未観測」表示、frame boundaryでのpause、snapshot復元とdeterministic replayの境界が必要。
- **次に確認すること:** Phase 0で既存MCP TCP／QLocalSocket／Named Pipeの接続候補、共有されるsemantic schema、diagnostic buildと通常buildのruntime有効化方針を確定する。

### Layer modulation は opacity以外へ拡張しない

- **関連:** `Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`ArtifactCore/include/Audio/Modulation/Router.ixx`。
- **状態:** opacityの評価、保存、Undo基盤は実装済み。Inspector導線とruntime確認は未完。
- **判断:** Transformはvariant／physics／layoutとの評価順を定義するまで追加しない。

### 物理 — rigid joint／polygon colliderの動作確認を先行する

- **関連:** `Artifact/src/Composition/ArtifactAbstractComposition.cppm`、`ArtifactCore/src/Physics/Physics2D.cppm`。
- **状態:** jointとpolygon colliderの導線は実装済み、runtime未検証。
- **確認すること:** 重力の画面座標符号、scrub復元の制約、凹形状の凸近似、SoftBody／MPMとの接触。

### 3D rendering — AOVと実GPU契約は別スライスに保つ

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`Artifact/src/Render/DiligentImmediateSubmitter.cppm`。
- **状態:** World Position AOVのtarget基盤はあるが、書き込みPSO／runtime検証は未実装。
- **判断:** 2D／2.5Dの完成度を優先し、AOV、GPU skinning、ray tracingは個別の受入条件を定義してから進める。

### プロキシ生成 — Out-of-Process 専用ワーカー (ArtifactProxyWorker) 方式の採用

- **関連:** `Artifact/src/Layer/ArtifactVideoLayer.cppm`、`Artifact/include/Proxy/ProxyService.ixx`。
- **状態:** `ArtifactProxyWorker.exe` の native / Media Foundation / ffmpeg 経路と JSON Lines 通知を実装。native は実エンコーダー名と検出候補も返す。host は jobId/outputPath/outputBytes を照合し、Project View から queue キャンセルも可能。Eighth、音声再エンコード、hardware encoder、auto fallback、staged package の worker 実機確認済み。Media Foundation は H.264 入力で成功するが、ProRes 入力は非互換で、極小出力は 64px/axis にクランプする。成功・失敗・キャンセル時の partial cleanup と、検証ツールの worker timeout 回収も確認済み。host UI 統合の実機確認は未完。
- **判断:** 本体プロセスの安定性（クラッシュ・OOM 巻き添え防止）と FFmpeg C API 直接利用（進捗通知・GPU HW エンコード）を両立するため、専用の子プロセスワーカー（`ArtifactProxyWorker`）を設けて非同期 IPC で連携する構成を正規方針とする。

### Proxy worker — 成果物の原子性と timeout 回収を同じ受入条件にする

- **関連:** `Artifact/src/Worker/ArtifactProxyWorker.cpp`、`tools/proxy_worker_smoke_test.py`、`Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`。
- **状態:** 実装済み、host UI実機確認待ち。
- **判断:** worker の exit code だけでは成功とみなさず、completed message、final output、partial cleanup、timeout／強制終了後の状態を一組で検証する。これにより、ハングや中断を有効な proxy と誤認する経路を受入段階で検出できる。

## 保留中の設計判断

### シェイプレイヤー VP 操作の増強範囲（2026-09-02 ユーザー質問）

- **質問:** 「シェイプレイヤーのVP操作機能増強いけそうか」
- **解釈:** `VP = メインコンポジション Viewport`（`taste.md` の communication-integrity 規範に従い grep で `Viewport`/`TextViewport` へ展開、コード上に独立した `VisualProgramming` 系は無いため）。
- **現状（コード読みで確認済み、未検証含む）:**
  - `Artifact/src/Tool/ArtifactToolManager.cppm:44-46` に `ToolType::Shape / Rectangle / Ellipse` が定義され、`ToolType::Shape` は `ArtifactToolService` で `Shape modeling` 入口と紐付け済み（`docs/done/MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md:258`）。
  - `Artifact/src/Layer/ArtifactShapeLayer.cppm` は `ShapeType`（Rect/Square/Ellipse/Star/Polygon/Triangle/Line）+ `customPolygonPoints_` + `CustomPathVertex { pos, inTangent, outTangent, smooth }` + `customPathClosed_` を保持し、`evaluatePathAt(frame)` でパス頂点キーフレーム評価を実装。
  - `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` のシェイプ系 VP ハンドラ:
    - `mousePress` L22853-22923 で Rectangle/Ellipse/Shape ツールのドラッグ → 矩形/楕円/スター/ポリゴン/三角のシェイプレイヤー新規作成（選択レイヤー有りは mask 経路、無しなら `RectangleToolMode::Shape/EllipseShape/StarShape/PolygonShape/TriangleShape` で `ArtifactShapeLayer` を生成、L27803-27873）。
    - `mousePress` L22940-22991 で Pen ツールが Shape レイヤー選択時のみマスクではなくカスタムパスを `pendingShapePathVertices_` へ追加（開始点クリック or Enter で確定、Backspace で取消、Escape 取消）。
    - `mousePress` L23527 `beginShapePathVertexDrag`、`updateShapePathVertexDrag` L25095、`endShapePathVertexDrag` L27345 経由でカスタムパスの頂点/タンジェントドラッグ編集。`ShapePathVertexEditCommand` で Undo。
    - `mousePress` L23506-23527 で Line シェイプの端点ドラッグ (`isDraggingLineEndpoint_` / `draggingLineLayer_`) を実装。
    - `isDraggingShapePathVertex_` / `shapePathEditPending_` / `shapePathEditDirty_` / `hoveredShapePathVertex_` / `hoveredShapePathTangent_` / `draggingShapePathTangent_` (0=vertex / 1=in / 2=out) を state に保持 (L12488-12505)。
  - `ArtifactCompositionRenderOverlay.cppm` のシェイプレイヤー描画: L1028-1105 でカスタム Polygon の頂点ストローク描画、Line の 2 端点描画、customPathVertices のベジェ描画を実装。ただし **頂点ハンドル/タンジェントハンドル/選択ハイライト/セグメント挿入マーカーのオーバーレイ描画パスは未確認**（grep 上このファイルには vertex overlay / hit-area / ハンドルサイズ定数が Shape 用に出てこない）。
  - `ArtifactRenderLayerWidgetv2`（LayerEditorPanel 内、`MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md:255-279`）に vertex / segment / tangent のコンテキストメニュー・Ctrl-click 選択追加・Shift toggle・numbering・hover/select 表示・path vertex duplication・polygon vertex duplication・segment insert 経路がある。`MILESTONE_LAYER_EDIT_2026-04-25.md:209-219` で `customPolygonPoints` を `CustomPathVertex` へ拡張済み。
- **増強候補（メイン VP 視点で未着手／不足）:**
  - (1) シェイプ専用 vertex/tangent/segment overlay 描画の RenderOverlay 統合（`LayerEditorPanel` の機能をメイン VP へ移植した残骸: `INSIGHT_ARCHIVE_2026-09-01.md:4549-4551` の懸念「ビルド未実施。タンジェント smooth 反射の長さ保存比、パスキーフレームの UI は次段階」）。
  - (2) Rect の `cornerRadius` ハンドル（`hitTestCornerRadiusHandle()` は `ArtifactRenderLayerWidgetv2` にあり、メイン VP 側は `setSize()` を width/height ドラッグで更新する経路のみ、`MILESTONE_LAYER_EDIT_2026-04-25.md:60` に「ShapeEditCommand 同様」とあるが RenderController 側 grep で未確認）。
  - (3) Star の `starInnerRadius_` ハンドル、Polygon の頂点ドラッグ挿入。
  - (4) `ToolType::Shape` のプリセット図形選択 UI（Rect/Ellipse/Star/Polygon/Line/Triangle のアクティブ切替。現状シェイプ作成は `Shape` 単独か `Rectangle/Ellipse` ツールの `rectangleToolMode_` 切替のみで、Panel 上にプリセット導線なし）。
  - (5) シェイプ operator stack（TrimPaths/Merge Paths/Offset/Pucker/Rounded/Wiggle/ZigZag/Twist/HandDrawnWobble/Stroke taper、9種実装済）のシェイプ VP 上インスペクタ／数値ハンドルドラッグ編集。
  - (6) パスの open/closed トグル、smooth toggle、corner ↔ bezier 切替を VP ハンドルで（`MILESTONE_2D_SHAPE_MODELING_EDITING_2026-06-29.md:243-245` の selection grammar 整備と並ぶ）。
  - (7) シェイプレイヤー選択時の頂点/セグメント/タンジェントの選択 grammar をメイン VP 上で完成させ、`MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md:11-12,29` で計画中の `Artifact.Widgets.LayerEditor.ShapeOverlay` / `ShapeEditSession` / `ShapeHoverController` へ接続する。
- **価値／懸念:** AE 互換のシェイプ編集（特に頂点ドラッグ・tangent smooth・polygon segment insert・operator ハンドル）は既存の描画・データ層を破壊せずに機能を乗せられる層が既に厚く、メイン VP 側の実装ギャップはおおむね UI と routing 追加で済む。一方で (1) RenderOverlay への新規シェイプ専用 HUD 描画と `(7) ShapeEditSession` 抽出は `MILESTONE_LONG_MODULE_SPLIT_2026-08-31.md` と相互作用し、`ArtifactCompositionRenderController.cppm` 28214 行・`ArtifactShapeLayer.cppm` 3380 行・`ArtifactCompositionRenderOverlay.cppm` 1827 行という巨大ファイル状態では変更影響範囲の見積もりが難しい。`MILESTONE_FLUID_COMPONENT_VS_PYRO_DOMAIN_SPLIT_2026-07-01.md` の "incremental / stable" 方針に従い、まず最小スライスで 1 機能ずつ上げるのが安全。
- **次に必要なユーザー判断:**
  1. スコープ: 既存の `MILESTONE_SHAPE_SVG_EXPORT_AND_KEYFRAME_VERIFY_2026-08-22.md` Phase D（キャンバス頂点編集）と Phase E（複数シェイプ）をそれぞれ独立に進めるか、または一括で (1)〜(7) をフェーズ計画に起こすか。
  2. 編集ホスト: メイン VP で直接編集（既存 `ArtifactCompositionRenderController` を拡張）か、`ArtifactRenderLayerWidgetv2`（LayerEditorPanel）側に集約して「ソロビュー」相当の編集ペインにするか。
  3. データモデル: 現状の単一 primitive を維持して `ShapeType` をツールプリセットにマップするか、コア `ShapeGroup` ベース（Phase E）へ移行してから VP 操作を実装するか。

## 検証運用

- **2026-09-04 — AI セグメンテーションの Core 契約:** `ArtifactCore/include/AI/ImageSegmenter.ixx` と `ArtifactCore/include/Image/DepthMap.ixx`。**事実:** 既存の `applySegmentationMask()` は空実装で、推論結果を書き込む `DepthMap` API がなかった。**対応:** 推論を `IImageSegmenter`（正規化された1ch前景マスク出力）へ限定し、Core 側で bilinear resample、閾値・softness・反転、alpha乗算／置換を適用する共有契約を実装した。`refineSegmentationMask()`で閾値／softness、foreground expand／contract、featherの共通後処理を追加し、`segmentBatch()`で複数の静止画／フレームから非破壊マスクを一括生成できるようにした。`analyzeSegmentationMask()`は foreground coverage／平均信頼度／bounds を返し、空マスクや過大マスクを App 側で警告できる。連番では `stabilizeSegmentationMask()` が前フレームマスクを控えめに混ぜ、推論のちらつきを抑える（動き追従は行わない）。モデル未配置時は、非AI・低品質であることを明示した `LuminanceImageSegmenter` を高コントラスト素材用のフォールバックとして追加した。**価値／懸念:** ONNX／DirectML、CPU fallback、将来のGPU推論はいずれも同一結果型に接続できるが、実モデル・モデル資産契約・GPU経路／実機品質は未検証。**次に確認:** 人物セグメンテーションモデルを1つ選定し、静止画の alpha 結果と既存 GPU mask 合成の preview／export parity を確認する。

- **2026-09-04 — ONNX/DirectML セグメンテーションアダプタ:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**事実:** 既存のONNX DirectML実装はテキスト生成専用で、画像モデルの入力・出力を `IImageSegmenter` に正規化する実装がなかった。またONNX compile definition/link は `ArtifactCoreAI` ではなく親 target にのみ付与されていた。**対応:** NCHW float 入力、最終2次元をマスクとするfloat出力のONNXモデルを、DirectML優先で読み込み、`DepthMap`へ戻すアダプタを追加。入力RGBのscale／mean／stddevと出力のNone／Sigmoid／Softmax、複数出力モデルの `outputIndex` を設定可能にし、出力マスクは bilinear で元解像度へ戻してsoft matteの連続値を保つ。AI target 自身へONNX link/defineを付与した。**価値／懸念:** 背景除去モデルをCoreだけで動かせるが、複数入力・動的shapeなどは設定契約を拡張してから対応する。**次に確認:** 実モデル（例: U²-Net系）を配置し、人物／髪のマット品質、DirectML利用、失敗時メッセージを実機確認する。

- **2026-09-04 — ONNX image module の明示BMI参照:** `ArtifactCore/cmake/ArtifactCoreModuleReferences.cmake`。**事実:** `ArtifactCore` は実装 `.cppm` の primary interface attachment を自動dependency scanへ任せず、同ファイルで明示的なBMI参照を管理する。**対応:** `OnnxImageSegmenter.cppm` に primary interface と `Core.AI.ImageSegmenter` の参照を追加した。**価値／懸念:** Ninja/MSVCのdyndep不安定化を避けられるが、今後の新規 `import` 追加時にも同ファイルを同期する必要がある。**次に確認:** ユーザー許可後のCMake生成／ビルドで、OnnxImageSegmenterのIFC参照とONNXヘッダ解決を確認する。

- **2026-09-04 — ONNX image model diagnostics:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**対応:** `modelInfo()` に ready、DirectML有効状態、入力サイズ・チャンネル、入力／出力テンソル名をまとめた read-only snapshot を追加。**価値:** App/UIを変更せずに、モデル契約と実行バックエンドの診断を接続できる。**次に確認:** 実モデル読み込み時にsnapshotとONNX Runtimeのsession情報が一致すること。

- **2026-09-04 — ONNX segmentation JSON configuration:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**対応:** `loadOptionsFromJson()` を追加し、入力サイズ、前処理、letterbox、出力選択／activation、DirectML優先度を外部JSONから読み込む。既存sessionは設定変更時にresetする。**価値:** モデル資産を後で導入する際、コード変更なしにモデル固有契約を再現できる。**次に確認:** 実モデルの配布設定JSONを1つ作成し、モデル入力仕様と照合する。

- **2026-09-04 — ONNX segmentation letterbox pre-process:** `ArtifactCore/include/AI/OnnxImageSegmenter.ixx`、`ArtifactCore/src/AI/OnnxImageSegmenter.cppm`。**対応:** `preserveAspectRatio` と padding value を追加し、固定サイズモデルへletterboxで渡し、出力マスクを同じ座標変換で元解像度へ戻す処理を追加。既定はstretchで後方互換を維持。**価値:** 縦長素材や正方形モデルで人物形状を歪めずに推論できる。**次に確認:** 16:9／9:16／1:1の実モデル結果でpadding境界とmask座標を確認する。

- **2026-09-04 — セグメンテーション失敗診断の統一:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。**対応:** `IImageSegmenter::lastError()` を共通契約へ加え、`segmentBatch()` が未ready・各item失敗・不正itemの最後の理由を返すようにした。**価値:** App側の一括処理UIが推論失敗を空マスクと誤認せず、ユーザーへ具体的に表示できる。**次に確認:** 実ONNXモデル不在・不正モデル・正常モデルでエラーが期待どおり更新されること。

- **2026-09-04 — 複数セグメンテーションマスクのCore合成:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。**対応:** `combineSegmentationMasks()` に Replace／Union／Intersect／Subtract を追加。入力解像度が異なっても `DepthMap` のbilinear samplingで target座標へ合わせる。**価値:** 人物＋髪、AIマスク＋手動補正、複数推論モデルの結果をQImage経由なしに共通マットへ統合できる。**次に確認:** 異解像度マスクでの境界品質、連続アルファのSubtract意味論、GPU cutoutとのpixel parity。

- **2026-09-04 — セグメンテーション自動適用の受入れガード:** `ArtifactCore/include/AI/ImageSegmenter.ixx`。**対応:** `acceptsSegmentationMask()` と coverage／平均信頼度のしきい値設定を追加。**価値:** 空、または誤って画面全体を前景と判定したマスクを、Appが非破壊プレビューのまま停止・確認できる。**次に確認:** 実モデル別に人物／物体／背景なし素材の適正な閾値を決める。

- **2026-09-04 — OpenCV RotoBrush の IImageSegmenter adapter:** `ArtifactCore/include/AI/RotoBrushImageSegmenter.ixx`、`ArtifactCore/src/AI/RotoBrushImageSegmenter.cppm`。**事実:** 既存 `OpenCVRotoBrushEngine` はGrabCut初期マスク、前景／背景ストローク、Optical Flow伝播を実装済みだが、画像AIの共通契約へ未接続だった。**対応:** canonical BGRA float bufferを既存engineへ明示的に渡し、出力 `CV_8UC1` を `DepthMap` へ変換するadapterを追加。`propagateToNextFrame()` で、初期マスクを作成後の既存 Farneback flow 伝播をCore APIとして公開した。**価値:** モデルが無い環境でも、手動補正付きマットをONNX経路と同じbatch／refine／apply経路に渡せ、連番ではRotoBrushの追従を利用できる。**次に確認:** 現行engineのストローク座標・GrabCutマット・OpenCV例外時・大きなオクルージョンでの実機結果を確認する。

- **2026-09-04 — 軽量タスク facade の配置:** `ArtifactCore/include/Thread/LightweightTask.ixx` に、共有 `QThreadPool` を使う `executeLightweightTask` / `dispatchLightweightTasks` と、完了・キャンセル・失敗状態だけを持つ `LightweightTaskContext` を追加した。**事実:** 既存の `ThreadPool`、`Parallel`、`BackgroundTaskWorkerPool` は粒度や責務が異なる。**価値／懸念:** 短い非同期処理の入口を統一できる一方、context はタスク完了前に破棄できず、タスク内から `wait()` するとデッドロックする。**次に確認:** 実利用箇所を1つ選び、キャンセル・例外・pool飽和時の挙動をビルド／runtimeで検証する。

- **2026-09-04 — 2D Transform Gizmo の視覚ノイズ削減（Scale 中央 Y+ 軸線・Rotate 楕円重ね・軸 sweep 縮小）:** `Artifact/src/Widgets/Render/TransformGizmo.cppm` の `drawScaleCenterHandle` から Y+ 軸線 + tip ハンドルを撤去し、Rotate 描画ブロックから `drawEllipse` 2 本（X 軸赤 / Y 軸緑）と 68° sweep の X/Y 色分け弧 4 本のうち範囲を 36° に縮小。**事実:** 旧 `GIZMO_IMPLEMENTATION_STATUS_2026-04-10.md` の「Scale の中心→四隅 X 線」記述は既に解消済みで、現コードの X 線正体は中央ハンドルの Y+ 軸線だった。Aspect Lock は `isCornerScaleHandle()` 側にあり、Center ハンドルの Y 軸線とは無関係。Rotate リングは `hitThickness = ringThickness * rotateRingHitBoost` で既に hit area と visual thickness が分離済み。**価値／懸念:** X 線ノイズ・4 軸 rainbow 効果・楕円重ねがそれぞれ薄れ、平面/画像レイヤーの Scale と Rotate 操作の視認性が上がるはず。`drawEllipse` ローカル関数（816 行）は未使用になるが残置、hit test・Undo・ショートカットには触れていない。**次に確認:** ユーザー許可後に `Artifact` のモジュールビルドを実行し、`ArtifactTransformGizmo` の IFC が正常に再生成され、Scale 4 隅ハンドル・Center ハンドル・Rotate リング・Leader・Drag arc の描画が既存と一致することを確認。

- **2026-09-04 — M-VP-9 Navigation Contract 現状マップの固定:** `docs/technical/MILESTONE_VIEWPORT_NAVIGATION_CONTRACT_STATUS_2026-09-04.md` を新規作成し、既存実装の静的マップ（Alt+LMB orbit / MMB pan / Wheel zoom / `PreviewOrbitSnapshot` による orientation・pan・zoom 保存復元 / Frame Selected・All・View Undo・Redo の QAction + QShortcut 経路 / `activeViewport()` 系）と未着手項目（navigation cross 表示 / active viewport 細い枠 / preview-only と camera layer の厳密分離 / pivot・orbit source selector / surface snap）を表形式で明文化した。**事実:** `ArtifactCompositionEditor.cppm:9220-9272` の `setPreviewOrbitMode` は camera state のみを snapshot 化し、navigation session フラグ（`isAltOrbiting_` / `isPanning_` / `isAltZooming_`）は含まれない。`maskNavigationLocked` 経路は ON 時の抑制のみ。Work Cursor は配置・中央化・消去・overlay 表示まで既存、Pivot source 切替と surface snap は未着手。**価値／懸念:** AGENTS.md の「RenderScheduler / DX12 パスはシビア扱い」「既存挙動を不用意に変えない」「新規 signal/slot 接続禁止」「QPainter / QImage / QtCSS 禁止」に従うと、navigation cross 追加は Editor → Overlay への状態渡し経路が必要で pane manager (M-VP-2) 移行と密結合のため、Phase 3 では**コード改変ではなく状態マップの固定**で止めた。**次に確認:** ユーザー許可後に (1) preview-orbit snapshot に `isAltOrbiting_` / `isPanning_` / `isAltZooming_` フラグを含めた場合の復元整合、(2) navigation cross を `previewOrbitMode_` ON 時のみ theme token のみで描画する場合の最小実装可否、(3) active viewport 細い枠を pane manager 移行なしで 1 段重ね描画できるかどうか、を順に判断する。

- **2026-09-04 — FFmpeg C API のモジュール境界:** `ArtifactCore/src/Codec/FFmpegThumbnailExtractor.cppm` では、vcpkg の FFmpeg ヘッダが C リンケージを自動付与しない構成だったため、`extern "C"` でグローバルモジュールフラグメント内のヘッダ群を包む必要がある。**事実:** 未解決シンボルが `?av...` と C++ 修飾されていたが、修正後は通常リンクまで進み、`/WHOLEARCHIVE` は複数定義を起こした。**価値／懸念:** C++20 module の import／リンク問題に見えても、まず ABI のリンケージ名を確認する。**次に確認:** FFmpeg を参照する他の module 実装でも同じヘッダ配置を維持する。

- **2026-09-02 — C++ module split target の IFC 参照は分岐順に注意:** `ArtifactCore/CMakeLists.txt` では `src/Mask/` の包括分岐が個別の `RotoMask.cppm` 分岐より先に評価されるため、後置した個別参照設定だけでは実際のコンパイルコマンドに反映されない。**関連:** `ArtifactCore/CMakeLists.txt`, `RotoMask.cppm`, `ConfigSchema.cppm`, `Artifact/CMakeLists.txt`。**価値／懸念:** split target 化では「設定が存在する」だけでなく、最終的な source property の適用順と生成コマンドへの反映を確認する必要がある。**次に確認:** ユーザー許可後の再生成・ビルドで、対象コマンドに `/reference` が現れ、C1199 が解消することを確認する。

- ビルド・テスト・CMakeはユーザーの明示許可後に実行する。
- runtime検証済みになった項目は、このファイルからアーカイブへ移す。
- 実装済みの細かな履歴や重複した検証候補は、新規Insightとして追加せずアーカイブを更新する。
## 2026-09-04 — Spatial Audio Object契約を既存3D Audio Layerへ接続

- **関連:** `Artifact/src/Layer/ArtifactSpatialAudioLayer.cppm`、`ArtifactCore/include/Audio/Spatial/SpatialParams.ixx`、`ArtifactCore/src/Audio/Spatial/SpatialRenderer.cppm`
- **事実:** 既存のSpatialRendererは距離減衰・azimuth panningを実装済みだったが、Audio Objectのstable ID、spread、gain、mute、enabledの保存契約が不足していた。
- **対応:** stable UUIDとAudio Object状態を追加し、JSON保存/復元、Property経路、最小ステレオspreadを既存レンダラーへ接続した。
- **未検証:** ビルド・実機再生・旧プロジェクトfixtureによる復元は未実行（ユーザー許可待ち）。
- **次の確認:** M-AU-9.2としてcallback境界でのallocation/lock不在、mono/stereo入力、seek/restart時の状態リセットを確認する。

## 2026-09-04 — mono空間音源のstereo preview拡張

- **関連:** `ArtifactCore/src/Audio/Spatial/SpatialRenderer.cppm`
- **事実:** 入力がmonoの場合、従来の出力チャンネル数判定が1chを維持し、左右のazimuth gainを利用できなかった。
- **対応:** Phase 1のpreview契約として、出力バッファが1ch以下でも最低2chを確保し、stereo layoutを設定するよう修正した。
- **未検証:** 実機再生とサンプルレート別の音量・位相確認は未実行。

## 2026-09-04 — 7.1.4レイアウト契約の共通化

- **関連:** `ArtifactCore/include/Audio/AudioSegment.ixx`、`ArtifactCore/src/Audio/AudioBus.cppm`、`ArtifactCore/src/Audio/AudioDownMixer.cppm`、`ArtifactCore/src/Codec/FFMpegAudioDecoder.cppm`
- **事実:** 既存レイアウト列挙には 7.1.4 がなく、12ch入力が汎用Stereoへフォールバックしていた。
- **対応:** 12chを `Surround714` として識別し、バス確保、downmixerの恒等マッピング、リングバッファ、LipSync、FFmpeg decoder、LFE判定へ接続した。
- **価値／懸念:** 7.1.4素材のチャンネル数とレイアウトを失わず保持できる。現時点では各スピーカーへの object 配分係数、UI／Render Queue選択、7.1.4からの明示的downmix係数は未実装・未検証。
- **次に確認:** 7.1.4 bedの標準チャンネル順を固定し、Render QueueとPreviewの出力選択へ接続する。

## 2026-09-05 — Particle 3D のレイヤー変換境界

- **関連:** `Artifact/src/Layer/ArtifactParticleLayer.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`、`ArtifactCore/include/Graphics/ParticleData.ixx`
- **事実:** `ArtifactParticle3DLayer` は `is3D=true` で camera view/projection 経路へ入り、3D分岐では2D `QTransform` を避けている。`ArtifactFormParticleLayer` も Grid3D 時に同じ経路へ入るが、レイヤーのモデル行列は未設定だった。
- **対応:** `ParticleRenderData`、Core constant buffer、GPU cull shader、vertex shader、Diligent submitterを通るmodel matrix経路を追加し、Form Particle のGrid3Dにも接続した。2D経路はidentity model matrixを維持する。
- **価値／懸念:** camera orbit とlayer transformの責務を分離できる。constant-buffer layoutを変更したため、D3D12/Vulkan双方でshader/PSO再生成を伴う。
- **次に確認:** runtimeで Particle 3D の位置・回転・scale を個別に変更し、camera orbitとは独立して反映されるか、GPU cullと2D Particleに回帰がないか確認する。

## 2026-09-05 — PhysicsSystem のモジュール内コンテナ破棄

- **関連:** `ArtifactCore/src/Physics/PhysicsSystem.cppm`、`ArtifactCore/include/Physics/MpmSolver2D.ixx`、`ArtifactCore/src/Memory/SharedPtr.cppm`
- **事実:** MSVC 19.51 は、exported `PhysicsSystem` の `std::map<LayerID, SharedPtr<MpmSolver2D>>` を破棄するテンプレート展開中に C1001 を発生させた。`MpmSolver2D` は import 済みの完全型であり、所有契約の不備は確認されていない。
- **対応:** `LayerID` には `std::hash` がないため `IdMap` は採用せず、該当ストアを既存の `NamedVector` による小さなキー付きエントリへ移した。デストラクタもクラス外 default 定義へ置き、キー別の登録・取得・削除・列挙の契約を保持しつつ、MSVCが落ちる `std::_Tree` と `std::_Hash` の実体化を排除した。
- **未検証:** 影響する最小経路は Core の起動・PhysicsSystem singleton の終了時破棄。ユーザーのビルドで C1001 が解消すること、および Physics/Material solver の生成・破棄を確認する。

## 2026-09-05 — Native Dock の右端編集面を限定

- **関連:** `Artifact/src/AppMain.cppm`、`Artifact/src/Widgets/ArtifactMainWindow.cppm`
- **事実:** Native Dock Surface では Inspector 系に加え、Layer View、Contents Viewer、Audio Mixer も右端へ登録されていた。AI Cloud は既に起動時非表示だった。
- **対応:** Layer View を Composition Viewer の中央タブ、Contents Viewer を Project の左タブ、Audio Mixer を Timeline の下部タブへ移した。右端は Inspector / Properties / Components / Effects を優先し、AI Cloud は非表示のままにした。
- **未検証:** 初期レイアウト、既存保存レイアウトからの復元、Timeline 未生成時の Audio Mixer の下部配置は未実行。

## 2026-09-05 — Default ワークスペースの初期可視性を軽量化

- **関連:** `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- **事実:** Default ワークスペースは Composition Viewer / Project / Asset Browser / Inspector / Effects / Properties と Timeline を同時に可視化していた。
- **対応:** Default は Composition Viewer / Project / Inspector のみを表示し、Asset Browser、Effects、Properties と Timeline を初期非表示にした。Animation ワークスペースは Timeline の自動表示を維持する。
- **未検証:** 保存済みレイアウト復元後の可視状態、および明示的に開いた Asset Browser / Timeline の操作性は未確認。

## 2026-09-05 — ギズモ入力座標と即時描画の境界

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、`ArtifactCompositionRenderOverlay.cppm`、`Artifact/src/Render/ArtifactIRenderer.cppm`
- **事実:** press/move の入口で物理ピクセルへ変換済みだが、projected frame press と3D dragで DPR を再乗算していた。フレームの8ハンドルより3D軸判定が優先されていた。描画は即時ではなく頂点を蓄積し、flush 時のカメラ行列を使う。即時描画と判断して末尾 flush を削除したのは誤りだった。
- **対応:** 二重変換を削除し、近いフレームハンドルを優先。フレーム描画前に flush し、選択イベントでも base composite を無効化。標準モードは移動軸とフレームに整理した。
- **未検証の別件:** 過去フレームの平面操作にも同様の DPR 再乗算が見える。今回の現行フレーム修正範囲から分離し、motion frame操作を次に確認する。共通矢印プリミティブの形状変更はライト等の矢印にも反映されるため外観確認が必要。
- **確認待ち:** 平面追加直後、100/150/200%表示倍率での8方向ドラッグ、回転モード、Undo/キャンセル。ビルド・実機操作は未実行。
- **水色全面表示への修正:** フレーム末尾の `flushGizmo3D()` を復元し、カメラ行列のリセット前にハンドル頂点を送信する。水色全面表示の解消は実機未確認。
- **初期フィット・リサイズ追補:** 通常2D平面のフレームに scene camera が使われる経路を canvas pan/zoom に統一。外側余白を描画・pick・snap・固定点の全箇所から除去した。ドラッグ中の単一レイヤー再同期を止め、固定点補正は2Dのvisual/local scale比と3D回転を考慮する。単一レイヤーのdrag通知は既存の間引き処理を使用し、releaseで最終値を強制通知する。負荷改善と初期フィット、回転・親付きレイヤーの対辺固定は実機未検証。
- **位置飛びの再調査:** `positionXAt/YAt` は初期位置を含まないoffsetで、`snapshotAt` が初期位置を加算することをCore実装で確認。ギズモのpress/modal/group/release保存を後者へ変更した。`UndoManager::push` は即redoするため、releaseでoffsetを絶対位置として再適用すると位置が飛ぶ。通常ビュー行列は実際の `canvasToViewport` から構成し、ギズモ描画による2D行列上書きを撤去、drag rayは開始カメラを保持する。直交ギズモのサイズは描画viewの倍率から計算する。ユーザー報告の3症状について実機での改善確認は未完了。
- **正面直交ビューへの統一:** ユーザー指定により起動時・2D復帰をFrontへ統一。正面の投影は直交、その他の方向は既存透視投影を使用する。直交カメラはprojection側にzoomを保持するため、ギズモのサイズ補償もviewport zoomへ合わせた。コンポジション境界の水色重ね描きを中立色へ変更。panにも既存のinteraction通知を追加し、操作中のLODとreadback抑制を有効にする。パン・ズームが重い主因と改善量は未測定で、実機確認が必要。
- **スナップ設定:** 既存ViewメニューのsnapGuidesチェックと永続化経路を再利用し「コンポジション／ガイドにスナップ」としてprojected frameの移動・リサイズへ接続。Frontのみ、Altで一時解除、10 logical pxの閾値。コンポジション境界はフレームと同じ投影を使用する。OFF時は吸着ガイドを消去。定規は未設定時に非表示、保存済み設定は維持。実機でON/OFF・再起動・Alt・ズーム倍率別の吸着・Undoは未検証。
- **正面消失・スナップ追補:** Qt直交行列の近側は負のNDC深度になるが、PrimitiveRenderer3Dのshaderは投影結果をそのままSV_Positionへ送っていた。Frontの直交投影をD3D12/Vulkanの0..1深度へ補正した。移動軸のpress経路はprojectedFrameMoveを立てないため吸着対象外だったので、位置適用方式は変えずScreen移動とWorld/ViewのX/Y移動を吸着対象へ追加した。軸拘束と直交するガイドは表示しない。ローカル軸の吸着、Shift精密操作・Ctrl量子化との併用は未検証。正面での再表示、通常移動・8方向リサイズの吸着、OFF/Alt、ドラッグ確定・Undoを実機で確認する必要がある。GPUリソース・同期・キャッシュの寿命は変更していない。

## 2026-09-05 — 頂点のみPLY点群の最小受入

- **関連:** ArtifactCore/src/Geometry/MeshImporter.cppm (loadPly)、Artifact/src/Layer/Artifact3DModelLayer.cppm (draw)、3D系ファイルフィルタ3箇所
- **事実:** loadPly は aceCount<=0 を拒否していたため頂点のみPLYが読めず、FileTypeDetector は既に ply を Model3D 扱いなのに開く側のフィルタ3箇所に *.ply が無かった。Mesh::isValid() は polygon>0 必須だが Artifact3DLayer::loadFromFile は ertexCount>0 判定なので importer 側の修正だけで meshLoaded_ まで届く。generateRenderData() は polygon 無しで空を返すため drawMesh 経路では何も出ない。
- **対応:** loadPly で face無しを受入れ、頂点色 (red/green/blue 系・uchar/float両対応) を color アトリビュートへ格納、頂点上限200万で拒否。Artifact3DLayer::draw の Solid/Wireframe 両経路に polygon==0 時の点描画フォールバック (3軸クロス、32768点cap・stride間引き、頂点色・opacity反映、trace points-submitted) を追加。フィルタ3箇所へ *.ply 追加。.ixx 変更なし。
- **未検証:** ビルド・実機表示 (頂点のみPLY、色付きPLY、既存ポリゴンPLYの回帰)、200万超・バイナリPLYのエラー表示、大規模点群の描画負荷。バイナリPLY/LAS/LAZ/E57 は対象外のまま。

## 2026-09-05 — 点群フォールバックの継続 (選択表示・テスト)

- **関連:** Artifact/src/Layer/Artifact3DModelLayer.cppm (drawSelectionOutline)、	ests/ArtifactCore/MeshImporterPlyTest.cpp、	ests/ArtifactCore/CMakeLists.txt
- **事実:** polygon==0 の点群は選択アウトラインの polygon ループが空振りで何も出なかった。MeshImporter への gtest は存在せず、	ests/models/test.obj を参照するテストも無かった。drawFractureOverlay は mesh 非依存のため点群でも安全。
- **対応:** 選択時は world-space の bounds box (12辺) を outline 色で描画。PLY は一時ファイル自己完結の gtest 6件 (face無し/face0/uchar色/ポリゴン回帰/欠損/バイナリ拒否) を追加し、ArtifactCoreMeshImporterPlyTest として登録。
- **未検証:** ビルド・テスト実行はユーザー指示で保留 (ArtifactCore の増分ビルドは MeshImporter.cppm のコンパイル・リンクまで成功済み、Artifact 側は中断)。

## 2026-09-05 — バイナリPLYと点サイズ調整

- **関連:** ArtifactCore/src/Geometry/MeshImporter.cppm (loadPly)、Artifact/src/Layer/Artifact3DModelLayer.cppm、	ests/ArtifactCore/MeshImporterPlyTest.cpp
- **事実:** バイナリPLYは finiteness・色・face を含め未対応だった。点の見た目は bounds 由来の自動サイズのみで調整手段が無かった。
- **対応:** loadPly をヘッダ構造体解析へ組替え、binary_little/big_endian に対応 (型: char〜double・list face・uchar/float等の色正規化、200万点上限・truncated 検出は ASCII と共通)。Artifact3DLayer に 
ender.pointSize (0.25〜8.0、既定1.0、JSON保存・Inspector・set 反映、crossHalf に乗算) を追加。.ixx 変更なし (normalLength と同方式)。テストはバイナリ実ペイロード (LE/BE/truncated) へ置換え。
- **検証:** ArtifactCore 増分ビルド成功、Artifact3DModelLayer.cppm.obj 単体コンパイル成功。Python ミラーで LE/BE 値と byteswap 経路を確認。テスト実行・実機表示は未実施。LAS/LAZ/E57 は対象外のまま。

## 2026-09-05 — 非圧縮LASの最小受入とE57見送り判断

- **関連:** ArtifactCore/src/Geometry/MeshImporter.cppm (loadLas)、ArtifactCore/include/Geometry/MeshImporter.ixx (Backend::Las)、ArtifactCore/src/File/FileTypeDetector.cppm、3D系フィルタ3箇所、	ests/ArtifactCore/MeshImporterLasTest.cpp
- **事実:** LAS 1.0-1.4 の非圧縮 point format 0-8 はヘッダ固定オフセット (scale/offset/count) と record 先頭の XYZ・format別RGB位置で読める。LAZ は同一シグネチャのまま圧縮本体のため別 decoder が要る。E57 (ASTM E2807) は XML＋packet/codec バイナリ構成で、Qt の XML だけでは binary section の実装が数百行規模になる。
- **対応:** loadLas を追加 (format 0-8、XYZ+RGB/intensity→gray、上限200万点、waveform/未知format・不正scale・truncated は明示エラー、LAZ は非対応メッセージ)。dispatch・Backend::Las (末尾追加)・FileType・フィルタ3箇所へ las 配線。gtest 4件 (format2 RGB / format0 intensity / waveform拒否 / 非LAS拒否) を登録。E57 は外部libなしの自前実装を見送り。
- **検証:** ArtifactCore・ArtifactCoreFile 増分ビルド成功。Python ミラーで header/record オフセットと期待値を照合。テスト実行・実機表示は未実施。

## 2026-09-05 — 空間音声の時刻評価と広域出力の境界

- **関連:** `Artifact/src/Layer/ArtifactSpatialAudioLayer.cppm`、`Artifact/src/Layer/ArtifactAbstractLayer.cppm`、`ArtifactCore/src/Audio/Spatial/SpatialRenderer.cppm`
- **事実:** 音声要求は start frame を持つが、`getGlobalTransform4x4()` は現在タイムライン時刻を参照する。SpatialRenderer の係数配列は 8 要素で、出力 scratch のチャンネル数を元にループする。
- **今回の対応:** SpatialAudio の出力 scratch を空から開始し、mono/stereo 素材の stereo preview に限定した。
- **懸念（未検証）:** 先読み／export 時の位置評価が要求音声時刻からずれる可能性がある。汎用レンダラーへ 12ch scratch を直接渡す将来経路では固定長配列の範囲外アクセスに注意が必要。
- **価値／次の確認:** 任意時刻の親子 transform 評価を共通 API で提供できるか確認する。7.1.4 接続前に出力 layout と係数容量の契約を確定する。

## 2026-09-05 — 点群のvoxel間引き (LOD最小スライス)

- **関連:** ArtifactCore/src/Geometry/MeshImporter.cppm (decimatePointCloud)、	ests/ArtifactCore/MeshImporterPlyTest.cpp
- **事実:** 取込上限200万点でも描画は32768クロスへstride間引きするだけで、メモリ (2M点で約100MB超) と形状代表性に課題があった。octree/out-of-core は別規模の設計になる。
- **対応:** 頂点のみ点群に voxel 間引きを追加 (budget 262144、初回cellは体積/上限の立方根、不足時は最大8回半分化、セル先着・入力順決定性・色連動、NaN/極端座標ガード)。PLY/LAS 両経路で適用し、間引き時は qInfo で before→after を出す。gtest に30万点バイナリの間引き・決定性テストを追加。
- **検証:** ArtifactCore・ArtifactCoreFile 増分ビルド成功 (Property.ixx の C5202 は既存)。Python ミラーで cell・kept数・先頭点保持を確認。テスト実行・実機表示は未実施。

## 2026-09-05 — 点群テスト13件が実実行で成功・QDataStreamの罠

- **関連:** 	ests/ArtifactCore/MeshImporterPlyTest.cpp (9件)、	ests/ArtifactCore/MeshImporterLasTest.cpp (4件)
- **事実:** ARTIFACT_BUILD_TESTS=OFF のため再configure (-DARTIFACT_BUILD_TESTS=ON) して実行。初回は binary 4件が失敗したが、原因はローダーではなく QDataStream の既定 DoublePrecision (float が8バイトで書かれる) だった。デバッグテストで bodyHex を確認し、setFloatingPointPrecision(SinglePrecision) で解決。LAS側は整数/double明示書きのため当初から成功。
- **対応:** binary fixture 全件に SinglePrecision を指定、デバッグテストは削除。PLY 9件・LAS 4件の全成功を確認。
- **教訓:** バイナリ fixture を QDataStream で書く場合は SinglePrecision を必ず指定すること。
- **未検証:** 実機での点群表示・選択・Inspector (アプリ全体ビルドは未実施)。

## 2026-09-05 — 3D Stroke (PathTube trim) と Lux (Light glow) の最小実装

- **関連:** ArtifactCore/.../Procedural3DGenerators.{ixx,cppm}、Artifact/src/Layer/ArtifactProcedural3DLayer.cppm、Artifact/src/Layer/ArtifactLightLayer.cppm、Artifact/include/Layer/ArtifactLightLayer.ixx、	ests/ArtifactCore/PathTubeTrimTest.cpp
- **事実:** PathTube に trim 概念が無く、Light層に可視 glow が無かった。ArtifactLightLayer::shouldIncludeInFinalRender() は false のため Light の draw 内容はビューポート限り。3D層描画時は Scoped3DLayerCamera により particle 3D カメラが有効 (ArtifactCompositionRenderController.cppm:7924) のため、Light の draw 内 drawParticles は world 座標で解釈される。
- **対応:** PathTube に 	rimStart/trimEnd (既定0/1で旧挙動一致、taper/twist/UV は trim 範囲追従、反転は空メッシュ) を追加し、層の JSON・Inspector・setter を配線。Light層に Light/Glow・Glow Size・Glow Intensity (既定ON/1/1、Point/Spot/Area のみ、range/cone連動サイズ・additive billboard 1 sprite) を追加し、JSON・Inspector・setter を配線。最終レンダーへの glow は render-queue 側の別件として残す。
- **検証:** ArtifactCore ビルド、ArtifactProcedural3DLayer・ArtifactLightLayer 単体TUコンパイル、trim gtest 4件全成功。実機表示は未実施。

## 2026-09-05 — Mesh法線生成とbounds sphere

- **関連:** ArtifactCore/include/Mesh/Mesh.ixx、ArtifactCore/src/Mesh/Mesh.cppm、ArtifactCore/src/Geometry/MeshImporter.cppm (loadPly)、	ests/ArtifactCore/MeshGeometryTest.cpp
- **事実:** PLYポリゴンは法線ゼロで書き出され、ライティングが壊れていた (STLはfacet法線を自前計算、ufbxはソース保持)。Mesh にトポロジからの法線生成が無く、bounds も AABB のみだった。
- **対応:** Mesh::computeVertexNormals() (面積重みスムーズ法線、縮退面スキップ、未使用頂点は(0,0,1)、revision bump) と oundingSphereCenter/Radius() (AABB版、updateBoundsで同時更新) を追加。loadPly は面あり時のみ法線生成。gtest 5件 (quad/縮退/点群no-op/sphere/PLY経由) を追加。
- **検証:** MeshGeometryTest 4件・MeshImporterPlyTest 10件の全成功を確認。

## 2026-09-05 — 真の弧長サンプリングと解析的接線

- **関連:** ArtifactCore/.../BezierCalculator.{ixx,cppm}、ArtifactCore/.../BezierPathSampler.{ixx,cppm}、	ests/ArtifactCore/BezierArcLengthTest.cpp
- **事実:** evaluatePath はセグメント毎t均等であり、sampleEquidistant/sampleByCount/sampleWithTangents の「等間隔」は不正確だった (不等長セグメントで偏る)。接線はeps差分近似だった。外部呼出しはサンプラ内部のみで、変更の波及は閉じている。
- **対応:** 弧長テーブル (32分割/segment・二分探索・t補間) を追加し、sampleEquidistant/sampleByCount/sampleWithTangents を弧長経路へ切替 (シグネチャ不変)。pointAt はt均等のまま互換維持し、pointAtArcLength/	angentAtArcLength/sampleArcLength を新設。evaluateTangent (解析的導関数・縮退時(1,0)) を追加し、	angentAt を含め差分近似から置換え。縮退パスは有限値を返す。
- **検証:** gtest 6件 (不等長均等・端点・直線接線・縮退・閉ループ・解析接線) の全成功を確認。

## 2026-09-05 — CatmullRom/Hermite実装とbezierEvaluateの式バグ修正

- **関連:** ArtifactCore/include/Geometry/Interpolate.ixx、	ests/ArtifactCore/KeyframeSplineTest.cpp
- **事実:** InterpolationType::CatmullRom/Hermite は宣言のみで dispatch は Linear 落ちだった。また ezierEvaluate の Newton ソルバとy評価式に余分な mt3 項があり (P0=(0,0) なのに +mt3)、全Bezierイージングがずれていた。テストが easy-ease 中点 5.0 に対し 6.25→39.32 を返したことで発覚。Pythonミラーで正値 (0.1292/0.5/0.8708) を確認。
- **対応:** hermiteInterpolate/catmullRomInterpolate を追加し、KeyframeInterpolator::evaluate で隣接キー参照の CR (均一) と有限差分接線 Hermite (非均一対応) を実装。2点版 interpolate() は Linear 維持。bezierEvaluate は mt3 を除去し正規形へ。gtest 8件を追加。
- **検証:** 8件全成功。既存 Linear/Bezier 挙動の回帰テストを含む。Bezier全般の値が変わるため、既存プロジェクトの見た目差分は実機で要確認。

## 2026-09-05 — CatmullRom/Hermiteのメニュー露出

- **関連:** Artifact/src/Widgets/Menu/ArtifactAnimationMenu.cppm、Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cppm
- **事実:** pplyInterpolationToSelectedKeyframesImpl は type を汎用設定するため、メニュー追加だけで CR/Hermite が適用可能だった。キーフレーム色・ラベル・形状は型別 switch で、新規型は default 落ちだった。
- **対応:** Animationメニュー (キーフレーム補間) と Timeline右クリック (Interpolation) に Catmull-Rom/Hermite を追加。ショートカットは追加しない。キーフレーム色 (紫系2色)・ラベル・形状 (六角/五角) を追加。EasingLab は単区間previewのため対象外。
- **検証:** 両TUの単体コンパイル成功。実機のメニュー表示・適用・保存復元は未実施。

## 2026-09-05 — 不足easingの一括実装

- **関連:** ArtifactCore/include/Geometry/Interpolate.ixx、	ests/ArtifactCore/EasingFunctionsTest.cpp
- **事実:** enumにありながら dispatch が Linear 落ちだった型が多数 (Smooth/EaseOutIn/Quadratic/Cubic/Quartic/Quintic/Exponential/Logarithmic/Sine/Circular/Cosine)。2点版 interpolate() の Bezier は eturn start のままだった。
- **対応:** 純alpha系19種を追加 (CubicIn/InOut、Quartic/Quintic各3種、SineIn/InOut、Circular各3種、Exponential各3種、Logarithmic、Cosine、EaseOutIn、Smooth; Quadratic→EaseIn、Cosine/Smoothは同一曲線)。新規は alpha clamp 付き。Bezierスタブは Linear フォールバックへ (呼び出し側はbezierInterpolateへ迂回済み)。gtest 5件 (端点・既知中点・dispatch・Keyframe経由・範囲外有限) を追加。
- **検証:** 5件全成功。Spring/SmoothDamp (状態持ち)、CustomCurve/Polynomial (係数要)、色文脈系、2点Hermite/CR は対象外のまま。

## 2026-09-05 — EasingLab候補の拡張とBezierプレビュー修正

- **関連:** ArtifactCore/include/Animation/EasingCurveUtil.ixx、Artifact/src/Widgets/Timeline/EasingLabWidget.cppm、	ests/ArtifactCore/EasingLabCurveTest.cpp
- **事実:** EasingLab候補は16種で、新規 easing (Quartic/Quintic/Sine/Circular 等) が未露出だった。同ファイルの Bezier プレビューにも ezierEvaluate と同じ余分な mt3 項があった。
- **対応:** EasingType に EaseOutIn/Smooth/Quartic/Quintic/Sine/Circular を追加し、評価・名称・Interpolation対応・候補一覧を配線。Bezier の mt3 を除去。EasingLab の対応ラベルを追加。gtest 4件を追加。
- **検証:** 4件全成功、EasingLabWidget 単体TUコンパイル成功。実機のダイアログ表示は未実施。

## 2026-09-05 — マット検証 (順序・premult・GPU上限・輝度)

- **関連:** ArtifactCore/.../LayerMatte.{ixx,cppm}、Artifact/src/Render/ArtifactCompositionViewDrawing.cppm、Artifact/src/Render/ArtifactRenderQueueService.cppm、	ests/ArtifactCore/MatteStackTest.cpp
- **事実:** stack順序 (参照順・初回引継) と premult 一貫性 (RGBA一律乗算) は CPU/GPU/Core で一致。相違点: (1) GPU は3マット超で stack 全体を無適用化 (CPU は全数適用)、(2) GPU は Stretch 以外・ネスト/3D/adjustment ソースで stack 全体を無適用化、(3) Core evaluateMatteStack は alpha のみで luma 未対応、(4) 実働経路 (preview CPU・GPU queue) は BT.601 で一致する一方 Core 既定は 709、(5) Core MatteStackMode に Difference が無かった。evaluateMatteStack/MatteEvaluator に実働呼出しは無い。
- **対応:** MatteStackMode::Difference を Core + view の switch に追加 (末尾追加・既定値不変)。>3マット時に preflight Warning 診断を追加。gtest 11件 (sample/combine/apply/順序/skip/反転/空passthrough/roundtrip) を追加。
- **検証:** 11件全成功。TU単体コンパイル成功。実コンポでの受入れ・GPU実機は未実施。

## 2026-09-05 — マスク編集ハンドルは画面座標で判定する

- **関連:** `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`、VP マスク編集
- **事実:** レイヤーローカル座標でハンドルのヒット距離を測ると、レイヤーの非一様スケールやズームにより見た目のクリック領域と判定がずれる。フェザー値ゼロでは実ハンドルが頂点と重なり、直接ドラッグを開始できない。
- **対応:** ハンドル判定を viewport のピクセル距離に統一し、競合時は最も近い候補を選ぶ。ゼロ・フェザーには画面上だけの最小距離ハンドルを表示し、ドラッグ開始点からの法線方向差分でフェザーを立ち上げる。
- **価値／次の確認:** マスク頂点とセグメントのヒット判定も同じ画面距離モデルへ段階的に揃えると、極端なレイヤー変形時の操作一貫性をさらに高められる。実機で非一様スケールしたレイヤーの操作感を確認する。
