# HieroPlayer 由来「Viewer Inspection Controls」導入前検討

**最終更新:** 2026-09-26
**対象:** `docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md` の P1-10 / P1-11 / P1-13 / P1-14
**出典:** `docs/analysis/HIEROPLAYER_GAP_ANALYSIS_2026-09-22.md` の P0 #1〜#6
**ステータス:** 導入前検討（コード変更なし・ビルド未実施）

## 目的

HieroPlayer 由来の未実装候補 4 件について、**実装を始める前に現コードへ照合し、
「本当に未実装か」「既存機能を二重実装してしまうか」「どの文書が古いか」を確定する。**

この作業は 2026-09-26 の P1-12 検証で同じ罠が 2 度続けて発生したことを受け、
残り 4 件を一括で再検証するものである。P1-12 では「マイルストーンは Not Started だが
実コードは既に実装済み」であり、再実装すると既存機能を二重作することになっていた。

## 結論サマリ

| 項目 | 文書表記 | 実コード（2026-09-26 照合） | 判定 |
| --- | --- | --- | --- |
| P1-10 Clipping 警告 | Not Started | 表示専用 compute 段の接続点は P1-5 露出で整ったが、警告判定そのものは無い | **未着手（導入可）** |
| P1-11 スコープ + ROI | Not Started | ヒストグラム / 波形 / スコープ / パラードは**全て実装済み**。GPU 版ソースも存在するが**ビルド除外**。ROI 集計のみ未実装 | **一部実装済・残は ROI と GPU 化の設計** |
| P1-13 OCIO 表示色空間 | Not Started | **OCIO は実依存として統合済み**。ベイク済み 3D LUT による表示変換も存在するが **composition-space cache 経路にのみ適用**。per-viewport 切替と `setOCIOConfig` は無い | **半分実装済・接続位置の設計が要る** |
| P1-14 アスペクトマスク | Not Started | safe-area 描画は既存（`ViewportOverlay::drawSafeAreaAndOrigin`）。数値指定によるマスクのみ無い | **未着手（導入可・前提あり）** |

**結論として、4 件すべてが「Not Started」のまま着手すると既存要素との重複または
接続位置の誤りが起きる。P1-11 と P1-13 は「未実装」ではなく「接続・範囲の設計」なので、
文書の書き換えが実装に先行する。**

## P1-10 Clipping 警告（over/under exposure の false color）

### 現状（確認できた事実）

- 警告を出すための表示専用 compute 段が **P1-5 露出（2026-09-26 実装）** で既に用意された。
  `finalizeGpuRenderToViewport` の `presentationSRV` 選択直前という、単一で明確な接続点である。
- over/under の閾値をどこで持つかが未定義（設定値か、露出段の cbuffer に同居させるか）。
- false color（赤/青の警告色）を描く既存の overlay 経路は未確認。

### 導入前の検討事項

1. **接続点を P1-5 と共有してよいか。** 同一 cbuffer に閾値と有効フラグを同居させるか、
   別 compute pass として独立させるか。同一 pass なら低コストだが、Clipping を無効にしても
   露出段の dispatch 自体を避ける工夫が要る。
2. **閾値の正解値。** HieroPlayer は 0.0 / 1.0（または量子化 8bit/10bit/12bit 基準）。
   Artifact は線形 float で合成しているため、量子化基準にするか線形 1.0 基準にするかは仕様レビューが必要。
3. **`tempUAV` の共有。** P1-5・P1-10・チャンネル表示が同一 scratch を使うため、
   同一フレームで「露出＋クリッピング」を両方適用する設計は現状未成立。
   露出段の cbuffer に統合する形なら自然に 1 pass に収まる。
4. **AGENTS.md 遵守。** 既存 `QColorDialog` は禁止。警告色は `FloatColor` の定数で持つ。

## P1-11 スコープ（Histogram / Waveform / Vector）+ ROI

### 現状（確認できた事実・実コード照合）

**表示側は既に実装済み。** 文書が「Not Started」と書いているため未着手に見えるが、
実コードは 4 スコープすべてを持つ。

- `ArtifactColorSciencePanel`（`Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm`）は
  既に **4 スコープ（Histogram / Waveform / Vectorscope / Parade）を 2×2 ダッシュボードで保持**し、
  180ms タイマーで `captureCurrentFrameImage()` から更新する。View > Utility Panels > Color Science で開く。
- `ArtifactCompositionEditor.cppm` にも独立したスコープ dialog があり、4 スコープをタブ表示する。
- `ArtifactContentsViewer` にも Parade タブがある。
- 共有 4 widget（`HistgramWidget` / `WaveformScopeWidget` / `VectorScopeWidget` /
  `ParadeScopeWidget`）は `ArtifactWidgets` 側に実装済みで、CMake で force-include 済み。

