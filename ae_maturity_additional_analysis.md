# AE-Like 成熟度追加分析 — 既存1-31以外の問題点

**調査対象**: `Artifact/src/`, `ArtifactCore/src/`, `ArtifactRenderer/src/`, `ArtifactWidgets/src/`, `tests/`, build scripts  
**制約**: ソースコードのみ。`docs/` は未読。

---

## P0 — クリティカル/即時修正必須

### 1. メモリリーク: createComposition() が new したものを返さない
- **発生箇所**: `Artifact/src/Project/ArtifactProject.cppm:229`
- **内容**: `new ArtifactComposition(id,params)` を割り当てたが、`result` に代入せずに空の結果を返している。呼び出し元は success=true かつ id=nil を受け取り、リークが発生。`
- **影響**: デフォルトコンポジション作成時にメモリリーク。

### 2. 常に `true` を返す `isProjectClosed()`
- **発生箇所**: `Artifact/src/Project/ArtifactProjectManager.cppm:1023`
- **内容**: `isProjectClosed()` が条件なしで `return true;` を返す。UI は常に「プロジェクト未オープン」状態と判断し、保存/書き出しが不能になる。`
- **影響**: プロジェクト管理UIの動作全体に支障。

### 3. 代入演算子が空、等価比較が常に false
- **発生箇所**: `Artifact/src/Project/ArtifactProjectSetting.cppm:186-197`
- **内容**: `operator=` が空ボディで何もコピーしない。`operator==` が `return false;` 固定。
- **影響**: 設定の値渡し/比較/containers でのキー照合がすべて破綻。

### 4. createNewComposition() が生成結果を捨てて nil を返す
- **発生箇所**: `Artifact/src/Project/ArtifactProjectManager.cppm:277`
- **内容**: `currentProjectPtr_->createComposition("")` の戻り値の ID を破棄し、未初期化 `Id{}` を返す。
- **影響**: 新規コンポジション作成が常に失敗。

### 5. ArtifactRenderController が 23 行の空スタブ
- **発生箇所**: `Artifact/src/Render/ArtifactRenderController.cppm:1-23`
- **内容**: `namespace ArtifactCore { }` の空名前空間のみで実装ゼロ。コンパイルは通るが意味を持たない。
- **影響**: 公開 API の契約が空虚。

### 6. テクスチャビュー取得失敗時に null を強制書き込み
- **発生箇所**: `Artifact/src/Render/ArtifactOffscreenRenderer.cppm:92`
- **内容**: `pDevice_->CreateTexture()` の戻り値を確認せず、失敗しても `pRenderTarget_` が `nullptr` のまま進み、`GetDefaultView()` で落ちる。
- **影響**: GPU リソース不足時にクラッシュ。

### 7. デストラクタが例外を投げうる
- **発生箇所**: `ArtifactCore/src/Graphics/GPUTexture.cppm:91`
- **内容**: カスタムデストラクタ内で `Diligent::Result` を送出。C++ 規格でデストラクタは例外禁止。
- **影響**: スタックアンウィンド中に `std::terminate()`。

### 8. noexcept 関数内で例外が発生しうる
- **発生箇所**: `Artifact/src/Render/Software/ArtifactSoftwareImageCompositor.cppm:708`
- **内容**: `run()` が `noexcept` 宣言だが、`processAligned()` 内の `ImageF32x4_RGBA` 確保で例外なら `std::terminate()`。
- **影響**: メモリ不足時にプロセス強制終了。

### 9. バックエンドacceptが "diagnostic" のみで実質機能なし
- **発生箇所**: `ArtifactRenderer/src/ExternalFrameRenderer.cpp:1336`
- **内容**: `normalizeBackend()` が "cpu"/"software"/"auto" を "diagnostic" にマップし、GPU マルチバックエンドディスパッチが存在しない。
- **影響**: フェーズ4レンダラーエンジンがプレースホルダー。

### 10. ヘッダーが import のみで forward-declaration 相当に退化
- **発生箇所**: `ArtifactPr/include/ArtifactPrMainWindow.hpp:1`
- **内容**: `#pragma once` + `import ArtifactPr.MainWindow;` のみ。インライン宣言がすべて消えている。
- **影響**: 前方宣言としてのみ機能。インクルード单位の契約が消失。

