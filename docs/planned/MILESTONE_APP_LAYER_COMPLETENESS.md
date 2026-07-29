# マイルストーン: アプリ層完成度向上

> 2026-03-20 作成

## 目標

コアモジュールのスタブ実装を埋め、サービス層・ツール層・エフェクト層の接続を完成させる。
UI/UX やレンダリングパイプラインの新機能ではなく、**既に宣言済みだが未実装のメソッドを実装する**ことが主目的。

---

## Phase 1: サービス層の穴埋め (手軽に着手可能)

### M-APP-1 ApplicationService 完成 ✅
- [x] `initialize()` / `shutdown()` の実装
- [x] サービスファサード (project, clipboard, tool アクセサ)
- [x] `isProjectOpen()`, `applicationVersion()`

### M-APP-2 ArtifactToolService 完成 ✅
- [x] EditMode / DisplayMode 管理
- [x] ToolManager へのバインディング
- [x] シグナル (editModeChanged, displayModeChanged, toolChanged)

### M-APP-3 ArtifactEffectService 実装 ✅（静的確認 2026-07-30）
- `Artifact/include/Service/ArtifactEffectService.ixx` / `Artifact/src/Service/ArtifactEffectService.cppm`
- [x] Effect factory、available effect 列挙
- [x] レイヤー／Compositionへの追加・削除・enabled変更・並べ替え
- [x] レイヤーEffectの複製とproperty更新
- [x] ProjectService経由のUndo連携（layer add/remove、enabled、order）
- [x] Effect presetの保存・読込
- runtime UI操作と実機確認は未実施

### M-APP-4 ArtifactAudioService 実装 ⚠️（facade/device選択の静的実装済み・runtime確認待ち）
- `Artifact/include/Service/ArtifactAudioService.ixx`
- [x] 物理Audio output device名の列挙（`availableOutputDeviceNames()`）
- [x] 選択deviceのPlayback backendへの適用（`setOutputDeviceName()` → `ArtifactPlaybackEngine`）
- [x] 選択device名のQSettings永続化（`audio/outputDeviceName`）
- [x] current compositionのaudio layer bus生成とMaster既定ルーティング
- [x] master volume / muteはPlayback出力だけに適用し、Core masterとの二重gainを防止
- [x] layer busのvolume / pan / mute / solo操作
- [x] Composition Audio Mixerのmaster操作をAudioService facadeへ、strip操作をCore busへ接続

facade sliceと物理Audio device列挙・選択・設定永続化の公開契約は静的実装済み。指定名は次回のPlayback audio device openに使われ、既に開いているdeviceは再オープン対象になる。実機runtime確認と設定UI接続は未実施。ユーザー方針によりbuild / testは実施していない。

### M-APP-5 TranslationManager 実装 ✅
- `Artifact/include/Translation/TranslationManager.ixx`
- [x] directory / file単位のJSON文字列テーブル読み込み
- [x] locale正規化、起動時選択、英語fallback付き切り替え
- [x] nested key flatten、fallback、引数置換を含む翻訳ルックアップ
- [x] 再ロード時のstale fallback除去とtransactional file / locale切り替え
- [x] active localeとfallbackを統合したloaded key列挙

静的実装済み。ユーザー方針によりbuild / testは実施していない。

---

## Phase 2: Undo/Redo の穴埋め

### M-APP-6 AddLayerCommand / RemoveLayerCommand 実装 ✅
- `Artifact/src/Undo/UndoManager.cppm`
- `SetPropertyCommand` と `MoveLayerCommand` は実装済み
- [x] `AddLayerCommand::redo()` → `comp->appendLayerTop(layer)` / `appendLayerBottom(layer)`
- [x] `AddLayerCommand::undo()` → `comp->removeLayer(layer->id())`
- [x] `RemoveLayerCommand::redo()` → 元インデックス保存 + `comp->removeLayer(layer->id())`
- [x] `RemoveLayerCommand::undo()` → `comp->insertLayerAt(layer, originalIndex_)`
- [x] `shared_ptr<void>` → `ArtifactCompositionPtr` に型変更
- [x] `label()` にレイヤーID表示

---

## Phase 3: ツール層の接続

### M-APP-7 EditMode → ツール自動マッピングの UI 接続
- `ArtifactToolService::setEditMode()` は実装済み
- [x] 既存ツールバーdispatcher → `toolService()->setEditMode()` 接続
- [x] 既存tool shortcutを維持し、選ばれたtoolからEditModeを自動判定（専用V/T/M/Pは競合のため不採用）
- [x] 既存`ToolChangedEvent`経路によるツール選択UIとサービスの双方向バインディング

### M-APP-8 DisplayMode → ビューポート表示切り替え
- `ArtifactToolService::setDisplayMode()` は実装済み
- [x] Layer Viewerの既存Display menu (Color/Alpha/Mask/Wireframe) とDisplayMode serviceを同期
- [x] Composition Viewの既存Color/Alpha channel操作とDisplayMode serviceを同期
- [x] serviceのprogrammatic変更をShow / Focus / Window activation時にviewportへpull
- [x] Mask/WireframeはLayer Viewer責務、Composition ViewはColor/Alpha channel責務として分離
- [x] Composition Viewは既存`Alt+2` Color / `Alt+3` Alphaを維持し、数字単独shortcutは既存操作との競合を避けて追加しない

静的実装済み。ユーザー方針によりbuild / testは実施していない。

---

## Phase 4: エフェクトパイプライン接続

