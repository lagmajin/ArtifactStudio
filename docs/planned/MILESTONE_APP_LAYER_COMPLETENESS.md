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

### M-APP-3 ArtifactEffectService 実装
- `Artifact/include/Service/ArtifactEffectService.ixx` (31行、メソッド0)
- [ ] Effect 作成 / 適用 / 削除 API
- [ ] レイヤーへのエフェクト適用
- [ ] エフェクトスタック管理
- [ ] Undo 連携

### M-APP-4 ArtifactAudioService 実装
- `Artifact/include/Service/ArtifactAudioService.ixx` (16行、コンストラクタのみ)
- [ ] Audio device 管理
- [ ] 再生ルーティング
- [ ] ボリューム制御

### M-APP-5 TranslationManager 実装
- `Artifact/include/Translation/TranslationManager.ixx` (19行)
- [ ] 文字列テーブル読み込み
- [ ] ロケール切り替え
- [ ] 翻訳ルックアップ

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
- [ ] ツールバーボタン → `toolService()->setEditMode()` 接続
- [ ] キーボードショートカット (V=View, T=Transform, M=Mask, P=Paint) 実装
- [ ] メインウィンドウのツール選択 UI とサービスの双方向バインディング

### M-APP-8 DisplayMode → ビューポート表示切り替え
- `ArtifactToolService::setDisplayMode()` は実装済み
- [ ] ビューポートの表示モード (Color/Alpha/Mask/Wireframe) を DisplayMode で制御
- [ ] ショートカット (1=Color, 2=Alpha, 3=Mask, 4=Wireframe)

---

## Phase 4: エフェクトパイプライン接続

### M-APP-9 AbstractGeneratorEffector::apply() 実装
- `Artifact/src/Generator/AbstractGeneratorEffector.cppm`
- [ ] `apply()` → コンポジションパイプラインへの接続
- [ ] `applyToLayer()` → レイヤーへの適用
- [ ] SolidGenerator / GradientGenerator / NoiseGenerator の統合テスト

### M-APP-10 DAGExecutor 効果評価
- `Artifact/include/Engine/DAG/Executor.ixx`
- DAG スケジューリングは実装済み
- [ ] 効果ノードのバックエンド評価 (CPU/GPU)
- [ ] エフェクトスタック → DAG ノードへのマッピング

### M-APP-11 PlaybackEngine::renderFrame() 実装
- `Artifact/src/Playback/ArtifactPlaybackEngine.cppm`
- 現在: ダミー画像 ("Frame N" テキスト) を返す
- [ ] レイヤーコンポジット
- [ ] エフェクト適用
- [ ] 出力ピクセル生成
- ※ レンダーパイプライン全体の完成度に依存

---

## Phase 5: データ/永続化層

### M-APP-12 PreCompose 時間変換
- `ArtifactCore/src/Composition/PreCompose.cppm`
- [ ] `convertTime()` → ネストされたコンポジションの時間変換実装
- [ ] `getRemappedTime()` → タイムリマップ対応
- [ ] `unprecompose()` → レイヤー復元処理

### M-APP-13 VideoLayer::generateProxy() 実装
- `Artifact/src/Layer/ArtifactVideoLayer.cppm`
- [ ] FFmpeg プロセス呼び出し
- [ ] プロキシ生成とパス管理

### M-APP-14 AspectRatio::setFromString()
- `ArtifactCore/src/Core/AspectRatio.cppm`
- [ ] "16:9" 形式の文字列パース

---

## Phase 6: 拡張/プラグイン

### M-APP-15 OFX ホスト基盤
- `Artifact/src/Effetcs/Ofx/` 全体
- [ ] OFX SDK ヘッダ統合
- [ ] ホスト構造体セットアップ
- [ ] プラグインスキャン
- 優先度: 低い (サードパーティ効果の互換性)

### M-APP-16 ArtifactWebBridge 完成
- `Artifact/src/Widgets/WebUI/ArtifactWebBridge.cppm`
- [ ] `selectLayer()` の LayerID 構築
- [ ] `setEffectProperty()` のエフェクトルックアップ
- [ ] `getProjectInfo()` のコンポジション/レイヤー数
- [ ] `getSelectedLayerProperties()` の JSON 生成

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
| **高** | M-APP-3 (EffectService) | エフェクト UI に不可欠 |
| **高** | M-APP-7 (EditMode UI) | ツールサービスを活かすには UI 接続が必要 |
| **中** | M-APP-9 (Generator::apply) | パイプライン接続ポイント |
| **中** | M-APP-10 (DAG eval) | レンダリングの根幹 |
| **低** | M-APP-15 (OFX) | サードパーティ互換、実装コスト高い |