### 11. 0 バイトの空ファイルがコンパイル対象に含まれる
- **発生箇所**: `ArtifactWidgets/src/Graphics/NodeWireGraphicItem.cpp`
- **内容**: ファイルサイズ 0 バイト。`.ixx` ヘッダーへの import もない。`BackendSettingWidget.cpp` も同様に 0 バイト。
- **影響**: モジュール UE 境界の破綻、ビルドノイズ。

### 12. ハードコードされたユーザー固有パスで他環境でビルド/デプロイ不能
- **発生箇所**: `CMakePresets.json:29`, `CMakePresets.json:32`, `build_artifact.bat:2`, `build_artifact_core.bat:3/6`, `build_artifact_final.py:11`, `build_artifact_target.py:6`, `build_artifactcore.py:11/41`, `build_and_report.py:11`
- **内容**: `C:/Users/lagma/...`, `X:\dev\artifactstudio` (小文字) などが直書き。
- **影響**: 他の開発者/CI ではビルド不能、またはデプロイ先が `Desktop/Artifact` に固定される。

### 13. DSL インタープリタ全体がスタブ
- **状態**: 部分修正
- **発生箇所**: `Artifact/src/Tool/AIDSL/src/DSLParser.cppm:381-500`
- **内容**: `CommandNode::compile()` はまだ全メソッド `nullptr` 返却だが、`QueryNode::execute()` は固定ダミーJSONではなく部分応答を返すようになり、`dryRun()`/`execute()` も解析サマリとクエリ結果を返すように変更済み。
- **影響**: コマンド実行は未完だが、DSL の観測・検証は以前より機能する。

---

## P1 — 高重大度（スタブ、未配線、振る舞い破綻）

### 14. SimpleSpline がダミーデータを返す
- **発生箇所**: `Artifact/src/Generator/CloneGenerator.ixx:76-77`
- **内容**: `getPoint()` が `{{0,0,0},{0,0,0}}` 固定、`pointCount()` が 0 固定。
- **影響**: スプライン配布の CloneGenerator は常に Linear 相当に退化。

### 15. GeneratorManager モジュールが空
- **発生箇所**: `Artifact/src/Generator/GeneratorManager.cppm:10`
- **内容**: `namespace {}` のみ。ジェネレータの登録/参照/シリアライズが存在しない。
- **影響**: ジェネレータ機能のホスティング自体が空虚。

### 16. マテリアルノードのトポロジカルソートが未実装
- **発生箇所**: `Artifact/src/Graphics/...` (該当ファイル)
- **内容**: `NodeGraph::getTopologicalOrder()` が追加された順に返す。「ユーザーが正しい順で追加したと仮定」というコメント付き。
- **影響**: ノード接続順によっては HLSL コンパイル失敗や不正描画。

### 17. ShaderNode の Vector3 ピンが HLSL で型変換失敗
- **発生箇所**: `Artifact/src/Graphics/` (ShaderNodeBase.cppm)
- **内容**: `PinType::Vector3` に対する case がなく default で `"0"` (整数文字列) を返す。
- **影響**: HLSL float3 期待個所に integer 0 が入り、シェーダーコンパイルエラー。

### 18. プロジェクト診断ルールが中身コメントアウト
- **発生箇所**: `ArtifactCore/src/Diagnostics/ValidationRules.cppm:23-199`
- **内容**: `MissingFileValidationRule`, `CircularDependencyValidationRule`, `ExpressionValidationRule`, `PerformanceValidationRule` の validate 本体が block commented out。
- **影響**: DiagnosticEngine は何もエラーを検知しない。

### 19. DiagnosticEngine::validateDelta が差分検証しない
- **発生箇所**: `ArtifactCore/src/Diagnostics/DiagnosticEngine.cppm:73`
- **内容**: `validateDelta()` が毎回 `validateAll()` を呼ぶスタブ。コメントに「TODO: 差分検証の実装」。
- **影響**: 大規模プロジェクト編集時の応答性直結。

### 20. 非同期画像書き出しマネージャーが空
- **発生箇所**: `Artifact/src/Render/` (AsyncImageWriterManager.cppm)
- **内容**: `enqueueImageWrite()` が空ボディ。スレッドプールもキューも起動しない。
- **影響**: レンダリング結果の非同期ディスク書き出しが動作しない。

### 21. 非同期ファイル書き出しマネージャー(asio) が空
- **発生箇所**: `Artifact/src/Render/` (asio_async_file_writer.cppm)
- **内容**: コンストラクタ/デストラクタ以外の API が定義されていない空の箱。
- **影響**: Boost.Asio 採用箇所が機能しない。

