# MILESTONE: AI Tooling Expansion

作成日: 2026-04-21

## 目的

ArtifactStudio の AI を「会話できる」状態から、「読み取り」「提案」「安全な操作」「作業自動化」まで広げる。

このマイルストーンは、AI を単発機能として増やすのではなく、共通の tool surface と context surface を整備して、後続機能を載せやすくするための土台を定義する。

---

## 方向性

### 1. Read
- 現在のプロジェクト、選択、アクティブコンポジション、レンダーキューを安定して読めるようにする
- AIContext の snapshot を強化し、説明文と実データを分ける
- まずは「何が起きているか」を正確に答える

### 2. Suggest
- キーフレーム、カラー、レイアウト、リネームなどの提案を出せるようにする
- 提案はそのまま実行せず、UI で確認できる形にする
- 既存ワークフローを壊さず、補助として役立つ形を優先する

### 3. Act Safely
- 破壊的でない操作から順に、確認付き write tool を増やす
- 既存の `*Service` 経路を再利用し、AI から直接 low-level API を触らせない
- dry-run / confirmation metadata / undo 連携を前提にする

### 4. Automate
- ワークスペース操作、バッチ処理、レンダー起動、プロジェクト整形をまとめて扱えるようにする
- 後から CLI / MCP / local assistant / cloud assistant のどれからでも同じ tool を呼べるようにする

---

## Phase 1: Context と Inspection の強化

### Goal
AI が「何があるか」を高精度で把握できるようにする。

### Tasks
- `AIContext` の snapshot を拡張する
- project / composition / selection / render queue の要約を安定化する
- 説明カタログを整理し、tool description を実データと分離する
- read-only inspection tool を優先して増やす

### Phase 1-1: Context Snapshot
- project summary, active composition, selected layers, render queue state を 1 つの snapshot にまとめる
- JSON 化できる安定 surface を優先する
- UI 表示用と tool 読み取り用を同じ元データから派生させる

### Phase 1-2: Inspection Tools
- `list_compositions`
- `get_active_composition`
- `get_selected_layers`
- `get_render_queue_summary`
- `get_project_overview`

### Phase 1-3: Description Catalog
- tool description と実データの分離
- 機能説明の重複削減
- ローカル / cloud / MCP で同じ説明を再利用できる形にする

### Phase 1-4: Read-only Diagnostics
- エラー時の一次診断に必要な情報を要約する
- missing file / missing dependency / empty selection / inactive composition を判別しやすくする
- 既存の診断 UI と矛盾しない形で返す

### Related
- `ArtifactCore/docs/ArtifactCore_AI_Feature_Map_2026-04-10.md`
- `docs/planned/MILESTONE_AI_MCP_TOOL_BRIDGE_2026-04-10.md`
- `docs/planned/MILESTONE_IN_APP_LLM_INTEGRATION_2026-04-08.md`

---

## Phase 2: Safe Write Tools

### Goal
AI が安全に編集できる最小セットを揃える。

### Tasks
- rename / duplicate / move / select / import などの基本操作を tool 化する
- render queue の enqueue / start / inspect を tool 経由にする
- destructive action は confirmation を必須化する
- dry-run と実行の差を UI とログで明確にする

### Phase 2-1: Basic Mutations
- rename layer / composition / asset
- select layer / composition
- duplicate layer / composition
- move layer order
- import asset

### Phase 2-2: Render Queue Actions
- queue render job
- start render job
- inspect job status
- list failed / pending / completed jobs

### Phase 2-3: Confirmation and Dry-run
- destructive action は必ず dry-run を返せるようにする
- confirmation metadata を UI 側に返す
- 既存 undo / redo 経路に乗る操作だけを write tool にする

### Phase 2-4: Workspace Automation Surface
- project setup
- composition setup
- asset organization
- selection-driven batch operation

### Phase 2-5: Execution Governance
- operation log
- confirmation log
- failure reason summary
- permission / policy summary
- prompt context への反映

### Related
- `docs/planned/MILESTONE_AI_WORKSPACE_AUTOMATION_2026-04-10.md`
- `docs/planned/MILESTONE_AI_COMMAND_SANDBOX_2026-04-10.md`
- `docs/planned/MILESTONE_AI_SAFE_WRITE_TOOLS_2026-04-21.md`
- `docs/planned/MILESTONE_AI_SAFE_WRITE_TOOLS_PHASE3_2026-04-21.md`

---

## Phase 3: Creative Assist

### Goal
制作中の「手が足りないところ」を AI が埋める。

