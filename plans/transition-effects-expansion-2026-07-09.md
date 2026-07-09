# トランジションエフェクト拡充 — 計画（2026-07-09）

## 方針（ユーザー確定）
- 3系統のトランジション列挙は**現状維持（独立）**し、必要なブリッジのみ追加。
- NLE→Video エンジン配線（M1）は**新規ブリッジ .cppm**（.ixx は変更しない）。
- ArtifactPr(D)→NLE(A) ブリッジは **write-through ミラー**（UI/JSON は現状維持）。

## 現状（調査で確定した事実）
トランジションは3系統＋デモが独立しており、互いに未接続。
- **(A) NLE.Core** `ArtifactCore/include/NLE/Core.ixx`: 正式データモデル `TransitionKind`(17)、`Transition` 構造体、`Clip.attachedTransitions`/`Track.transitions`、`NLEProjectStore::createTransition`、`SequenceEditor::insertTransition/setTransitionKind`。ソース・オブ・トゥルース。
- **(B) Video エンジン** `ArtifactCore/include/Video/{AbstractTransition,TransitionFactory}.ixx` + `src/Video/Transitions/*.cppm`(16実装): `AbstractTransition::process(DecodedVideoFrame left, right, TransitionContext)` インプレース合成。`TransitionFactory` シングルトン＋各 .cppm の静的registrar。
- **(C) Artifact Effect** `Artifact/include/Effect/ArtifactTransition.ixx` + `src/Effect/ArtifactTransition.cppm`: `TransitionType`(実質36値)、QObjectベース、`TransitionManager::applyTransition(QImage,QImage,output,progress)`。独立実装。alpha 破棄バグあり。
- **(D) ArtifactPr デモ** `ArtifactPr/include/ArtifactPrEditorEngine.ixx` + `ArtifactPrMainWindow.cppm`: 4種 `TransitionType`、Timeline UI (TransitionPanel/TransitionWidget/TimelinePanel)、JSON保存。NLE未接続。

### 確定バグ・欠落
1. **`Video.ixx` 登録漏れ**: 16実装中 `GradientWipeTransition`/`IrisWipeTransition`/`BlockDissolveTransition` の3つが `export import` されていない → 静的registrarが初期化されず `availableKinds()` が13止まり。
2. **`TransitionFactory::create()` 未配線**: 呼び出し元がプロジェクト内に0件（デッドコード）。
3. **(C) alpha 破棄**: `Format_RGB32` 固定でalpha消失、登録が約10個で残りは `nullptr` フォールバック。

### フォーマット相違（M1 実装で発覚）
- `Video.Transitions.*::process()` は `CpuFrameView` 経由の **uint8 RGBA8** を前提。
- Artifact 側レイヤバッファは **RGBA32F (`ImageF32x4_RGBA`)**。
- よってブリッジで RGBA32F↔RGBA8(CpuVideoFrame) 変換が必須。

### 駆動点の前提欠落（M1b で対応）
- 調査時は `CompositionRenderController` が駆動点と推定したが、**実際は `CompositionRenderController` も `ArtifactCompositionEditor` も `NLEProjectStore` を import していない**（Artifact 側で NLE を import する箇所は0件）。
- NLE ストアを描画チェーンまで届ける注入点が存在しないため、実際の駆動配線は M1b に分離。

