# UI Scriptability And Adaptive Surface Milestone

**最終更新:** 2026-08-15

## Static Audit (2026-08-15)

現状は `WorkspaceAutomation`／`CommandIR` による検証付きの操作 API、workspace の保存・復元、既存 widget の theme token／`QPalette` 利用、context shortcut helper などの部品は存在する。一方、このマイルストーンが求める「UI surface を定義から再構成する」統合モデルは確認できない。

- Scriptable surface: panel/tool/command/layout/theme を共通 JSON 定義から登録し、ユーザー作成 surface を既存 shell に差し込む registry は未確認。WorkspaceAutomation は UI 構成 API ではなく、主に project/layer/playback/render の command facade。
- Contextual keymap: shortcut helper や個別 action は存在するが、Timeline／Composition Viewer／Layer List／Inspector の優先順位・衝突解決・context 別再割り当てを一元管理する keymap model/editor は未確認。
- Timeline density: タイムラインには個別の列・表示状態があるものの、`compact/standard/detailed` preset、列順・最小幅・表示責務を共通 policy として保存する機構は未確認。
- High DPI/theme: SVG 資産、theme token、`QPalette`/owner-draw の方針は現行実装と整合するが、1x/1.5x/2x の検証、全主要 surface の統一、high-contrast/color-safe/large-hit-target の選択可能な preset は未確認。

判定: 設計方針と周辺部品は部分的に存在するが、Phase 1 の共通 surface schema／registryからPhase 5のアクセシブル presetまで未達。既存 command facade を UI 定義 registry と同一視しないことが重要である。

> 2026-06-07 作成

## 目的

`Artifact` の UI を、固定されたパネル配置とツール列の集合から、
**ユーザーが構造・操作・見た目を自分で再構成できる制作環境**へ進める。

このマイルストーンは、単なるレイアウト保存やテーマ切替ではなく、
以下を同時に扱う。

- パネルやツールをスクリプトまたは宣言的定義で組み立てられること
- ショートカットをコンテキストごとに再定義できること
- タイムラインの情報密度を列単位で調整できること
- 4K / 高 DPI / アクセシビリティ前提のスケーリングとテーマを標準装備すること

## 現状との差分

- いま実装されている中心は `WorkspaceMode` の切替と `ArtifactWorkspaceManager` による保存/復元
- `scriptable surface` や `contextual keymap` はまだ構想段階
- タイムラインの列密度制御も、ここで書いている粒度までは未到達
- この文書は現行実装の仕様書ではなく、拡張ロードマップとして扱う

## 背景

現状の UI は、dock と個別 widget の組み合わせとしては十分に伸びている一方で、
ユーザーが自分の作業様式に合わせて「道具箱そのもの」を組み替える自由度がまだ弱い。

特に不足しているのは次の 4 点。

- 細かなツールバーやパネル構成を外部定義できない
- ショートカットの再割り当てがコンテキスト単位で十分ではない
- タイムラインの `switch / mode / parent-link` などの列を個別に隠せない
- 高 DPI 環境で、アイコンの小ささと拡大時のぼやけの両方が残る

## Goal

- UI パネル、ツール、コマンド、ショートカットをデータ駆動で構成できるようにする
- タイムラインの表示密度を列単位で調整できるようにする
- `Timeline / Composition Viewer / Layer List` で独立した keymap context を持てるようにする
- `dark / high-contrast / color-safe` を標準テーマとして扱えるようにする
- アイコンとレイアウトの高 DPI 対応を、拡大ではなくネイティブ解像度とベクター資産で支える

## Scope

- `Artifact/src/Widgets`
- `Artifact/src/Widgets/Timeline`
- `Artifact/src/Widgets/PropertyEditor`
- `Artifact/src/Widgets/Menu`
- `Artifact/src/Widgets/Dock`
- `Artifact/src/Service`
- `Artifact/src/Tool`
- `docs/WIDGET_MAP.md`
- `docs/planned/MILESTONE_UI_THEME_SYSTEM_2026-03-30.md`
- `docs/planned/MILESTONE_TOOLBAR_APP_INTEGRATION_2026-04-17.md`
- `docs/planned/MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md`
- `docs/planned/MILESTONE_TIMELINE_VISUAL_LANGUAGE_2026-03-31.md`

## Non-Goals

- 既存 UI を HTML/CSS/JS で全面置換すること
- QtCSS を新規追加して見た目を寄せること
- 新しいグローバル signal / slot を増やすこと
- サブモジュール側へこの方針を押し出すこと

## Design Principles

- UI は「見た目」ではなく「構成可能な surface」として扱う
- command、tool、panel、shortcut は共通の名前空間で結ぶ
- widget ごとのローカル都合より、context と state を正本にする
- 低 DPI の小さいアイコンを前提にしない
- 重要な情報列は、常時表示ではなく可視化ポリシーで制御する
- アクセシビリティは例外ではなく標準プリセットとして持つ

## Functional Requirements

### 1. Scriptable Surface Definition

- パネルやツールバーの構成を、コード固定ではなく定義可能にする
- 少なくとも次を分離する
  - layout definition
  - tool definition
  - command binding
  - visibility policy
  - theme selection