### Tasks
- keyframe generation を最初の実用支援として入れる
- color grading suggestion を次の実用支援として入れる
- auto-reframe / background removal / style transfer はその後の拡張枠に分ける
- expression copilot / prompt helper / effect suggestion は補助線として扱う

### Phase 3-1: Keyframe Suggestion

### Goal
タイムライン上の動きから、使える提案を返せるようにする。

### Scope
- 既存キーフレームの解析
- 軌跡の単純化
- イーズ候補の提示
- 適用前プレビュー
- 単一レイヤーの transform / opacity から始める
- 必要なら時間サンプルの再解析へ進める
- 生成値は「自動適用」より「候補提案」を先に返す

### Output
- `AIKeyframeGenerator` から提案結果を返す
- `ArtifactTimelineWidget` で提案を見せる
- 適用は existing undo path を通す

### Related
- `docs/planned/MILESTONE_AI_KEYFRAME_SUGGESTION_2026-04-21.md`

### Phase 3-2: Color Grading Suggestion

### Goal
シーンに合う調整のたたきを返せるようにする。

### Scope
- 画像またはフレーム列の解析
- 露出 / コントラスト / 色温度 / 彩度の提案
- LUT または preset への落とし込み
- 適用前プレビュー
- 単一ショットの提案から始める
- プロパティ値と preset 値の両方を出せるようにする
- まずは安全な提案のみを返し、破壊的な自動変更は後段に回す

### Output
- `AIColorAnalyzer` / `ColorGradingSuggester` から提案結果を返す
- `ArtifactPropertyWidget` または color UI で提案を見せる
- 適用は既存の color grading 経路を通す

### Related
- `docs/planned/MILESTONE_AI_COLOR_GRADING_SUGGESTION_2026-04-21.md`

### Phase 3-3: Auxiliary Creative Helpers

### Goal
提案の横に置く補助機能を整理する。

### Scope
- auto-reframe
- background removal
- style transfer
- prompt helper
- effect suggestion
- auto-reframe は aspect ratio 変更時の補助に限定する
- background removal は matting / alpha extraction の補助として扱う
- style transfer は実験枠に置き、既存ワークフローの中核にはしない

### Related
- `docs/planned/MILESTONE_AI_ASSISTED_KEYFRAME_GENERATION_2026-04-11.md`
- `docs/planned/MILESTONE_AI_COLOR_GRADING_SUGGESTION_2026-04-11.md`
- `docs/planned/MILESTONE_AI_ASSISTED_FEATURES_2026-03-29.md`

---

## Phase 4: Workflow Automation

### Goal
AI を単発提案ではなく、作業の継続的な補助役にする。

### Tasks
- workspace preset の整備
- project cleanup / batch import / batch render 補助
- diagnostics 要約とエラー対処提案
- 反復操作のテンプレート化

### Phase 4-1: Workspace Presets
- workspace の保存 / 読み込み
- dock layout preset
- window state 復元
- role 別の初期 workspace

### Phase 4-2: Batch Operations
- batch rename
- batch relink
- batch import
- batch render
- macro / script entry

### Phase 4-3: Diagnostics Assistant
- failure reason summary
- missing dependency summary
- render queue failure triage
- startup / load time の簡易分析

### Phase 4-4: Reusable Automation Templates
- よく使う操作を template 化する
- AI が提案した操作列を保存できるようにする
- 既存の command palette や script hook と繋ぐ

### Related
- `docs/planned/MILESTONE_AI_WORKFLOW_AUTOMATION_2026-04-21.md`
- `docs/planned/MILESTONE_AI_WORKFLOW_AUTOMATION_PHASE3_2026-04-21.md`
- `docs/planned/MILESTONE_LOCAL_AI_CHAT_2026-04-01.md`
- `docs/planned/MILESTONE_AI_BASIC_ASSISTANT_2026-04-11.md`

---

## Completion Criteria

- AI がプロジェクト状態を安定して説明できる
- AI が確認付きで安全な編集操作を実行できる
- キーフレーム、カラー、ワークスペースのいずれかで実用的な支援ができる
- keyframe suggestion と color grading suggestion の最小実用版が別々に追える
- read / suggest / safe act / automate の入口が分離されている
- Tool surface が local / cloud / MCP のどれからでも再利用できる

## Suggested Order

1. Phase 1: Context と Inspection
2. Phase 2: Safe Write Tools
3. Phase 3-1: Keyframe Suggestion
4. Phase 3-2: Color Grading Suggestion
5. Phase 4: Workflow Automation

## Notes

- まずは tool 境界を揃える
- 破壊的操作は後回しにする
- AI の出力は「提案」と「実行」を分ける
