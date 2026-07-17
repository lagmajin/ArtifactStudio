# Milestone: 3D Look Development Foundation

**ステータス:** In Progress

**日付:** 2026-07-17

## 目的

Artifact の 3D レイヤーを、solid / unlit 寄りの確認表示から、制作中に照明と材質を
判断できるリアルタイム PBR 表示へ引き上げる。最初の到達点は、HDR 環境光、物理ベースの
材質、安定した主光源シャドウが同一の linear HDR render target 上で破綻なく共存すること。

GI は本 milestone の前提ではあるが、実装範囲には含めない。GI が Off の状態でも、材質、
反射、陰影、背景の見え方を成立させる。

## 成果イメージ

- HDRI を背景と照明に使い、金属・粗い表面の違いを読める。
- directional light の影で接地感と形状を読める。
- PBR / solid / wireframe を安全に切り替えられる。
- HDR linear 合成後に既存の表示変換へ渡し、GI・ポスト処理の追加先を固定する。

## 原則

- DiligentFX の `GLTF_PBR_Renderer` と既存の Diligent API を再利用し、DiligentEngine fork は変更しない。
- レイトレース用 `Material` とリアルタイム用 `PBRMaterial` は別責務に保つ。
- HDRI はアセットとして所有し、CPU readback、`QImage`、Qt 合成を描画本流に入れない。
- 影、PBR、IBL は同じ linear HDR target に合成し、トーンマップ前に加算する。
- DX12 / Vulkan で同一の ArtifactCore 契約を使い、backend 固有分岐を増やさない。

## フェーズ

## 実装状況

- PBR preview の固定アンビエントを、linear light の半球環境 fallback に置換済み。
- `PBRMaterialEffect` に Ambient Occlusion / Emissive Color / Emissive Intensity を追加済み。
- `ArtifactEnvironmentMapLayer` は設定変更ごとに revision を進め、後続の GPU cubemap / IBL cache が
  asset revision 単位で無効化できる状態にした。
- HDRI の GPU cubemap 変換、IBL resource 生成、directional shadow pass は未着手。

### Phase 0 — Render Contract と診断

- PBR、shadow、environment の入力・出力・resource lifetime を render pipeline に明文化する。
- linear color target、depth target、normal、camera、light の形式と所有者を固定する。
- PBR、shadow map、IBL resource の有効状態と失敗理由を render diagnostics に表示する。
- 既存 solid / wireframe の出力をリファレンスとして残す。

**Done when:** 各パスが使用する target と fallback が診断から判別できる。

### Phase 1 — PBR Material Fast Path

- `PBRMaterialEffect` を実行時用 `PBRMaterial` に接続する。
- `GLTF_PBR_Renderer` を既存 3D layer / viewer の描画パスへ統合する。
- Base Color、Metallic、Roughness、Normal、AO、Emissive を最小セットとして GPU に転送する。
- texture がない場合も定数値の fallback で PBR 表示を維持する。
- solid / wireframe / PBR の切替で target、depth、camera contract を壊さない。

**Done when:** 標準メッシュが metallic-roughness PBR で表示され、既存モードへ安全に戻せる。

### Phase 2 — HDR Environment Map と IBL

- `.hdr` / `.exr` の equirectangular HDRI を GPU cube map へ変換する。
- skybox、環境回転、intensity、background visibility を scene / layer 設定として扱う。
- irradiance cube、prefiltered specular cube、BRDF LUT を生成し、同一 HDRI の再生成を避ける。
- PBR の diffuse / specular IBL に接続し、HDRI 未設定時は中立環境へ安全に fallback する。
- HDRI の asset revision 変更時だけ関連 GPU resource を再生成する。

**Done when:** HDRI が背景・拡散照明・粗さ依存の反射に一貫して現れ、カメラ操作中に再生成しない。

### Phase 3 — Directional Shadow Foundation

- directional light 用の shadow map pass と shadow receiver path を追加する。
- 最初は 1 directional light を対象にし、bias、normal bias、shadow strength、shadow distance を明示パラメータ化する。
- camera frustum と shadow frustum の可視化・診断を用意する。
- 近景の解像感を優先し、距離に応じた安定した filter を適用する。
- 影なし、shadow resource 作成失敗、light 無効時は PBR / IBL 表示を維持する。

**Done when:** 静止カメラとゆっくりしたカメラ移動で、明らかな shadow acne、peter-panning、ちらつきがない。

### Phase 4 — Cascades と Look-Dev Controls

- 必要性を計測で確認した後に CSM を導入し、まず 2–4 cascade に限定する。
- cascade split、filter radius、max distance を品質モードで管理する。
- Inspector / viewer に、render mode、HDRI、environment rotation/intensity、shadow enable/quality の最小操作導線を追加する。
- material review 用に PBR channel と shadow map の debug view を追加する。

**Done when:** 代表シーンで近距離・中距離の影を安定して確認でき、材質レビュー用の操作が一箇所で完結する。

## 実装順と依存

1. Phase 0 を先に完了し、既存出力との差分と resource failure を追えるようにする。
2. Phase 1 で材質の受け皿を作る。
3. Phase 2 で HDRI / IBL を接続し、PBR の基準画を作る。
4. Phase 3 を single directional shadow として小さく入れる。
5. Phase 4 の CSM は、single map の shadow distance が不足した場合だけ進める。

GI は Phase 1–3 の linear HDR、PBR material、depth / normal、scene light 契約を消費する。GI 側から
PBR または shadow pass の resource を所有・再生成しない。

## 非スコープ

- DDGI / SSGI / RT GI の実装変更
- point / spot shadow、area light shadow
- ray-traced shadow
- MaterialX graph、clearcoat、anisotropy、transmission
- HDR10 / PQ / HLG export
- DiligentEngine の変更

## 完了条件

- PBR、HDRI IBL、directional shadow を同時に有効化しても既存 3D layer の表示を壊さない。
- GI Off の標準プレビューで、材質・反射・接地影を判断できる。
- HDRI または shadow resource の作成失敗時も、クラッシュせず明示的な fallback と診断を提供する。
- 既存の linear HDR → display transform 契約を保つ。
- DX12 / Vulkan の双方で同一の ArtifactCore API と resource contract を使用する。

## 関連文書

- `docs/planned/MILESTONE_PBR_SHADER_ELEMENT3D_LIKE_2026-07-09.md` — PBR 実装詳細
- `docs/planned/MILESTONE_ENVIRONMENT_MAP_2026-03-28.md` — HDRI / IBL 実装詳細
- `docs/planned/MILESTONE_CACHED_HYBRID_GLOBAL_ILLUMINATION_2026-07-17.md` — 後続の GI
- `docs/planned/MILESTONE_RAY_TRACING_DX_VULKAN_2026-05-16.md` — RT 拡張の境界
- `docs/planned/RENDER_BOUNDARY_CHANGE_SAFETY_CHECKLIST_2026-04-21.md` — render path 変更時の確認事項
