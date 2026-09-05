# マイルストーン: Construction Layer

**作成日**: 2026-06-05  
**状態**: 部分完了（Construction Layerの基盤・保存・表示・UI導線を実装、拡張construction itemと専用animation/runtime検証は未完了）
**最終更新:** 2026-09-05
**優先度**: 中

---

## 目的

`Construction Layer` は、**既定ではレンダーしない作業用の設計レイヤー** です。ユーザーが最終出力への包含をONにした場合はレンダー対象にします。

AE 風の制作では、レイアウト確認や構図検討のために次の情報を同じ制作文脈で持ちたい場面があります。

- line
- circle / ellipse
- grid
- annotation / note
- safe area
- orbit / motion guide

これらを viewport overlay に散らすのではなく、**親子付けできて、アニメーションできて、保存できる layer** としてまとめるのがこの提案の狙いです。

---

## 背景

現状のコードベースでは、grid / guide / safe margin / motion path / note 的な責務が別々の surface に分かれやすいです。

その結果、次の問題が起きます。

- 設計補助情報の置き場が分散する
- 編集対象と補助情報の関係が layer 階層として追いにくい
- 制作中の「仮置き情報」を composition 内で一元管理しにくい

`Construction Layer` はこの分散を整理し、**制作中の設計メモを layer として保持する** ための受け皿です。

---

## Scope

- composition 内での construction object の管理
- parent / child 階層
- animation / keyframe 対応
- editor 上での可視化
- project 保存 / 復元

---

## Non-Goals

- 暗黙に final render に出すこと（明示オプションによる出力は対応）
- review / annotation の履歴管理を置き換えること
- viewport の一時 overlay を全部置き換えること
- grid toggle や guide toggle の UI をそのまま廃止すること

---

## 定義

### Construction Layer

`Construction Layer` は、**出力結果には含めないが、編集時には見える設計情報を持つレイヤー** です。

基本ルール:

- final render では無視する
- project データには保存する
- parent / child を持てる
- transform を持てる
- time-based にアニメーションできる
- editor では表示可能だが、通常の content layer と視覚的に区別する

### LayerType 方針

`Construction Layer` は既存の `Null` や `Guide` を流用せず、**独立した `LayerType::Construction`** として追加する方針にします。

理由:

- `Null` は親子付け専用で、設計形状や annotation を持つ用途とずれる
- `Guide` は表示制御の意味合いが強く、保存可能な設計レイヤーとしては狭い
- `Construction Layer` は line / circle / grid / note / safe area / orbit をまとめるため、専用の型があったほうが model が読みやすい

実装上の基本方針:

- `ArtifactLayerFactory` に construction 分岐を追加する
- project の serialize / deserialize に含める
- final render path では `LayerType::Construction` を明示的に skip する
- editor / timeline / inspector では通常 layer と別の badge で扱う
- 既存プロジェクトに新型がない場合は影響しない

## 2026-08-15 現行実装監査

- `ArtifactConstructionLayer`、`LayerType::Construction`、factory 分岐、JSON 保存復元、Construction property group、GuideSet、SmartGuides 連携を確認した。
- Timeline／Inspector の Construction 表示、Layer menu からの作成導線、final render inclusion の明示フラグも実装されている。
- ただし line／circle／annotation／orbit などの拡張 item モデル、points／radius／text の専用 animation、複数 construction layer の runtime／再読込 parity は未確認。
- したがって基盤・保存・表示・UI導線は実装済み、提案された全 item／animation／runtime 完成条件は未達とする。

## Update 2026-08-15

- 現行コードでは `ArtifactConstructionLayer`、factory／JSON round-trip、Construction property group、SmartGuides 連携、Timeline／Inspector 表示、作成メニューを確認できる。
- line／circle／annotation／orbit 等の拡張 item、points／radius／text の専用 animation、複数 layer の runtime／再読込 parity は未完了または未確認。

---

## 収容する情報

Construction Layer が扱う対象は、次のような「制作のための補助形状 / 補助注釈」です。

- `Line`
  - アクション線、注釈線、方向指示
- `Circle` / `Ellipse`
  - orbit、注釈円、強調円
- `Grid`
  - 構図補助、均等配置、モジュール確認
- `Annotation`
  - ラベル、メモ、短い注釈
- `Safe Area`
  - action safe / title safe / 独自プリセット
- `Orbit / Motion Guide`
  - カメラや対象物の軌道確認、動線の確認

---

## 主要プロパティ

### 共通

- `id`
- `name`
- `parentId`
- `children`
- `enabled`
- `locked`
- `visibleInEditor`
- `visibleInTimeline`
- `color`
- `opacity`
- `blendMode` ではなく、construction 用の表示スタイル

### 位置・変形

- `position`
- `rotation`
- `scale`
- `anchor`
- `zOrder`

### 施工内容

- `itemType`
  - line / circle / grid / annotation / safe area / orbit
- `points`
  - 線分や軌道の制御点
- `radius`
  - 円 / 安全圏の半径系
- `gridSpacing`
  - grid の密度
- `text`
  - annotation の本文
- `preset`
  - safe area や grid のテンプレート

---

## アニメーション可能項目

すべてをアニメーション対象にする必要はないですが、少なくとも次は time-based に変化できるべきです。

- `visibility`
- `opacity`
- `position`
- `rotation`
- `scale`
- `points`
- `radius`
- `gridSpacing`
- `text` の表示状態
- `safe area preset` の切り替え

