# Render Center UI Draft (2026-03-28)

## Purpose

現在の `RenderQueueManagerWidget` は、render queue の UI として情報密度と責務の分離が弱く、一覧・編集・監視・履歴・再実行が 1 パネルに競合している。

このドラフトは、render 周辺を独立ページ `Render Center` として再構成する前提で、UI 仮説とレイアウト方針をまとめたもの。

関連:

- `docs/WIDGET_MAP.md`
- `docs/bugs/RENDER_SYSTEM_AUDIT_2026-03-27.md`
- `Artifact/src/Widgets/Render/ArtifactRenderQueueManagerWidget.cpp`

## Current Problems Hypothesis

`UI センスが微妙` に見える主因は見た目だけではなく、情報設計の歪みにあると考える。

### 1. 役割が混線している

現状の render queue には次の責務が同居している。

- ジョブ投入
- ジョブ一覧
- 個別ジョブ編集
- 実行監視
- 失敗確認
- 履歴閲覧
- 再実行
- プリセット管理

このため、画面全体に主役が存在せず、操作の優先順位が弱い。

### 2. 一覧が弱く、詳細に依存しすぎる

ジョブ一覧が `#番号 + 名前 + 進捗%` の 1 行表現に近く、以下が一覧時点で読みにくい。

- 出力形式
- 出力先
- 現在状態
- 失敗理由
- 重要度

結果として、ジョブの比較より「選んで右を見る」前提の UI になっている。

### 3. 失敗処理と履歴の扱いが中途半端

履歴欄は常設されているが高さ固定で読みづらく、失敗ジョブの回復導線とも統合されていない。

### 4. 全体操作と個別操作の階層が曖昧

`START RENDER / Pause / Stop / Rerun Selected / Rerun All Finished` が近いレベルで並び、主操作と例外操作が混ざって見える。

### 5. 独立した作業文脈として扱われていない

render は timeline 編集と異なる文脈を持つ。

- 編集中は軽く queue に送る
- 出力時は render 状態を集中管理する

この 2 つを同じ dock パネルで処理すると、どちらにも最適化されにくい。

## Direction

`Render Queue` を単なる dock widget ではなく、独立ページ `Render Center` として扱う。

ただし、完全分離ではなく 2 層構成を想定する。

### Layer 1: Editor Side

編集中の軽い導線は既存画面に残す。

- `Add to Queue`
- `Open Render Center`
- 現在のジョブ数や簡易状態

### Layer 2: Render Center

本格的な管理は専用ページで行う。

- ジョブ一覧
- 選択ジョブの詳細と編集
- 失敗ジョブの回復
- 実行履歴
- プリセット
- 診断

## One-Screen Layout Proposal

```text
┌────────────────────────────────────────────────────────────────────────────────────────────┐
│ Render Center                                                               [Start All]   │
│ 12 jobs  •  3 running  •  2 failed  •  5 done  •  Avg 48%                  [Pause] [Stop]│
├────────────────────────────────────────────────────────────────────────────────────────────┤
│ [Add Current Comp] [Add Multiple] [Presets]      Search...   [All v]   [Active First v]  │
├──────────────────────────────┬───────────────────────────────────────┬─────────────────────┤
│ Queue                        │ Job Inspector                         │ Recovery / Activity │
│                              │                                       │                     │
│ > Intro_v03                  │ Intro_v03                             │ Failed Jobs         │
│   Rendering   42%            │ Status: Rendering 42%                │ 1. LogoLoop         │
│   H.264 MP4 / 1080p          │ Preset: H.264 MP4                    │    frame 118 timeout│
│   out/intro_v03.mp4          │ Output: out/intro_v03.mp4 [Browse]   │    [Retry] [Open]   │
│                              │ Range: 0 - 240                       │                     │
│   LogoLoop                   │ Video: 1920x1080 / 30fps             │ Recent Activity     │
│   Failed                     │ Codec: H.264 / High                  │ 12:41 started Intro │
│   ProRes 4444 / 4K           │                                       │ 12:42 25% Intro     │
│   out/logo.mov               │ [Format Settings...]                 │ 12:43 Logo failed   │
│                              │                                       │                     │
│   MainCut                    │ Advanced                              │ Diagnostics         │
│   Pending                    │ Overlay X [0] Y [0]                  │ encoder: ffmpeg     │
│   PNG Sequence / 4K          │ Scale [1.0] Rot [0]                  │ backend: GPU        │
│   out/main_####.png          │                                       │ output writable     │
│                              │ [Duplicate] [Remove] [Retry Failed]   │                     │
│   TeaserA                    │                                       │                     │
│   Completed 100%             │                                       │                     │
│   GIF / 720p                 │                                       │                     │
│   out/teaser.gif             │                                       │                     │
├──────────────────────────────┴───────────────────────────────────────┴─────────────────────┤
│ Bottom Rail: total progress bar + compact event text                                         │
└────────────────────────────────────────────────────────────────────────────────────────────┘
```

