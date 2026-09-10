# Precompose 改善 B+F: パンくず移動と複製管理

**最終更新:** 2026-09-10

**ステータス:** Not Started

## 背景

AEのプリコンポーズ不満のうち、往復コスト (B) と複製・再利用事故 (F) を先行して解消する。残り (A/C/D/E) は検討事項として本書に留め、実装しない。

- 先行調査: `docs/planned/COMPOSITION_PRECOMPOSE_ANALYSIS_2026-04-17.md` (PreComposeManager の core 実装 TODO あり)
- 関連移行計画: `docs/planned/GROUP_CONTAINER_MIGRATION_PLAN_2026-08-27.md` (Phase 0/1 実装済み、Container 実体化は未着手)
- AE 不満の全体像: `docs/analysis/AE_PAIN_POINT_IMPROVEMENT_MAP_2026-08-13.md`
- Viewport 制約: `docs/design/composition-viewport/README.md` (キャンバス内は変更対象外、周辺 UI のみ採用)

## B. Breadcrumb + In-place 編集 (採用)

### B-1 パンくずナビゲーション

- Viewport 上部の描画領域外ヘッダーに `親 > 子 > 孫` パンくずを常設する。
- ダブルクリックで子へ潜る、`Shift+Esc` で親へ戻る。
- 各階層クリックで対象コンポジションへ切り替え、Timeline と Inspector の表示を同期する。
- 実装先の責務対応: `ArtifactCompositionEditor` が shell / 切替 routing、`ArtifactCompositionRenderWidget` は描画・直接操作に専念し、切替ロジックを持たない (`docs/WIDGET_MAP.md` 参照)。

### B-2 時刻・ズーム・選択の共有

- カレント時間・ズーム・選択レイヤーは親子で共有キャッシュし、往復でリセットしない。
- `framePosition` / zoom / selection を階層切替時に保存・復元する契約を定義する。
- Tracker 等のコントローラローカル状態は対象外 (切替で破棄する現行仕様を維持)。

### B-3 親ゴースト表示 (要別途判断)

- 子編集中の親半透明ゴースト・境界外表示はキャンバス内描画の変更になるため、本 milestone の既定範囲外とする。
- 採用モック (`docs/design/composition-viewport/README.md`) を根拠にせず、別途明示要求があれば独立スライスで検討する。

## F. 真の複製とインスタンス (採用)

### F-1 三択の標準化

- プリコン/グループの複製操作を以下に統一する:
  1. `Duplicate Deep` — 内容を完全分離複製
  2. `Instance (リンク)` — 中身編集が全参照に反映
  3. `Un-precompose` — 子内容を親へ展開して戻す (非破壊化)
- 誤って中身共有する True-Duplicate 事故をなくすため、デフォルト選択と確認表示を定義する。

### F-2 実装境界

- `PreComposeManager::precompose()` の未実装部分 (`COMPOSITION_PRECOMPOSE_ANALYSIS` §10) と `GroupContainer` 移行 (Phase 2 以降) に依存するため、まず契約定義から始める。
- 既存 `type: Group` JSON は読み込み互換を維持する (`GROUP_CONTAINER_MIGRATION_PLAN` の移行原則に従う)。
- Undo/Redo は既存 Undo 経路に載せ、新規グローバル signal は導入しない (AGENTS.md 制約)。

## 検討事項 (今回実装しない)

- **A. Non-destructive Group Comp** — Group Layer を内部 Composition として Expand/Collapse、属性移動ログ保持。`GROUP_CONTAINER_MIGRATION_PLAN` Phase 2-5 と統合検討。
- **C. 境界レス既定** — 内容 bounding box + margin 自動サイズ、`Fit to Content / Parent / Custom` 切替。GPU 優先・CPU readback 回避の制約あり。
- **D. 属性移動プレビュー** — Transform/Effect/Mask/Expr/Key の移動先チェックリスト、Collapse/連続ラスタライズ/ブレンド継続トグル。Timeline 左ペインは `Transform` のみに限定する制約と整合させる。
- **E. 時間ラッパー分離** — 子尺・fps 独立管理、親側は Clip 参照のみ。Time Remap の内外表示バッジは Timeline への集約バッジ追加禁止制約に注意。

## 非目標・制約

- キャンバス内の絵・選択枠・ハンドル・ギズモ・ガイド・HUD は変更しない。
- 新規シグナル&スロット接続、グローバルイベント配線を追加しない。
- QtCSS / `setStyleSheet()`、`QColorDialog`、`QImage` 新規採用、`QPainter` 新規合成をしない。
- ソフトレンダラーの新機能化をしない。GPU/Diligent 経路を優先する。
- 子リポジトリ (`Artifact` / `ArtifactCore` / `ArtifactWidgets`) の変更は本書の範囲外。必要になれば別途指示を受ける。
- ビルド・テスト・CMake 実行はユーザー許可後に行う。

