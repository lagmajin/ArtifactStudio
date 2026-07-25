# MILESTONE: AE ユーザビリティ・ギャップの実コード検証クロージャ

> 2026-07-10 作成（ブランチ: codex/2026-07-10）
> 親: main @ 912dbbf

## 目的

「AE より使いやすい物を作りたい」に向けて、ドキュメント上「missing」と
されていた項目を実コードで検証し、本当に未完成なものだけをクローズする。

検証の結果、Tier1 編集ペインポイント（キーフレームフォロー・エフェクト並び替え・
マーカー・ソーステキスト・オーディオスクラブ・オートオリエント・インアウトスライド・
トラックマット・キーフレームコピペ）は**すでに実装済み**だった。よって本マイル
ストーンは「実コードで確定した真の未実装」7 項目に絞る。

## 検証済み（実装あり・非対象）

以下はドキュメントの記述が古く、実コードでは動作しているため本マイルストーンの
対象外。回帰テスト等で保護状態を維持するだけ。

- キーフレーム前後フォロー + リップル: `shiftAnimatableLayerKeyframes` / `collectRippleLaterLayers`（`ArtifactTimelineWidget.cppm`）
- エフェクト並び替え: `moveEffectInLayerInCurrentComposition`（`ArtifactProjectService.cppm:2941`）
- マーカー: `ArtifactMarker` + `ArtifactInOutPoints`（`ArtifactInOutPoints.cppm`）
- キーフレームコピペ: `copySelectedKeyframes` / `pasteKeyframesAtPlayhead`（`ArtifactTimelineWidget.cppm`）
- ソーステキストキーフレーム: `setSourceTextAtFrame`（`ArtifactTextLayer.cppm`）
- オーディオスクラビング: `ArtifactAudioScrubController`（`ArtifactAudioScrubController.cppm`）
- オートオリエント: `AutoOrientMode::AlongPath` + `atan2`（`AnimatableTransform3D.cppm`）
- イン/アウトスライド: `slideTimingBy` + `SlideClipCommand`（`ArtifactAbstractLayer.cppm`）
- トラックマット: `LayerMatteReference` + Alt ドラッグリンク + `ChangeLayerMatteReferencesCommand`

## 真の未実装（本マイルストーンの対象）

| ID | 項目 | コード現状 | 影響 |
|---|---|---|---|
| M-UG-1 | ネストコンポ長さ伝播 | `PreCompose.cppm` の `parentToChildTime`/`childToParentTime` は TODO 恒等変換スタブ。`ArtifactAbstractComposition::setFrameRange` にカスケードなし | 親/子コンポ長さ変更が互いに伝播せず「10レイヤー地獄」 |
| M-UG-2 | アセットインスタンス共有 | `AssetManager`（`AssetManager.cppm`）が空スタブ、`AssetInstance` は計画のみ | 5 コピー = 5 回デコード / 5 回 GPU アップロード |
| M-UG-3 | Easy Ease 速度ベース | イージーボタンは固定ベジェ（0.42/0.58）のみ。`EasyEase` シンボルなし、velocity ベース自動タンジェントなし | AE の F9 の「隣接速度からの気持ちよさ」がない |
| M-UG-4 | 式ピックwhip | 式評価・エディタは実装済み。ドラッグでプロパティを繋ぐ AE 的 pickwhip は未実装（親子リンク pickwhip は別存在） | 式リンクがテキスト入力のみで面倒 |
| M-UG-5 | プリコンポーズ作成 | `ArtifactProjectService::precomposeLayersInCurrentComposition()` に実コンポ生成・レイヤー移動・復元情報・Undo 導線を実装済み（2026-07-25 静的確認） | ✅ 完了（runtime 検証はスキップ） |
| M-UG-6 | プロキシサービス統一 | `ArtifactProxyManager` に動画生成・パス管理・バッチ API を実装し、VideoLayer と Project View の動画キューを接続済み。画像サムネイル経路と品質 enum の完全統一は未完了 | 動画 proxy の共通生成経路は確立、runtime/enum整理待ち |
| M-UG-7 | テキストアニメータ専用トラック UI | エンジン・セレクタ・グリフ適用は実装済み。AE 的「アニメータ/セレクタ専用トラックパネル」なし（汎用プロパティトラック経由のみ） | アニメータ編集が直感的でない |

## 推奨実装順（影響範囲 × 体験向上）

1. **M-UG-3 Easy Ease 速度ベース** — 影響小、CurveEditor のタンジェント推定を velocity ベースに昇格するだけで AE の F9 体験に近づく
2. **M-UG-1 ネストコンポ長さ伝播** — 影響大だが「破綻しない」体験に最も効く
3. **M-UG-5 プリコンポーズ作成** — 現状スタブで作成が通らない
4. **M-UG-2 アセットインスタンス共有** — パフォーマンス基盤
5. **M-UG-4 式ピックwhip** — 編集 UX 向上
6. **M-UG-6 プロキシサービス統一** — 保守性・一貫性
7. **M-UG-7 テキストアニメータ専用トラック UI** — 視覚的編集性