## 変更対象ファイル
- 新規 `Artifact/include/Layer/ArtifactNLETransitionBridge.ixx`（M0+M1）
- 新規 `Artifact/src/Layer/ArtifactNLETransitionBridge.cppm`（M0+M1: 3実装 import で登録漏れ解消＋ブリッジ本体）
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`（M1b: NLE 注入後、:6370 周辺へ呼び出し）
- 新規 `ArtifactPr/src/NLETransitionMirror.cppm`（M2）
- `Artifact/src/Effect/ArtifactTransition.cppm`（M3: alpha 修正＋未実装具象の実装）

---

## M0 — Video.ixx 登録漏れ修正（実装ファイル経由）
`Video.ixx` は変更せず、新規ブリッジの `.cppm` から `Video.Transitions.GradientWipeTransition` / `IrisWipeTransition` / `BlockDissolveTransition` を `import` し、静的registrar を強制初期化。
効果: `availableKinds()` が 13 → 16（Cut は具象無しで意図通り）。

## M1 — NLE(A)→Video(B) ブリッジ実装（新規 .cppm）
新規 `Artifact/include/Layer/ArtifactNLETransitionBridge.ixx` + `Artifact/src/Layer/ArtifactNLETransitionBridge.cppm`（`module Artifact.Layer.NLETransitionBridge;`）。
- `import NLE.Core;` / `import Video.TransitionFactory;` / `import Video.AbstractTransition;` / `import Video.CpuFrameView;` / `import Image.ImageF32x4_RGBA;`
- M0 登録漏れ解消: ブリッジ `.cppm` から3実装を import。
- フォーマット変換: `rgba8Data()` / `setFromRGBA8()` を用いて RGBA32F↔RGBA8(CpuVideoFrame) 変換。
- `ImageF32x4_RGBA applyNLETransition(TransitionKind kind, const ImageF32x4_RGBA& left, const ImageF32x4_RGBA& right, const TransitionContext& ctx)`:
  - `auto* t = TransitionFactory::instance().create(kind);` null（Cut等）なら `left.DeepCopy()` をパススルー返却。
  - RGBA32F→RGBA8 変換、`process(decodedLeft, decodedRight, ctx)`（left を in-place 変更）→ RGBA8→RGBA32F 戻し → `delete t;` → 結果返却。

## M1b — 駆動配線（NLE ストアの注入、後続）
- `ArtifactCompositionEditor`/上位から NLE ストア参照を `renderController` へ届け、`drawLayerForCompositionView` の `videoLayer` ブロック（:6370 周辺）で `applyNLETransition` を呼ぶ。
- アクティブトラックの `Track.transitions` を走査し、現在 timelineFrame が `Transition::range` 内なら左右クリップフレームを取得して合成。
- 新 signal/slot なし、既存メソッド内の関数呼び出しのみ。

## M2 — ArtifactPr(D)→NLE(A) write-through ミラー
新規 `ArtifactPr/src/NLETransitionMirror.cppm`:
- マッピング表: `Crossfade→Crossfade`、`DipToBlack→Dissolve`(暫定、NLEにDip無し)、`WipeLeft→Wipe(Direction::RightToLeft)`、`WipeRight→Wipe(Direction::LeftToRight)`。UI表示名は維持。
- `EditorEngine::addTransition/deleteTransition` の末尾で対応 `SequenceEditor::insertTransition/removeTransition` を呼び、NLE ストアへミラー書き込み。`DemoSequence` は canonical のまま、JSON保存は変更なし。新 signal/slot 不要。

## M3 — Artifact Effect(C) 補完＋alpha 修正
- alpha バグ修正: `Format_RGB32` → `Format_RGBA8888` にして alpha を保持。
- 未実装具象の実装（WipeUp/Down/Radial/Clock/Diamond, SlideUp/Down, Push*, ZoomRotate, SpinZoom, FlipX/Y, CubeRotate, PageCurl, RippleDissolve, Pixelate, LightFlash, *Mask, Custom 等）で `nullptr` フォールバックを排除。
- `C.TransitionType ↔ NLE::TransitionKind` の静的マッピング表を1箇所に集約（将来統合用）。(C) は独立維持。
- 新アルゴリズムは原則 (B) Video エンジンへ `AbstractTransition` 派生を追加し、(C) は名前を持つだけ（具象は増やさない）。

---

## 制約遵守
- **.ixx 変更**: M0 は Video.ixx を変えずブリッジ .cppm で解消。M1 は新規 .ixx/.cppm。M1b/M2/M3 は既存 .cppm 編集で完結。
- **QtCSS/QImage 新規禁止**: ブリッジは `ImageF32x4_RGBA`/`DecodedVideoFrame` のみ。QImage は既存 IO 変換ヘルパのみ。
- **新 signal/slot 禁止**: 既存メソッド内の関数呼び出しで完結。
- **子モジュール非変更**: `ArtifactWidgets`/`libs`/`third_party` は一切触らない。

## 検証
1. 単体（ファクトリ）: `availableKinds().size()==16` で欠落3種を含む。`create()` は Cut のみ null 許容。
2. 単体（ピクセル）: 左右に既知パターン、progress=0 で left 一致・1 で right 一致・中間で線形補間。alpha 保持（M3）。
3. 統合（M1b）: トラック内遷移範囲で `applyNLETransition` が呼ばれ、プレビューフレームが連続変化。
4. 回帰（M2）: D で追加→保存→再読込→NLE ストアに同一 Transition が復元、JSON は変更なし。
5. 回帰（M3）: `TransitionManager` 登録数が enum 相当まで増、nullptr フォールバック不在を assert。
6. 手動: プレビュー再生で各種トランジションを目視。