## 受入条件

- [ ] B-1 パンくずの表示・潜る・戻る操作が定義され、対応 widget 責務が `WIDGET_MAP` と矛盾しない
- [ ] B-2 共有状態 (時間・ズーム・選択) の保存・復元契約が定義される
- [ ] F-1 三択の用語・デフォルト・確認表示が定義される
- [ ] F-2 既存 Group JSON 互換と Undo 経路利用の方針が明記される
- [ ] A/C/D/E が「検討」に留まり、実装着手条件が書かれる
- [ ] 付録のメニュー改善点が B/F と矛盾なく定義される
- [ ] 付録のタイムライン改善点が B/F と矛盾なく定義される

## 付録: AE 上部メニューバー不満と改善 (2026-09-10 追記)

AE 標準構成 `File / Edit / Composition / Layer / Effect / Animation / View / Window / Help` に対する不満と、本 milestone (B/F) との整合方針。本付録は定義・方針のみで、実装は B/F 本体を優先する。

### 全体構造

- 不満: 目的別に分散し、深い (`Pre-compose` は `Layer`、`Comp 設定` は `Composition`、`キーフレーム補助` は `Animation`、`ガイド` は `View`)。メニュー内検索が弱く、Effect は数百件のフラットリスト。よく使う項目の上出しができない。ショートカット変更導線が遠く、Blender 系 `G/R/S` と衝突しがち。Comp サイズ / fps / 色深度 / プレビュー解像度の現在状態が見えない。
- 改善方針: 横断検索のコマンドパレットを最優先の緩和策とする。`Recent / Favorite` を `File / Effect / Layer` 先頭に出す。`G/R/S/X/Y/Z/Shift/Ctrl/Esc/Enter` の予約確認なしに単一キーをハードコードしない。`ShortcutBindings` と workspace / tool context の衝突を先に調査する (AGENTS.md ショートカット整合ルール)。

### File

- 不満: `Incremental Save` / `Collect Files` / `Dependencies` が分散。自動保存・バージョン管理が弱い。
- 改善: `Collect + Relink + Health Check` を一箇所に寄せる。既存の `Project Health` / `AutoSave` への導線を `File` 直下に露出する。新規実装は作らない。

### Edit

- 不満: Undo が一直線で履歴パネルが弱い。`Duplicate` と中身を共有しない真の複製の区別がない。
- 改善: F 案の三択 (`Duplicate Deep / Instance / Un-precompose`) を `Edit` と `Layer` から同名で呼べるように統一する。History は既存 Undo 経路に載せ、新規グローバル signal を導入しない。

### Composition

- 不満: `Composition Settings` / `Preview` / `Background` が離れ、ネスト移動がメニューにない。
- 改善: B 案と連動し `Composition > Navigate` に `親 > 子` 移動を置く。`Shift+Esc で戻る` を明示する。`ArtifactCompositionEditor` が shell / 切替 routing を担い、`ArtifactCompositionRenderWidget` に切替ロジックを持たせない。

### Layer

- 不満: 最も肥大。`Pre-compose` / `Mask` / `Track Matte` / `Layer Style` / `3D` / `Time` が同列で優先度不明。
- 改善: 上位は `Transform` のみに限定し、Effect / Component 由来は下位・各専用面へ寄せる (`WIDGET_MAP.md` の Timeline 左ペイン制約と一致)。`Group / Un-group` と `Precompose` を隣接させ、F 案三択と同じ用語を使う。

### Effect

- 不満: サードパーティで埋もれる。お気に入り・最近使ったものが弱い。
- 改善: トップメニュー肥大化はしない。`Effect Browser + Recent + Favorite` ドックを正規導線とし、メニューはカテゴリへのショートカットのみにする。

### Animation

- 不満: `Add Key` / `Keyframe Assistant` / `Text Animator` / `Track Motion` / `Time Remap` が混在し、破壊的かどうか不明。
- 改善: `Add (非破壊)` と `Bake (破壊)` を用語で分ける。Timeline 左ペインへの集約バッジ追加はしない。

### View

- 不満: `Resolution` / `Guides` / `Safe Area` / `Transparency Grid` がプレビューと設定に分散し、Viewport ごとに違う。
- 改善: Viewport 下部の表示/再生コントロールへ寄せる。トップメニューはトグル列挙ではなくプリセット切替にする。キャンバス内描画・ギズモ・ガイド本体の変更はしない (`docs/design/composition-viewport/README.md` 遵守)。

