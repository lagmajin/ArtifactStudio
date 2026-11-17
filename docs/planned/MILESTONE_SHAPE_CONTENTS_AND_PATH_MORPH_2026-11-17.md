# M-SHAPE-CM-1: シェイプ Contents グループとパスモーフィング UI

**最終更新:** 2026-11-17

**ステータス:** In Progress（P1 は 2026-11-17 に実装・静的確認済み。ビルド／実機は未確認）

## 目的

シェイプレイヤーを「アニメーションできるベクター」として使い切るため、次の2点を完成させる。

1. パスキーフレームを UI と Timeline から設定できるようにする（パスモーフィング）
2. 1レイヤー内にネストした Contents グループと Group Transform を持たせる

新規にゼロから作る機能ではない。2026-11-17 の実コード照合で、評価系とフラットな複数コンテンツは既に実装済みと確認した。本マイルストーンは **既存資産への UI・モデル接続** と **ホットパス修正** が主軸である。

## 前提（2026-11-17 実コード照合の確定事実）

### 既に実装済み

- パス評価: `Artifact/src/Layer/ArtifactShapeLayer.cpp:819-902` の `evaluatePathAt()`。L864-895 で `ShapePath::interpolate()` と `sampleEquidistant()` によるトポロジ違いのモーフ（星→ハート等）が動作する。Core 側 `ShapePath::interpolate()` は `ArtifactCore/include/Shape/ShapePath.ixx:168` に存在する。
- パスキーフレーム保存: `setPathKeyframe()`（同ファイル :2932）、JSON キー `shape.path.keyframes`。
- フラットな複数コンテンツ: `Artifact/include/Layer/ArtifactShapeLayer.ixx:81-163` の `ShapeContent` / `ShapeStackNode{Path,Fill,Stroke,Operator}`。CRUD は同 cpp :4208-4272、スタック操作 :4574-4683、スナップショット :7465-7480。
- コンテンツ別 Transform: `shape.content.N.transform.*`（:6275-6292 / :6824-6846）。実質的に Group Transform に相当するが、ネストはしない。
- 階層モデルは Core 側に存在: `ArtifactCore/include/Shape/ShapeGroup.ixx` と `processedPaths()`（`ArtifactCore/src/Shape/ShapeGroup.cpp:254-286`）。ネスト変換の契約テストは親リポ `tests/ArtifactCore/ShapeGroupContractTest.cpp` にある。
- Operator 11種、Solo View 編集、GPU native 描画、SVG 入出力、Shape↔Mask 変換と live link も既存。

### 未実装・未接続

- `shape.path.keyframes` を設定する UI が無い（API と評価のみ）。Timeline へのproperty露出もなし。
- アプリ側に Group ノード／ネスト／グループ blend／名前・折りたたみが無い。`toCoreShapeLayer()`（同 cpp :3734-3910）は毎回フラットな `PathShape` へ変換しており、Core の階層を活かしていない。
- 真のブール演算は `resolveContentVisPaths()`（同 cpp :1623-1669）と Core `MergePaths::process()`（`ArtifactCore/include/Shape/AeOperators.ixx:188-225`）が QPainterPath に依存する。プロジェクトルール上新規拡大禁止。

### 既存ドキュメントとの食い違い

- `docs/planned/MILESTONE_SHAPE_SVG_EXPORT_AND_KEYFRAME_VERIFY_2026-08-22.md` は D-5 / D-6 / Phase E を「未着手」と記載するが、実コードでは D-5/D-6 相当とフラットな複数コンテンツが存在する。
- `docs/analysis/REPORT_SHAPE_GAP_UPDATE_2026-09-10.md` の F13 はパスモーフィング UI を「費用対効果最大」と位置づけるが、評価ロジックは既に実装済み。
- 仕様書のチェックボックスをそのまま受入条件に使わず、本計画の確定事実を優先する。

## Phase 計画

