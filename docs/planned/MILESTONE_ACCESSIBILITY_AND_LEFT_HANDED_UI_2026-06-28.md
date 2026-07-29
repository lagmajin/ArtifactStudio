# Milestone: Accessibility and Left-Handed UI Support (M-ACC-1)

**マイルストーンID**: M-ACC-1
**作成日**: 2026-06-28
**優先度**: P2 (Medium)
**推定工数**: 2-5日
**カテゴリ**: Accessibility / UI Ergonomics / Input
**状態**: Phase 1〜2 の静的実装は主要ウィジェットへ適用済み — runtime 検証、RTL レイアウト、Phase 3 の全体統合は未完了
**依存**: 既存 UI レイアウト、入力系、メニュー配置、設定保存基盤

### 2026-07-29 Implementation Loop

- ✅ `adjustContextMenuPosition()` を Project View、Inspector、Graph、Status Bar、Audio Mixer、Timeline、Asset、Property、Render、Playback、Diagnostics の主要コンテキストメニューへ適用。
- ✅ Render Layer の背景モードコンテキストメニューにも同じ配置補正を適用。
- ✅ Application Settings のアクセシビリティ項目へ Accessible Name / Description を付与。
- ✅ Composition Graph と Status Bar にも Accessible Name / Description を付与。
- ✅ Composition Audio Mixer と Event Bus Debugger にも Accessible Name / Description を付与。
- ✅ Render Queue と通常 Audio Mixer にも Accessible Name / Description を付与。
- ✅ Composition Graph の検索／表示領域と Render Queue のジョブ一覧へ操作目的の Accessible Name / Description を付与。
- ✅ Composition Graph / Render Queue の検索欄へ `Accessibility::scaledSize()` による最小ヒット高さを適用。
- ✅ Application Settings のアクセシビリティ入力群にも `Accessibility::scaledSize()` による最小ヒット高さを適用。
- ✅ 左利き設定を既存のアクセシビリティ設定経路で利用し、新規のシグナル／スロット配線は追加していない。
- ✅ 変更ファイルの静的差分チェックとドキュメントインベントリ更新を実施。
- ⏳ 実アプリ上の左利き設定切り替え、RTL レイアウト、スクリーンリーダー／キーボード操作の runtime 検証は未実施。
- ⏳ 全ウィジェットの左右反転・ツールバー重心調整を含む Phase 3 は未実装。

---

## 目的

左利きのユーザーや、操作しづらい入力環境のユーザーに向けた UI 補助をまとめて整備する。
単なる左右反転ではなく、操作頻度の高い UI を無理なく使えるようにすることを目的とする。

---

## 背景

### 現状
- 左利き向けの専用 UI 補助は、現時点では明示的には見当たらない
- 既存には `reverse` / `mirror` / `flip` / `swap` 系の操作はあるが、これは主に編集機能であり、アクセシビリティ補助ではない
- 画面上の主要操作が右寄りに集まりやすく、入力の利き手に応じた最適化はまだ薄い
- キーボード中心、マウス中心、片手操作補助、視認性補助を別々に考えていないため、今後の拡張余地がある

### ねらい
- 左利きユーザーがメニューや主要ボタンを扱いやすくする
- 片手操作時に重要な操作を近い位置へ寄せる
- 視認性・ヒットしやすさ・入力しやすさを上げる
- 障碍の有無に限らず、操作補助として再利用できる形にする

---

## 想定対象

### 1. 左利き向け補助
- 主要ボタン配置の左右入れ替え
- コンテキストメニューや補助パネルの出現位置の優先調整
- 利き手に合わせたツールバーの重心調整

### 2. 片手・補助入力向け
- よく使うコマンドの近接化
- ショートカットの再配置候補
- ドラッグやホバーに依存しない代替操作

### 3. 視認性・操作性補助
- 高コントラスト化の補助
- ハンドルやヒット領域の拡張
- 重要操作のラベル明示

### 4. 障碍者向け補助
- 細かいポインティングを減らす
- 操作の再試行や遅延を許容する
- 誤操作を減らすための確認補助

---

## 方向性

- 左右反転だけを独立機能にしない
- アクセシビリティ設定の中に、左利き向けの補助を含める
- UI ごとに無理な共通化をせず、既存の widget 責務に沿って実装する
- 既存の入力・レイアウト・設定保存の経路を再利用する

---

## 先行検討項目

1. 利き手設定をどこに保存するか
2. 左利き時に入れ替えるべき UI をどこまでにするか
3. メニュー位置の自動切替を行うか
4. ショートカット再配置は別マイルストーンに分けるか
5. 色覚・視認性補助も同じ設定群に含めるか

---

## Phase 1

Phase 1 では、見た目の大改造ではなく、効果が高く副作用が小さい補助から入る。

### 1. 設定の土台を作る
- `Left-handed mode`
- `High-contrast assist`
- `Reduce precision dependence`
- `Larger hit targets`

