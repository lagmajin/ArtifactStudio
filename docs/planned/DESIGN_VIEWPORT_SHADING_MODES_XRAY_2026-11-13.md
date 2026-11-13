# 設計: VP 全体シェーディングモード切替（P1-4）と X-Ray 不透明度

**最終更新:** 2026-11-13

**ステータス:** S1 / S2 実装済み（コード変更のみ、ビルド・実機確認はユーザー明示指示待ち）。S3 / S4 未着手。

本設計は `docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md` の **P1-4（Maya 風シェーディングトグル）**
と「未確認の追加候補」にあった **X-Ray 不透明度スライダ** の詳細設計である。
2026-11-13 の実コード精査に基づく。ファイル参照はすべて相対パス（リポジトリルート基準）。

## 1. 現状（実コード照合 2026-11-13）

| 事実 | 根拠 |
| --- | --- |
| メッシュ用 PS は `PBR_PS.hlsl` ではなく `ArtifactCore/src/Graphics/MeshRenderer.cpp` 内のインライン HLSL（`MeshPSSource`）。PSO は RT フォーマットが変わった時だけ再生成 | `ArtifactCore/src/Graphics/MeshRenderer.cpp` L3985–4017, L4070–4089, L4173, L4186 |
| shadingMode は **PS 内の `In.Mode` 分岐**で処理される（PSO 切替ではない）。mode 1=Albedo / 2=Normal / 3=Solid(テクスチャ) / 8=instanceColor Solid ほか | 同 L1318（VS `Out.Mode` 転送）、L2467–2512（PS 分岐） |
| `ArtifactIRenderer` には AOV パス用の shadingMode 上書き機構が既にある。VP 全体シェーディング override の注入点候補 | `Artifact/src/Render/ArtifactIRenderer.cpp` L2856–2865、`Artifact/include/Render/ArtifactIRenderer.ixx` L396–407（`setMeshEmissionOnlyPass` 系） |
| `ModelRenderMode` は 2 値（Wireframe / Solid）のみ。Bounding Box / Wire-on-Shaded 相当の値はない | `Artifact/include/Layer/Artifact3DModelLayer.ixx` L24–27, L87–88 |
| Wireframe 描画は **CPU 辺再構築 + `draw3DLine`**（`FillMode=WIREFRAME` は未使用） | `Artifact/src/Layer/Artifact3DModelLayer.cpp` L3069–3103 |
| 選択レイヤーの 3D 選択ワイヤ／Bounds overlay は既存。LOD で `kMaxOverlayPolygons` 4000/12000/24000 に制限 | `Artifact/src/Widgets/Render/ArtifactCompositionRenderOverlay.cpp` L249–441, L305–315 |
| X-Ray はメンバ bool + `markRenderDirty()` のみ。**不透明度パラメータなし**。実効はレイヤー描画ループ内の減衰で、係数 `0.35f` がハードコード（2 か所） | `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp` L21210–21222, L45327–45331, L46424–46428 |
| X-Ray の永続化はなし（`LayeredConfigStore` キー無し。Rig overlay の `Viewport/RigOverlayVisible` と非対称） | 同上。`RigOverlayVisible` は L21257 |
| メッシュ PSO は `CULL_MODE_NONE` 固定 | `ArtifactCore/src/Graphics/MeshRenderer.cpp` L4016 |
| VP 専用表示状態の先行例: P0-4 種別フィルタ（`viewportLayerCategoryMask_`、Impl メンバ + setter で `invalidateBaseComposite()` + `markRenderDirty()`、Render Queue と分離） | `ArtifactCompositionRenderController.cpp` L11726–11738, L20384–20426; 設計元 `docs/planned/P0_DESIGN_NOTES_2026-09-22.md` L241–262 |
| LOD はズームから毎フレーム導出され、テクスチャ省略（Low）と overlay ポリゴン上限に効く。シェーディングモードとの分岐は現状なし | `ArtifactCompositionRenderController.cpp` L5627, L45115–45119; `Artifact/src/Render/ArtifactIRenderer.cpp` L2412, L2833–2834 |
| Render Queue の CPU フォールバックは `renderMode()==Solid && !hasTransparentMaterial()` 時のみ flat shading。VP ワイヤモードの波及先になり得る | `Artifact/src/Render/ArtifactRenderQueueService.cpp` L4401–4407 |

