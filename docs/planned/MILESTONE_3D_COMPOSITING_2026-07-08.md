# Milestone: 3D Compositing（コンポジション内ライブ 3D シーン） (2026-07-08)

**最終更新:** 2026-08-15
**ステータス:** Phase 1A〜1K の静的実装を確認済み。Phase 2/3 の本格統合と実機検証待ち。

### 実装状況（2026-08-15 確認）

`ArtifactCompositionRenderController` と GPU レンダリング経路を確認した結果、Model3D、単色／Shape／Image／SVG／Text／Video の 3D カード、標準 Stroke、Precomp GPU 出力を同一の 3D 合成経路へ接続し、不透明・透明の depth 契約、Material の PBR 入力、Light Layer、共有 depth attachment まで実装済み。残課題はカメラ／レイヤー運用を含む Phase 2 の本格的なシーン合成、Phase 3 の depth／mask／DOF 連携、および runtime での遮蔽・透明境界・音声を含む総合検証。

> 2026-07-14: Phase 1A として、連続する不透明 Model3D レイヤー間で
> composition preview の depth attachment を共有する最小経路に着手。
> Phase 1B として、Light Layer の Directional / Point / Spot / Ambient と
> light linking を Model3D の GPU pixel shader へ接続。
> Phase 1C として、metallic-roughness / normal / occlusion の各Material入力を
> linear texture として接続し、Scene Light 側をPBR応答へ拡張。
> Phase 1D として、Material opacity / opacity texture を透明境界に含め、
> 透明Model3Dを共有depthから分離してdepth testあり・depth writeなしで描画。
> Phase 1E として、不透明な単色2Dレイヤーをモデル行列付き3Dカードとして描画し、
> 連続するModel3D / 2Dカード間で同じdepth attachmentを共有。
> Phase 1F として、Imageレイヤーの既存GPUキャッシュSRVを3Dカードへ直接接続し、
> 不透明ピクセルだけdepth write、半透明ピクセルはdepth testのみの2パスに分離。
> Phase 1G として、SVG / Textが既に保持する`ImageF32x4_RGBA`も同じGPUカード経路へ接続。
> Phase 1H として、単色Fillの閉じたShape輪郭をear clippingで三角形化し、
> QImage再ラスタライズなしで専用3D depth PSOへ直接接続。
> Phase 1I として、RAM cacheに存在するVideoフレームを同期decodeなしでGPUカードへ接続し、
> Imageと同じalpha-split depth契約でModel3D / 2Dカードとの遮蔽へ参加。
> Phase 1J として、標準スタイルのShape Strokeをローカル空間のquad列として直接三角形化し、
> Fillと同じ3D depth契約へ接続。taper / gradient / dash / 非標準joinは既存経路を維持。
> Phase 1K として、既存のoffscreen color targetから同一GPU textureのSRVを明示取得できる
> APIを追加。Precomp GPU output registryがCPU readbackなしで親3Dカードへ渡すための所有権境界を固定。

---

## 1. 概要

コンポジションビューポート上で、インポート済み 3D モデル・カメラ・ライトを「レイヤーとして」配置し、3D 空間で合成（ライブ 3D コンポジット）する機能。Nuke の 3D コンポジット相倖。

## 2. なぜ必要か（AE ライクなモーショングラフィックスとして）

- `MILESTONE_3D_MODEL_IMPORT_AND_CONTENTS_VIEWER_2026-03-29.md` でモデル import は存在するが、それを comp 内でカメラ/レイヤーと 3D 的に合成する経路がない。
- ロゴ 3D・プロダクト 3D・空間演出など、モーショングラフィックスで 3D と 2D の混在合成は必須。
- AE は「ネイティブ 3D レイヤー + カメラ」でこれをカバーしている。

## 3. 参照元ツール

- **Nuke** — 3D ビュー / ScanlineRender / カメラ・ジオメトリ・ライトのコンポジット。
- **After Effects** — 3D レイヤー・カメラ・ライト・レイヤー距離順ソート。

## 4. 現状（ソース確認・2026-08-15）

- `ArtifactCompositionRenderController` に専用の 3D composition 経路があり、Model3D／3Dカード／共有depth／Light／Material／Precomp GPU output を接続済み。旧来の専用名称検索だけでは現状を判定できない。
- 関連基盤:
  - `MILESTONE_3D_MODEL_IMPORT_AND_CONTENTS_VIEWER_2026-03-29.md`（import 済み）。
  - `MILESTONE_3D_VIEWPORT_ORBIT_PAN_PREVIEW_MODE_2026-06-07.md`（viewport 操作）。
  - `MILESTONE_LIGHT_LINKING_2026-03-31.md` / `MILESTONE_ENVIRONMENT_MAP_2026-03-28.md`（ライト/IBL 基盤）。
  - Diligent レンダラ（`libs/DiligentEngine`）による 3D 描画パス。

## 5. スコープ（提案 Phase）

- **Phase 1 — 3D レイヤーとしての配置**
  - import モデルを comp レイヤーとして登録し、transform を既存 2D レイヤーと統合。
  - カメラレイヤー追加と viewport からの射影。
- **Phase 2 — 3D 合成パス**
  - Z 深度に基づくレイヤーソート、ライト/IBL の適用、Diligent での 3D→comp 合成。
- **Phase 3 — 深度・マスク連携**
  - 深度チャンネル出力、3D マット、Depth of Field 等の後処理接続。