---

## M-UG-1: ネストコンポ長さ伝播

### ゴール

コンポジションの frame range（長さ）変更が、参照するプリコンポレイヤーの
outPoint へ、またプリコンポ内の子レイヤーへ、双方向に伝播する。

### 現状コード

- `ArtifactCore/src/Composition/PreCompose.cppm`
  - `NestedTimeUtils::parentToChildTime(parentTime, precompLayerId)`（~301）: `Q_UNUSED` + `return parentTime;`（恒等スタブ、TODO あり）
  - `childToParentTime(childTime, precompLayerId)`（~312）: 同上スタブ
  - `getRemappedTime(...)`: 同上スタブ
  - `convertTime(...)`: ネストツリーを辿るが上記恒等変換のみ
  - `nestingInfo`（~65）: 親子関係のみ（`getParentComposition` / `getCompositionNesting`）、長さ同期なし
- `Artifact/src/Composition/ArtifactAbstractComposition.cppm:1879`
  - `setFrameRange(...)`: `impl_->frameRange_` をローカル更新 + work-area clip + `changed()` のみ。子/親へのカスケードなし
- `Artifact/src/Service/ArtifactProjectService.cppm:2457-2550`
  - プリコンポーズ作成時に `childParams.setDurationFrames(...)` + プリコンプレイヤーの `setOutPoint(...)` を**一度だけ**初期コピー（伝播ではない）

### 完了条件

- `setFrameRange` 変更時に、ネストしている子コンポの長さを再計算（または
  親プリコンプレイヤーの outPoint を更新）
- 子コンポ長さ変更時、それを参照する親プリコンプレイヤーの outPoint を更新
- `parentToChildTime`/`childToParentTime` が `timeRemap` / `inPoint` オフセットを
  反映した実変換になる
- 無限ループ・循環参照（A⊃B⊃A）でスタックオーバーフローしない

## M-UG-2: アセットインスタンス共有

### ゴール

同一ソース（パス/フットエッジ）から作られた複数レイヤーが、1 回のデコード・
1 回の GPU アップロードを共有する（refcount）。

### 現状コード

- `ArtifactCore/include/Utils/AssetManager.ixx`: `class AssetManager` — コン
  ストラクタ/デストラクタ/削除済コピーのみ。メソッドなし
- `ArtifactCore/src/Asset/AssetManager.cppm`: `Impl` は空。`acquire`/`release`/
  キャッシュ/refcount なし
- `ArtifactCore/include/Asset/AssetInstance.ixx`: 存在しない（計画のみ）
- `Artifact/src/Render/DiligentDeviceManager.cppm:75`: `refCount` は GPU デバイ
  ス共有用でアセットとは無関係

### 完了条件

- `AssetInstance`（shared_ptr refcount）の導入
- `AssetManager` に `acquireByPath(path)` / `release(id)` / デコード済ペイロード
  キャッシュ
- レイヤー生成経路が `AssetManager` 経由でインスタンスを取得・共有

## M-UG-3: Easy Ease 速度ベース

### ゴール

AE の F9（Easy Ease）のように、選択キーフレームの前後キーフレームとの時間・
値の差分（速度）から自動で bezier タンジェントを決定する。

### 現状コード

- `Artifact/src/Widgets/ArtifactTimelineWidget.cppm`
  - `keyframeEaseInButton_`/`keyframeEaseOutButton_`/`keyframeEaseInOutButton_`（~4217）
  - `applyInterpolationToSelectedKeyframes(InterpolationType)`（~5696 接続）
  - `applyInterpolationToSelectedKeyframesImpl`（~2608）: Bezier 時に
    `cp1_x=0.42, cp1_y=0.0, cp2_x=0.58, cp2_y=1.0` を**固定**でセット（velocity 非考慮）
- `Artifact/src/Widgets/ArtifactCurveEditorWidget.cppm:236`: 「Estimate tangent for
  keyframe (slope between neighbors)」コメントあり、単純 slope 推定のみ

### 完了条件

- 隣接キーフレーム間の `Δtime` / `Δvalue`（速度）からタンジェントを計算
- Ease In/Out/InOut それぞれで速度ベースハンドルを適用
- 固定 0.42/0.58 へのフォールバック（隣接がない等）を保持

## M-UG-4: 式ピックwhip

### ゴール

式エディタ内で、プロパティ名/レイヤーをドラッグで式に挿入（AE 的 pickwhip）。

### 現状コード

- `ArtifactCore/src/Property/AbstractProperty.cppm:278`: 式評価（`setExpression`/
  `evaluateValue`）実装済み
