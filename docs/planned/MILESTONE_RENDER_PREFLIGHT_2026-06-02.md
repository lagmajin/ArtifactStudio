# MILESTONE: Render Preflight / Output Safety Check

**Date**: 2026-06-02  
**Status**: Proposed  
**Priority**: High  
**Related**: `docs/MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md`, `docs/planned/MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_PHASE1_EXECUTION_2026-05-12.md`, `docs/planned/MILESTONE_BACKGROUND_UTILITY_WORKER_PROCESS_2026-04-22.md`, `docs/planned/MILESTONE_APP_DIAGNOSTIC_COHESION_2026-05-13.md`, `docs/planned/MILESTONE_APP_DEBUGGER_RENDER_COST_BREAKDOWN_2026-04-24.md`

---

## 概要

3D プリンタや CAM の「出力前検査」を、映像の render/export 前に持ち込む。

この機能は、出力を止めるための厳格な gate というより、**失敗しやすさを事前に見える化する preflight** として設計する。  
Error はブロック対象、Warning は案内対象、Info は見積もりや補助情報として扱う。

---

## 何を見るか

### 1. Render Time Estimate

- フレーム数
- 解像度
- エフェクト数
- レイヤー数
- 参照キャッシュの有無
- 過去の同種ジョブの実績

出力:

- おおよその所要時間
- レンジ見積もり
- 予測信頼度

### 2. VRAM / Memory Estimate

- 1 フレームあたりの推定中間バッファ数
- 最大同時テクスチャ面積
- 既存 cache の再利用可否
- 4K / 8K / HDR のような重い条件

出力:

- 推定 VRAM 使用量
- 安全マージン
- 危険度

### 3. Breakage Detection

- 黒フレームの可能性
- 破綻フレームの可能性
- 音ズレの可能性
- missing asset / missing proxy
- alpha / color space / format mismatch

出力:

- 検出理由
- 影響範囲
- 該当フレーム帯

### 4. Spec / Delivery Check

- 解像度
- frame rate
- コーデック
- bit depth
- color space
- alpha 要否
- 納品プリセットとの整合

出力:

- 仕様不一致
- そのまま出してよいか
- どこを直すべきか

---

## 実現方針

### Phase 1: Static Preflight

**目標**: 実際のレンダーを走らせずに、入力情報だけで危険度を出す。

- project / composition / render settings を読む
- missing asset, invalid output setting, obvious mismatch を検出する
- Error と Warning を分ける

**完了条件**:

- 出力前に止めるべきものが読める
- false positive が多すぎない

### Phase 2: Heuristic Cost Model

**目標**: レンダー時間とメモリ使用量の概算を出す。

- レイヤー数と effect stack を重み付けする
- high-res layer, cache miss, heavy effect に加点する
- 履歴がある場合は過去実績で補正する

**完了条件**:

- 時間見積もりがレンダーキューに表示できる
- VRAM の危険ラインが読める

### Phase 3: Sample Render Probe

**目標**: 少数フレームを試し、実際の failure sign を拾う。

- 代表フレームの sample render
- black frame / alpha anomaly / obvious color mismatch の検査
- audio 付きジョブなら sync drift の兆候を確認

**完了条件**:

- 「事前の予測」と「実際の症状」がつながる
- 失敗の兆候が具体的に出る

### Phase 4: User-Facing Preflight Surface

**目標**: 診断結果を出力直前の画面で読みやすくする。

- render queue / export dialog に preflight summary を出す
- App Debugger に cost breakdown を出す
- Problem View へも同じ結果ソースを流す

**完了条件**:

- ユーザーが出力前に判断できる
- 警告の根拠と対処が読める

---

## データ契約

```text
RenderPreflightResult
├─ severity (Error/Warning/Info)
├─ category (MissingAsset/FormatMismatch/MemoryRisk/SyncRisk/QualityRisk)
├─ message
├─ description
├─ confidence
├─ estimatedTime
├─ estimatedMemory
├─ affectedRange
└─ fixHint
```

---

## Non-Goals

- 出力の自動修復
- AI が最終判断を断定すること
- render path の heavy logic を UI へ持ち込むこと
- preflight を render 本体より重くすること

---

## 推奨入力ソース

- `ArtifactProjectManager` の validation 結果
- `ArtifactComposition` / `CompositionFinalEffectStack`
- `ArtifactRenderScheduler`
- `ArtifactCompositionPlaybackController`
- cache / proxy / missing asset の状態
- render settings / output preset

---

## 推奨順序

1. static preflight
2. cost estimate
3. sample probe
4. UI surface unification

---

## Related Execution Memo

- [`MILESTONE_RENDER_PREFLIGHT_PHASE1_EXECUTION_2026-06-02.md`](./MILESTONE_RENDER_PREFLIGHT_PHASE1_EXECUTION_2026-06-02.md)

---

## 期待効果

- 納品前の事故を減らせる
- 出力が重い理由をユーザーが理解できる
- AE / Premiere より「安心して出せる」感を作りやすい
