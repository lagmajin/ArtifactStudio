# Milestone: Mask Alpha Roundtrip Lite (Trace to Mask)

**最終更新:** 2026-09-10
**Status:** Contract / 実装待ち
**Goal:** 輝度キーやクロマ結果などのアルファから編集可能Bezierマスクを起こし、再トレースで更新する往復を、軽量範囲で使えるようにする。Magnetic snap等の重い吸着は対象外とし、後段に隔離する。

## 調査結果 (2026-09-10, コード確認済み)

- `fromAlphaMask(const void* outMat, const MaskConversionParams&)` は `Artifact/src/Mask/MaskPath.cppm:830-897` に実在。`cv::Mat*` として受け、`CV_8UC1` は固定閾値127、`CV_32FC1` は固定閾値0.5で二値化する。`MaskConversionParams` (`Artifact/include/Mask/MaskPath.ixx:57-62`) に閾値フィールドはなく、`simplificationTolerance=1.5 / cornerThreshold=10.0 / minPathVertices=6 / closedPath=true` のみ。しきい値可変は本MS範囲外の構造変更になる。
- 輪郭抽出は `RETR_CCOMP + CHAIN_APPROX_NONE` → `approxPolyDP` 単純化。hierarchyで外側=Add/穴=Subtractを付与。座標は `x/cols, y/rows` の正規化。`angle >= cornerThreshold` をコーナー (接線ゼロ)、それ以外を前後平均の1/6接線で平滑化する。
- 既存の類似導線は `ArtifactLayerMenu::handleConvertShapeToMask/handleLinkShapeToMask` (`Artifact/src/Widgets/Menu/ArtifactLayerMenu.cppm:3604-3674`)。`AddLayerMaskCommand` は同ファイル内ローカル (`:478-525`) で serialize を持たず、labelも `Create Text Mask` 固定。一方 `MaskEditCommand` (`Artifact/src/Undo/UndoManager.cppm:2647-2675`) は before/after `vector<LayerMask>` スナップショット+`encodeMask` serialize済みで、`handleLoadMaskPreset (:3774-3784)` が同パターンの再利用例。Liteは後者に寄せる。
- `WIDGET_MAP.md` に LayerMenu の明記はないが、Timeline左 (`ArtifactLayerPanelWidget`) は行操作限定、右 (`ArtifactTimelineTrackPainterView`) が直接操作ownerの原則がある。新規バッジ・新規signal/slot・単一キー追加はしない。

## Why

AEのAuto-traceは生成して終わりで、生成後の手直し導線が弱い。Artifactは`MaskPath::fromAlphaMask / fromQPainterPath / toShapePath` (`Artifact/include/Mask/MaskPath.ixx:151-165`) の変換器を既に持つが、編集導線に未接続。本マイルストーンは変換の往復だけを製品化し、重い吸着・マージは別段に回す。

## Scope

- `Artifact/include/Mask/MaskPath.ixx` / `Artifact/src/Mask/MaskPath.cppm` (`fromAlphaMask`既存の露出調整のみ。閾値フィールド追加はしない)
- Trace実行のUI入口1点: Layerメニューのマスク面に `アルファからマスク生成` を1項目追加する。Timeline右ペイン・Viewport右クリック・左ペイン行バッジには増やさない
- `MaskConversionParams`のプリセット化 (tolerance/corner/minVerticesのみ。thresholdは固定0.5/127のまま)

## 契約: UI入口・有効条件 (Phase1/2共通)

- 置き場所: `ArtifactLayerMenu` の mask面 (`convertShapeToMaskAction` 群と同面)。既存の `シェイプをマスクに変換 / リンク` の隣に置き、用語は `アルファからマスク生成 (Trace)` に統一する。
- 有効条件: 選択レイヤーが1件あり、現フレームの単一チャンネルalpha (`CV_32FC1` 0..1 または `CV_8UC1`) が取得できる場合のみ enabled。取得不可・空輪郭の場合は `QMessageBox::information` で理由表示してno-op (既存convert導線と同一UX)。
- 選択・フォーカス・再生状態を変更しない。`ArtifactCompositionRenderWidget` に切替ロジックを持たせない。
- ショートカットは新規にハードコードしない (AGENTS.md ショートカット整合ルール)。必要になれば `ShortcutBindings` 衝突調査を別途行う。

## 契約: プリセット (thresholdなしLite)

- 閾値は固定 (`CV_32F`→0.5、`CV_8U`→127)。プリセットは以下3点の初期値とし、実素材3種の頂点数測定で見直す:

| Preset | simplificationTolerance | cornerThreshold | minPathVertices | closedPath | 狙い |
|---|---|---|---|---|---|
| Tight | 0.8 | 5.0 | 8 | true | 輪郭忠実。頂点増を許容 |
| Default | 1.5 | 10.0 | 6 | true | 現行既定値そのまま |
| Loose | 3.0 | 25.0 | 6 | true | 平滑・頂点抑制 |