### M-APP-9 AbstractGeneratorEffector::apply() 実装
- `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- [x] legacy `Generator.Effector`の存廃決定 — **対象外／撤回**
- [x] 現行`ArtifactAbstractEffect` generator群へのpreset / parameter移行 — **本契約では実施しない**
- [x] 移行後に未使用stubを削除 — **対象外／撤回**

2026-07-13監査では`AbstractGeneratorEffector`の利用箇所がなく、任意layerへ生成bufferを注入する正式契約も存在しない。現行generatorは`ArtifactAbstractEffect` pipelineで実装されているため、旧抽象を直接接続する案は撤回する。したがって本項目は「未完了」ではなく、現行アーキテクチャでは対象外として扱う。

### M-APP-10 DAGExecutor 効果評価（部分実装）
- `Artifact/include/Engine/DAG/Executor.ixx`
- [x] 有向依存のin-degree計算と依存解決順の並列スケジューリング
- [x] ThreadPoolへのenqueueと全タスク待機
- [x] 実行前の`EffectGraph::compile()`検証と、循環／不正接続時の失敗返却
- [x] エフェクト未設定ノードを`Cached`にせず`Error`として扱う
- [x] ノード評価失敗を`evaluateGraph()`の戻り値へ集約
- [x] 画像エフェクト評価中の例外をノード`Error`へ変換
- [x] 画像入力変更時に旧出力バッファを無効化
- [x] 画像入力を明示したノードの`applyConfigured()`評価と出力バッファ保持
- [x] 画像出力から接続先`ImageBuffer`入力へのDAG伝播
- [x] 未接続の非画像バックエンドを`Cached`にせず`Error`として返す安全境界
- [x] `Error` ノードから下流へ stale／partial image buffer を伝播させず、独立枝だけを継続
- [x] 同一入力ポートへの複数 producer 接続を拒否し、in-degree と入力バッファの対応を一意化
- [x] 接続解除時に対象ノードと下流ノードへ dirty を伝播し、旧出力の再利用を防止
- [ ] 全ステージの効果ノード実データ／バックエンド評価（画像入力を明示したノード以外は未接続）
- [ ] 非画像を含むエフェクトスタック → DAGノードの完全な入力／出力バッファマッピング
- cycle／missing inputを含むruntime検証は未実施

### M-APP-11 PlaybackEngine::renderFrame() ✅（静的実装済み・runtime確認待ち）
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
- [x] `generateCompositionThumbnail()` を介したcomposition canvas生成
- [x] composition sizeへのpreview scalingとframe positionの一時切り替え・復元
- [x] composition未提供／描画失敗時の明示的なpreview unavailable fallback
- レイヤー／一部effectの最終品質、連続再生、runtime表示は未検証

---

## Phase 5: データ/永続化層

### M-APP-12 PreCompose 時間変換 ✅（静的確認 2026-07-30）
- `ArtifactCore/src/Composition/PreCompose.cppm`
- [x] `convertTime()` → composition nesting hierarchyを使ったsource/target間変換
- [x] `getRemappedTime()` / parent-child time helpers
- [x] `unprecompose()` / `restorePrecompose()` → レイヤー復元処理
- runtimeの複雑なnesting・undo/redo確認は未実施

### M-APP-13 VideoLayer::generateProxy() ✅（静的実装済み・runtime確認待ち）
- `Artifact/src/Layer/ArtifactVideoLayer.cppm`
- [x] FFmpegプロセス呼び出しとquality別scale/H.264/AAC生成
- [x] プロキシ生成・パス管理、batch/clear/has/info API
- FFmpeg実行環境での生成・再生・失敗時cleanupは未検証

### M-APP-14 AspectRatio::setFromString() ✅（静的確認 2026-07-30）
- `ArtifactCore/src/Core/AspectRatio.cppm`
- [x] `16:9` / `16/9` / `1920x1080` パース
- [x] decimal ratioの有理近似と正規化
- invalid input / runtime利用箇所の確認は未実施

---

## Phase 6: 拡張/プラグイン

### M-APP-15 OFX ホスト基盤
- `Artifact/src/Effetcs/Ofx/` 全体
- [ ] OFX SDK ヘッダ統合
- [ ] ホスト構造体セットアップ
- [ ] プラグインスキャン
- 優先度: 低い (サードパーティ効果の互換性)

### M-APP-16 ArtifactWebBridge 完成 ✅
- `Artifact/src/Widgets/WebUI/ArtifactWebBridge.cppm`
- [x] `selectLayer()` の LayerID構築とProjectService選択経路
- [x] selected layer限定のeffect ID / display nameルックアップ
- [x] JSON scalar / array / objectの型を保持したeffect property更新
- [x] EffectService経由のproperty存在確認と変更通知
- [x] 実project有無、全composition数、current layer数のproject情報
- [x] selected layer / effect / property groupのJSON生成

静的実装済み。ユーザー方針によりbuild / testは実施していない。

---

## 実装順序の推奨

```
Phase 1 (サービス層) ──→ Phase 2 (Undo) ──→ Phase 3 (ツール接続)
                                         └──→ Phase 4 (エフェクト接続)
Phase 5 (データ層) ──→ Phase 6 (拡張)
```

| 優先度 | マイルストーン | 理由 |
|---|---|---|
| **最優先** | M-APP-6 (Undo) | 宣言済み API を呼ぶだけ、パターン明確 |
| **高** | M-APP-3 (EffectService) ✅ | 静的実装済み、runtime確認待ち |
| **高** | M-APP-7 (EditMode UI) | ツールサービスを活かすには UI 接続が必要 |
| **対象外** | M-APP-9 (Generator::apply) | 旧抽象の直接接続案は撤回済み |
| **中** | M-APP-10 (DAG eval) | レンダリングの根幹 |
| **低** | M-APP-15 (OFX) | サードパーティ互換、実装コスト高い |
