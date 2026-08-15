# Creative Effect CPU/HLSL Dual Backend

**最終更新:** 2026-08-15
**Status:** CPU creative effect と `ArtifactAbstractEffect` の汎用 CPU／GPU／AUTO bridge、HLSL／compute 基盤は存在するが、creative effect 全体への接続と parity 検証は未完了

## 目的

`ArtifactCore` の creative effect を、テストしやすい CPU 実装を残したまま、HLSL backend も提供できる構造へ段階移行する。

狙いは次の 3 点。

- CPU 実装を reference として残す
- HLSL 実装を同じ effect ID で選べるようにする
- CPU/HLSL の差分を自動で比較できるようにする

## 現状認識

- `ArtifactAbstractEffect` は `ComputeMode::CPU/GPU/AUTO` と GPU implementation fallback を持つが、creative effect 全体がこの bridge に接続されているわけではない
- `CreativeEffectManager` は effect stack を順に `process()` する既存経路を維持しており、creative effect ID単位の parity 契約はまだない
- `ShaderManager` / `Graphics.Compute` / `LayerBlendPipeline` は既に HLSL の実行基盤を持っている
- `docs/EFFECT_SYSTEM_SPECIFICATION.md` には effect のカテゴリと将来の GPU 化方針があるが、backend 分離の実装計画はまだ薄い

## 方針

### 1. effect definition と executor を分ける

effect の「何をするか」と「どう実行するか」を分離する。

- definition: effect ID, 表示名, パラメータ schema, category
- executor: CPU / HLSL / fallback

### 2. CPU を canonical にする

既存の CPU 実装は reference として維持する。

- デバッグ時の基準出力
- GPU 実装の fallback
- regression test の oracle

### 3. HLSL は backend 実装として追加する

HLSL 版は CPU の自動変換ではなく、同じ parameter schema を使う別実装にする。

- 共有できる math helper は小さく切り出す
- ノイズや乱数は seed を明示する
- 画像処理は linear 空間前提に寄せる

## 実装段階

### Phase 1: effect metadata の共通化

- effect ID の統一
- parameter schema の明文化
- category と backend capability の追加
- `CreativeEffectFactory` が backend を返せる下地を作る

### Phase 2: CPU reference の固定

- 既存 CPU effect を `reference` として明示する
- `CreativeEffectManager` の stack 処理はそのまま維持する
- 主要 effect の入出力サンプルを固定化する

### Phase 3: HLSL backend の追加

- `EffectExecutor` の GPU 実装を追加する
- `ShaderManager` / compute pipeline へ乗せる
- effect ごとに HLSL source を提供する

### Phase 4: CPU/HLSL 比較テスト

- 同じ入力フレームを CPU/HLSL の両方で処理する
- 画素差分の許容誤差を定義する
- effect ごとの reference output を保存する

### Phase 5: UI 露出

- effect 追加時に backend 選択を出す
- CPU only / GPU available / fallback の表示を出す
- デバッグ時に CPU へ切り替えやすくする

## 初期対象 effect

比較的 HLSL 化しやすく、CPU reference が役に立つものから始める。

- `Posterize`
- `Pixelate`
- `Mirror`
- `Fisheye`
- `Halftone`

後回しにするもの:

- `Glitch`
- `OldTV`
- 乱数依存が強い effect

## 受け入れ条件

- CPU 実装だけで従来どおり動く
- HLSL backend を個別 effect で選べる
- CPU と HLSL の出力差を比較できる
- テスト用に CPU を必ず残せる

## 関連

- [docs/EFFECT_SYSTEM_SPECIFICATION.md](/x:/Dev/ArtifactStudio/docs/EFFECT_SYSTEM_SPECIFICATION.md)
- [ArtifactCore/src/Graphics/Effect/CreativeEffect.cppm](/x:/Dev/ArtifactStudio/ArtifactCore/src/Graphics/Effect/CreativeEffect.cppm)
- [ArtifactCore/src/Graphics/Effect/CreativeEffectManager.cppm](/x:/Dev/ArtifactStudio/ArtifactCore/src/Graphics/Effect/CreativeEffectManager.cppm)
- [ArtifactCore/src/Graphics/Effect/CreativeEffectFactory.cppm](/x:/Dev/ArtifactStudio/ArtifactCore/src/Graphics/Effect/CreativeEffectFactory.cppm)
- [Artifact/src/Render/ShaderManager.cppm](/x:/Dev/ArtifactStudio/Artifact/src/Render/ShaderManager.cppm)
- [ArtifactCore/src/Graphics/Compute.cppm](/x:/Dev/ArtifactStudio/ArtifactCore/src/Graphics/Compute.cppm)

## 2026-07-25 実装監査

判定: CPU creative effect 群と個別の GPU compute 基盤は存在するが、本マイルストーンの dual-backend 抽象と parity 検証は未実装。

- `CreativeEffect` は `process(VideoFrame&, CreativeEffectContext&)` だけを持つ CPU 実行 API で、backend capability、GPU executor、fallback 理由、reference mode は定義されていない。
- `CreativeEffectManager` は有効な effect を stack 順に `process()` するだけで、CPU/HLSL の選択や GPU resource / dispatch の分岐を持たない。
- `CreativeEffectFactory` と各 effect の CPU reference は存在する。一方、初期対象の Posterize / Pixelate / Mirror / Fisheye / Halftone に対して、この CreativeEffect ID と同じ schema を使う HLSL backend の組み合わせは確認できない。
- 別系統の `Graphics.Compute` や `LayerBlendPipeline`、Artifact 側の GPU effect wrapper は HLSL 実行基盤として利用可能だが、CreativeEffect の backend bridge や共通 parameter upload を証明するものではない。
- CPU/HLSL の同一入力比較、許容誤差、reference output 保存、UI の backend/fallback 表示は未実装。
- 次の実装単位は、まず definition / parameter schema と CPU/HLSL executor capability の最小契約を導入すること。

ビルド・実行確認はリポジトリ方針により未実施。

## 2026-08-15 現行実装監査

- CPU 側の `CreativeEffect`／`CreativeEffectManager`／`CreativeEffectFactory` と各 effect の reference 実装は現行コードに存在する。
- `Graphics.Compute`、`SinglePassShader`、`PointwiseEffectFusion`、`LayerBlendPipeline`、`ShaderManager` など、別系統の GPU／HLSL 実行基盤も拡張されている。
- ただし CreativeEffect ID と parameter schema を GPU executor に橋渡しする backend 選択、fallback 理由、共通 upload、CPU/HLSL 同一入力比較は確認できない。
- よって Phase 1〜2 は CPU 側基盤、Phase 3〜5 は未実装または未検証。ビルド／GPU 実行は行っていない。