**GPU 版はソースだけ存在し、ビルド除外。**

- `ArtifactCore/include/Graphics/Compute/ScopeComputer.ixx` と
  `ArtifactCore/src/Graphics/Compute/ScopeComputer.cppm`（Vectorscope / Waveform / Parade compute）が
  完全実装だが、`ArtifactCoreSources.cmake` には **`.ixx` のみが登録**され、`.cppm` は
  ビルド対象から外れている（`ArtifactCore/cmake/ArtifactCoreSources.cmake:281`）。
- `ArtifactCore/include/Graphics/Compute/Histogram.ixx` も同様で `.cppm` は未登録。
  この `HistogramComputer::computeLuminanceRegion` は**リージョン限定 GPU ヒストグラム**で、
  ROI スコープの土台になり得る。
- `ColorScopeRenderer`（`ArtifactCore/include/Color/Grading/ColorScopes.ixx`）も
  6 メソッドが実体を持つが、**呼び出し元 0 件**（デッドコード）。

**未実装は ROI 集計と部分 readback のみ。**

- ROI でスコープ集計を範囲制限する仕組みは無い。readback は全フレーム（`captureCurrentFrameImage`）が基本。
- `readbackTextureViewToFloatBuffer`（`ArtifactIRenderer.cppm:2256` 付近）は事前確保リングを持つ
  float readback だが、サブ矩形指定は無い。
- なお既存 4 widget は CPU 実装で QPainter を使う。P1-11 の受入条件には
  「`QImage` / `QPainter` 合成の新規利用は禁止」とあるが、これは新規実装の禁止であって
  既存 widget の問題ではない。

### 導入前の検討事項

1. **「スコープを表示する」部分は既存。** 新しい表示面を増やさず、既存
   `ArtifactColorSciencePanel` に「ROI 集計」だけを足すのが最小変更。
2. **ROI 集計の実装方式。** (a) CPU: ROI の矩形だけ切り出して既存 widget に渡す。
   (b) GPU: ビルド除外の `ScopeComputer` / `Histogram` を有効化し、`computeLuminanceRegion` を使う。
   (b) は submodule（ArtifactCore）変更とビルド登録を伴うため、AGENTS.md により
   親側からの単独実現は不可。**fork 運用／パッチ運用の判断が必要。**
3. **既存 IRR（P0-3）との矩形共有。** `Impl::interactiveRenderRegionRect_`（`QRectF`、`:14857`）が
   既に矩形を保持しているが、これは「部分レンダー矩形」であり ROI の集計矩形としては
   定義されていない。共用するか、専用の ROI 矩形を別に持つかに決める必要がある。
4. **`ArtifactWidgets` は submodule。** 既存 4 widget の改修は親リポジトリからは行えない。
   ROI 追加が既存 widget の変更を要する場合は、同じ fork 判断が必要。

### 決定（2026-09-26、実コード照合で確定）

**消費側 widget の API は `void updateFrame(const QImage&)` のみ**
（`ArtifactWidgets/include/Color/HistgramWidget.ixx:32`、`WaveformScopeWidget.ixx`、
`VectorScopeWidget.ixx`、`ParadeScopeWidget.ixx:35`）。ROI 矩形を表す引数は無く、
矩形集計のロジックは widget 側の Impl 内にある。

このため **CPU 側 ROI は submodule を一切変更せず実現できる**。
`refreshScopesFromViewport`（`ArtifactColorSciencePanel.cppm:1235`）が既に
`captureCurrentFrameImage()` の戻り値（全フレーム QImage）をローカルに持っているため、
ROI 有効時だけ `frame.copy(x, y, w, h)` で切り出して 4 widget に渡す、
という 1 箇所の差し替えで済む。既存の `lastScopeFrameKey_` による早期 return はそのまま使え、
未 ROI 時は挙動が一切変わらない。

- **採用: CPU 側 ROI 集計（submodule 変更なし）** を第一段とする。
- **GPU 化（`computeLuminanceRegion`）は次段** とし、 submodule の fork／パッチ判断は
  CPU 側 ROI の実測（ROI 面積と応答時間の相関）をしてから行う。
- これにより P1-11 は submodule の門に阻まれず着手できる。

## P1-13 OCIO 表示色空間切替（Viewer color transform）

### 現状（確認できた事実・実コード照合）

**OCIO は実依存であり、統合済み。** 文書・メモの「未統合」は古い。

- `vcpkg.json` に `opencolorio`。`ArtifactCore/CMakeLists.txt:4349-4353` で
  `find_package(OpenColorIO CONFIG REQUIRED)`、`target_link_libraries(ArtifactCore PUBLIC ...)`。
