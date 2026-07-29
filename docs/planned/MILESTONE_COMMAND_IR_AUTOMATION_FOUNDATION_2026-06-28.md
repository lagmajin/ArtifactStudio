# M-CMD-1 Command IR / Automation Foundation

作成日: 2026-06-28
ステータス: 部分完了（CommandRequest/Result/validation/vocabulary、CommandExecutor、App executorとWorkspaceAutomation/Python bridgeを実装、全commandのtransactional rollback・UI/automation経路統一・preview/explain・runtime検証は未完了）
対象:
- `ArtifactCore/`
- `Artifact/`
- `docs/planned/`
位置づけ:
- `Artifact Core`
- `C++ API / Python API`
- `Command IR`
- `MCP / DSL / AI Agent`

参照:
- `docs/FEATURE_DICTIONARY_2026-04-17.md`
- `docs/AI_API_EXTENDED_REFERENCE.md`
- `docs/DEBUG_MCP_PSEUDO_BREAKPOINT_PLAN_2026-06-04.md`
- `docs/planned/MILESTONE_PYTHON_API_SCRIPTING_2026-03-30.md`
- `docs/planned/MILESTONE_SCRIPT_CONSOLE_2026-06-16.md`
- `docs/planned/MILESTONE_SCRIPT_MENU_MACRO_ENTRY_EXECUTION_2026-05-31.md`

---

## 1. 目的

`ArtifactStudio` に AI / MCP / DSL / Python scripting を本格導入する場合、low-level API をそのまま外部 automation surface に露出すると次の事故が起きやすい。

- 実行順序が壊れる
- 途中失敗で project 状態が半端になる
- Undo 粒度が細かく壊れる
- UI 経由の操作と automation 経由の操作で結果がずれる
- Core API の変更が AI surface 破壊に直結する

この milestone の目的は、`API をそのまま AI に触らせない` 方針を明文化し、**Command IR を automation の正規入口にする**こと。

---

## 2. 核心方針

### 2.1 層の分離

```text
Artifact Core
  -> stable domain API
Artifact App / Services
  -> editor-aware orchestration
Command IR
  -> validated, undo-safe, transactional intent layer
MCP / DSL / Python / AI Agent
  -> external control surfaces
```

重要なのは、AI が `addLayer()`, `setTransform()`, `setKeyframe()`, `applyEffect()` を好きな順序で直叩きしないこと。

automation surface は API 呼び出し列ではなく、**意図を持った command** を投げる。

### 2.2 Core API は残す

この milestone は Core API を隠すことが目的ではない。

- C++ 実装内部では既存 Core API を継続使用する
- Python bridge も最終的には Core API を使ってよい
- ただし AI / MCP / external automation の primary surface は Command IR に寄せる

つまり、`Core API = 実装用`, `Command IR = automation 用` の役割分担を固定する。

### 2.3 Primitive + Macro の二層

高レベル command だけに寄ると、task-specific command が無限に増える。
逆に low-level command だけだと、AI が危険な手順責任を背負う。

そのため、最初から 2 層を前提にする。

- `Primitive Command`
  - 汎用で再利用可能
  - validation / transaction / undo の単位
- `Macro Command`
  - workflow intent を短く表す
  - 内部では Primitive を束ねる

---

## 3. Command IR の設計原則

### 3.1 不変条件

- 1 command = 1 明確な Undo 単位、または 1 transaction group
- command 実行前に validation 可能
- command 実行結果は success / failure / diagnostics を返す
- 途中失敗時は partial mutation を残さない
- UI からの操作と automation からの操作で、できる限り同じ command path を使う
- 新しい global signal / slot 配線を増やさない
- render backend や Diligent 低レベル API を automation surface に露出しない

### 3.2 Command 実行結果の shape

最低限、各 command は次を返せる shape を持つ。

- `success`
- `errorCode`
- `message`
- `diagnostics[]`
- `createdObjectIds[]`
- `affectedObjectIds[]`
- `undoLabel`

### 3.3 Core Result Contract

`ArtifactCore` 側では、`CommandRequest` と `CommandResult` を shared contract として持つ。

- `CommandRequest`
  - `type`
  - `target`
  - `arguments`
  - `metadata`
- `CommandResult`
  - `success`
  - `valid`
  - `executed`
  - `type`
  - `error`
  - `undoLabel`
  - `diagnostics`
  - `details`

