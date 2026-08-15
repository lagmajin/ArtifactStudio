# マイルストーン: Animated Image Export

> 2026-03-27 作成
> 2026-08-15 現行コード監査

**進捗状態:** Phase 1〜4 は実装済み、Phase 5 は部分実装。実ファイル出力と品質／alpha 検証待ち。

## Update 2026-08-15

Render Queue の共通 FFmpeg sequence encode 経路を更新し、GIF は全フレーム palettegen／paletteuse と無限 loop、APNG は `apng` codec と無限 loop、Animated WebP は `libwebp_anim` と alpha 対応 pixel format／無限 loopを使うようにした。実FFmpeg環境での各形式の書き出し、透明背景、再生互換性、palette品質は未検証。

### 実装状況（2026-07-25 確認）

FFmpegEncoder の GIF／APNG／Animated WebP codec・container 列挙、loop 設定、Render Queue の各 preset と「アニメ画像」分類を確認した。残課題は palette 品質診断、frame delay／透明背景の形式別警告、実際の各形式ファイル出力と再生互換性の検証。

### 2026-08-15 実装確認

- Animated image 用の GIF／APNG／Animated WebP preset と Render Queue の「アニメ画像」分類を確認した。
- FFmpeg encoder 側で codec／container の候補列挙と loop 関連設定がある。一方、形式別の palette 最適化、frame delay の明示的な警告、alpha 制約の UI 診断は確認できない。
- 「候補を選択できる／既存 FFmpeg 経路へ渡せる」状態と、「各形式を実際に書き出し、再生互換性を保証する」状態を分離して扱う。
- 実ファイル出力、透明背景、色品質、frame range、runtime 再生確認は未実施。

判定: **Format surface、preset／分類、encoder 入口は実装済み。品質診断、形式別制約表示、実出力・互換性検証は pending。**

## Update 2026-08-15

現行コードを追加確認した。Render Queue には GIF／APNG／Animated WebP の preset と「アニメ画像」分類があり、FFmpeg encoder 側にも codec／container の候補列挙と loop 設定入口がある。したがって候補選択、preset分類、既存encoder経路への接続は実装済みとして扱える。

未完了・未検証なのは、GIF palette最適化、形式別 frame delay／alpha制約の診断、透明背景・色品質・frame range、各形式の実ファイル出力と再生互換性である。Phase 1〜4は静的実装済み、Phase 5と実出力受入れは pending とする。

## 目的

`動画格納できる画像` を、単発の GIF 対応ではなく、Web 配布や軽量プレビューに使えるアニメーション画像出力として整理する。

このマイルストーンでは、`GIF` / `APNG` / `Animated WebP` を中心に、静止画連番とは別の出力系として扱う。

---

## 背景

現状の出力系は主に以下で構成される。

- MP4 / MOV / WebM などの動画出力
- PNG / JPEG / EXR などの連番画像出力
- ProRes / DNxHD などの編集向け中間形式

一方で、Web や共有用途では次のような軽量なアニメーション画像が欲しい。

- GIF
- APNG
- Animated WebP

これらは「動画を画像として持つ」用途に向いているが、動画出力とは別に品質特性と制約がある。

---

## 方針

### 原則

1. 動画出力とアニメーション画像出力を分ける
2. まずは Web 向けの互換性と扱いやすさを優先する
3. GIF は制限が強いので、APNG / Animated WebP も並走候補にする
4. palette / alpha / frame delay の扱いを出力プリセット側に明示する
5. 既存の FFmpeg ベース出力をできるだけ活用する

### 想定用途

- Web 共有
- 軽量な工程確認
- ループ再生の簡易プレビュー
- SNS / チャット送信用の短尺アニメ

---

## 既存資産

- `ArtifactCore/src/Image/FFmpegEncoder.cppm`
- `ArtifactCore/src/Image/FFmpegEncoder.Helpers.cppm`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
- `Artifact/src/Render/ArtifactRenderQueuePresets.cppm`
- `ArtifactCore/src/Image/FFmpegEncoder.Test.cppm`

---

## Phase 1: Format Surface

### 目的

アニメーション画像出力の候補を出せるようにする。

### 作業項目

- GIF / APNG / Animated WebP の可否を列挙できるようにする
- render queue の preset に候補を追加する
- 出力形式ごとの拡張子とコンテナを整理する
- 「動画」ではなく「animated image」として扱う

### 完了条件

- UI で animated image の候補が見える
- 出力形式が動画と混ざらない

### 進捗メモ

- 2026-03-27: `GIF Animation` / `APNG Animation` / `Animated WebP` の preset を追加
- 2026-03-27: preset selector に `アニメ画像` カテゴリを追加
- 2026-03-27: `FFmpegEncoder` 側で gif / apng / webp の codec 可否と container 可否を列挙可能にした

---

## Phase 2: GIF Path

### 目的

最低限、GIF 出力を実現する。

### 作業項目

- palette 生成の前処理を入れる
- frame delay を扱う
- loop 設定を扱う
- 透過の制約を明示する

### 完了条件

- GIF が書き出せる
- Web 用の簡易アニメとして使える

---

## Phase 3: APNG / Animated WebP

### 目的

GIF より実用性の高い animated image を追加する。

### 作業項目

- APNG 出力
- Animated WebP 出力
- alpha / 色数 / 圧縮率の調整
- GIF との差分説明

### 完了条件

- GIF 以外の web 向け animated image が選べる
- alpha を要する用途が扱える

---

## Phase 4: Preset / UX Integration

### 目的

render queue と preset に自然に組み込む。

### 作業項目

- preset 名と説明の整理
- `Web GIF` / `Web APNG` / `WebP Animation` の用途表記
- output dialog の分類整理
- 静止画連番との混同防止

### 完了条件

- ユーザーが「動画」ではなく「animated image」として選べる
- 互換性重視と品質重視の違いが分かる

---

## Phase 5: Quality / Diagnostics

### 目的

低品質・破綻を避ける。

### 作業項目

- palette 品質の診断
- frame delay の精度確認
- alpha が落ちるケースの警告
- 透明背景の扱い確認

### 完了条件

- 画質劣化の原因を説明できる
- 形式ごとの制約を把握できる

---

## 推奨順

1. GIF Path
2. APNG / Animated WebP
3. Preset / UX Integration
4. Quality / Diagnostics

---

## Related

- `docs/planned/MILESTONE_FEATURE_EXPANSION_2026-03-25.md`
- `docs/planned/MILESTONE_MENU_APP_INTEGRATION_2026-03-27.md`
- `docs/planned/MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md`
- `ArtifactCore/src/Image/FFmpegEncoder.cppm`
- `Artifact/src/Render/ArtifactRenderQueueService.cppm`