- `ArtifactCore/src/Color/OCIOConfig.cppm` が `OCIO::Config::CreateFromFile` で実 `.ocio` を読み、
  `Artifact/src/Color/ArtifactOCIOManager.cppm` が `OCIO::ConstProcessorRcPtr` を apply する。
- `docs/memo/OCIO_MISSING_FEATURES_2026-08-01.md:10-20`（実 OCIO 未統合という記述）と
  `:59-70`（TransferFunction は 4 種のみ）は**いずれも古い**。TransferFunction は
  17 種（sRGB / Rec709 / Rec2020 / PQ / HLG / ACEScc / S-Log3 / CanonLog を含む）。

**表示変換（LUT bake）は存在するが、接続位置が限定されている。**

- `ViewportColorPipeline::ensureLUT`（`Artifact/src/Widgets/Render/ViewportColorPipeline.cppm:63-69`）が
  設定キーが変わったときだけ `bakeViewTransformLUT(33, lut, 0.0f, 4.0f)` で 33³ LUT を再ベイクし、
  `ArtifactFinalPostProcess` の 3D LUT として保持する。
- 実際の適用は `applyDisplayColorTransform`（`ArtifactCompositionRenderController.cppm:10346`）で、
  呼び出しは **composition-space GPU cache 経路の 2 か所のみ**（`:42141` / `:42155`）。
- **`finalizeGpuRenderToViewport`（メインの present 経路）には入っていない。**
  よって現在の OCIO 変換は「composition-space cache が有効なときだけ効く」状態。

**文書が指す `setLUT` / `setOCIOConfig` は未実装（これは文書の通り）。**

- `setOCIOConfig` はコード中に存在しない（docs のみ）。
- `setLUT` は Color Settings / Color Management の effect setter であり、
  renderer の表示変換 API ではない。
- View menu の `useDisplayColorManagementAction`（`ArtifactViewMenu.cppm:1541`）は
  **connect されておらず、押しても何も起きない**。

### 導入前の検討事項

1. **P1-13 の実態は「per-viewport 表示色空間切替」である。** OCIO 基盤と LUT bake は既にあり、
   足りないのは (a) present 経路への適用、(b) per-pane 状態、(c) UI 接続。
2. **(a) present 経路への適用が最大の関門。** `finalizeGpuRenderToViewport` は
   `tempUAV` を露出（P1-5）とチャンネル表示で 2 度使い回している。OCIO 変換を第 3 の
   scratch として差し込むなら ping-pong バッファが要る。composition-space cache 側に
   既に scratch（`postProcessUnorderedAccessView`）があるため、再利用できるか要検討。
3. **順序の契約。** OCIO 表示変換は sRGB エンコードの直前に置くのが正解だが、
   現状の `finalizeGpuRenderToViewport` には明示的な 8bit/sRGB encode が無い
   （swapchain 側の暗黙変換）。P1-5 露出と同じ「表示専用段」の枠で統一する設計が素直。
4. **per-viewport 状態は `PaneState` の領域。** P1-3 / P1-8 と同一の所有境界になる。
   Quad 分割時の期待動作（4 ペイン独立か、全ペイン同期か）を先に決める。
5. **UI は Color Science パネルの OCIO group が既存。** ここに per-pane 選択を足すか、
   View menu に足すかを決める。`useDisplayColorManagementAction` は死んでいるので、
   実装するか削除するかを判断する。

### 決定（2026-09-26、実コード照合で確定）

**`PaneState` は `ArtifactCompositionEditor.cppm:8586` のファイルローカル struct で、
ファイル内に留まっている**（`.ixx` には無い）。要素は `paneId / rect / view / controller / visible`
の 5 つだけで、表示色空間のような表示設定は持っていない。

この 2 点から次が確定する。

- **per-pane の OCIO 状態を `PaneState` には足さない。** ファイルローカル struct を
  display 設定で膨らませると、Editor と Controller の所有境界が曖昧になる
  （AGENTS.md の「責務分離」に反する）。かわりに既存の契約に従い、
  **`CompositionRenderController` インスタンスごとに 1 個の状態として持つ**。
  Quad 時は各ペインが自分の controller インスタンスを持つため、これで
  per-viewport 状態は自然に成立し、P1-3 / P1-8 と同じ所有境界に揃う。
- **`useDisplayColorManagementAction` は削除する。** connect が無く ON/OFF が無意味なため、
  死んだ UI を残すと「切り替えたつもりだが何も起きない」状態になる。
  実 UI は Color Science パネルの OCIO group を正本とする。
