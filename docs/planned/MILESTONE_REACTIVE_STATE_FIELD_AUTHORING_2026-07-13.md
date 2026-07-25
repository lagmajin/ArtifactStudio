# Reactive State & Field Authoring Roadmap

**ステータス:** In Progress

作成日: 2026-07-13  
対象: Audio Reactive Binding + Bake / Composition State Studio / Live Field Authoring UX

## 1. 目的

既存の property mutation、live control recording、state override、transform field を再利用し、次の制作フローを一続きにする。

```text
audio analysis
  -> external control mapping
  -> layer property preview
  -> bake to keyframes
  -> named composition state
  -> field-based spatial variation
```

新しい node editor や global event wiring は作らず、既存の layer / property / timeline / composition viewport を操作の中心に保つ。

## 2. 現在地

### Audio Reactive

実装済み:

- `AudioAnalyzer::AnalysisResult` の `rms / peak / low / mid / high` を入力にできる
- `ArtifactAbstractComposition::applyAudioAnalysis(...)` が `audio.*` address を既存 external control 経路へ渡す
- mapping transform は scale、offset、clamp、invert、smoothing を保持できる
- live control recording は sampling interval、dead zone、cancel restore、keyframe記録を持つ

不足:

- 実機での音声解析・preview・record・bake確認
- 実機でのlive recording Commit / Undo確認

### Composition State

実装済み:

- `CompositionStateVariant` と property override集合
- stateの追加、削除、有効化、切替
- baseline復元を含むstate切替
- composition JSON round-trip

不足:

- state並べ替え
- Diffの同時snapshot描画
- Takesへの明示変換が必要になった場合の変換UI
- 実機でのbaseline復元・A/B toggle確認

### Live Field Authoring

`MILESTONE_LIVE_FIELD_AUTHORING_UX_2026-07-04.md` は In Progress。radial / box / linear、viewport drag、Undo、stack操作、共通channel、Text / Shape / Cloner consumerまで実装済み。実機確認は未実施。

残作業は独立したfield engineの追加ではなく、既存milestoneの以下を継続する。

- CPU / GPU共通のclone color契約
- Shape vertex attribute対応後の頂点単位weight
- viewport操作と保存互換性の実機確認

## 3. 推奨実装順

### Phase A: Audio Reactive Authoring

1. Property Editorの既存property行から `Static / Keyframes / Audio` を選択できる薄いsource surfaceを作る
2. sourceを `amplitude / peak / low / mid / high` から選ぶ
3. gain、offset、clamp、invert、smoothingを既存mappingへ保存する
4. raw値とprocessed値を同じsurfaceでmonitorする
5. Preview / Arm / Record / Commit / Cancelを既存live recording APIへ接続する

Current status:

- Composition所有の`CompositionAudioReactiveBinding`を追加しJSON保存
- 同じaudio sourceを複数の数値propertyへ割当可能
- amplitude / peak / low / mid / high、gain、offset、clamp、invert、smoothingを保持
- `applyAudioAnalysis(...)`からbindingごとにprocessed値を既存layer property mutationへ適用
- live recording中はbinding IDまたは`audio.*` address単位でkeyframe記録
- cancel時は既存snapshot経路で元value / keyframesへ復元
- Animation > Expression > Audio Reactiveからfocused propertyへ割当・解除可能
- binding変更はComposition単位のUndoに対応
- binding単体previewでraw / processed値を表示し、確認後property値を復元
- work area / layer range / custom rangeのoffline audio bakeを実装
- 48kHz音声解析からframe単位に決定的なsampleを生成
- tolerance-based線形keyframe reductionでsample点の最大誤差を制限
- bake結果をvalue / keyframes込みの単一Undoとして適用
- attack / release envelopeをbindingへ保存し、preview / live / bakeで共有
- focused bindingのArm、sample stride、dead zone、Commit、Cancelを既存live recording APIへ接続
- Cancelはrecord開始前のvalue / keyframes snapshotへ復元
- Commitは全対象propertyのbefore / after value・keyframesを一つのUndoへ束ねる

