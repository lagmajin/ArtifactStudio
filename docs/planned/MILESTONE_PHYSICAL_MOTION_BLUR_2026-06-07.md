# Physical Motion Blur Milestone

> 2026-06-07 作成

## 目的

モーションブラーを「雰囲気のためのぼかし」ではなく、
**サブフレーム運動を積分して得られる物理ベースの時間的露光表現**として扱う。

このマイルストーンでは、以下を同時に満たすことを狙う。

- GPU ベースで高速に評価できること
- サブフレームの動きベクトルを正確に積分すること
- プレビューでは簡易版を軽く表示し、本レンダリングでは高精度版を使えること
- シャッター角度だけでなく、開口形状も調整できること

## 背景

現状のモーションブラーは、機能の入口は見えているが、制作現場で期待される
「カメラの露光として自然な残像」までは届いていない。

特に不足しているのは次の 4 点。

- レンダリングが重く、ON にすると使いにくい
- サンプル分布が単純で、残像の質が人工的
- シャッター角度を変えたときの表情が荒い
- プレビューと最終レンダリングの整合が弱い

## Goal

- GPU 上で motion vector を積分する物理モーションブラーを実装する
- シャッター開口を `angle` だけでなく `triangle / trapezoid / custom` 形状で扱えるようにする
- `preview` 用の簡易ブラーと `final render` 用の高精度ブラーを分離する
- 速度に応じた可変サンプルや重要度サンプリングを使い、重さを抑える

## Scope

- `Artifact/src/Effect`
- `Artifact/src/Composition`
- `Artifact/src/Widgets/Render`
- `Artifact/src/Widgets/Timeline`
- `ArtifactCore/include/Graphics`
- `ArtifactCore/src/Graphics`
- `Artifact/shaders`
- `docs/planned/MILESTONE_AE_FEATURE_ENHANCEMENT_ROADMAP_2026-04-12.md`
- `docs/planned/MILESTONE_AE_PARITY_BACKLOG_2026-04-29.md`

## Non-Goals

- CPU での全フレーム総当たり積分
- 単純な Gaussian / directional blur だけでモーションブラーを名乗ること
- 既存の再生・キャッシュ・輸出経路を壊すこと
- モーションブラー以外の時間系エフェクトを一度に全部作り直すこと

## Design Principles

- 「見た目の近似」より「露光の意味」を優先する
- preview は軽さ、final は正確さを優先する
- motion blur は layer state ではなく、motion sample と shutter profile の組み合わせで考える
- シャッター形状は角度の派生量ではなく、時間重みの分布として扱う
- 高速化は省略ではなく、サンプル計画と GPU 実装で達成する

## Functional Requirements

### 1. GPU Physical Integration

- サブフレームの motion vector を使って、露光区間を積分する
- 静止画に対する後段 blur ではなく、時間サンプルの加算で作る
- velocity / depth / occlusion を使って、破綻したにじみを抑える

### 2. Shutter Profile Control

- `shutterAngle`
- `shutterPhase`
- `sampleCount`
- `shape`

  を持つ

- `shape` は少なくとも次をサポートする
  - `rectangular`
  - `triangle`
  - `trapezoid`
  - `custom`
- `custom` は時間重みカーブとして定義できるようにする

### 3. Preview And Final Split

- timeline / composition viewer では簡易版 preview を表示する
- render/export では高精度版を使う
- preview は低サンプル数でも動きの方向と重さが分かることを重視する
- final は残像の形と密度を優先する

### 4. Quality And Performance Policy

- sample count を固定値だけでなく、速度や画面占有率で調整できるようにする
- 遅い対象は LOD / partial update / cache を使って負荷を下げる
- 露光が短い場合はサンプルを減らしても自然に見えるようにする

## Phases

### Phase 1: Motion Sample Model

- 目的:
  - motion blur の入力を明確にする

- 作業項目:
  - motion vector / depth / occlusion の入力契約を整理する
  - shutter profile の表現を定義する
  - preview と final の品質レベルを分ける

- 完了条件:
  - 何を積分するかをコード上で説明できる
  - 既存の layer / composition state と競合しない

### Phase 2: GPU Accumulation Path

- 目的:
  - 物理積分を GPU で行う

- 作業項目:
  - サブフレームサンプリングの描画/蓄積パスを作る
  - velocity に応じた sample distribution を実装する
  - オクルージョンや深度破綻の抑制を入れる

- 完了条件:
  - 高速移動の残像が極端に汚くならない
  - sample count の増加が結果に反映される

### Phase 3: Preview Blur Path

- 目的:
  - 編集時に軽い見た目を返す

- 作業項目:
  - 低サンプル・低負荷の簡易ブラーを用意する
  - UI 操作中は preview 設定へ切り替える
  - 本レンダリングと見た目が大きく乖離しないようにする