- **Quad 分割時の動作は「4 ペイン独立」。** 各ペインが自分の controller インスタンスを
  持つので、状態を controller に持たせる以上、これ以外の選択肢はない。
  「全ペイン同期」は，露出のような表示設定の性格に合わないため採らない。

## P1-14 アスペクトマスク（16:9 等の表示専用マスク）

### 現状（確認できた事実）

- safe area の描画は既存：`ViewportOverlay::drawSafeAreaAndOrigin`
  （`Artifact/src/Widgets/Render/ViewportOverlay.cppm:14`）が `showSafeArea` 時に
  キャンバス四隅からマージンを引いて枠を描く。
- 状態は `CompositionRenderController::Impl::showSafeMargins_`（`:13381`）に保持され、
  `setShowSafeMargins`（`:19091`）/ `isShowSafeMargins`（`:19107`）で公開されている。
- UI は `ArtifactCompositionEditor` の toggle と View menu の `showSafeMarginsAction` から操作可能。
- **「1.85:1」「16:9 のような数値指定のマスク」は無い。** 現行はタイトルセーフを
  一定マージンで描く仕様。

### 導入前の検討事項

1. **P1-14 は「既存 safe-area の派生」であり、前提は満たされている。** 新規描画基盤は不要。
2. **仕様選択。** (a) アスペクト比から内接矩形を計算して枠を描く（1.85:1, 2.00:1 など）、
   (b) 現行のタイトルセーフを数値指定に拡張。HieroPlayer のマスクは (a) の形式。
3. **どの比率を並べるか。** 1.33 / 1.78 / 1.85 / 2.00 を全部描くか、1 つだけ選択させるか。
   `drawSafeAreaAndOrigin` の引数にアスペクト比 1 個分を追加するのが最小変更。
4. **マージンの定義。** アスペクトマスクはタイトルセーフとは別枠として描くか重ねるか。
   既存 `drawSafeAreaAndOrigin` のマージン計算と混線しないよう要設計。
5. **「表示専用」の維持。** マスクは overlay 描画なので finalPresentSRV には影響しない。

## 導入順の推奨と、残る判断点

### 確定した事項（2026-09-26）

1. **文書の書き換えは完了済み。** マイルストーンの P1-11 / P1-13 行を「一部実装済」へ更新した。
2. **P1-11 は submodule 門に阻まれない。** 消費側 widget の API が `updateFrame(QImage)` のみで
   矩形集約が widget 側にあるため、`refreshScopesFromViewport` で `frame.copy()` する 1 箇所の
   差し替えで済む。GPU 化は次段の判断事項で，不要な子モジュール変更を避けられる。
3. **P1-13 の per-pane 状態は controller インスタンスに持たせる。** `PaneState` は
   ファイルローカルで表示設定を持たないため、Controller 側に置けば Quad 分割時の
   4 ペイン独立が自然に成立する。`useDisplayColorManagementAction` は削除する。

### 残る判断点

4. **P1-10 の閾値の正解値。** 線形 float の 1.0 基準か、量子化（8/10/12bit）基準か。
   Artifact は線形合成のため仕様レビューが必要。露出段 cbuffer に同居させるのが最小変更。
5. **P1-11 の ROI 矩形を IRR（P0-3）と共用するか。** `interactiveRenderRegionRect_` は
   部分レンダー矩形であり集計矩形としては未定義。専用矩形を別に持つ方が責務は明確だが、
   UX としては IRR と 1 つの矩形を同期させたい。どちらを取るかを決める。
6. **P1-14 の比率セットと枠の重ね方。** 1.33 / 1.78 / 1.85 / 2.00 の全部か選択式か、
   タイトルセーフと別枠か重ねるか。
7. **P1-13 の present 経路適用。** OCIO 変換を第 3 の scratch として差し込む場合の
   ping-pong バッファを新設するか、composition-space cache 側の scratch を再利用するかを
   ping-pong バッファを新設するか、composition-space cache 側の scratch を再利用するかを
   実装時に検討する。どれも動的確保は禁止なので、事前確保したバッファが要る。

## 関連文書

- `docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md`（P1-10 / P1-11 / P1-13 / P1-14）
- `docs/analysis/HIEROPLAYER_GAP_ANALYSIS_2026-09-22.md`（P0 #1〜#6 の出典）
- `docs/analysis/VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22.md`（DCC 比較）
- `docs/memo/OCIO_MISSING_FEATURES_2026-08-01.md`（**注意: 上半部と #9 は古い。OCIO は統合済み、TransferFunction は 17 種**）
- `Insight.md`（2026-09-26 の P1-12 照合記録、および本検討の判断確定記録）