- ユーザーが自作したパネルや補助ツールを、既存 shell に差し込める入口を用意する

### 2. Contextual Keymap

- `Timeline`
- `Composition Viewer`
- `Layer List`
- `Inspector`

  それぞれで独立した shortcut context を持てるようにする

- 同じキーでも context によって別 command を割り当てられるようにする
- キーバインドの衝突は、優先順位とスコープで解決できるようにする
- UI 上から keymap の確認と再割り当てができるようにする

### 3. Timeline Density Controls

- `switch / mode / parent-link / blend / track options` などの列を個別に表示制御できるようにする
- 列の順序変更と最小幅調整を持たせる
- 密度プリセットを少なくとも次の 3 段階で用意する
  - compact
  - standard
  - detailed
- 表示オフでも編集責務が壊れないようにする

### 4. High DPI And Icon Policy

- SVG 優先のアイコン資産を前提にする
- `1x / 1.5x / 2x` を個別画像で増やすのではなく、スケール対応可能な描画を基本にする
- アイコンサイズは theme token と density preset から決める
- ぼやける拡大ではなく、適切な logical size で再描画する

### 5. Accessible Theme Set

- `dark`
- `high-contrast`
- `color-safe`
- `large-hit-target`

  を標準テーマ候補として扱う

- テーマは配色だけでなく、余白、境界線、フォーカス可視性、選択表現も含めて切り替える
- `QPalette` と owner-draw を基本にし、`QSS` 依存を増やさない

## Phases

### Phase 1: Surface And Command Model

- 目的:
  - UI の構成単位を固定しすぎないための共通モデルを作る

- 作業項目:
  - panel / tool / command / shortcut の責務を整理する
  - scriptable 定義の最小スキーマを決める
  - 既存 widget を壊さず差し込める拡張点を決める

- 完了条件:
  - どの UI 要素が command を持ち、どれが state view か説明できる
  - 既存の toolbar / menu / timeline と矛盾しない

### Phase 2: Contextual Shortcut Layer

- 目的:
  - shortcut をコンテキスト別に再定義できるようにする

- 作業項目:
  - `Timeline / Composition Viewer / Layer List` の context を整理する
  - keymap の優先順位と衝突解決を定義する
  - shortcut editor の UI 入口を設ける

- 完了条件:
  - 同じキーに複数の意味を持たせられる
  - context 切替時に stale binding が残らない

### Phase 3: Timeline Density And Column Visibility

- 目的:
  - タイムラインの情報密度をユーザーが調整できるようにする

- 作業項目:
  - 列の表示切替を実装する
  - compact / standard / detailed を定義する
  - ヘッダと row のレイアウトが崩れないようにする

- 完了条件:
  - `switch / mode / parent-link` を個別に隠せる
  - 低解像度でも高解像度でも破綻しない

### Phase 4: High DPI And Icon Refresh

- 目的:
  - 4K / 高 DPI での見づらさを減らす

- 作業項目:
  - icon サイズと spacing を density token 化する
  - SVG ベースの描画ポリシーを統一する
  - hover / active / disabled の状態差を読みやすくする

- 完了条件:
  - 拡大時にぼやけた印象が減る
  - 主要 surface のアイコンが同じルールで読める

### Phase 5: Accessible Theme Presets

- 目的:
  - 標準テーマをアクセシビリティ前提で揃える

- 作業項目:
  - high-contrast と color-safe のプリセットを定義する
  - focus ring、選択色、警告色を見直す
  - theme 切替で既存操作系が壊れないことを確認する

- 完了条件:
  - dark 以外の標準テーマを選べる
  - 色だけに依存しない状態差がある

## Related Milestones

- `docs/planned/MILESTONE_UI_THEME_SYSTEM_2026-03-30.md`
- `docs/planned/MILESTONE_UI_THEME_SYSTEM_ROLLOUT_2026-04-02.md`
- `docs/planned/MILESTONE_TOOLBAR_APP_INTEGRATION_2026-04-17.md`
- `docs/planned/MILESTONE_SHORTCUT_CONTEXT_MAP_2026-04-21.md`
- `docs/planned/MILESTONE_SHORTCUT_CUSTOMIZATION_2026-04-10.md`
- `docs/planned/MILESTONE_TIMELINE_VISUAL_LANGUAGE_2026-03-31.md`
- `docs/planned/MILESTONE_TIMELINE_LAYER_SPECIALIZATION_2026-04-23.md`
- `docs/planned/MILESTONE_ACCESSIBILITY_2026-03-28.md`

## Acceptance Checklist

- UI 構成をデータ駆動で扱う方針が説明できる
- ショートカットのコンテキスト分離が明確になっている
- タイムライン列の表示制御が情報密度調整として成立している
- 4K / 高 DPI で破綻しない icon policy がある
- dark 以外の標準テーマが計画に含まれている

## Next Step

このマイルストーンを実装へ進める前に、まずは `command / context / layout / theme` の最小スキーマを確定し、
既存の `toolbar`, `menu`, `timeline` にどこまで共通化を入れられるかを見極める。

## Minimal Schema Draft