### 22. ArtifactAbstractComposition::usedAssets() が常に空
- **状態**: 修正済み
- **内容**: レイヤー JSON 内の `sourcePath` / `filePath` 系を再帰収集し、安定化した AssetID を返すように変更。
- **影響**: プロジェクトパッケージング/アセット収集の空配列問題を解消。

### 23. removeLayerById() が空実装
- **状態**: 修正済み
- **内容**: `removeLayerById()` を既存の `removeLayer()` に接続。
- **影響**: ID 指定レイヤー削除が機能するようになった。

### 24. ディストリビューションキー管理: GPUResource の RAII が不完全
- **発生箇所**: `Artifact/src/Render/ArtifactFrameCache.cppm` (eviction callback)
- **内容**: キャッシュ追跡コールバック内で `expiredBlocks.clear()` を実行。reentrant/ABA 問題のリスク。
- **影響**: フレームキャッシュ破損、UseAfterFree。

### 25. 3Dレイヤーで使用する Mesh::loadFromFile/saveToFile が false 固定
- **状態**: 修正済み
- **内容**: OBJ 形式ベースの薄いロード/セーブを実装し、`position` / `normal` / `uv` 属性とポリゴンを読み書き可能にした。
- **影響**: 3D レイヤーのメッシュ I/O が最低限動作するようになった。

### 26. プロジェクト統計の projectName が未設定
- **状態**: 修正済み
- **内容**: `project->settings().projectName()` を `ProjectStats::projectName` に反映。
- **影響**: 統計ダイアログのプロジェクト名欄が埋まる。

### 27. FFmpeg ラッパー他が常に成功を返すスタブ
- **状態**: ? 修正済み（open/play/pause/stop は実装済み）
- **内容**: `open()` をファイル存在チェック付きにし、`play()` / `pause()` / `stop()` を状態管理付きの薄い実装へ変更。
### 28. タイムライン巨大単一ファイル群 (保守性/ビルド時間)
- **発生箇所**: `Artifact/src/Widgets/Timeline/ArtifactTimelineWidget.cpp` (7300行), `ArtifactLayerPanelWidget.cpp` (5600行), `ArtifactTimelineTrackPainterView.cpp` (5000行)
- **内容**: 3ファイルで合計550KB超。モジュール分割なし。
- **影響**: ビルド時間、コードレビュー負荷、リファクタリングの心理的コスト。

### 29. Qt スレッドアフィニティ違反
- **発生箇所**: `Artifact/src/Render/ArtifactHDRMonitor.cppm:27/88`
- **内容**: `Q_EMIT settingsChanged()` / `Q_EMIT analysisComplete(result)` が worker スレッドから直接 emit。
- **影響**: Qt の非同期接続を使う前提のスロットで再入/クラッシュ。

---

## P2 — 中程度/技術的負債

### 30. DiligentDeviceManager の device_ が破棄同期されない
- **発生箇所**: `Artifact/src/Render/DiligentDeviceManager.cppm` (SharedRenderDeviceState)
- **内容**: `globalDevice.load()` は atomic だが、`device_` 破棄時に `globalDevice.store(nullptr)` が in-flight reader と同期しない。
- **影響**: ダングリングポインタ参照。

### 31. ProjectedCorners が凸四角形不変証明的保証なし
- **発生箇所**: `Artifact/src/Render/ArtifactCompositionViewDrawing.cppm`
- **内容**: `std::array<Point2D, 4>` を public 保持し、凸包検証なしでラスタライズ。
- **影響**: 縮小表示で退化入力時にガベッジフィル / GPU クラッシュ。

### 32. Rubi 菱形ループの guard が 4096 固定
- **発生箇所**: `Artifact/src/Render/ArtifactIRenderer.cppm:136-173`
- **内容**: 多角形三角剖法 (ear clipping) の while ループに `guard < 4096`。高頂点多角形で途切れる可能性。
- **影響**: 複雑なシェイプパスのレンダリング失敗 = ブラックアウト。

### 33. D3D12 専用工場関数ハードコード
- **発生箇所**: `Artifact/src/Render/ArtifactOffscreenRenderer2D.cppm:75`
- **内容**: `resolveD3D12Factory()` のみ。Vulkan/Metal フォールバックなし。
- **影響**: マルチバックエンド対応の設計相反。