`commandVocabulary()` は各 type の説明文も返し、`describeType()` 相当の参照点として使える。
`undoLabel` は command type ごとに自動生成し、Undo / history 表示にそのまま流せる。

`commandVocabulary()` の返却イメージは次のような `QVariantList` でよい。

```json
[
  {
    "type": "set_keyframes",
    "required": ["target.layerId", "target.propertyPath", "keys[]"],
    "description": "Set multiple keyframes for one property path"
  },
  {
    "type": "move_layer",
    "required": ["target.layerId", "newIndex"],
    "description": "Move a layer within the current composition"
  }
]
```

### 3.4 Core Executor Contract

`ArtifactCore` 側では、`CommandExecutor` を validation / execute の差し込み口として定義する。

- `validate(request)`
- `execute(request)`

この時点では具象 executor の完成は不要だが、`WorkspaceAutomation` が将来の typed executor に切り替わる余地を残す。

`WorkspaceAutomation` には、既存実装を `CommandExecutor` に乗せる concrete executor entry point も用意する。

### 3.5 Dry-run / Validate-only

MCP / AI では「実行前に安全確認したい」要求が強い。
そのため command executor は、早い段階で次を持つべき。

- `validate(command)`
- `execute(command)`
- optional `preview(command)` or `explain(command)`

`preview` / `explain` は必須ではないが、将来の agent planner や UI confirmation に流し込めると便利な補助口として考える。

### 3.6 Stable identifier first

AI が `layer 3` や UI row index に依存すると壊れやすい。
Command IR の target 指定は、早い段階で stable identifier を優先する。

候補:

- `layerId`
- `compositionId`
- `propertyPath`
- `effectInstanceId`
- optional `humanAlias`

名前解決や selection 解決は bridge layer 側で行い、IR 自体は解決済み target を優先する。

---

## 4. 推奨 Command 構成

### 4.1 Primitive Command 候補

最初に固定したいのは、After Effects 的な主要操作を十分カバーしつつ、まだ汎用性が高い粒度。

- `CreateLayer`
- `DeleteLayer`
- `DuplicateLayer`
- `ReorderLayer`
- `SetPropertyValue`
- `SetKeyframes`
- `RemoveKeyframes`
- `AddEffect`
- `RemoveEffect`
- `SetEffectPropertyValue`
- `CreateNullAndParentSelection`
- `SetParent`
- `SelectObjects`
- `BatchCommand`

`BatchCommand` は単なる配列ではなく、**all-or-nothing transaction** を前提にする。

### 4.2 Macro Command 候補

Macro は workflow shortener として少数から始める。

- `CreateIntroLogoMotion`
- `FadeInFromBelow`
- `PopScaleBounce`
- `ApplyLowerThirdPreset`
- `CenterSelectionInComp`
- `RevealSelectionTransform`

原則:

- Macro は UI object を直接触らない
- 内部で Primitive に分解される
- 失敗時は途中状態を残さない
- built-in macro と user macro で同じ descriptor shape を共有できる形を目指す

---

## 5. 最小 IR shape

JSON 自体を仕様確定とみなす必要はないが、automation surface の最小 shape は次に近いものを想定する。

### 5.1 Primitive 例

```json
{
  "type": "batch",
  "label": "Intro logo setup",
  "commands": [
    {
      "type": "set_keyframes",
      "target": {
        "layerId": "layer.logo",
        "propertyPath": "transform.position"
      },
      "keys": [
        { "time": 0.0, "value": [960, 620] },
        { "time": 0.6, "value": [960, 420], "ease": "out_back" }
      ]
    },
    {
      "type": "set_keyframes",
      "target": {
        "layerId": "layer.logo",
        "propertyPath": "transform.scale"
      },
      "keys": [
        { "time": 0.0, "value": [85, 85] },
        { "time": 0.6, "value": [100, 100], "ease": "out_back" }
      ]
    }
  ]
}
```

### 5.2 Macro 例

```json
{
  "type": "macro",
  "macroId": "create_intro_logo_motion",
  "arguments": {
    "targetLayerId": "layer.logo",
    "start": 0.0,
    "duration": 0.8,
    "style": "pop",
    "overshoot": 0.35
  }
}
```

### 5.3 Primitive 直呼び例