最初の実装では、完全なスクリプトランタイムを目指さず、
**定義ファイルで UI を再構成できる最小単位**から始める。

### 1. Surface Definition

```json
{
  "surfaceId": "timeline.main",
  "title": "Timeline",
  "kind": "panel",
  "region": "Workspace.Timeline",
  "visibility": "visible",
  "density": "standard"
}
```

---

## Next Execution Slice

### Phase 1A の着手点

- `surfaceId / kind / region / visibility / density` を UI 構成の最小スキーマとして先に固定し、toolbar / menu / timeline で共通に読める形にする
- command 定義は `type / label / target / args` の薄い形から始め、実行本体は既存の command path に寄せる
- scriptable surface は runtime を作り込まず、まずは declarative 定義を既存 widget に差し込める入口だけを作る

### Phase 2A の着手点

- shortcut context は `Timeline / Composition Viewer / Layer List / Inspector` の 4 つから始め、同一キーの意味を context で分ける
- conflict resolution は優先順位と scope だけを先に決め、UI 上の編集画面は後回しにする
- stale binding を避けるため、context 切替時の再解決経路を既存 shortcut registry に閉じる

### Phase 3 前提

- timeline density は `compact / standard / detailed` の 3 段階を先に固定し、列単位の可視化はその後に広げる
- high DPI と theme は、surface / shortcut の共通モデルが安定してから token 化する
- 新しい global signal / slot を増やさず、既存の toolbar / menu / timeline の責務境界を壊さない

- `surfaceId`
  - UI surface を一意に識別する
- `title`
  - 表示名
- `kind`
  - `panel / toolbar / popup / inspector / overlay` など
- `region`
  - keymap と layout が参照する context 名
- `visibility`
  - `visible / hidden / collapsed`
- `density`
  - `compact / standard / detailed`

### 2. Command Definition

```json
{
  "commandId": "timeline.toggleParentColumn",
  "label": "Toggle Parent Column",
  "category": "Timeline",
  "surface": "timeline.main",
  "shortcut": "Alt+P"
}
```

- `commandId`
  - command の正規名
- `label`
  - UI 表示名
- `category`
  - menu や shortcut editor の分類
- `surface`
  - 主な対象 surface
- `shortcut`
  - デフォルト割り当て

### 3. Shortcut Binding

```json
{
  "context": "Panel.Timeline.Left",
  "bindings": [
    { "key": "Delete", "commandId": "timeline.removeLayer" },
    { "key": "M", "commandId": "timeline.toggleMute" }
  ]
}
```

- `context`
  - `Workspace / Panel / Modal` の解決単位
- `bindings`
  - その context で有効な割り当て

### 4. Theme Preset

```json
{
  "themeId": "high-contrast",
  "palette": "high-contrast",
  "density": "standard",
  "iconScale": 1.0,
  "focusRing": true
}
```

- `themeId`
  - プリセット名
- `palette`
  - 実際の色定義セット
- `density`
  - spacing / icon / row height の基準
- `iconScale`
  - logical size の倍率
- `focusRing`
  - フォーカス可視性の有効化

### 5. Timeline Column Policy

```json
{
  "surfaceId": "timeline.left",
  "columns": [
    { "id": "name", "visible": true, "width": 220 },
    { "id": "switch", "visible": false, "width": 44 },
    { "id": "mode", "visible": true, "width": 60 },
    { "id": "parentLink", "visible": false, "width": 72 }
  ]
}
```

- `id`
  - 列の識別子
- `visible`
  - 表示切替
- `width`
  - 最小または初期幅

## Schema Rules

- JSON はまず人間が読めることを優先する
- command と shortcut は 1 対 1 に固定しない
- surface 定義は widget 実装に直接依存しない
- context 名は `docs/WIDGET_MAP.md` の命名に合わせる
- theme と density は分離可能にする
- column visibility は表示だけを切り替え、編集責務を消さない

## Source Alignment Notes

- 実装の起点は [Artifact/src/Widgets/ArtifactMainWindow.cppm](/X:/Dev/ArtifactStudio/Artifact/src/Widgets/ArtifactMainWindow.cppm)
- workspace 保存/復元は [Artifact/src/Core/ArtifactWorkspaceManager.cppm](/X:/Dev/ArtifactStudio/Artifact/src/Core/ArtifactWorkspaceManager.cppm)
- メニュー統合は [Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm](/X:/Dev/ArtifactStudio/Artifact/src/Widgets/Menu/ArtifactViewMenu.cppm)

この文書のうち、現状でコードに近いのは `High DPI` と `theme` の方向性で、
`scriptable surface` と `contextual keymap` はまだ設計要求として残っている。

## 現行コード監査 (2026-08-15)

`WorkspaceAutomation`／`CommandIR` の検証付き command facade、workspace 保存・復元、theme token／`QPalette`、個別 shortcut helper は現行コードで確認できる。一方、surface registry、定義からの panel／layout 再構成、context 別 keymap model、timeline density／column policy の共通保存、1x／1.5x／2x・high-contrast の runtime parity は確認できない。従って基盤部品は実装済みだが、scriptable surface と adaptive policy の統合 milestone は未完了とする。