### 34. RenderCommandBuffer のバッファサイズがマジックナンバー
- **状態**: 修正済み
- **内容**: バッファ上限・トリム比率の定数に根拠コメントを付与し、用途が分かる名前付き定数へ整理。
- **影響**: 過剰/不足 eviction の読み違いリスクを低減。

### 35. PrimitiveRenderer3D に π 等が直書き
- **状態**: 修正済み
- **内容**: 度→ラジアン変換と `2π` を名前付き定数に置換。
- **影響**: 丸め誤差のコピペリスクと可読性の問題を解消。

### 36. FolderItem の子要素所有権が未規定
- **発生箇所**: `Artifact/src/Project/ArtifactProjectItems.cppm:49`
- **内容**: `new FolderItem()` で割り当て後、親の破棄が保証されない場合にリーク。
- **影響**: 大規模プロジェクトでメモリリーク蓄積。

### 37. TreeFilterProxyModel が空シェル
- **状態**: 修正済み
- **内容**: ctor で再帰フィルタ・大文字小文字無視・動的ソートを有効化。
- **影響**: プロジェクトツリーの検索/フィルタがベース設定で機能しやすくなった。

### 38. exportProject2() が空スタブ
- **状態**: 修正済み
- **内容**: `exportProject2()` から既存 `exportProject()` を呼ぶように変更。
- **影響**: ラッパー経由でも実際の書き出しが走る。

### 39. 物理 voronoiFracture が空 + 重心ダミー
- **状態**: 修正済み
- **内容**: `splitPolygon` の重心を実計算化し、`voronoiFracture` を境界クリップベースで返すよう変更。
- **影響**: 破壊エフェクトの基礎分割が動くようになった。

### 40. DSL リテラル解決がプレースホルダー文字列
- **状態**: 修正済み
- **内容**: `resolveLayerRef()` / `resolveCompRef()` の失敗時戻り値を空値に変更。
- **影響**: 不正な参照がプレースホルダー ID として誤認されなくなった。

### 41. ハードコード FPS 30 の残党
- **状態**: 修正済み
- **内容**: カメラレイヤーのフレーム時刻をコンポジションの fps に追従させ、度→ラジアン変換も定数化。
- **影響**: 24/60fps コンポジションでもカメラ軌道が再生速度と一致する。

### 42. Construction Layer が未定義
- **状態**: 新規要求
- **内容**: レンダーされない作業用の設計レイヤーを、正式な layer 概念としてはまだ持っていない。線 / 円 / グリッド / 注釈 / 安全圏 / 軌道を同一の construction surface に置き、親子付けとアニメーションを許したいが、現状は grid / guide / safe margin / note / motion path 的な責務が分散している。
- **影響**: 設計情報を composition 内で一元管理しにくく、作業中の補助情報を layer として安定運用できない。

### 43. Keyframe Pattern Generator / Key Pattern Dialog
- **状態**: 新規要求
- **内容**: 選択中の property に対して、あらかじめ定義したルールから keyframe 列を自動生成するダイアログ。`EasingLabDialog` のような単一補間の調整ではなく、複数 keyframe の並び・間隔・振幅・周期・遷移形状をまとめて生成する入口として使う。
- **用途**:
  - `Stagger` - 複数レイヤーを 2f ずつ遅らせる
  - `Pulse` - `Opacity` / `Scale` をドクドク変化させる
  - `Bounce` - ロゴ / テキストの弾み
  - `Shake` - `Position` / `Rotation` の揺れ
  - `Loop` - 一定周期で繰り返す
  - `Ramp` - `0→100` のような単調変化
  - `Wave` - `Sin` 波っぽい上下 / 回転
  - `Step` - カクカク変化
  - `Random Hold` - グリッチ / ちらつき
  - `Overshoot` - 行き過ぎて戻る
  - `Settle` - 減衰しながら安定
  - `Beat Sync` - `BPM` に合わせて key を生成する
- **影響**: 手打ちで keyframe を並べる作業を減らし、反復系・リズム系・揺れ系の演出を素早く作れるようになる。

### 44. Responsive Layout を持つ Composition
- **状態**: 新規要求
- **内容**: `ResponsiveComposition` を別種のコンポとして増やすのではなく、`Composition` に `Responsive Layout` 機能を付ける方向で整理する。1つの composition の中に複数の layout variant を持たせ、`16:9` / `9:16` / `1:1` のような出力先ごとの差分を同居させたい。
- **用途**:
  - `16:9` - 通常の横長出力
  - `9:16` - 縦長の SNS / モバイル向け出力
  - `1:1` - 正方形の配信用途