この段階では、まず設定を保存・復元できるだけでよい。
UI 全体へ一気に適用せず、後続の widget が参照できる共通設定として整える。

### 2. 主要な操作補助だけ先に入れる
- コンテキストメニューの出し位置を利き手に合わせて調整する
- クリックやドラッグで困りやすい箇所のヒット領域を少し広げる
- 重要操作のラベルやアイコン密度を見直す
- ホバー依存の補助を、必要ならクリック補助に置き換える

### 3. 左利きで効きやすい UI から触る
- タイムライン
- プロパティ
- 主要なツールバーやメニュー
- レンダービューの補助 UI

### 4. まずは非破壊で入れる
- 既存の左右配置を壊さない
- レイアウト切替は設定で無効化できるようにする
- 機能本体ではなく、補助表示と入力補助を優先する

### Phase 1 の完了条件
- [x] 利き手設定を保存できる
- [x] いくつかの主要 widget がその設定を読める
- [x] 左利き向け補助の有効/無効を切り替えられる
- [x] 既存 UI の挙動を壊さずに複数の補助が入っている

---

## Phase 2

Phase 2 では、Phase 1 の設定土台を使って、実際の操作体験を少しずつ改善する。

実装追加: `ArtifactTimelineWidget` の主要ツールボタンとProperty Editorのレイヤー状態ボタンが `Larger hit targets` 設定に連動。
実装追加: `Prefer high-contrast hints` 有効時、タイムライン主要操作ボタンのラベルを太字化。
実装追加: 色覚補助設定をタイムライン主要操作ボタンの文字色へ適用。
実装追加: タイムライン主要操作ボタンへ既存ラベル・ツールチップ由来のAccessible Name/Descriptionを設定。
実装追加: `Reduce hover dependency` 有効時、選択サマリーがホバー中キーフレーム情報に依存しない。
実装追加: Property Editorのレイヤー状態ボタンにもAccessible Name/Descriptionを設定。

### 1. 入力補助の拡張
- 片手操作しやすい位置に主要アクションを寄せる
- 頻繁に使うコマンドを近くにまとめる
- ポインティング精度が必要な箇所を減らす
- キーボード操作で代替できる導線を増やす

### 2. 視認性補助の拡張
- 高コントラスト補助を適用できるようにする
- 重要なハンドルや境界を見やすくする
- 文字やアイコンが埋もれやすい箇所を優先して改善する
- 状態表示を色だけに依存しないようにする

### 3. widget 別の最適化
- `ArtifactTimelineWidget`
  - ハンドル、選択域、再生補助の見直し
- `ArtifactPropertyWidget`
  - 折りたたみ、ラベル、操作密度の見直し
- `ArtifactCompositionRenderWidget`
  - 補助 UI の表示位置やヒット領域の調整
- `ArtifactMainWindow`
  - 左利き時の起点位置やメニュー導線の調整

### 4. 使い分けの整理
- 左利き設定は「配置の近さ」を優先する
- 障碍者向け補助は「誤操作の減少」と「見やすさ」を優先する
- どちらにも効く補助は共通化する
- 片方にしか効かない補助は設定を分ける

### Phase 2 の完了条件
- [x] 主要 widget の一部で、利き手に応じた配置や補助が反映される
- [x] 視認性補助が少なくとも 1 つ以上の UI に適用される
- [x] 左利き向けと障碍者向けの共通・個別が整理される

Phase 2実装完了（ランタイム検証 pending）。

## 2026-07-25 実装監査

共通設定の保存・復元、`Accessibility` ヘルパー、メニューバーの左利き時 RTL、タイムライン／Property Editor の大きいヒットターゲット・高コントラスト・色覚補助・Accessible Name/Description は実装を確認できる。`ArtifactProjectView`、`ArtifactInspectorWidget`、`ArtifactCompositionGraphWidget`、`ArtifactStatusBar`、`ArtifactCompositionAudioMixerWidget`、`ArtifactAudioMixerWidget`、`ArtifactTimelineTrackPainterView`、`ArtifactLayerPanelWidget`、`ArtifactAssetBrowser`、`ArtifactPropertyWidget`、`ArtifactPropertyEditor`、`ArtifactPropertyEditorAnimatorCount`、`ArtifactCompositionRenderWidget`、`ArtifactRenderLayerWidgetv2`、`ArtifactRenderQueueManagerWidget`、`ArtifactPlaybackControlWidget`、`ArtifactDebugConsoleWidget`、`EventBusDebuggerWidget` の主要コンテキストメニューは `adjustContextMenuPosition()` を使って左利き時の出現位置を補正する。主要 widget 全体の左右配置やショートカット再配置は未実装である。したがって Phase 1〜2 は設定と一部 widget 適用まで、Phase 3 の横断統合は未完了とする。実際の設定変更後の UI 更新、RTL レイアウト崩れ、支援技術からの読み上げは runtime 未確認。

---

## Phase 3

Phase 3 では、より広い範囲へ展開し、既存の操作習慣を壊さない範囲で完成度を上げる。