```json
{
  "type": "set_keyframes",
  "target": {
    "layerId": "layer.logo",
    "propertyPath": "transform.position"
  },
  "keys": [
    { "time": 0.0, "value": [960, 620] },
    { "time": 0.6, "value": [960, 420], "ease": "out_back" }
  ]
}
```

この milestone では JSON parser を先に完成させること自体は目的ではない。
先に必要なのは、**C++ 内の typed command model と executor**。

`set_keyframes` / `batch_set_keyframes` は `time` の短い入力を受け取りつつ、内部では `timeValue` / `timeScale` を保持する。必要なら DSL 側で `timeValue` / `timeScale` を明示してもよいが、最小例としては `time` だけで十分である。

### 5.4 Result shape

command executor の戻り値は最低でも次の情報を持つ。

- `success`
- `valid`
- `executed`
- `type`
- `error`
- `undoLabel`

AI/MCP 側は、`success` だけでなく `valid` と `executed` を見て、入力ミス・未実行・実行失敗を分けて扱えるようにする。

### 5.5 Validate example

```json
{
  "success": true,
  "valid": true,
  "executed": false,
  "type": "set_keyframes",
  "error": "",
  "undoLabel": "Set Keyframes",
  "diagnostics": {},
  "details": []
}
```

この形なら、AI は `valid: false` を入力ミス、`valid: true` かつ `executed: false` を runtime 未実行として扱える。

---

## 6. 実行経路

### 6.1 推奨パイプライン

```text
MCP tool / Python / DSL
  -> request decode
  -> name / selection / alias resolution
  -> typed Command IR
  -> validator
  -> transaction / undo wrapper
  -> core/app service execution
  -> structured result
```

### 6.2 Resolver を分離する

`"logo"` のような名前解決は command executor 本体で抱え込みすぎない。

- `Resolver`
  - 名前 -> stable id
  - selection keyword -> object set
  - property alias -> canonical path
- `Validator`
  - 対象存在確認
  - property mutability
  - time range
  - type compatibility
- `Executor`
  - 実際の mutation

この分離で、MCP / Python / UI macro が同じ executor を共有しやすくなる。

実務上の順序は `commandVocabulary()` で使える type と required fields を確認し、`validateCommand()` で入力を先に弾き、`executeCommand()` を mutation の唯一の入口にする流れがよい。

#### Resolver input example

```json
{
  "selection": "logo",
  "aliases": {
    "propertyPath": "pos"
  },
  "resolved": {
    "layerId": "layer.logo",
    "propertyPath": "transform.position"
  }
}
```

この段階で名前や略称を stable id / canonical path に変換し、executor には解決済みの target だけを渡す。

### 6.3 Layer flow map

```text
AI / MCP / Python / UI macro
  -> commandVocabulary()
  -> validateCommand(command)
  -> Resolver
  -> executeCommand(command)
  -> Undo-safe core/app services
```

- `commandVocabulary()` は表面の使い方を列挙する
- `validateCommand()` は入力を止める
- `Resolver` は曖昧 target を canonical 化する
- `executeCommand()` は mutation を実行する
- core/app services は Undo-safe に結果を反映する

---

## 7. Undo / Transaction 方針

この milestone の成否は `Undo` に強く依存する。

### 7.1 ルール

- Primitive は単独でも Undo 可能
- Batch / Macro は 1 つの Undo action にまとまる
- partial success は基本禁止
- 途中失敗時は rollback する

### 7.2 最初の現実解

最初から完全 rollback engine を作らなくてもよい。
ただし最低でも、次の順で育てる。

1. execute 前 validation を厚くして失敗を前段で止める
2. undo macro grouping で 1 action 化する
3. reversible primitive を増やす
4. 必要箇所だけ compensating rollback を追加する

---

## 8. API 公開方針

### 8.1 C++ API

- Core / App service の low-level API は内部向けに維持
- command executor 用の narrow facade を追加
- `.ixx` の public API 拡張は最小に留める

### 8.2 Python API

Python から low-level API を全部 expose する方向は避ける。

まずは:

- `artifact.execute_command(...)`
- `artifact.validate_command(...)`
- `artifact.list_available_macros()`

のような command-oriented surface を正規にする。

必要なら内部用 / advanced 用として low-level bridge を残すが、AI には primary にしない。