- **影響**: コンポ種別を増やさずに、出力先別のレイアウト差分を同一 composition 内で管理できる。将来的に `4:5` や `21:9` を追加しやすくなる。
- **データ構造案**:
  - `Composition` 本体は従来どおり保持し、その中に `ResponsiveLayoutSet` を持たせる
  - `ResponsiveLayoutSet` が複数の `LayoutVariant` を管理し、`activeVariantId` で現在の編集対象を表す
  - `LayoutVariant` は `name`, `baseSize`, `aspectRatio`, `safeArea`, `contentAnchor`, `layoutRules`, `enabled` を持つ
  - `layoutRules` には、余白、拡大縮小方針、crop 方針、ガイド表示、フォールバックなどを入れられるようにする
- **互換方針**:
  - 旧データに `responsiveLayout` が無い場合は、単一 variant として扱う
  - 既存 composition は壊さず、そのまま `default` variant に読み替える
  - 保存時は variant が 1 個でも `ResponsiveLayoutSet` を書く形に寄せる
  - `export` / `preview` では `activeVariantId` が無い場合、最初の variant または default policy に従う
- **UI 案**:
  - 左側に variant list、中央に preview、右側に property inspector を置く
  - `Add Variant` / `Duplicate` / `Rename` / `Set Active` / `Delete` を用意する
  - `16:9 / 9:16 / 1:1` はタブまたはピルで切り替える
  - active variant のみ編集ハンドルを出し、safe area と content anchor をオーバーレイ表示する
  - export ダイアログでも variant を明示選択できるようにする

---

## 追加調査: GPU パス/スレッド安全性の新規発見 (P0-P1)

| # | 種別 | 発生箇所 | 概要 |
|---|------|----------|------|
| G-1 | リソースリーク | `Artifact/src/Render/GPUTextureCacheManager.cppm:277` | resize と upload の競合で古いステージングバッファが開放されずリーク |
| G-2 | スレッド競合 | `GPUTextureCacheManager.cppm:355` | `textureCache_` の `reserve()` が mutex 外で走行、UB |
| G-3 | 例外安全 | `PrimitiveRenderer2D.cppm:809` | ctor 内で `build()` 例外が shared_ptr GPU buffer をリークさせる可能性 |
| G-4 | GPU OOM 伝播漏れ | `PrimitiveRenderer3D.cppm:303/322` | `CreateBuffer()`/`CreateSampler()` 戻り値未確認、null で後続 `Map` が落ちる |
| G-5 | GPU OOM 伝播漏れ | `TextureManager.cppm:full` | `CreateTexture()` 失敗で nullptr 伝播、呼び出し側の null チェック不備 |
| G-6 | テクスチャ参照の所有権曖昧 | `RenderCommandBuffer.cppm:BpptPkt` | `ITextureView*` 生ポインタ、寿命要件が文書化されていない |
| G-7 | ポインタ寿命 | `CompositionRenderer.cppm:8` | `renderer_` が生ポインタ。`ArtifactIRenderer` 破棄後にダングリング参照 |
| G-8 | QImage キャッシュキー不安定 | `PrimitiveRenderer3D.cppm:438` | QImage 内部実装に依存したキーでキャッシュミス/誤取得リスク |

---

## テストインフラの欠損 (P0)

- **発生箇所**: `tests/CMakeLists.txt:38-40`
- **内容**: `add_subdirectory(Artifact)`, `add_subdirectory(ArtifactWidgets)` がコメントアウト。ArtifactCore の `UtilsTest.cpp` 1ファイルだけが存在。
- **影響**: 主要モジュールの回帰テストが存在しない。

---

## まとめ: Gitstatus 風の問題分類

| カテゴリ | P0 | P1 | P2 |
|----------|----|----|----|
| プロジェクト管理/値物体 | 5 | 2 | 1 |
| レンダーリングリソース/例外安全 | 4 | 5 | 2 |
| スレッド/タイミング | 1 | 4 | 1 |
| ツール/DSL/ジェネレータ | 1 | 6 | 2 |
| 診断/バリデーション | 1 | 3 | 0 |
| ビルド/スクリプト/環境 | 4 | 8 | 6 |
| テストカバレッジ | 1 | 0 | 3 |
| GPU/Graphics 低レベル | 5 | 3 | 3 |