- 完了条件:
  - preview で motion blur の有無と方向が把握できる
  - 編集操作のレスポンスを阻害しない

### Phase 4: Shutter Shape Editing

- 目的:
  - シャッターの開閉形状を表現できるようにする

- 作業項目:
  - triangle / trapezoid / custom curve を実装する
  - UI から shape を選べるようにする
  - 角度・位相・形状の関係を整理する

- 完了条件:
  - 角度だけではない残像の質を調整できる
  - camera-like な表情差を作れる

### Phase 5: Performance Tuning And Caching

- 目的:
  - 実運用で使える速度に寄せる

- 作業項目:
  - adaptive sampling
  - early-out
  - cache reuse
  - ROI / partial update の見直し

- 完了条件:
  - ON にしても作業継続可能な応答性を維持する
  - heavy shot でも破綻しにくい

## Related Milestones

- `docs/planned/MILESTONE_MOTION_BLUR_2026-03-29.md`
- `docs/planned/MILESTONE_AE_FEATURE_ENHANCEMENT_ROADMAP_2026-04-12.md`
- `docs/planned/MILESTONE_AE_PARITY_BACKLOG_2026-04-29.md`
- `docs/planned/MILESTONE_MOTION_NEXT_2026-06-02.md`
- `docs/done/MILESTONE_RENDER_PREFLIGHT_2026-06-02.md`
- `docs/planned/MILESTONE_TIMELINE_FEATURE_IMPLEMENTATION_2026-04-03.md`

## Acceptance Checklist

- GPU ベースの時間積分として説明できる
- preview と final の経路が分かれている
- shutter angle 以外の形状制御がある
- 高速移動時の見た目が単純 blur より自然である
- 編集中の重さが作業を邪魔しない

## 2026-07-28 実装着手

判定: Phase 1 完了、Phase 2 実装中。モーションブラー固有の責務を `RenderPipeline` へ密結合させず、独立した `MotionBlurPass` として接続した。

入力契約:

- `color`: 現在の合成色バッファ
- `velocity`: 画面空間速度バッファ
- `depth`: 深度バッファ（オクルージョン抑制用）
- `output`: 作業用の別バッファ
- `MotionBlurSettings`: enabled / shutter angle / phase / sample count / quality

責務分離:

- `RenderPipeline`: 色・速度・深度・作業用テクスチャの提供
- `MotionBlurPass`: compute PSO、リソースバインド、ディスパッチ、サンプル積分
- `ArtifactCompositionRenderController`: パスの実行順と preview/final の設定選択

現段階では `motionblurCS.hlsl` の既存実装を直接 `RenderPipeline` に埋め込まず、独立パスから再利用できる形へ整理する。Phase 2 の完了条件は、速度・深度を入力に取り、作業用バッファへ書き出して合成バッファへ戻せることとする。

実装:

- `Artifact/include/Render/ArtifactMotionBlurPass.ixx`
- `Artifact/src/Render/ArtifactMotionBlurPass.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`

`MotionBlurPass` は色・速度・深度・出力を入力契約とし、速度に沿ったサブサンプルを深度差で重み付けして GPU compute で積分する。成功時だけ `RenderPipeline::swapAccumAndTemp()` を行うため、パス失敗時は従来の resolve にフォールバックする。

未完了: shutter shape（triangle / trapezoid / custom）、adaptive sampling、preview/final の品質差、実機での shader compile / runtime 検証。

ビルド・実行確認はリポジトリ方針により未実施。

## Next Step

`MotionBlurPass` の入力契約と compute 実行境界を実装し、その後に preview / final の sample policy を接続する。

## 2026-07-25 実装監査

判定: Phase 1〜5 は未着手。既存の velocity / motion blur 基盤はあるが、物理的な時間積分までは接続されていない。

- `ArtifactMotionBlur` の既存処理は Directional / Radial / Zoom / Velocity / Camera / Transform 系の汎用エフェクトであり、物理的なサブフレーム蓄積そのものではない。
- Render 側には velocity texture / SRV / resolve と 3D mesh の velocity-only pass があるが、専用の physical motion blur accumulation pass、深度・オクルージョンを考慮した temporal resolve は確認できない。
- shutter angle / phase / sample count と、rectangle / triangle / trapezoid / custom の shutter profile を扱う実装は確認できない。
- preview / final の経路分離、adaptive sampling、early-out、cache reuse、ROI 更新も未接続。
- したがって、現状は後続実装の前提となる velocity 基盤と簡易 blur 基盤まで。次の実装単位は、まず motion sample のデータ契約と shutter profile の定義。

ビルド・実行確認はリポジトリ方針により未実施。