| Phase | 内容 | 状態 | 主な対象 |
| --- | --- | --- | --- |
| P1 | パスモーフィング UI（Inspector / Timeline 接続） | **実装済み（静的確認のみ、2026-11-17）** | `ArtifactShapeLayer`、PropertyEditor、Timeline |
| P2 | パスモーフ評価のホットパス修正 | **実装済み（静的確認のみ、2026-11-17）** | `evaluatePathAt()`、`ArtifactShapeLayer::Impl` |
| P3 | Contents グループの設計レビューとモデル実装 | Not Started（設計レビュー必須） | `ArtifactShapeLayer`、`toCoreShapeLayer()` |
| P4 | 既存ドキュメントの照合更新・runtime 受入 | Not Started | `docs/`、受入観点 |

## P1 — パスモーフィング UI

### 現状

`shape.path.keyframes` は JSON 往復と `setPathKeyframe()` 経由でしか設定できない。Inspector にも Timeline にも行が無いため、ユーザーはキーフレームを打てない。評価側はトポロジ違いまで補間できるのに、入口が無い状態。

### 実装

- パスキーフレームを既存の property 順に沿って Inspector と Timeline へ露出する。表示名は `Path` 系の既存命名に合わせる。
- キーフレームの追加・削除・値設定は、専用 Undo を新設するか既存 `SetLayerPropertyKeyframesCommand` / スナップショット系コマンドに載せるかを実装前に決める。1操作＝1 Undo を満たすこと。
- 頂点数の異なるパス間モーフでは、対応づけに `sampleCount`（現状 64）を使う。UI 側でサンプル数と頂点順の対応ガイドを表示し、star→heart→gear のような代表的ケースを手順として提示する。
- 新規 signal/slot は追加しない。`LayerChangedEvent` と既存のコマンド／サービス経路を使う。

### 検証

- 2形状間のキーフレーム追加 → 再生で形状が補間される → Undo/Redo で往復する。
- 同一トポロジと異なるトポロジの両方で、保存／再読込後もモーフが再現される。
- Timeline 左ペインの標準プロパティグループは `Transform` のみという規約を維持する。パス用行は既存のシェイプ専用セクションに置き、非 Transform グループを一般レイヤーへ露出しない。

### 実装メモ（2026-11-17）

- `ArtifactShapeLayer` に純粋な JSON 操作ヘルパーを追加（`pathKeyframeFrames()` / `pathKeyframeDocumentWith()` / `pathKeyframeDocumentWithout()`）。レイヤー自体は変更せず、更新後のドキュメントを返すだけなので、呼び出し側が Undo コマンドで包める。
- `getLayerPropertyGroups()` に `Path` グループと `shape.path.keyframes`（String・animatable）を登録。`setLayerPropertyValue()` に同パスの分岐を追加し、Undo/Redo 時に `markDirty()` と `changed()` を確実に発行する。
- Inspector は `ArtifactPropertyPresentation` の basic プロファイルに `Path` を追加するだけで露出させ、他のシェイプグループは従来どおり非表示。専用エディタはキー一覧の表示と、選択キーの別フレームへの複製・削除を行う（新規キーの追加はダイヤモンド側の責務）。
- キーフレームダイヤモンド（Inspector / Timeline 左ペイン）は `shape.path.keyframes` を特別扱いし、現在の再生位置で `customPathVertices()` をキャプチャして `SetLayerPropertyValueCommand` を push する。標準の `SetLayerPropertyKeyframesCommand` は JSON 値をundoできないため使わない。
- Timeline 左ペインには専用の `Path` グループ行と `Path Keyframes` プロパティ行を合成行として追加し、ダイヤモンドの塗り状態も JSON のフレーム一覧から判定する。右ペインのドープシート／カーブエディタへの擬似キーフレーム表示は今回の範囲外。
- 新規 signal/slot、QtCSS、QColorDialog、QImage 化、QPainter 合成は追加していない。