### 8.3 MCP

MCP は read-mostly から始め、mutation は command executor 経由に限定する。

初期候補:

- `get_project_snapshot`
- `list_layers`
- `validate_command`
- `execute_command`
- `list_macros`

---

## 9. 非目標

この milestone では、次はやらない。

- D3D12 / Diligent backend の低レベル command 化
- 任意 C++ API の完全自動公開
- unrestricted Python mutation surface
- user-facing natural language editor の完成
- full macro recording / free-form replay の完成
- すべての layer / effect / property を一気に IR 化

---

## 10. 実装フェーズ

### Phase 1: Command Vocabulary Lock (P0)

目的:
最小 command set と target / result / diagnostics の shape を固定する。

作業:

- Primitive command 一覧を確定
- target identifier 方針を確定
- result / diagnostics struct を決める
- validation failure taxonomy を決める

Done criteria:

- `CreateLayer`, `SetPropertyValue`, `SetKeyframes`, `AddEffect`, `BatchCommand` の shape が決まる
- `success / errorCode / diagnostics / undoLabel` を返す result が固定される
- 名前解決前の request と解決後の typed command を分離できる

### Phase 2: Typed Command IR + Executor (P0)

目的:
C++ 内で typed command を validate / execute できる最小実体を作る。

作業:

- `CommandVariant` or equivalent typed model
- `CommandValidator`
- `CommandExecutor`
- `BatchCommand` の Undo grouping

Done criteria:

- C++ から typed command を直接実行できる
- validation-only 実行ができる
- batch 実行が 1 Undo 単位にまとまる

### Phase 3: Resolver Layer (P1)

目的:
外部 surface からの曖昧 target を安全に解決する。

作業:

- layer name / selection token resolver
- property alias -> canonical path resolver
- diagnostics を伴う resolve failure

Done criteria:

- `"selected_layers"` や `"logo"` を解決して typed command に変換できる
- resolve failure が structured diagnostics で返る
- resolver が executor から独立した責務として差し込める
- canonical path / stable id の変換点が 1 箇所にまとまる

### Phase summary

| Phase | What is locked | What remains |
| --- | --- | --- |
| 1 | command vocabulary / result shape | typed IR plumbing |
| 2 | typed validate / execute / undo grouping | resolver integration |
| 3 | ambiguous target resolution | macro and planner polish |
| 4 | Python API on top of Command IR | end-to-end runtime proof |

### Phase 4: Python Command API (P1)

目的:
Python surface を command-oriented に固定する。

作業:

- `execute_command`
- `validate_command`
- command result の Python binding
- built-in macro enumeration

Done criteria:

- Python から command 実行が可能
- low-level API を知らなくても主要 automation ができる
- 失敗時に structured error を受け取れる

### Phase 5: MCP Command Surface (P1)

目的:
MCP に安全な mutation surface を追加する。

作業:

- read-only snapshot tools
- command validation tool
- command execution tool
- tool response と diagnostics の整備

Done criteria:

- MCP 経由で `validate -> execute` ができる
- Undo / transaction 境界が app 内で維持される
- direct low-level mutation tool を作らずに済む

### Phase 6: Typed Command Contract Module (P1)

目的:
command vocabulary を実装から分離し、`ArtifactCore` 側で参照できる shared contract にする。

作業:

- `Core.AI.CommandIR` を追加
- supported command vocabulary を contract として公開
- `WorkspaceAutomation` は contract を読むだけに寄せる

Done criteria:

- command vocabulary が 1 箇所に集約される
- AI surface と core contract が分離される
- 後続の typed executor を差し込みやすくなる

### Phase 7: Built-in Macro Starter Pack (P2)

目的:
workflow value の高い macro を少数提供する。

作業:

- `CenterSelectionInComp`
- `CreateNullAndParentSelection`
- `PopScaleBounce`
- `ApplyPresetToSelection`

Done criteria:

- 代表 macro が 3 から 5 個使える
- すべて Primitive 経由で実行される
- UI / Python / MCP のどこから呼んでも同じ結果になる

---

## 11. 最初の一歩

最初の実装は広げすぎず、次の順がよい。

1. `SetPropertyValue`
2. `SetKeyframes`
3. `BatchCommand`
4. `execute_command / validate_command`
5. その上で最初の macro を 1 個だけ乗せる