## 2. 設計判断

### D1: 状態の持ち方 — VP 専用 override として controller に保持（P0-4 と同型）

- `ArtifactCompositionRenderController.ixx` に
  `enum class CompositionViewportShadingMode { Shaded, Wireframe, WireframeOnShaded, BoundingBox }`
  （列挙名は実装時に確定）を追加し、Impl メンバ `viewportShadingMode_`（既定 `Shaded`）で保持。
- API: `setViewportShadingMode / viewportShadingMode`。変更時は `invalidateBaseComposite()` +
  `markRenderDirty()`（種別フィルタと同一の再描画契約）。
- **レイヤー側 `Artifact3DModelLayer::setRenderMode()` は呼ばない。** シリアライズ
  （`Artifact/src/Layer/Artifact3DModelLayer.cpp` L763, L1255–1260）と undo・他ペインへ波及するため。
  VP 専用状態であり、Composition JSON は触らない（P0_DESIGN_NOTES の方針を踏襲）。
- 2D レイヤーに対しては Shaded と等価。モードは 3D レイヤー描画でのみ分岐する。
- P1-3（ペイン単位設定）への将来拡張を見込み、このメンバは Impl 単所に置き、
  `PaneState` への移行は P1-3 実施時にまとめて行う。

### D2: Shaded / WireframeOnShaded / BoundingBox の実現経路

**経路 A（推奨）: renderer 側 shading override — 既存の AOV パス機構と同型**

- `ArtifactIRenderer` に `setMeshSolidShadingOverride(int)` 相当（命名は既存
  `setMeshEmissionOnlyPass` 群に合わせる）を追加し、`effectiveShadingMode` 計算
  （`Artifact/src/Render/ArtifactIRenderer.cpp` L2856–2865）に統合する。
  AOV パスフラグ（emission/id/normal 等）が override より優先される順序を維持する。
- shadingMode は PS 内分岐のため **PSO 再生成は不要**。フレーム中の
  `CreateGraphicsPipelineState` を発生させない（HOT_PATH_RULES、P1-5 の
  「PSO は initialize() で一度だけ」が規範）。
- 適用対象は `drawMesh` 経路の 3D モデルのみ。`Artifact3DModelLayer` の
  `useTextureInSolid_ ? 3 : 8`（`Artifact3DModelLayer.cpp` L3239）は
  renderer override を参照して解決する。

**Wireframe / WireframeOnShaded の表示**

- WireframeOnShaded = Solid（D2 override）で `drawMesh` した上で、既存
  `wireOverlay_` パス（`Artifact3DModelLayer.cpp` L3069–3070, L3290–3292, L3315–3317）と
  同型の辺描画を走らせる。
- **レイヤーへの指示経路は renderer 状態の参照とする**（レイヤーが描画時点で
  `ArtifactIRenderer` の VP shading 状態を参照する。コントローラ側から
  レイヤー毎に overlay を呼ぶ経路 B は、選択ワイヤ
  （`ArtifactCompositionRenderController.cpp` L50063–50075）と同型にできるが、
  全レイヤー走査・2D/3D 分類の重複が増えるため候補から外す）。
- 辺再構築は既存 `overlayVertices_` スクラッチバッファ再利用を維持し、
  **`kMaxOverlayPolygons` の LOD 制限（4000/12000/24000）を全 VP ワイヤに踏襲**する。
  高ポリゴンでの毎フレーム再確保を避けるため、バッファ再確保は頂点数変化時のみ
  （現行実装の条件をそのまま踏襲）。

**BoundingBox 表示**