- `Artifact/src/Widgets/ArtifactExpressionCopilotWidget.cppm`: エディタ・オート
  コンプ・バリデーション実装済み（`thisComp`/`thisLayer` サジェスト）
- `Artifact/src/Widgets/Timeline/ArtifactLayerPanelWidget.cpp` の `pickWhip*`:
  親子リンク用のみ（式とは無関係）

### 完了条件

- 式コンパニオンからプロパティ/レイヤーをドラッグで式テキストへ挿入
- 挿入される参照表記が `ExpressionEvaluator` の文法（`thisLayer`/`thisComp.layer("name")`）と一致

## M-UG-5: プリコンポーズ作成

### ゴール

プリコンポーズ作成が実際に新規コンポジションを作り、選択レイヤーを移動する。

### 現状コード

- `ArtifactCore/src/Composition/PreCompose.cppm:90`: `PreComposeManager::precompose()`
  は ID 生成 + nestingInfo 記録のみ。`result.success=true` だがコンポ/レイヤー移動なし（スタブ）
- `Artifact/src/Service/ArtifactProjectService.cppm:2581`: `unprecomposeLayerInCurrentComposition`
  は実装済み（レイヤー実移動 + Undo）。作成側と対になっていない

### 完了条件

- `precompose()` が実コンポを生成し、選択レイヤーを子へ移動 + Undo 記録
- 作成後のプリコンプレイヤー outPoint が子長さと一致（M-UG-1 と連動）
- `PreComposeManager` スタブを実サービス経路へ統合（または作成側も `ArtifactProjectService` へ）

## M-UG-6: プロキシサービス統一

### ゴール

`ArtifactProxyManager` を実装し、プロキシ生成・パス管理を 1 箇所へ統一する。

### 現状コード

- `Artifact/include/Proxy/ProxyService.ixx`: `ArtifactProxyManager` の動画生成・パス・バッチ API を実装済み
- `Artifact/src/Layer/ArtifactVideoLayer.cppm`: `generateProxy()` と decode controller の proxy 切替をサービス経由に接続済み
- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm`: 動画はサービス、画像は JPG サムネイル経路に分岐。`ProxyQuality` 系はまだ完全統一ではない

### 完了条件

- `ArtifactProxyManager` に実生成・バッチ・キャッシュ・パス管理を実装
- ProjectManagerWidget の ad-hoc 生成をサービスへ移譲
- `ProxyQuality` enum を 1 箇所へ統一

## M-UG-7: テキストアニメータ専用トラック UI

### ゴール

AE 的「アニメータ/セレクタ」をタイムライン上の専用トラックとして編集可能にする。

### 現状コード

- `ArtifactCore/include/Text/TextAnimator.ixx`: `RangeSelector`/`WigglySelector`/
  `TextAnimatorEngine` 実装済み
- `Artifact/src/Layer/ArtifactTextLayer.cppm:120`: `animators_` ベクトル + 
  `perGlyphMode_` で `applyAnimatorStack` 適用（~3596）実装済み
- 専用トラックパネル: コード上に存在せず、汎用プロパティトラック（`text.animators.N.*`）経由のみ

### 完了条件

- タイムラインにテキストアニメータ専用トラック（アニメータ追加/選択/セレクタ表示）
- 既存 `TextAnimatorEngine` のセレクタ/プロパティ編集と整合

---

## 非ゴール

- AE の Roto/Paint、プラナートラッカー、3D マテリアル、.mogrt/AEP/PSD/Lottie
  インポート、OCIO/ACES カラーマネジメント本実装（別マイルストーン）
- キーフレーム基盤の堅牢化（→ `MILESTONE_CORE_KEYFRAME_ROBUSTNESS_2026-07-10.md`）
- グラフエディタ/ブレンドモード/式評価自身の再実装（すでに実装済み）

## Validation Checklist

- [ ] ネストコンポ長さ変更が親子双方向に伝播し、循環でクラッシュしない
- [ ] 同一ソース 5 レイヤーでデコード/GPU アップロードが 1 回になる
- [ ] Easy Ease が隣接キーフレーム速度からタンジェントを決定する
- [ ] 式エディタからドラッグでプロパティ参照を挿入できる
- [ ] プリコンポーズ作成が実コンポを生成し Undo で戻る
- [ ] プロキシ生成が `ArtifactProxyManager` 経由に完全統一される（動画経路は完了、画像サムネイルは別責務）
- [ ] テキストアニメータが専用トラックで編集できる
- [ ] 既存プロジェクトのシリアライズ往復が壊れない

## Next Execution Slice（推奨）

M-UG-3（Easy Ease 速度ベース）から着手。影響範囲が CurveEditor/TimelineWidget
のタンジェント計算に閉じており、回帰テストで安全性を確保しやすい。
