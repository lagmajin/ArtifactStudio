# Milestone: Typography Preset & Motion Style UI (M-TY-2)

## 🎯 目的
`ArtifactCore` に実装された高度なタイポグラフィ・アニメーターエンジン（文字単位の Skew, Tracking, 3D 位置等）を、ユーザーが直感的に、かつ短時間で活用できるようにするための UI 層を構築する。

## 🏗️ アーキテクチャ構成
1. **`TextAnimatorPreset`**: 
   - アニメーター設定（RangeSelector, AnimatorProperties, Interpolation）を JSON 形式で保存・復元するデータ構造。
2. **`TypographyInspector`**:
   - `ArtifactTextLayer` 専用のタイポグラフィ制御パネル。
   - 文字単位のプロパティ（個別文字選択・オーバーライド）を視覚的に編集する。
3. **`PresetBrowser`**:
   - サムネイル付きでテキストアニメーションを選択できるライブラリ UI。

## 📅 実装フェーズ

### Phase 1: プリセット・システム基盤 (2026-03-30 - 2026-04-05)
- [ ] `TextAnimatorPreset` クラスの実装 (JSON Serialization/Deserialization)
- [ ] 標準的なプリセットデータの作成
  - Typewriter, Fade-on, Slide-up, Glitch Basic
- [ ] `ArtifactTextLayer` へのプリセット一括適用ロジックの実装

### Phase 2: タイポグラフィ・インスペクタの拡充 (2026-04-06 - 2026-04-12)
- [ ] `CharacterPropertyWidget` の実装
  - 全体設定とは別に、特定の文字範囲に適用される Animator のスタックを表示・編集。
- [ ] 文字単位のカーニング・トラッキング調整用スライダー。
- [ ] Skew (傾き) / Blur / Z-Offset の視覚的ツマミ。

### Phase 3: モーション・スタイル UI (2026-04-13 - 2026-04-20)
- [ ] `TypographyPresetGallery`: アイコンまたはアニメーションGIF付きのプリセット選択ウィジェット。
- [ ] ドラッグ＆ドロップによるプリセット適用。
- [ ] モーショングラフィックス向け「イージング・プリセット」のテキストアニメーター連携。

## 🚀 期待される成果
- ユーザーが数クリックで After Effects レベルの高品質なテキストアニメーションを作成できるようになる。
- 文字単位の細かい微調整が、コードを書かずに Inspector 上で完結する。

## 🔗 関連マイルストーン
- [M-TY-1 Advanced Typography Engine](MILESTONE_ADVANCED_TYPOGRAPHY_ENGINE_2026-03-29.md) (Core 実装)
- [M-FE-5 Templates / Presets](MILESTONE_FEATURE_EXPANSION_2026-03-25.md)