### Window / Workspace

- 不満: ワークスペース保存が分かりづらく壊れやすい。モード別配置がない。
- 改善: `現在の配置をこのモードに保存` を優先し、モード → dockState マッピング永続化の方針で検討する。QADS blob と `DockLayoutDocument` の二重管理は一本化方向でのみ触る。

### 本 milestone との対応

| 付録項目 | 対応先 |
|---|---|
| Navigate メニュー | B-1 パンくずと同義語・同操作に統一 |
| 複製メニューの統一命名 | F-1 三択を Edit/Layer/右クリックで同一名に |
| コマンドパレット / Recent / Favorite | B/F の緩和策。独立スライス候補 |
| Window 配置保存 | 本 milestone 範囲外。別 milestone 候補 |

## 付録: AE タイムライン不満と改善 (2026-09-10 追記)

AE Timeline に対する定番不満と、本 milestone (B/F) との整合方針。本付録は定義・方針のみで、実装は B/F 本体を優先する。

### 重い・迷子になる

- 不満: 100 層超でスクロール・ズーム・選択が重い、現在行を見失う。Shy / Solo / Lock / 展開が多く、どれが表示に効いているか不明。Graph / Dope Sheet / Layer Bar 切替でコンテキスト喪失。
- 改善: 左 (`ArtifactLayerPanelWidget`) はレイヤー列と行操作に限定し、composition / preview 責務を持たせない。右の直接操作 owner (`Panel.Timeline.Right`: clip / keyframe / playhead / selection) を明示し、focus 中 context を崩さない。`Selected` 系の常時表示バッジは新規追加せず、行ハイライト・既存状態表示で表現する。

### キー操作が遠い

- 不満: キー移動・複数選択・補間変更のクリック数が多い。イージング調整が Graph Editor に潜らないとできない。`U` を知らないと埋もれる。
- 改善: 既存 Baseline (`Space`, `J/K/L`, `I/O`, `U=Keyframes Only`, `Tab=curve editor mode`) を壊さない。新規単一キーのハードコードをせず、Blender 系 `G/R/S/X/Y/Z/Shift/Ctrl/Esc/Enter` 予約と `ShortcutBindings` / workspace / tool context の衝突を先に調査する。

### 時間の二重管理

- 不満: Comp 尺・Work Area・RAM Preview 範囲・In/Out・Stretch・Time Remap がバラバラ。プリコン内外の時間表示ずれ。マーカーが Comp/Layer で別物。
- 改善: Comp 尺 / Work Area / Preview 範囲 / Clip 参照を集約表示し、編集は各専用面に寄せる。Time Remap 内外の Timeline 行バッジ化はしない (集約バッジ禁止制約に注意し、Inspector 側表示を優先)。

### 親子・ネストが見えない

- 不満: Parent リンクが 1 列プルダウンで切れやすい。プリコン内キーを親から触れない。Track Matte / Mask / Effect 参照が別列で追えない。
- 改善: B 案と連動し、親子 Timeline 切替はパンくずと同一操作・同一用語に統一する。時刻・ズーム・選択は共有し、往復でリセットしない。ネスト内キーの親からのぞき見は読取表示までとし、編集は潜ってから行う方針とする。F 案三択は右クリック・Edit・Layer で同一名に統一する。

### 検索・整理が弱い

- 不満: 名前検索・ラベル色・フィルタが貧弱。Audio 波形・連番サムネ・3D スイッチ同列で密度過剰。
- 改善: 標準プロパティグループは `Transform` のみに限定し、`Motion` / `Components` / 物理 / エフェクト / 素材 / ソース固有を左ペインに露出させない。マスク/マットは既存専用行のみ使う。検索・フィルタは左ペインの行操作に限定する。

### ホットパス制約

- スクロール・再生・scrub・フレーム更新・GPU コマンド構築では重い非ゼロ確保、フレーム毎の大容量確保・再確保・深いコピー・ロック付き汎用アロケータ・texture 再生成・大容量 readback・画像変換・プレビュー再生成を持ち込まない。
- 診断ログは category / 明示 flag で遅延評価し、`QString::arg()`・文字列連結・整形・JSON 化を先行実行しない。flush は失敗・終了・チェックポイント等の境界にまとめる。

### 本 milestone との対応

| 付録項目 | 対応先 |
|---|---|
| 親子 Timeline 切替 | B-1 パンくずと同一操作・同一用語 |
| 時刻・ズーム・選択共有 | B-2 保存・復元契約に従う |
| 複製三択の右クリック統一 | F-1 用語・デフォルトに従う |
| 時間表示集約・検索痩身 | 独立スライス候補。本 milestone では定義のみ |