補足:

- note 自体の文字列をキーフレームで変えるより、表示状態や位置をアニメーションさせる方が実務では使いやすい
- orbit / guide は、点列や中心点を持つ形でアニメーションしやすくする

---

## 親子関係

Construction Layer は parent / child を持てます。

### ルール

- parent の transform は child に継承される
- child の construction object は parent の空間で解釈される
- 循環参照は禁止
- parent を削除する場合は child の再親子付け方針を明示する

### 使い方の例

- camera orbit の中心点を parent に置き、補助線群を child にぶら下げる
- safe area と注釈を同じ construction group にまとめる
- grid を親レイヤーの移動に追従させる

---

## レンダー規則

### Final Render

- `construction.includeInFinalRender` は既定OFF。OFF時はexport / render queue / final frameでスキップする。
- ON時は最終出力へ含める。`isGuide` も整合させ、既定のguide除外で再度落ちないようにする。

### Editor Render

- editor viewport では表示してよい
- 通常の content layer と見分けられる style にする
- selection / hover / lock 状態は editor 表示として残してよい

### 変換方針

- 必要なら通常 layer へ変換できる
- ただし変換は明示操作に限定する
- 暗黙に content layer へ混ぜない

---

## UI 方針

- layer panel で通常 layer と見分けられる badge を出す
- inspector では construction item の型別編集を出す
- viewport では construction geometry を editor-only overlay として描く
- timeline では animation 可能なプロパティだけを見せる

### 見せ方の原則

- final image の邪魔にならない
- でも「何を設計しているか」は読める
- 補助線は薄く、注釈は読める、選択時だけ強調する

---

## 実装フェーズ案

### Phase 1: Data Model

- `Construction Layer` のデータ型を定義する
- 保存 / 読み込みを通す
- parent / child を持てるようにする

### Phase 2: Editor Visibility

- viewport で construction object を表示する
- selection と lock の基本状態を扱う
- final render から除外する

### Phase 3: Animation

- position / rotation / scale / opacity を time-based に扱う
- line / circle / safe area / orbit の制御点をアニメーション可能にする

### Phase 4: Presets and Creation

- line / circle / grid / note / safe area / orbit の作成プリセットを用意する
- 既存の guide / overlay 操作から移行しやすくする

### Phase 5: Conversion / Cleanup

- 必要なら通常 layer や shape layer に変換する導線を作る
- 一時的な construction content を整理・削除しやすくする

---

## 成功条件

1. Construction Layer を project 内に保存できる
2. 親子付けしても破綻しない
3. timeline で必要な項目をアニメーションできる
4. final render には明示的に包含をONにした場合だけ出力される
5. editor では設計情報として十分読める

---

## 関連文書

### 2026-09-05 実装追補

- 任意Line / Circle / Annotationと保存済み水平・垂直GuideSetを既存ArtifactIRendererで描画する。円は128分割、注釈は既存の変換付き文字描画を使う。
- Inspectorの `Construction · Line / Circle / Annotation` グループからenabled・opacity・座標・半径・文字を編集する。型ごとの末尾には無効な追加用項目を表示する（各型128項目まで）。enabledをONにすると表示され、OFFは内容を保持する。追加用グループの再表示にはInspectorの再構築／レイヤー再選択が必要な場合がある。
- type/ordinalのプロパティパスを固定し、既存の値変更Undoに接続する。追加をUndoすると無効な項目が残るが、表示と編集値は復元する。項目の消去／並べ替えUIは未実装。
- VPのSelectionツールで選択したConstruction Layerに固定ピクセルサイズのハンドルを表示する。Line端点／中央、Circle中心／4方向の半径ハンドル、Annotation配置点をドラッグできる。ドラッグ開始のカメラとworld inverseでローカル平面へrayを交差させる。Shiftは移動をローカルX/Yに拘束する（半径は距離操作）。
- ドラッグのreleaseは `Edit construction item` Undoを1件追加し、Esc／右クリックは開始値へ戻す。ロック・選択ロック・非表示・選択レイヤー／時刻変更時は編集を拒否またはキャンセルする。項目更新はSource dirtyで描画キャッシュを無効化する。VPでの文字入力・新規項目の作図は未対応で、作成／文字編集はInspectorを使用する。
- 表示中のgrid、thirds、center、safe area、baseline、custom guide、line端点／中点、circle中心／4基準点、annotation配置点をVP吸着候補へ渡す。親子transform適用後に投影し、既存のsnap ON/OFF・Alt解除・候補キャッシュを使う。
- 極端に大きく密なgridは各方向約2048区間に制限し、表示と吸着で同じspacingを使う。
- GPU/Diligent資源・同期・backendは変更しない。新規QImage／QtCSS／signal-slot接続なし。
- ビルド・テスト・実機確認は未実行。Inspectorの編集・Undo・保存復元・親子transform・スナップと最終出力ON/OFFの実機検証が残る。項目固有のアニメーション、楕円、寸法線、下絵画像は今回の対象外。

- `docs/WIDGET_MAP.md`
- `ae_maturity_additional_analysis.md`
- `docs/planned/MILESTONE_COMPOSITION_MOTION_PATH_OVERLAY_2026-03-28.md`
- `docs/planned/MILESTONE_REVIEW_COMPARE_ANNOTATION_2026-03-28.md`
- `docs/planned/MILESTONE_AD_PRODUCTION_ACCELERATOR_2026-05-28.md`