## P2 — 評価のホットパス修正

### 現状

`evaluatePathAt()` L864-895 は、キーフレーム間のトポロジが異なるフレームごとにパスと64サンプルを構築する。レンダリング／フレーム更新のホットパスで毎回の再確保に当たり、AGENTS.md の「重い非ゼロアロケーションを発生させない」に反する。

### 実装方針

- キーフレーム区間と時刻が変わらない限り再計算しないキャッシュを、既存の bounded 2-frame geometry cache と同じ考え方で追加する。
- サンプル用の作業領域は呼び出し側から渡す固定容量バッファ、または事前確保済みプールを使う。フレーム中のコンテナ拡張と一時コピーを避ける。
- どうしても残る確保については、箇所・頻度・理由をコードコメントと最終報告に記載する。

### 実装メモ（2026-11-17）

- `ArtifactShapeLayer::Impl` に2つの bounded キャッシュを追加。
  - `parsedPathKeyframes_`: キーフレーム JSON のパース結果を `contentRevision_` 単位で1エントリだけ保持。再生中の毎フレームの JSON パース、`std::map` 構築、全キーのデシリアライズを排除。
  - `morphIntervalBasis_`: 現在のキー区間 `(f0, f1)` と `contentRevision_` をキーに、両端点の64点等距離サンプルを `std::array<QPointF, 64>` で保持。区間をまたいだ最初のフレームだけ `ShapePath::interpolate` 相当の高コスト処理を行い、以降は配列上の線形補間と等距離リサンプリングのみ。
- リサンプリングは `ShapePath::sampleEquidistant()` と同じ「`i / (count - 1) * 全長` を線分を順に走査して求める」方式を固定長配列で再実装。`interpolate()` が端点パスの closed 時にのみ閉じる仕様に合わせ、open パスではセグメント数を63にする。
- 旧経路は1回のトポロジモーフで `ShapePath` の Impl 3個、`subpaths()`、3回の `sampleEquidistant(64)`（各約65回の `flatten()`）など数百回の確保を発生させていた。新経路では区間変更時の1回だけに限定される。
- **残存する例外確保（HOT_PATH_RULES 70-78）**: 公開 API が `std::vector<CustomPathVertex>` を返すため、1評価につき64要素の vector 確保が1回残る（箇所: `evaluatePathAt()` の `morphedVerts`）。容量は固定・小さく、呼び出し側バッファを渡す API 変更を伴わないため残した。キー区間変更時のみ发生的 `ShapePath` 構築・`sampleEquidistant` の確保は、頻度がキー遷移1回に限定される bounded なコールドパスとして扱う。
- 新規 signal/slot、QtCSS、QImage 化、QPainter 合成は追加していない。

### 検証

- 同一フレームの再描画でモーフ評価が再実行されないこと。
- モーフ中のフレーム時間と allocation 計測を比較し、邸下させないこと（計測はユーザーのビルド許可後）。

## P3 — Contents グループ

### 設計レビューが必要な理由

既存ドキュメントも Phase E を「最大・設計レビュー推奨」としている。ネストの有無は `toCoreShapeLayer()` の変換契約、operator の適用範囲、保存形式に波及するため、実装前に方式を確定する。

### 方式の選択肢

| 方式 | 変更範囲 | 利点 | 懸念 |
| --- | --- | --- | --- |
| A: アプリ側スタックに Group ノードを追加 | `Artifact` のみ | Core 変更不要。既存 CRUD / Undo / JSON を延長できる | Core の `ShapeGroup` と二重モデルになる。変換契約の整理が必要 |
| B: アプリモデルを Core `ShapeGroup` へ昇格 | `Artifact` + `ArtifactCore` | ネスト変換が Core 契約テストのまま正当化される | 影響範囲が大きい。既存フラットスタックの互換層が必要 |

**推奨:** まず A で Group ノードと Group Transform を導入し、B は別マイルストーンで検討する。最終判断は設計レビューで行う。