Done criteria:

- 任意の数値propertyへaudio sourceを割り当てられる
- previewは既存property mutation経路だけを使う
- cancelで編集前へ戻り、commitでkeyframeが残る
- 新規global signal / slotを追加しない

### Phase B: Audio Bake Quality

1. attack / release envelopeをsource transformとして追加する
2. bake範囲をwork areaまたは明示範囲に限定する
3. tolerance-based keyframe reductionを追加する
4. bake全体を1 Undo単位にする
5. offline renderではlive captureに依存せず、確定済みkeyframeを評価する

Done criteria:

- 同じ解析入力と設定から決定的なbake結果を得られる
- reduction前後の最大誤差が指定tolerance以内
- bake中断やcancelで残骸が残らない

### Phase C: Composition State Studio

1. 既存Composition系surfaceに最小state listを追加する
2. create / duplicate / rename / delete / activateを提供する
3. 選択propertyをactive stateのoverrideとしてcapture / removeできるようにする
4. override中のpropertyへ控えめな状態表示を付ける
5. state切替と編集をUndo対応にする

Current status:

- Composition Settingsへ既存state listとactive表示を追加
- create / duplicate / rename / delete / activate / deactivateをコード操作なしで実行可能
- state listとactive stateを一つのUndo snapshotとして復元
- deactivateとUndo適用時は既存baseline復元経路を再利用
- focused propertyを型維持した値でactive stateへcapture / remove可能
- 初回capture時のproperty値をbaselineとして保持し、解除とUndoで復元
- state listにoverride件数を表示
- active stateでoverride中のProperty Editor rowへ`STATE`表示とstate名tooltipを追加
- Source Textの既存supplementary表示とは競合せず結合

Done criteria:

- stateとoverrideの作成・切替がコード操作なしで完結する
- state解除時にbaselineへ正しく戻る
- stateはrevision、responsive variant、Takeとして扱われない

### Phase D: State Comparison

1. Sandbox Editsの比較概念を再利用してstate A/Bを比較する
2. changed property listを表示する
3. viewportはsplitまたはtoggle comparisonから最小方式を選ぶ
4. stateからTakeを作る場合はコピーではなくoverride集合の明示変換にする

Current status:

- Composition Settingsでstate Aとstate B（またはbaseline）を選択可能
- layer / property path単位でoverride集合を比較し、A/B値のchanged property listを表示
- comparison pairをcomposition JSONへ保存し、存在しないstate IDは読込時に除外
- 既存View > Compare A/Bでstateを切替し、Compare Offで比較開始前のstateへ復元
- composition切替時も比較開始前のstateを復元してsessionを終了
- Compare menuへA/Bのstate名またはBaselineを表示
- Diffの同時snapshot描画は未実装

### Phase E: Field Authoring Completion

1. 既存field listのenable、strength、invert、reorderを完成させる（active fieldの直接操作sliceは完了）
2. radial共通契約をbox / linearへ拡張する（完了）
3. overlay、hit-test、evaluationのshape差をdescriptorへ閉じる
4. scalar influence `0..1` を共通出力にする
5. text、shape、cloner / modifierの順でconsumerを増やす

Current status:

- scalar influenceの共通出力とstack blendを実装済み
- text consumerへの接続は完了
- shape consumerへのalpha接続は完了
- cloner / modifier後の追加instance weight接続は完了
- weight / scale / time offsetの共通channel契約を実装済み
- cloner / modifier後の追加instanceへweight / scale / time offsetを同じ評価点から適用済み
- colorはCPU callbackとGPU instance dataの表現を同時に統一する必要があるため保留
- viewport field handleは`Alt + drag`時だけ入力を取得し、通常のSelection Tool操作と競合しない

Done criteria:

- 3 shapeで同じ選択・drag・stack操作が成立する
- field consumerがtransform専用データへ直接依存しない
- viewport tool ownershipがmask / path / transform gizmoと競合しない

## 4. 共通設計境界

- Audio Reactiveは「時間方向のproperty source」
- Composition Stateは「名前付きproperty override集合」
- Fieldは「空間方向のinfluence」
- Takesは複数override集合をレンダー単位に束ねる機能であり、State Studioへ混ぜない
- source、state、fieldの最終適用先は既存property evaluationとする
- `QImage`、Qt composition、QtCSS、`QColorDialog`を追加しない
- Diligent / D3D12 backendへは触らない

## 5. 最初の実行slice

最初は Phase A のうち、Audio source assignmentとmonitorだけを実装対象にする。

1. external control mappingの既存public APIと保存ownerを確定する
2. Property Editorの既存行へsource affordanceを置ける箇所を特定する
3. `audio.amplitude / peak / low / mid / high` の割当を通す
4. raw / processed monitorを既存更新経路で表示する
5. record UIはmonitorが安定してから接続する

このsliceではattack / release、keyframe reduction、State Studio、field拡張を同時に実装しない。

## 6. 関連文書

- `docs/analysis/REPORT_CROSS_APP_FEATURE_OPPORTUNITIES_2026-07-04.md`
- `docs/planned/MILESTONE_LIVE_FIELD_AUTHORING_UX_2026-07-04.md`

## 7. 2026-07-13 静的検証

| 対象 | 証拠 | 状況 |
|---|---|---|
| Audio binding | Composition JSON、複数binding API、Animation menu authoring、Property row表示 | 実装済み |
| Audio preview / record | binding単体monitor、Arm / Commit / Cancel、before / after一括Undo | 実装済み |
| Audio bake | Work Area / Layer / Custom、48kHz解析、attack / release、tolerance reduction | 実装済み |
| State Studio | CRUD、activate / deactivate、focused property capture / remove、Undo | 実装済み |
| State comparison | changed property list、A/B pair保存、viewport toggle、Off時復元 | 実装済み |
| Field authoring | radial / box / linear、stack、Undo、Text / Shape / Cloner consumer | 実装済み |
| 禁止事項 | 新規QtCSS、`QColorDialog`、global signal / slot、Qt compositionなし | 静的確認済み |
| Source hygiene | 変更sourceのCRLF、`git diff --check` | 確認済み |
| Runtime | build / test / CMake | ユーザー許可待ちのため未確認 |

実機確認が完了するまでは文書を`In Progress`に維持する。
- `docs/planned/MILESTONE_GENERATOR_MODIFIER_FIELD_STACK_2026-07-01.md`
- `docs/planned/MILESTONE_SANDBOX_EDITS_2026-06-07.md`
- `docs/planned/MILESTONE_TAKES_SYSTEM_2026-07-08.md`

## 8. 確認範囲

- 文書とソースの静的確認のみ
- 機能実装は`Artifact`子リポジトリのみ変更
- `ArtifactCore` / `ArtifactWidgets`子リポジトリはこのmilestoneでは変更していない
- 親リポジトリは本milestone文書と生成済み文書indexを変更
- build、test、CMakeは実行していない

## 2026-07-25 実装監査

- Audio Reactive の binding／mapping／preview／record／bake、Composition State の CRUD／override／baseline 復元、Field の radial／box／linear と Text／Shape／Cloner consumer は、既存の計画記載とソース構造が一致する。
- State comparison の changed property list、A/B pair 保存、viewport toggle と復元は実装済みだが、Diff の同時 snapshot 描画は未実装である。
- State の並べ替え、final output px 等の実機依存確認、Field の color channel 契約、各種 runtime 受け入れ確認は未完了または未検証である。
- したがって静的実装は高い進捗にあるものの、文書の `In Progress` と runtime 未確認の判定を維持する。
