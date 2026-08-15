# Milestone: Motion Graphics Template System (mogrt-like)

**最終更新:** 2026-08-15
**ステータス:** 部分実装（TemplateSlot／TemplateLock／公開プロパティ接続、ArtifactTemplateDocument の JSON 保存契約あり。抽出・配置・ライブラリ・完全な入出力は未完了）

作成日: 2026-06-01
親: `MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md` (Essential Graphics / Motion Graphics)
関連: `MILESTONE_AE_FEATURE_ENHANCEMENT_ROADMAP_2026-04-12.md` (.mogrt 相当), `MILESTONES_BACKLOG.md`

---

## 目的

AE Essential Graphics のモーショングラフィクステンプレート（`.mogrt`）に相当する
「再利用可能なアニメーション・アセットブロック」を Artifact 上で作成・配置・カスタマイズ
できるようにする。既存の `ArtifactEffectPreset` とレイヤー構造を土台に
「テンプレート」を定義し、インポート/エクスポート可能にする。

---

## 既存の土台

- `ArtifactEffectPreset` — effect stack の保存／再適用
- `ArtifactPresetManager` — preset の管理 singleton
- `ArtifactTextLayer` + `TextAnimatorEngine` — text animator 実装済み
- `ArtifactCompositionLayer` — ネストしたコンポジットの配置
- `ArtifactPropertyWidget` preset 保存／読込 UI
- `ArtifactCore/src/Script/Expression/` — expression によるパラメータ制御
- `AFTER_EFFECTS_PARITY_COMPARISON_2026-05-30.md` — mogrt 要求仕様あり

---

## 未着手要素

- 「テンプレート」オブジェクトのシリアライズ形式 (.artemplate 拡張子等)
- ソースレイヤーを block として分解し、`入力`（編集可能パラメータ）と
  `固定アニメーション`（timeline keyframe 群）を定義する
- 配置時にパラメータを Inspector 経由で上書きできる UI
- 複数の mogrt-like template を横断検索するブラウザ/ライブラリ
- .mogrt との互換読込（最低限'unzip → 解析 → import'）を Phase x で検討

---

## フェーズ

### Phase 1: Template データモデル

新規ファイル:
- `ArtifactTemplateDocument` — テンプレート定義のルート
  - exposedParams: `[{ name, type, defaultValue }]`
  - layerSnapshot: `ArtifactLayer` のツリーを JSON で保持
  - keyframeSnapshot: 対象 timeline の keyframe 群を保持
  - animationClip: テンプレート固有の時間範囲（任意）
- `ArtifactTemplateManager` — テンプレートの CRUD
- 既存 `ArtifactCompositionManager` と独立に `ArtifactProjectManager` 下に
  `Templates/` フォルダを置き、`.artemplate` で保存

### Phase 2: Import / Export

- 新規メニュー: File → Import Motion Template… / Export As Motion Template…
- Export: 現在のレイヤー選択 + keyframe 範囲 → `ArtifactTemplateDocument`
  - exposedParams として Inspector から編集されていた property を抽出
- Import: `.artemplate` を composer にロードし、新規レイヤーツリーとして追加
- Undo/Redo は `AddLayerCommand` のバリエーションで対応

### Phase 3: Inspector 入力カスタマイズ

- テンプレート配置後、Inspector に「Template Parameters」セクションを表示
- パラメータ種別:
  - Scalar / Point2D / Color / Bool / Text / Dropdown(enum)
- パラメータ変更 → template 内の紐付け property に伝播
- キーフレームを上書きした場合、テンプレート root を「break off」として
  独立レイヤーへ展開する操作を提供

### Phase 4: Template Library Browser

- `ArtifactTemplateBrowser` widget（Asset Browser とタブで切替可能）
- カテゴリ / タグ / サムネイル（静止フレーム抽出）表示
- ドラッグ＆ドロップで Composition へ配置
- カスタムテンプレートの追加・削除・お気に入り

### Phase 5: .mogrt 互換読込（option）

- After Effects の mogrt は unzip パッケージで、内部に .aex 相当と
  JSON ヘッダ + サムネイル
- Artifact では .mogrt を unzip → 最低限のレイヤー構造 + パラメータ定義を
  抽出し `ArtifactTemplateDocument` へ変換
- 実際の AE エフェクト実体は再現不可のため「param + keyframe + layer tree」のみ mapper

## Update 2026-08-15