### 実装観点（方式 A 採用時）

- `ShapeStackNodeType` に `Group` を追加し、子ノード範囲と group transform / blend / 名前 / collapse 状態を持たせる。
- Group Transform は既存の `shape.content.N.transform.*` と同じ評価順に載せ、子コンテンツの transform との合成順序を明確にする。
- operator がグループ内の子だけに効くか、スタック全体に効くかを仕様化する。
- 追加・削除・並べ替えは既存のスタック Undo と JSON スナップショットへ畳む。
- `toCoreShapeLayer()` は Group を Core `ShapeGroup` へ変換するか、現状どおりフラット化するかをレビューで決める。

### 検証

- グループ入れ子、Group Transform と子 Transform の合成、operator の適用範囲が意図どおり。
- 保存／再読込でグループ構造・名前・blend・transform が往復する。

## P4 — ドキュメント更新・受入

- `docs/planned/MILESTONE_SHAPE_SVG_EXPORT_AND_KEYFRAME_VERIFY_2026-08-22.md` の D-5 / D-6 / Phase E を実コード照合結果へ更新する。
- `docs/analysis/REPORT_SHAPE_GAP_UPDATE_2026-09-10.md` の F13 を「評価実装済み・UI 未接続」へ修正する。
- `docs/analysis/GAP_AE_NUKE_2026-08-01.md` のシェイプ欄も同じ趣旨で更新する。
- runtime 受入（ビルド、module hygiene、D3D12 / Vulkan、Undo 往復、パスモーフの画素確認）はユーザーの明示指示後に実施する。

## 受入条件

- シェイプレイヤーでパスキーフレームを UI から追加・編集でき、再生でモーフが見える。
- 1操作が1回の Undo／Redo で往復する。
- パスモーフ評価がフレームごとの大容量確保を発生させない。
- Contents グループの設計レビューが記録され、採用方式が文書化されている。
- 新規 signal/slot、QtCSS、QColorDialog、QImage 化、QPainter 合成を追加しない。
- boolean 演算の QPainterPath 依存を拡大しない。

## 検証状況

- 2026-11-17: 実コード照合と P1 の実装を実施。`git diff --check` は親・子ともにエラーなし。
- 2026-11-17: P1 と P2 の実装を実施。`git diff --check` は親・子ともにエラーなし。
- 2026-11-17: ビルド・テスト・CMake・実機確認は未実施（ユーザー指示待ち）。P1 / P2 の受入はこれらが完了するまで pending。特に `ShapeLayerAnimationPersistenceContractTest::TopologyChangesMorphBetweenKeysAndSurviveJsonRoundTrip` の64頂点・有限値・round-trip一致を再確認する必要がある。
- 2026-11-17: ブランチ確認。親と `Artifact` は `codex/2026-11-13-dev-r2`、`ArtifactCore` と `ArtifactWidgets` は `codex/2026-11-13-dev` で不一致。コミット前に親子を同じ開発ブランチへ揃えること。

## コミット／プッシュ前の確認

AGENTS.md の親子リポジトリ workflow に従う。子を先にコミット／プッシュし、親の gitlink を更新してから親をプッシュする。ブランチ名は親子で統一する（上記の不一致を解消してから着手）。

## 関連文書

- `docs/planned/MILESTONE_SHAPE_SVG_EXPORT_AND_KEYFRAME_VERIFY_2026-08-22.md`
- `docs/analysis/REPORT_SHAPE_GAP_UPDATE_2026-09-10.md`
- `docs/analysis/REPORT_SHAPE_ANIMATION_PERSISTENCE_TEST_2026-10-29.md`
- `docs/shared/IDEAS_PLANE_SHAPE_IMAGE_CREATIVE_2026-08-18.md`
- `docs/technical/HOT_PATH_RULES.md`
- `Insight.md`（2026-11-17 の項目）