## Information Architecture

### Left: Queue

画面の主役。選択前でも状態を比較できることが重要。

各ジョブで最低限見えるべき情報:

- 状態
- 名前
- 出力形式
- 解像度またはプリセット
- 出力先
- 進捗

望ましい性質:

- 色ではなく構造で読める
- 失敗ジョブを上位に寄せられる
- 実行中ジョブをひと目で追える

### Center: Job Inspector

選択中ジョブだけを編集する領域。

優先順:

1. 状態
2. 出力先
3. フォーマット要約
4. フレーム範囲
5. 回復または補助操作
6. 高度設定

フォームを先に並べるのではなく、要約を先に見せる。

### Right: Recovery / Activity

独立ページ化の価値が最も出る領域。

ここに置くもの:

- failed jobs
- failed reason
- retry actions
- recent activity
- diagnostics

`History` は単独セクションとして下に大きく置くより、この列に集約する方が自然。

## Interaction Principles

### Global actions

ページ上部に置く。

- Start All
- Pause
- Stop

### Selection actions

選択ジョブに紐づける。

- Duplicate
- Remove
- Retry Failed
- Open Output

### Exception actions

失敗や warning に関する行動は、Recovery 領域に寄せる。

## Visual Tone

### Keep

- DCC 的な高密度 UI
- 暗めのベース
- 実務寄りの読みやすさ

### Avoid

- タイトルの装飾過多
- 背景色ベタ塗りで状態表現をしすぎること
- 主操作と補助操作の同格表示
- 常時大きい履歴欄

### Style hints

- 状態色はピルや小インジケータに限定
- 行背景は極力ニュートラル
- 主要 CTA は `Start All` のみ強くする
- セクション見出しは静かに、情報を先に読む

## Naming Proposal

`Render Queue` より `Render Center` または `Export Center` の方が、実際の責務に合っている。

理由:

- queue だけでは並び替えリストに見える
- 実際には監視、回復、診断、設定まで含む

暫定的には `Render Center` を推す。

## Mapping To Existing Implementation

現在の `ArtifactRenderQueueManagerWidget` を完全に捨てる必要はない。既存部品を整理して再配置する発想でよい。

### Reuse candidates

- ジョブ取得と更新の service 連携
- output path 編集
- output settings dialog 導線
- frame range 編集
- overlay transform 編集
- progress / status 監視
- preset 選択ロジック

### Needs redesign

- job list item 表現
- bottom action 群の優先順位
- history の常設表示
- failed job 回復導線
- preset の見せ方

### Likely migration path

1. `RenderQueueManagerWidget` の責務を棚卸しする
2. 独立ページ shell を作る
3. queue / inspector / recovery の 3 カラムに整理する
4. history を recovery/activity に寄せる
5. editor 側には軽量導線だけ残す

## Draft Scope Notes

このドキュメントは UI 仮説であり、以下は未確定。

- 実際のクラス命名
- dock と page の共存方式
- Project View からの遷移導線
- review / share 系との接続
- failed frames 専用ビューの詳細設計

## Next Draft Candidates

- low-fidelity wireframe v2
- Qt 実装に落とす最小差分案
- editor 側の `Add to Queue` 導線仕様
- render / review / share の統合導線
