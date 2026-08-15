# MILESTONE: Smart Fallbacks

日付: 2026-06-07

**最終更新:** 2026-08-15

## Update 2026-08-15

- `FallbackPolicy`／`FallbackTracker` はカテゴリ別 action、fallback value、warning、イベント履歴を持ち、Font／Image／Color／Effect の主要経路で記録される。`FallbackDiagnosticsPanel` はイベント一覧、カテゴリ filter、clear を提供する。
- 画像 layer の missing／readback 経路、未知 effect の bypass、無効 color token の magenta、font 解決の fallback を現行コードで確認できる。レンダー側にも software／CPU／RAM preview 等の個別 fallback reason がある。
- Asset loading 全体への統一 policy 適用、プロジェクト設定からの policy 編集、export 前の missing／fallback 集約は未完了または未確認。判定は Phase 1 完了、Phase 2 部分実装、Phase 3 表示まで実装済みだが運用機能未完了を維持する。

フォント、画像、色、エフェクトが見つからないときに、安全で説明可能な代替ルールへ落とし込む。

## Goal

`Font missing -> Noto Sans JP`、`Image missing -> placeholder`、`Color token missing -> magenta warning`、`Effect unsupported -> bypass + warning` のような graceful degradation を統一する。

## Non-Goals

- 失敗を隠して静かに無視する設計にはしない
- 既存のアセット解決を無条件で置き換えない
- 低レベル render path を広く変更しない
- 新規の global signal-slot 経路を増やさない

## Core Concept

- missing asset は「見つからない」ことを明示する
- fallback は静かな代替であり、必要なら warning を残す
- bypass 可能なものは処理を止めずに進める
- 代替先をプロジェクト設定で調整できるようにする

## Typical Fallback Rules

- font missing
  - `Noto Sans JP` などの安全な代替へ切り替える
- image missing
  - placeholder を表示する
- color token missing
  - magenta warning を返す
- effect unsupported
  - bypass して warning を残す

## Why It Matters

- 本格ツールでは「壊れない」ことが重要
- プラグイン差や環境差があっても編集体験を維持しやすい
- export 失敗の前に warning を出しやすい
- `Sandbox Edits` や `Export Matrix` と組み合わせると不具合の見え方がよくなる

## Phase 1: Fallback Policy

目的: 代替ルールを統一する。

- タイプごとに fallback を定義する
- warning の出し方を揃える
- bypass できるケースを明示する

確認観点:

- 同じ missing には同じ fallback が返る
- warning が残る
- 代替の有無が追える

## Phase 2: Asset and Style Resolution

目的: 実際の解決経路に fallback を差し込む。

- font resolve
- image resolve
- color token resolve
- effect resolve

確認観点:

- missing でも編集や preview が継続できる
- 代替先の変更ができる
- warning が追跡できる

## Phase 3: Diagnostics

目的: 何が代替されたかを見える化する。

- warning log
- missing asset list
- fallback reason

確認観点:

- 何が missing だったか後から追える
- export 前に問題を集約できる
- user が意図的に fallback を調整できる

## Implementation Notes

### 2026-07-25 実装監査

`FallbackPolicy`／`FallbackTracker` の型、カテゴリ別イベント記録、font／image／effect／color の主要 fallback と Diagnostics Panel は実装を確認した。未着手として記載されている Asset loading 経路への統合と、policy をプロジェクト設定から変更する UI は引き続き未実装である。したがって Phase 1 は完了、Phase 2 は主要4カテゴリの部分実装、Phase 3 はイベント表示まで実装済みだが export 前集約・設定編集は未完了とする。

### 実装済み (Phase 1 + Phase 2 一部)

- `Core.Diagnostics.FallbackPolicy` — コア型
  - `FallbackCategory` enum: Font / Image / Color / Effect / Asset / Other
  - `FallbackAction` enum: Fallback / Bypass / Warning / Strict / Ignore
  - `FallbackEvent` struct: timestamp / category / action / originalId / resolvedId / message
  - `FallbackPolicy` struct: action / fallbackValue / warningMessage / enabled / logWarning
    - `defaultFont()`, `defaultImage()`, `defaultColor()`, `defaultEffect()` 静的ファクトリ
  - `FallbackTracker` singleton: イベント記録 / カテゴリ別取得 / 警告管理 / ポリシー設定
  - `tryFallbackPolicy<T>()` ヘルパーテンプレート

- **Font resolution** (`Font.FreeFont`): `resolvedFamily()` が FallbackTracker 経由でフォールバックを記録するように変更
- **Effect resolution** (`Graphics.Effect.Creative.Factory`): 未知のエフェクト名で bypass + warning を FallbackTracker に記録
- **Color palette** (`Artifact.Color.Palette`): 無効なカラートークンが FallbackTracker 経由で magenta 代替を記録

### ファイル一覧

| ファイル | モジュール |
|---|---|
| `ArtifactCore/include/Diagnostics/FallbackPolicy.ixx` | `Core.Diagnostics.FallbackPolicy` |
| `ArtifactCore/src/Diagnostics/FallbackPolicy.cppm` | (実装) |
| `ArtifactCore/include/Font/FreeFont.ixx` | `Font.FreeFont` (変更) |
| `ArtifactCore/include/Graphics/Effect/CreativeEffectFactory.ixx` | `Graphics.Effect.Creative.Factory` (変更) |
| `Artifact/src/Color/ColorPaletteManager.cppm` | `Artifact.Color.Palette` (変更) |

### 追加実装
- **Image layer fallback** (`Artifact.Layer.Image`): `loadFromPath()` と `toQImage()` のフォールバックパスで FallbackTracker に記録
- **FallbackDiagnosticsPanel** (`Artifact.Widgets.Diagnostics.FallbackPanel`): フォールバックイベントを TreeWidget で一覧表示、カテゴリフィルター、クリア機能
  - `Artifact/include/Widgets/Diagnostics/FallbackDiagnosticsPanel.ixx`
  - `Artifact/src/Widgets/Diagnostics/FallbackDiagnosticsPanel.cppm`

### 未着手
- Asset loading 経路への FallbackPolicy 導入
- FallbackPolicy のプロジェクト設定での調整 UI

## Integration Notes

- `Smart Fallbacks` は `Collision-Aware Layout` や `Named Guides` と独立していてよい
- `Export Matrix` と組み合わせると、出力先ごとの不足を安全に処理できる
- `Sandbox Edits` で fallback 前後の見え方を比較しやすい
