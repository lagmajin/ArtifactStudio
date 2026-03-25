# Creative Effect CPU/HLSL Dual Backend

## 目的

`ArtifactCore` の creative effect を、テストしやすい CPU 実装を残したまま、HLSL backend も提供できる構造へ段階移行する。

狙いは次の 3 点。

- CPU 実装を reference として残す
- HLSL 実装を同じ effect ID で選べるようにする
- CPU/HLSL の差分を自動で比較できるようにする

## 現状認識

- `CreativeEffect` は CPU 前提の単一実装になっている
- `CreativeEffectManager` は effect stack を順に `process()` するだけで backend 抽象がない
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
