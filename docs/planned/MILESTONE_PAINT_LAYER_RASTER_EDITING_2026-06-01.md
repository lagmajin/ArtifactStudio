# マイルストーン: Paint Layer / Raster Editing Foundation

> 2026-06-01 作成
> Status: Planned

## 目的

`paintLayer` を、Photoshop の「普通に描けるピクセルレイヤー」に近い体験として導入する場合の設計土台を整理する。

このマイルストーンは、いきなり Photoshop 全機能を再現することではなく、

- composition に置ける
- brush / eraser で直接描ける
- timeline / inspector / undo と自然につながる
- software / Diligent の両方で破綻しにくい
- `QImage` を本流に増やさず、明示的な変換境界を守る

ところまでを対象にする。

## 背景

現状の Artifact では、

- `image layer` は既存の source asset を置く責務が強い
- `brush / clone stamp / eraser` は tool ownership の候補として整理されている
- renderer 側は `QImage` 依存を減らし、`ImageF32x4_RGBA` などの内部表現へ寄せたい

という前提がある。

そのため `paintLayer` は、

- 既存 image layer に雑に paint 機能を足す
- viewport の一時 overlay と永続ピクセル編集を混ぜる
- Qt widget paint と compositor paint を同一責務にする

といった形では入れない方が安全。

## 位置づけ

`paintLayer` は新しい tool ではなく、新しい layer type として扱う。

- `PaintLayer`
  - 永続ピクセル内容を持つレイヤー本体
- `BrushTool` / `EraserTool`
  - `PaintLayer` に stroke を適用する入力手段
- `Mask`
  - paint 内容そのものとは分けて扱う
- `Overlay.Composition`
  - ブラシカーソル、プレビュー円、stroke preview などの一時表示

つまり「描く対象」と「描く道具」と「一時表示」を分離する。

## 非目標

- clone stamp / heal / liquify まで最初から入れること
- PSD 完全互換の pixel layer stack を一気に再現すること
- adjustment layer / smart object / filter gallery を同時に設計すること
- 既存 `image layer` をそのまま `paintLayer` に置き換えること
- `QColorDialog` や QtCSS を足して見た目だけ先に作ること

## 原則

1. `paintLayer` は `image layer` の別名にしない
2. 永続編集対象は project-owned buffer で持つ
3. `QImage` は import/export と Qt 境界の明示変換に限る
4. viewport overlay と pixel commit は別フェーズで扱う
5. 新規 signal/slot を増やさず、既存 command / service / event route に寄せる
6. Diligent / DX12 低レベルへは直接広く触らず、まず renderer 境界で閉じる

## 想定モデル

### Layer model

- `PaintLayer`
  - name / visibility / opacity / blend / transform / time range を持つ
  - paint content buffer を持つ
  - source asset 必須ではない
  - 必要なら初期化時に blank / imported image から生成できる

### Buffer model

- canonical buffer:
  - `ImageF32x4_RGBA` または同等の内部表現
- optional cache:
  - preview 用 GPU texture
  - undo 用 stroke delta
- compatibility boundary:
  - `QImage` への明示変換

### Editing model

- pointer drag 中:
  - overlay preview
  - stroke point 蓄積
- stroke commit 時:
  - paint op を buffer に適用
  - layer invalidate
  - undo command を記録

## フェーズ

### Phase 1: Boundary Audit

#### 目的

既存の image layer / tool / renderer / undo 経路のどこに `paintLayer` を差し込むべきかを確定する。

#### 作業項目

- `image layer` の current responsibility を棚卸しする
- composition viewport の tool routing を確認する
- overlay 描画と永続描画の境界を確認する
- `QImage` が hot path に残っている箇所を洗い出す
- undo に乗せられる最小単位を決める

#### 完了条件

- `PaintLayer` の責務が `image layer` と混ざらず説明できる
- stroke preview と stroke commit の境界が明文化される
- 最初に触るべきファイル群が特定できる

### Phase 2: PaintLayer Core Representation

#### 目的

`PaintLayer` を layer system に追加するための最小モデルを用意する。

#### 作業項目

- 新しい layer type を定義する
- blank layer 作成 API を用意する
- pixel buffer ownership を layer に持たせる
- bounds / width / height / resolution policy を定義する
- persistence の保存形式を決める

#### 完了条件

- empty `PaintLayer` を composition に追加できる設計になっている
- layer save/load で pixel content の保存方針が定義される
- source なし layer と imported-from-image layer の両方を扱える

### Phase 3: Brush Stroke Pipeline

#### 目的

ブラシ入力から buffer 更新までの最小編集経路を成立させる。

#### 作業項目

- `BrushTool` の ownership を app/tool 側で固定する
- stroke sample 収集モデルを決める
- brush dab 合成ルールを定義する
- opacity / flow / hardness の最小セットを決める
- `EraserTool` を「別レイヤー種別」ではなく別 apply mode として扱う