- `cornerThreshold` は `angle >= threshold` をコーナー (接線ゼロ) とする現行意味のまま。Tightほどコーナー多め、Looseほど平滑多めになる。
- プリセット選択UIは実行ダイアログまたはメニュー直下の3択に留め、新規ドック・新規signal/slotを作らない。
- 閾値可変 (`MaskConversionParams` への threshold 追加) は本MS対象外。要望があれば子リポジトリ変更として別途指示を受ける。

## Non-Goals

- Magnetic / edge-snap吸着編集 (別マイルストーン候補)
- 自動再トレース追従、トラッキング連動
- `toPolygon`細分割数やear clipping上限の根本対策 (必要なら別途)
- GPU化 (`MILESTONE_GPU_MASK_COMPUTE_PIPELINE`側の責務)

## Phases

### Phase 1: Trace to Mask (単発)

- 選択レイヤーの現フレームalpha (`CV_32FC1` 0..1、または `CV_8UC1`) を既存の取得経路 (レンダーキャッシュ/互換フォールバック境界) から受け取り、コールドパスで `fromAlphaMask` へ渡す。フレーム更新・スクロール・scrub・GPUコマンド構築のホットパスでは実行しない。新規の `QImage` 化・`QPainter` 合成・プレビュー再生成・大容量readbackを持ち込まない。
- 生成された `MaskPath` 列 (0件ならno-op) を、新規 `LayerMask` 1件に束ねて対象レイヤー末尾へ追加する。追加は `MaskEditCommand(layer, before, after)` で行う (`before`=既存mask列のコピー、`after`=before+新規1件)。`AddLayerMaskCommand` は使わない (serializeなし・label固定のため)。
- 保存・再読込は `MaskEditCommand::serialize` / `encodeMask` の既存経路に載せ、新規JSONキー・新規signal/slotを追加しない。
- 命名は `Trace <preset> <frame>` 形式に統一し、`MaskMode` は変換器の付与 (外側Add/穴Subtract) をそのまま維持する。feather/opacity/expansionは既定値のまま変えない。

**Done when:** 静止画レイヤーでキー結果→Bezierが1操作で生成され、Undo/Redo・保存再読込できる。空alphaでは情報表示のみでクラッシュ・誤Undo積みがない。

### Phase 2: 再トレース差分更新 (任意)

- 対象は Phase1 で生成した `LayerMask` スロット1件に限定する (任意の手編集マスクへの上書きはしない)。同一スロットに対する再実行で頂点列を置換する。
- 置換は Phase1 と同じ `MaskEditCommand(layer, before, after)` を使い、置換前に旧頂点列を含む `before` を退避する。新規コマンド種別・新規signal/slotは作らない。
- 頂点数不一致時の補間は「step置換の現行補間仕様に従う」との前提を要検証とする (`sampleAtFrame` は `MaskPath.cppm:387` に実在するが、頂点数不一致時のstep扱いの実コード確認は未了)。Phase2着手前に `LayerMask` の頂点数不一致時挙動を確認し、本書に追記する。
- プリセット変更の再実行でマスクが更新され、Undoで旧頂点列に戻れる。

**Done when:** しきい値変更の再実行でマスクが更新され、Undoで戻れる。

### Phase 3候補 (本MS対象外): Magnetic snap

- 近傍エッジ吸着、Shiftで吸着解除
- 高頂点時の間引き・上限ガード見直しとセットで別途起案

## Risks

- `simplificationTolerance=1.5 / cornerThreshold=10.0`既定値依存が強い。実素材での頂点数分布の確認が必要。上記Tight/Loose初期値は未測定の仮置き。
- 閾値固定 (0.5/127) のため、髪・ガラス等の半透明境界は分離/欠落しうる。閾値可変は別途起案し、本MSではプリセット名に含めない。
- 高頂点化でラスタライズ負荷増。`toPolygon(16)`と合成順序の見直しは本MSではやらない。
- QImageホットパス持ち込み禁止。変換・エッジ検出はコールドパス限定。
- `AddLayerMaskCommand` 流用案は serialize なし・label固定のため不採用とした。将来使う場合は serialize 追加が前提。
- 子リポジトリ (`Artifact`/`ArtifactCore`) のコード変更は本書の範囲外。実装時は別途指示を受ける。ビルド・テスト・CMake実行はユーザー許可後に行う。

## 受入条件 (契約)

- [ ] UI入口が Layerメニュー mask面の1点に限定され、Timeline/VPへの追加がない
- [ ] Tight/Default/Loose の3値と閾値固定の旨が定義される
- [ ] Phase1 が `MaskEditCommand` + `encodeMask` 経路に載り、新規signal/slot・新規JSONキーなし
- [ ] 空alpha時のno-op+情報表示が定義される
- [ ] Phase2 の頂点数不一致時挙動の要検証事項が残り、無断で仕様断定しない
- [ ] ホットパス・QImage・QPainter・QtCSS・QColorDialog・新規ショートカットの禁止制約を満たす

## Next verification

- 実素材3種 (人物髪/ガラス/テロップ) でのTrace頂点数と再ラスタライズ差分の目視確認
- ビルド・実行はユーザー許可後に実施