- `ArtifactCore::TemplateSlot`／`TemplateLockSchema` は JSON serialization／migration、編集可能フィールド判定、lock guard を持ち、`TemplateLockEditorWidget` から設定・読み込み・保存できる。
- `ParametricComposition` には template slots／slot values の JSON 変換があり、Master Properties の exposed property／instance override と接続する基盤も存在する。したがって旧「テンプレート基盤が未着手」という記述は現行コードと一致しない。
- 一方、計画書の `ArtifactTemplateDocument`／`ArtifactTemplateManager`、`.artemplate` 専用保存形式、選択レイヤーと keyframe の完全な export／import、`ArtifactTemplateBrowser`、タグ／サムネイル検索、`.mogrt` 互換読込は確認できない。
- 判定は **TemplateSlot／TemplateLock／Master Properties 接続は部分実装、mogrt-like template の作成・配布・ライブラリ workflow は未完了** を維持する。ビルド・テスト・runtime 確認は未実施。
- 2026-08-15: `Artifact.Template.Document` に `ArtifactTemplateDocument` の最小データモデルを追加し、id／name／sourceLayerIds／exposedParameters／layerSnapshots／metadata／TemplateLockSchema の JSON 保存・読込を登録した。`fromLayers()` は各 layer の既存 JSON と `masterProperties` を抽出し、`instantiateLayers()`／`appendToComposition()` で復元・配置できる。`ArtifactTemplateParametersWidget` は document の設定・取得に対応し、Inspector の Template 専用 tab から現在 layer の公開パラメータを表示・編集できる。`ArtifactTemplateLibrary` で `.artemplate` の保存・列挙・読込・削除を追加し、`ArtifactTemplateLibraryWidget` で一覧更新と選択テンプレートの読込 UI も追加した。Asset Browser の中央コンテンツには Assets／Templates タブを追加し、テンプレート一覧を実導線へ接続した。テンプレート項目のダブルクリックで現在 Composition へ layer を追加し、`AddLayerCommand` を通じて Undo 履歴へ登録する処理も実装した。テンプレート一覧から `application/x-artifact-template` の DnD payload を出し、Composition Editor 側で受けて現在 Composition へ Undo 付きで配置できるようにした。実素材での互換性は次段階。
- mogrt 読込については、`ArtifactCore/include/third_party/miniz.h` は存在するが、現行 Artifact 側に ZIP reader の利用箇所は確認できなかった。依存境界を確認せずに新しい ZIP 実装を追加する段階ではないため、`.mogrt` の unzip／manifest mapping は未実装として維持する。

---

## 検証条件

- 文字 + wiggle animator + drop shadow のシーケンスを
  "Lower Third" テンプレートとして保存 → 再読み込み → 同じ keyframe が復元
- Inspector から text / color / position の 3 params を露出し、
  配置後に値を変えられる
- テンプレートを Composition に複数配置し、params が独立に作用する
- Library Browser で "Lower Third" / "Call Out" の 2 カテゴリを検索・配置

---

## 関連ファイル（新規/変更）

- `Artifact/include/Template/ArtifactTemplateDocument.ixx` (新規)
- `Artifact/src/Template/ArtifactTemplateDocument.cppm` (新規)
- `Artifact/src/Template/ArtifactTemplateManager.cppm` (新規)
- `Artifact/src/Widgets/Template/ArtifactTemplateBrowser.cppm` (新規)
- `Artifact/src/File/ArtifactProjectPackager.cppm` ( Templates/ フォルダ追加)
- `Artifact/src/Widgets/PropertyEditor/ArtifactPropertyWidget.cppm` (Template params セクション)

---

## 見積

- Phase 1: 10–14h
- Phase 2: 8–12h
- Phase 3: 10–16h
- Phase 4: 12–18h
- Phase 5 (option): 14–20h

合計: 54–80h (Phase 5 省略時: 40–60h)

---

## Static audit follow-up (2026-07-25)

現行ソースには `TemplateSlot` / `TemplateLock` / `TemplateVariation`、Parametric Composition の template slot / slot value 変換、Master Properties 系の template lock・slot 接続が存在する。また既存の effect preset と property preset の保存・再適用基盤もある。

ただし、`ArtifactTemplateDocument` / `ArtifactTemplateManager`、`.artemplate` の独立保存・読込、選択レイヤーからの exposed parameter 抽出、Template Parameters 専用 Inspector、Template Browser、複数配置時の独立 override、`.mogrt` 互換読込は確認できない。既存の TemplateSlot は、このマイルストーンのモーショングラフィクステンプレート完成を直接証明しない。

### Audit status

- Phase 1: 部分実装 — template slot / lock の基盤はあるが、template document と manager は未確認
- Phase 2: 未実装相当 — Motion Template の専用 import / export route は未確認
- Phase 3: 部分実装 — slot / master property の override 基盤はあるが、専用 Inspector UI は未確認
- Phase 4: 未実装相当 — Template Browser / library / drag-and-drop は未確認
- Phase 5: 未実装相当 — `.mogrt` 解析・変換は未確認
- Definition of Done: このマイルストーンは未完了。既存の template slot / preset 基盤を再利用候補として保持する