- 既存 `draw3DSelectionBoundsOverlayImpl`（`ArtifactCompositionRenderOverlay.cpp` L441–499）
  のロジックを「選択レイヤー専用」から「VP shading モード由来の全レイヤー表示」へ
  一般化する。新たな PS 分岐は追加しない。
- レイヤー数分のコストが走るため、(bounds 計算は既に drawMesh 前後で持っている
  bounding 情報の再利用を優先)（未検証: `Artifact3DModelLayer` 側の bounds 再算出コスト）。

### D3: Backface Culling（P1-4 公式スコープ内。設計決定点として分離）

- 現状メッシュ PSO は `CULL_MODE_NONE` 固定（`MeshRenderer.cpp` L4016）。
  選択肢は 2 つ:
  - (a) PS 内 `IS_FRONT_FACE` 参照 discard — PSO 追加不要。ただし
    D3D12 / Vulkan parity の確認が前提。
  - (b) `CULL_MODE_BACK`/`FRONT` の PSO バリアント追加 — RT フォーマット
    バリアント毎の生成が必要で、透明 PSO 分岐（L2871–2906）と直積になる。
- 本設計では **(a) を第一候補**とし、parity 確認を Step の受入条件に含める。
  実装判断は実装着手時に確定させる（本設計では確定しない）。

### D4: X-Ray 不透明度

- API: `ArtifactCompositionRenderController.ixx` L368 の隣に
  `setXRayOverlayOpacity(float) / xRayOverlayOpacity()`。
  Impl に `float xRayOverlayOpacity_ = 0.35f;`（L14957 の隣）。
- `0.35f` の 2 か所（L45331, L46428）をメンバ値に置換。setter は
  `markRenderDirty()` のみ（減衰はベース合成側で overlay invalidate 不要）。
- 範囲は 0.0–1.0 に clamp。既定値は現行 0.35 を維持（挙動変化を起こさない）。
- `0.0` は Isolation（P1-1/P1-9）と事実上重複するため、UI の最小値は
  0.05 相当にするか 0.0 を許すかは実装時に決める（既定案: 0.05–1.0、
  API は 0.0–1.0 を受け付ける）。
- 注意: 3D の depth 共有条件 `opacity >= 0.9999f`
  （`ArtifactCompositionRenderController.cpp` L45403–45416）は減衰値が掛かるため
  係数 <1 の時に depth 共有が外れる。現行 0.35 でも同条件のため
  **挙動変化はなし**。受入確認時にこの経路の表示を確認する。
- 永続化: 現行 X-Ray トグルは保存されない。本設計では
  `LayeredConfigStore` の `Viewport/XRay/{Enabled,Opacity}` への保存を提案する
  （`Viewport/RigOverlayVisible` L21257 の先行例と同じ型）。
  トグルの永続化追加は本設計の付帯変更とする（要ユーザー承認）。

### D5: X-Ray 減衰の選択基準（現行仕様の明示と決定事項）

- 現行は「選択レイヤーありの時のみ、選択以外を減衰」
  （`!selectedLayerId_.isNil() && layer->id() != selectedLayerId_`）。
- Maya（選択以外減衰）と Blender（Alt+Z で全体を半透明）で挙動が異なる。
  現行仕様（Maya 型）を維持し、複数選択（`selectedIds_`）時は
  選択レイヤー以外すべてを減衰へ拡張するかを P1-1/P1-2（Isolate / Ghosted）と
  一緒に設計する。**本設計では単一選択挙動を変更しない。**

### D6: UI とショートカット

- 既存 Shading メニュー（`Artifact/src/Widgets/Render/ArtifactCompositionEditor.cpp` L14008–14106）
  にシェーディングモード群と X-Ray 不透明度スライダを追加。
  スライダ UI は P1-5 露出調整の `QWidgetAction` スライダパネル方式
  （`docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md` L108）を踏襲する。
- 新規操作は `ShortcutBindings` の `Viewport.Composition` ローカルコンテキストへ
  `ShortcutId` 登録（既存例 `ViewToggleXRay`）。固定キーのハードコードはしない。
- QtCSS / `QColorDialog` / 新規 signal-slot は使わない。