## 6. リスク / 未確認事項

- 既存 2D レイヤーブレンドパイプライン（`LayerBlendPipeline`）との責務分離が必要。
- パフォーマンス（3D 再評価コスト）の設計。

## 7. 関連文書

- `docs/planned/MILESTONE_3D_MODEL_IMPORT_AND_CONTENTS_VIEWER_2026-03-29.md`
- `docs/planned/MILESTONE_3D_VIEWPORT_ORBIT_PAN_PREVIEW_MODE_2026-06-07.md`
- `docs/planned/MILESTONE_LIGHT_LINKING_2026-03-31.md`
- `docs/planned/MILESTONE_ENVIRONMENT_MAP_2026-03-28.md`

---

## 8. 実行順

このマイルストーンは、次の順で進めると責務が崩れにくい。

1. 既存の 3D model / camera / light レイヤーが comp 側でどこまで見えるかを確認する
2. 3D レイヤーを comp 内に置くデータモデルを先に固定する
3. camera layer と viewport 射影を分離し、preview-only の視点変更を壊さないようにする
4. 環境マップと light linking を、3D 合成の入力条件として後から接続する
5. 深度・マスク連携は最後に回す

### Phase 1 の最初の着手点

- `import model` を comp layer として登録する入口を確認する
- 3D layer の transform を 2D layer と同じ編集経路に寄せる
- 3D camera の存在を viewport HUD で読めるようにする

### Phase 1 の完了条件

- import 済み 3D モデルを comp の一部として配置できる
- camera / viewport の責務が混ざらない
- 3D scene の状態が review しやすい

## 9. Precomp GPU Output Registry（Phase 1K の契約）

`ArtifactCompositionLayer` を 3D card として合成する前に、child composition
ごとの GPU 出力を controller 所有の registry に保持する。`QImage` thumbnail は
この経路の入力にしない。

1 entry は少なくとも次を同時に持つ。

- `sourceCompositionId` / `childFrame` / 出力サイズ / render generation
- color RTV と、それが所有する同一 texture の SRV
- child の depth target（親の scene depth へ転用しない）
- child の描画完了後だけ有効になる `ready` 状態

更新手順は、親フレームに対して child frame を解決し、Master Property override
scope を開始して child の GPU pass を実行し、scope を必ず復元してから entry を
`ready` とする。親は `ready` の SRV のみを `draw3DTexturedCard()` へ渡す。
レンダー中の RTV を SRV として同時に bind しない。

最初の実装範囲は Normal blend・mask/effect なし・循環参照なしの precomp に限定する。
未対応の precomp は既存 2D 経路を維持し、GPU output registry の不完全な entry を
表示に利用しない。

2026-07-14 時点では、この限定条件を満たす **2D child comp** を専用 color/depth
target へ描画し、`ready` 後に親の 3D textured card として合成する最小経路まで実装。
child に 3D、adjustment、mask、rasterizer effect、非Normal blend、nested precomp
がある場合は registry を使わず既存経路へ戻す。

### Static audit follow-up (2026-07-25)

- `ArtifactCompositionRenderController` resolves the active camera, scene lights, light-link filtering, shared preview depth, 3D cards/shapes/textures, Model3D, and the precomp GPU-output registry in one explicit composition path.
- The render queue also exposes Depth/Normal/Velocity/ID channel handling and recognizes camera/light/model/procedural 3D layer types.
- Phase 1A-1K is therefore supported by current source evidence. Phase 2/3 requirements—full camera/layer scene operation, depth/mask/DOF post-processing, and end-to-end occlusion/transparency/audio runtime checks—remain incomplete or unverified.
- No build, test, or runtime execution was performed because the repository instructions require explicit approval first.

## Update 2026-08-15

現行コードを追加確認した。CompositionRenderController は active camera／scene lights／light-link filtering、共有 preview depth、3D cards、Model3D、Precomp GPU output registry を同一の3D合成経路で扱う。単色／Shape／Image／SVG／Text／Video のカード、Material のPBR入力、透明境界、Light Layer、depth attachmentの接続も確認できる。

一方、カメラ／レイヤー運用の全面的なscene管理、depth／mask／DOF後処理、複雑なprecomp条件、遮蔽・透明境界の実機parityは未完了または未検証。Phase 1A〜1Kの静的実装は確認済み、Phase 2〜3とruntime受入は pending とする。

## Update 2026-08-15

- `Artifact3DLayer::fromJsonProperties()` で、既存レイヤーへ source-less／`FixedGeometry3D::Auto` のJSONを復元した際に旧meshが残るstale restoreを修正した。
- 復元対象にモデルsourceがない場合はCubeへ戻し、静止画レイヤーと同じくsourceなし状態を明示的に確定する。
- missing model source の復元時も旧meshを残さず、source pathを保持した未ロード状態へ遷移するようにした。
- 3D transform snapshot の時刻基準を固定30fpsからレイヤーの `compositionFrameRate()` へ変更し、非30fpsコンポジションでのフレームずれを防いだ。
- Camera／Lightレイヤーもfpsを整数丸めせずcompositionの実数fps（例: 29.97）で `RationalTime` を生成するよう揃えた。
- 3D選択枠、固定平面投影、ピッキングrayのtransform snapshotにも同じfps基準を適用し、描画と編集操作の時刻ずれを除去した。
- ビルド・3D runtime検証・遮蔽parity確認は未実施。