### 1. 設定全体の統合
- アクセシビリティ設定を 1 箇所に集約する
- 左利き / 視認性 / 入力補助の関連項目を整理する
- 将来の設定追加に耐える構成にする

### 2. 操作導線の横断最適化
- タイムライン、プロパティ、レンダー、メニューの補助を揃える
- 補助 UI の出方を widget ごとにばらつかせすぎない
- 同じ意味の操作が別 widget で矛盾しないようにする

### 3. 追加候補
- ショートカットの再配置プリセット
- 色覚補助
- フォントサイズ補助
- マウス追跡量やホバー依存を減らす補助

### Phase 3 の完了条件
- アクセシビリティ関連の補助が、個別機能ではなく一つのまとまった体験として扱える
- 左利きユーザー向けの使いやすさが、主要画面で一貫している
- 障碍者向け補助が、将来の拡張を阻害しない形で整理されている

---

## 対象ファイル一覧

| 区分 | ファイル | 変更内容 |
|---|---|---|
| **変更候補** | `Artifact/src/Widgets/ArtifactMainWindow.cppm` | 補助 UI の配置切替、メニュー出し位置の基点調整 |
| **変更候補** | `Artifact/src/Widgets/ArtifactMenuBar.cppm` | メニュー重心の調整 |
| **変更候補** | `Artifact/src/Widgets/Render/ArtifactCompositionRenderWidget.cppm` | ヒット領域や補助表示の調整 |
| **変更候補** | `Artifact/src/Widgets/Timeline/ArtifactTimelineTrackPainterView.cpp` | ハンドル、操作領域、視認性の補助 |
| **変更候補** | `Artifact/src/Widgets/ArtifactPropertyWidget.cppm` | 折りたたみ、操作密度、補助表示 |
| **変更候補** | `Artifact/src/Service/ArtifactProjectService.cppm` | アクセシビリティ設定の永続化 |
| **新規候補** | `Artifact/include/Settings/AccessibilitySettings.ixx` | 利き手・補助設定の共通定義 |
| **新規候補** | `Artifact/src/Settings/AccessibilitySettings.cppm` | 設定実装 |

---

## 実装メモ

- 左利き向けは「左右を反転する」より、「操作頻度の高いものを近づける」方が実用的
- 障碍者向け補助は、表示補助と入力補助を分けると拡張しやすい
- 既存の `QTimer` や `singleShot` を増やす前に、まず配置と操作導線の整理を優先する
- ショートカットの大規模再編は副作用が大きいので、独立マイルストーン化を検討する

---

## 非目標

- UI を完全に左右反転するだけの単機能実装
- すべての widget に一律のアクセシビリティ改修を一度に入れること
- キーボードリマップや OS レベル設定の全面代替
- 視覚支援・入力支援・利き手補助を無関係な機能として分断すること

---

## 完了条件

- 左利き向けの補助が、単発の UI 修正ではなく設定可能な機能として扱える
- 障碍者向け補助の検討項目が、将来の拡張先としてドキュメント化されている
- どの widget で何を直すかの責務境界が把握できる

---

## Next Execution Slice

### Phase 1A の着手点

- `Left-handed mode / High-contrast assist / Reduce precision dependence / Larger hit targets` を共通設定として先に固定し、保存・復元の土台だけ作る
- メニュー出し位置とヒット領域拡張は、`ArtifactMainWindow` / `ArtifactMenuBar` / `ArtifactCompositionRenderWidget` のような主要導線から順に当てる
- 左利き補助は「左右反転」ではなく「操作頻度の高いものを近づける」方針を最初に明示する

### Phase 1B の着手点

- `ArtifactTimelineWidget` と `ArtifactPropertyWidget` を先に見直し、ホバー依存を減らす補助を優先する
- 高コントラスト補助は、色だけでなく境界・ラベル・状態表示の順に反映する
- 既存 UI を壊さないよう、補助の有効/無効を widget 側で参照するだけの薄い契約にする

### Phase 2 前提

- ショートカット再配置や大規模リマップは別マイルストーンとして扱う
- 左利き補助と障碍者向け補助は、共通設定と個別設定を分けてから拡張する
- 新規 global signal / slot は増やさず、既存設定保存経路を流用する

## Static audit follow-up (2026-07-25)

- `Settings.Accessibility` and `ApplicationSettingDialog` provide persisted handedness, large-target, high-contrast, font-scale, color-deficiency, and reduced-hover-dependency settings.
- The timeline, Property Editor, Composition View/Controller, Main Window, and Menu Bar statically consume portions of those settings; accessible names/descriptions and scaled hit targets are present.
- `adjustContextMenuPosition()` exists but no caller was found, and a repository-wide scan did not confirm broad left-handed placement or shortcut remapping. Phase 3 cross-widget consistency therefore remains incomplete.
- Runtime setting changes, RTL layout integrity, and screen-reader announcements remain unverified. No build or runtime verification was performed under the repository policy.
