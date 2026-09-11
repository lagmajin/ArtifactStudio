**最終更新:** 2026-09-11

# 3Dレイヤー AE/Element3D/C4D 劣位点と改善メモ

前回整理した劣位点のメモ化。一次情報は `docs/analysis/THREED_LAYER_FEATURE_GAP_DCC_COMPARISON_2026-08-08.md`、`docs/analysis/REPORT_DCC_GAP_3D_TEXT_2026-08-18.md`、`docs/analysis/AE_PLUGIN_COMPARISON_MEMO_2026-08-20.md`、`docs/analysis/ADVANCED_RENDERING_GAP_2026-08-13.md`、`docs/analysis/AE_PAIN_POINT_IMPROVEMENT_MAP_2026-08-13.md`。

## 1. 劣るところ

### vs AE標準3D
- Collapse Transformations / Continuously Rasterize相当なし。子コンポをQImage化して親へ渡すためネスト3D境界が切れる。
- カメラはPOI/2ノード・Ortho・DOF/MB実配線・プリセットまで実装済み(2026-09-11再確認)。残りはクリックフォーカス、POI中心オービットの実機確認、カメラ別シャッター(低)。
- 3軸回転はモデル/行列/ギズモ/保存は3軸化済み、プロパティUI契約が未整理 → [x] 整理済み。
- フォーカス面ギズモは実装済み(`ArtifactCameraLayer.cppm:179-182`)。

### vs Element3D
- パーティクル散布/グループ複製なし。`InstanceData/draw(instanceCount)`基盤は完了。
- OBJシーケンス相当のアニメーション再生なし。glTF/FBX skinningはCPU LBS/Rigid/DQ部分実装。
- 環境反射/IBLは実装済みもGPU prefilter/runtime未確認。シャドウはDirectional/Spotハードのみ。
- マテリアルはプリセット止まり。テクスチャトランスフォーム、反射プローブ、頂点カラー描画反映なし。

### vs C4D
- デフォーマ0種(Bend/Twist/Taper/FFD等なし)。
- MoGraph Clonerは別系統で存在、3D拡張/Field/Effector未接続。
- ジオメトリノード/モディファイアスタック/ノードマテリアルなし。
- VDB/SDF、テッセレーション/サブディビジョン、法線ベイク、UV編集、頂点/辺/面選択、LOD自動生成なし。
- DDGI/VXGI/RT反射はシェーダ資産のみで未配線。稼働GIはSSGIのみ。RenderGraphは診断専用。
- レイトレBLAS/TLASはカウンタのみで実ジオメトリ未対応。

## 2. 改善できそうな順(小→大)

1. テクスチャトランスフォーム(offset/scale/rotation per texture) — 影響局所。
2. 頂点カラー描画反映 — 読込あり、シェーダ/PBRバインド追加のみ。
3. ソフトシャドウ仕上げ — 3x3 PCF/softness実装済み、パラメータ露出と品質確認。
4. 3軸回転プロパティUI整理 — Core済み、UI契約のみ。
5. SSAO/反射プローブ — 1パス追加級。
6. IBL runtime検証/環境共有キャッシュ/skybox — 実装済み部分の受入れ。
7. Cloner 3D拡張(グリッド/ランダム/パス沿い+instanced draw)。
8. glTFアニメ sampling/bake。
9. デフォーマ(Bend/Twist/Taper) modifier stack。
10. RenderGraph実行化、DDGI/VXGI配線、BLAS/TLAS実体化、Point/Area影/CSM。

## 3. 次の作業
- [x] 頂点カラー描画反映 — `color`属性→RenderData→GPU頂点バッファ→PBR乗算を接続。
- [x] テクスチャトランスフォーム — Material共有UV offset/scale/rotation→定数→PS変換を接続。
- [x] ソフトシャドウ仕上げ — UI 0〜500とsoftness 0〜2の飽和を解消し0〜20へ整合。
- [x] 3軸回転プロパティUI整理 — 3D層のTransformへZ軸/回転X・Y/回転Z別名を追加。
- [x] PBR係数取込 — ufbx metallic/roughness factorを既定マテリアルへ適用。
- [x] Cloner 3D最小接続 — 3Dソース解決+drawMeshInstanced+world instance化。
- [x] アニメ再生モード/速度 — Loop/Hold/PingPong+speedを評価・保存・UIへ接続。
- [x] マテリアル環境強度 — IBL間接光のマテリアル別スケールを定数・保存・UIへ接続。
- [x] クリックフォーカス — Alt+ダブルクリックで3Dヒット点へフォーカス距離を設定。
- [x] Clone生成上限 — 全モード4096capで巨大グリッドのハングを防止。
- 上記1〜4から着手し、1件ずつ既存経路に接続する。
- Diligent/DX12低レベル変更は最小化。新規signal/slot、QtCSS、QColorDialog、QImage本流追加はしない。
- ビルド・テスト・CMake再生成はユーザー許可後に実行する。