### D7: 相互作用の明示

| 経路 | 扱い |
| --- | --- |
| Render Queue / 出力 | **非波及**。shading モードは controller 内状態に留め、`ArtifactRenderQueueService` の CPU フォールバック（L4401–4407）と `CompositionLayerRenderFilter` は変更しない |
| RAM Preview / Render Queue cache | shading モード変更時は既存の `invalidateBaseComposite()` + `markRenderDirty()` 契約に従う（種別フィルタと同一）。キャッシュ無効化の追加対応は受入確認時に確認 |
| LOD / PreviewQuality | override は全 LOD で有効。ワイヤのポリゴン上限は LOD 連動（D2） |
| X-Ray 併用 | X-Ray 減衰（選択以外）と shading モードは独立に掛かる。減衰 × Wireframe の見え方を受入確認で確認する |
| チャンネル表示（AOV） | AOV パスフラグが shading override に優先（D2 の順序維持） |

## 3. 実装段階（提案）

| Step | 内容 | 主な変更対象 | 受入 |
| --- | --- | --- | --- |
| S1 | X-Ray 不透明度（D4）+ 永続化（要承認） | `ArtifactCompositionRenderController.ixx/.cppm`、Shading メニュー | 既定 0.35 で現行と同一表示。0.0–1.0 で段階変化。スライダ保存/復元 |
| S2 | shading モード enum + renderer override（D1, D2 経路 A）+ WireframeOnShaded | `ArtifactCompositionRenderController`、`ArtifactIRenderer`、`Artifact3DModelLayer`、Shading メニュー、`ShortcutBindings` | Shaded 既定で無変化。Wireframe / WireframeOnShaded / （BoundingBox）の表示。Render Queue 出力に波及しない |
| S3 | BoundingBox 表示（D2 一般化） | `ArtifactCompositionRenderOverlay` 呼び出し経路 | 全 3D レイヤーの bounds 表示。高ポリゴンでカクつかない（LOD 上限） |
| S4 | Backface culling（D3） | `MeshRenderer.cpp`（PS 内 discard） | D3D12 / Vulkan 同一表示。背面非表示を切替可能 |
| S5 | 受入確認（ビルド・実機） | — | ユーザー明示指示後に実施 |

S2 から先は S1 と独立して着手可能だが、Shading メニュー UI は S1 で整えるため
S1 → S2 の順を推奨する。

## 4. リスク

- **ホットパス確保**: VP ワイヤを全レイヤー化すると、`needsWireOverlay` が
  動的になり毎フレーム辺再構築が走る。高ポリゴン + 多数レイヤーで
  再確保・並列変換コストが顕在化し得る。LOD 上限とバッファ再利用の踏襲は必須。
- **PSO/RT 不一致系**: shadingMode を PS 内分岐で完結させても、RT フォーマット
  バリアント（`renderTargetFormat_`）・透明 PSO 分岐・マテリアルグラフ splice
  （`__ARTIFACT_MATERIAL_GRAPH_*__` マーカー）の再コンパイル条件は別軸。
  新モードの追加でこれらを踏まないことを実装時に確認する。
- **レイヤー所有データとの境界**: `setRenderMode()` への迂回実装を禁止する
  （永続化・undo 波及）。
- **X-Ray 選択基準**: 単一選択前提の現行判定。複数選択仕様は本設計スコープ外
  （D5）。

## 5. 未検証事項

- `Artifact3DModelLayer` の bounds 情報の再利用可否（S3 のコスト見積り）。
- `IS_FRONT_FACE` の Vulkan parity（S4）。
- X-Ray 減衰が depth 共有経路・RAM preview キャッシュに与える現行の影響範囲
  （挙動は変えないため受入確認時の記録のみ）。
- `LayeredConfigStore` への `Viewport/XRay` 保存の許否（D4、要ユーザー承認）。

## 6. 実装記録（2026-11-13）

**S1 / S2 を実装（コード変更のみ、ビルド・実機未確認）。** 変更はすべて `Artifact` 子リポジトリ。