最初の macro は `CreateIntroLogoMotion` より、`CenterSelectionInComp` や `PopScaleBounce` のような小さく閉じたものの方が安全。

### 11.1 最小実装スライス

次の順で詰めると、Command IR の完成に近づきやすい。

1. `SetPropertyValue`
2. `SetKeyframes`
3. `BatchCommand`
4. `validateCommand / executeCommand`
5. `Resolver` の入力変換
6. 最初の小さな macro 1 個

この順なら、low-level mutation を増やさずに AI surface を先に安定させやすい。

---

## 12. 成功条件

この milestone が成功したと言える条件は次。

- AI / MCP / Python が low-level API 直叩き前提でなくなる
- automation mutation が Undo-safe になる
- command failure が partial state を残しにくくなる
- Core API の内部進化と external automation surface を分離できる
- 後続の DSL や agent planner を Command IR 上に積める

---

## 13. メモ

- まずは typed C++ command model を先に作る。JSON や natural language は後段。
- property path と identifier の vocabulary 固定は早めにやる。
- `.ixx` を広げすぎず、実装は可能な限り `.cppm` / `.cpp` 側に寄せる。
- UI macro、Python、MCP が別々の mutation path を持つ状態は避ける。

---

## 14. Completion check

この milestone は、少なくとも次が揃えば「実装面は完了」とみなせる。

- `Core.AI.CommandIR` が shared contract として存在する
- `WorkspaceAutomation` が command vocabulary / validate / execute を公開する
- `set_keyframes` / `batch_set_keyframes` が Undo-safe である
- `move_layer` / `rename_layer` / `add_effect` が既存の Undo / service 経路を使う
- AI surface が low-level mutation 直叩きではなく command facade を使う

Evidence currently present:

- `Core.AI.CommandIR` module exists
- `WorkspaceAutomation` exposes `commandVocabulary / validateCommand / executeCommand`
- command examples and result shapes are documented

This aligns mainly with Phase 1 to Phase 3 in the summary above, with Phase 4 kept as a follow-on surface.

Still unverified in this workspace:

- runtime build / execution proof
- any end-to-end MCP or UI call path
- broader resolver / macro behavior beyond the documented contract

Next actions if we continue:

1. runtime verification of the command facade
2. one end-to-end MCP or UI call path
3. resolver integration for one ambiguous target
4. one minimal macro on top of the Command IR

Suggested verification commands:

- inspect `commandVocabulary()` return shape through the existing automation host
- call `validateCommand()` with one missing field case and one unsupported type case
- call `executeCommand()` with one `set_keyframes` request and confirm undo metadata
- inspect one ambiguous target through the resolver path before mutation
- run one small macro through the same command facade path

Verification checklist:

- `commandVocabulary()` returns the documented command list
- `validateCommand()` returns structured diagnostics for invalid input
- `executeCommand()` routes through the command executor and Undo-safe paths
- `set_keyframes` and `batch_set_keyframes` keep undo / time data intact
- at least one ambiguous target can be resolved before execution
- one small macro can reuse the same Command IR path

When verifying, check:

- returned command entries include `type`, `required`, and `description`
- validation failures distinguish missing fields from unsupported types
- execute results carry `success`, `valid`, `executed`, and `undoLabel`
- keyframe results preserve both compact frame input and original time base
- resolver output resolves to canonical target ids before mutation
- macro execution stays on the same command facade path

Note:

- this workspace pass documents the verification plan and evidence shape, but does not itself claim runtime proof
- runtime verification should be done separately with the suggested commands above

Evidence map:

- `commandVocabulary()` evidence -> returned vocabulary list with `type`, `required`, `description`
- `validateCommand()` evidence -> structured result with `valid`, `error`, `diagnostics`
- `executeCommand()` evidence -> structured result with `success`, `executed`, `undoLabel`
- keyframe evidence -> preserved time values plus undo-safe snapshot behavior
- resolver evidence -> canonical target ids before mutation
- macro evidence -> same command facade path as primitive commands

Use this map as the proof target when verifying runtime behavior; do not treat the documented shape alone as runtime confirmation.

まだ後続に残すもの:

- DSL parser の本格実装
- macro registry の拡張
- property path vocabulary の厳密固定
- resolver の独立化
- Command IR の型追加と schema 調整