#### 完了条件

- 1本の stroke を `PaintLayer` に反映できる
- brush と eraser が同じ stroke engine を共有できる
- pointer move 中に毎回 full recomposite を強制しない方針がある

### Phase 4: Composition Editor Integration

#### 目的

composition editor 上で `PaintLayer` を違和感なく編集できる入口を作る。

#### 作業項目

- brush cursor / radius preview の overlay 表示
- active tool と active layer の条件整理
- non-paint layer 選択時の fallback behavior 定義
- pan/zoom 中の stroke accidental input を防ぐ
- tablet pressure が無い場合の挙動を決める

#### 完了条件

- viewport 上で paint mode に入れる
- overlay 表示が renderer 境界で完結する
- transform 操作と paint 操作のモード衝突が整理される

### Phase 5: Render Path and Cache

#### 目的

`PaintLayer` の表示と invalidation を既存 compositor に安全に繋ぐ。

#### 作業項目

- buffer から preview texture への upload 境界を決める
- stroke 中の dirty rect 更新方針を定める
- software fallback の描画経路を定義する
- Diligent 側には texture consumer として渡す
- full layer reupload を避ける余地を設計に残す

#### 完了条件

- `PaintLayer` が既存 blend / opacity / transform と一緒に描画される
- stroke 1回ごとの invalidate 粒度が説明できる
- software と Diligent の責務差が整理される

### Phase 6: Undo / Persistence / Assetization

#### 目的

描いた内容を作業として成立させる。

#### 作業項目

- stroke 単位 undo を導入する
- autosave / project save に pixel content を載せる
- external file と internal embedded data の方針を決める
- thumbnail / layer preview との接続を整理する
- imported image から `PaintLayer` を作る導線を定義する

#### 完了条件

- 1 stroke = 1 undo の最小体験が成立する
- プロジェクト再読込後に paint 内容が失われない
- asset browser との関係が破綻しない

### Phase 7: Expansion Track

#### 目的

初期版の先に広げる余地を整理する。

#### 候補

- clone stamp
- selection-aware paint
- layer mask 連携
- symmetry / spacing / jitter
- filter brush
- per-stroke blend mode
- tile / wrap canvas

#### 完了条件

- 初期版を壊さずに拡張できる責務分離がある

## UI 方針

- `PaintLayer` 作成は menu / toolbar command から入る
- 色選択は `FloatColorPicker` または既存承認済み picker を使う
- ブラシ設定は Inspector か tool option surface に寄せる
- 新規 dock をいきなり増やさず、まず既存 surface に収める

## 技術上の注意

- `QImage` をレイヤー内部の canonical storage にしない
- 暗黙の CPU download / GPU upload をしない
- stroke 中の毎フレーム project-wide refresh を避ける
- Diligent backend の低レベル変更は renderer boundary の外へ広げない
- mask / matte / blend と paint の責務を最初から分離する

## 依存・関連

- [MILESTONE_PHOTOSHOP_LIKE_IMAGE_EDITING_2026-04-11.md](/X:/Dev/ArtifactStudio/docs/planned/MILESTONE_PHOTOSHOP_LIKE_IMAGE_EDITING_2026-04-11.md)
- [MILESTONE_TOOLBAR_APP_INTEGRATION_2026-04-17.md](/X:/Dev/ArtifactStudio/docs/planned/MILESTONE_TOOLBAR_APP_INTEGRATION_2026-04-17.md)
- [MILESTONE_VECTOR_LAYER_IMPORT_2026-03-25.md](/X:/Dev/ArtifactStudio/docs/planned/MILESTONE_VECTOR_LAYER_IMPORT_2026-03-25.md)
- [MILESTONE_ARTIFACT_IRENDER_2026-03-12.md](/X:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_ARTIFACT_IRENDER_2026-03-12.md)
- [MILESTONE_OIIO_IMAGE_PIPELINE_MIGRATION_2026-03-30.md](/X:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_OIIO_IMAGE_PIPELINE_MIGRATION_2026-03-30.md)
- [MILESTONE_VIDEO_QIMAGE_RETIREMENT_2026-04-15.md](/X:/Dev/ArtifactStudio/Artifact/docs/MILESTONE_VIDEO_QIMAGE_RETIREMENT_2026-04-15.md)

## 最初にやるなら

1. `Phase 1` の棚卸しで責務境界を確定する
2. blank `PaintLayer` を layer type としてだけ先に通す
3. hard round brush + eraser の 2 つに絞って stroke engine を作る
4. undo と save が成立してから clone stamp 以降へ進む

## 成功条件

- `paintLayer` を「ただの image layer 改造版」として扱わずに済む
- viewport tool と layer persistence が自然につながる
- 既存 render path を壊さずに最小の pixel editing を追加できる
- 将来の Photoshop-like expansion に耐える境界が残る