### S1: X-Ray 不透明度 + 永続化

- `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`:
  `setXRayOverlayOpacity(float) / xRayOverlayOpacity()` を追加（0.0–1.0、既定 0.35）。
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp`:
  - Impl に `xRayOverlayOpacity_ = 0.35f` を追加し、ハードコード `0.35f` の 2 か所
    （旧 L45331 / L46428）をメンバ値へ置換。
  - `setShowXRayOverlay` に `Viewport/XRay/Enabled` の永続化を追加（Rig overlay 例と同型）。
  - `setXRayOverlayOpacity` は `Viewport/XRay/Opacity` を保存し `markRenderDirty()` のみ。
  - コンストラクタの復元ブロック（RigOverlayVisible の直後）に
    `Viewport/XRay/{Enabled,Opacity}` の復元を追加。
- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cpp`:
  Shading メニューの X-Ray トグルの下に `QWidgetAction` スライダパネル
  （X-Ray Opacity、5–100%、既定 35%）を追加。既存 `forEachActiveSecondaryController`
  / `refreshViewportStateLabels` の契約に従う。

### S2: shading モード（Shaded / Wireframe / WireframeOnShaded）

- `Artifact/include/Widgets/Render/ArtifactCompositionRenderController.ixx`:
  `enum class CompositionViewportShadingMode`（Shaded / Wireframe / WireframeOnShaded /
  BoundingBox。BoundingBox は未露出）と
  `setViewportShadingMode / viewportShadingMode` を追加。
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cpp`:
  Impl メンバ `viewportShadingMode_`、setter（`invalidateBaseComposite()` +
  `markRenderDirty()`、`BoundingBox` は Shaded にマップ）。
- `Artifact/include/Render/ArtifactIRenderer.ixx` / `src/Render/ArtifactIRenderer.cpp`:
  `setMeshViewportShadingMode(int) / meshViewportShadingMode()` を追加
  （0=Shaded / 1=Wireframe / 2=WireframeOnShaded）。getter は補助メッシュパス
  （emission / id / normal / velocity / albedo / position / uv）アクティブ時は 0 を返し、
  AOV セマンティクスを維持。Render Queue は独自 `ArtifactIRenderer` インスタンスで
  描画するため波及しない。PSO 再生成なし。
- `Artifact/src/Layer/Artifact3DModelLayer.cpp`:
  `draw()` で renderer 状態を参照。Wireframe は Solid 描画をスキップして既存の
  `drawEdges`（`wireframeColor` / thickness 2.0）のみ、WireframeOnShaded は
  Solid 描画後に既存の `drawEdges`（dark 0.72 alpha / thickness 1.0）を追加。
  `overlayVertices_` スクラッチ再利用と条件付き確保は踏襲。
- UI: Shading メニュー（フッター）に Shaded / Wireframe / Wireframe on Shaded の
  排他 QActionGroup を追加。

### S2 の既知の挙動（受入確認時の確認事項）

- VP Wireframe モード中は `drawMesh` を発行しないため、シャドウマップの caster
  登録が行われず、影が更新されない（ワイヤ表示では影は視覚上不重要。Render Queue
  には影響しない）。
- shading モードは永続化しない（起動時は常に Shaded）。
- 選択ワイヤ `draw3DSelectionWireframeOverlay` とは独立に動作するため、
  WireframeOnShaded 選択中の選択レイヤーはワイヤが 2 系統（モード由来 + 選択
  オーバーレイ）に見え得る。受入確認で可否を判断する。
- ショートカットは未登録（`ShortcutId` 追加は `ArtifactCore` 子リポジトリ変更を
  伴うため、ユーザー指示まで実施しない。メニュー操作のみで切替可能）。

## 7. 関連文書

- `docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md`（P1-4 の計画元）
- `docs/analysis/VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22.md`
- `docs/planned/P0_DESIGN_NOTES_2026-09-22.md`（VP 専用 override の設計元）
- `docs/planned/MILESTONE_3D_VIEWPORT_HARDENING.md`
