# エフェクト UI 標準化 Milestone

**作成日:** 2026-06-07  
**ステータス:** 計画中  
**関連コンポーネント:** ArtifactInspectorWidget, ArtifactPropertyWidget, EffectStack, OFXHost, Preset Browser

---

## 概要

エフェクトごとに UI の見え方がバラバラになっている状態を解消し、すべてのエフェクトに共通の操作文法を持たせるためのマイルストーンです。

対象は標準エフェクトだけでなく、将来的なサードパーティ製エフェクトも含みます。
最初の目標は「どのエフェクトでも同じ場所から、同じ手順で、同じ種類の情報を扱える」ことです。

---

## 背景

現状は以下がエフェクトごとに散らばっています。

- プレビュー表示の有無
- プリセット保存の導線
- アピアランス設定の有無と配置
- ヒストグラム、境界処理、反転、ブレンド補助などの補助 UI

その結果、ユーザーは「このエフェクトには何があるのか」を毎回覚え直す必要があります。
標準化の狙いは、個別機能の増減ではなく、**UI 契約の共通化**です。

---

## 目標

- すべてのエフェクトに共通のヘッダ領域を持たせる
- `Preview` の表示切り替えを共通化する
- `Preset Save / Load` の導線を共通化する
- `Appearance` の扱いを共通化する
- サードパーティ製エフェクトも同じ UI ルールで表示できるようにする

---

## 標準 UI 契約

### 1. Common Header

全エフェクトに最低限以下を表示する。

- エフェクト名
- 有効 / 無効
- プレビュー状態
- プリセット操作
- アピアランス入口

Header は「見出し」ではなく、エフェクトの共通操作面として扱う。
個別エフェクトが独自に持つ設定は header に散らさず、後続の `Appearance` や `Advanced` に寄せる。

### 2. Preview Channel

プレビューは「そのエフェクトだけの表示状態」を示す標準項目にする。

- ON / OFF
- before / after の簡易比較
- 既定状態の視認性

### 3. Preset Contract

プリセットは各エフェクトのローカル機能ではなく、共通の保存文法として扱う。

- save
- load
- rename
- recent
- factory preset

### 4. Appearance Contract

`Appearance` はエフェクト専用の寄せ集め欄ではなく、境界処理や表示モードをまとめる標準セクションにする。

例:

- blur / sharpen の edge handling
- curve / level の表示補助
- repeat / reflect / clamp
- histogram / graph / matte preview

### Standard Effect Examples

最初に標準化対象として扱う代表例。

- `Gaussian Blur`
  - `Appearance`: repeat / reflect / clamp / edge bleed
  - `Preview`: before / after toggle
  - `Preset`: blur strength / quality / direction preset
- `Sharpen`
  - `Appearance`: edge handling / halo suppression / clamp
  - `Preview`: sharpen amount の見え方確認
  - `Preset`: subtle / standard / strong
- `Curves`
  - `Appearance`: histogram / graph / channel view
  - `Preview`: channel compare / before after
  - `Preset`: contrast / film / flat
- `Levels`
  - `Appearance`: histogram / input output black white / gamma
  - `Preview`: clipping / range indication
  - `Preset`: auto / broadcast / full range
- `Glow`
  - `Appearance`: blend mode / threshold / bloom edge
  - `Preview`: glow contribution switch
  - `Preset`: subtle / dreamy / intense
- `OFX Plugin`
  - `Fallback`: custom controls
  - `Appearance`: best effort で boundary / display params を抽出
  - `Preset`: host 側の共通 preset contract を使う

---

## データ契約

### Effect UI Descriptor

エフェクト UI は、描画実装とは別に descriptor で表現できるようにする。

最低限の項目:

- `displayName`
- `enabled`
- `previewMode`
- `presetScope`
- `appearanceSection`
- `fallbackSection`

### Section Classification

各パラメータは次のどれかに分類する。

- `Header`
- `Preview`
- `Preset`
- `Appearance`
- `Advanced`
- `Fallback`

分類の目的は UI の並び替えであって、パラメータの意味そのものを変更することではない。

### Compatibility Rule

- 標準エフェクトは descriptor を明示的に持つ
- OFX / サードパーティは既存メタデータから best effort で推定する
- 推定できない項目は `Fallback` に落とす
- UI が欠けるより、意味を維持して折りたたむことを優先する

---

## Phase 構成

### Phase 1: 共通ヘッダ定義

- エフェクト UI の共通 header grammar を決める
- 標準エフェクトと OFX エフェクトの両方で使える最小の共通語彙を固定する
- `Preview / Preset / Appearance` の表示順を統一する
- header の中で `toggle`, `preset`, `appearance`, `preview` を分離する
- 既存の effect inspector を壊さず、順次 descriptor を差し込めるようにする

完了条件:

- 主要エフェクトが同じヘッダ構造を持つ
- ヘッダの見た目と操作位置がエフェクトごとに大きく変わらない
- header の情報が property row に埋もれない

### Phase 1.5: Effect UI Bridge Skeleton

- `EffectStack` 側に descriptor を渡す最小のブリッジを作る
- `ArtifactInspectorWidget` から共通 header を引けるようにする
- `ArtifactPropertyWidget` は既存 row system のまま、section 切り替えだけ共通化する

完了条件:

- 1 つの effect で descriptor を出せる
- 既存 UI の破壊的変更なしに新しい section 名が見える

### Phase 2: Preset Workflow 統合

- effect preset の save / load / recent を共通メニューに統合する
- preset browser との接続点を作る
- 既存の個別 preset UI を共通導線へ寄せる
- `factory preset` と `user preset` を同じ一覧で扱うが、表示ラベルは分ける
- preset の保存先は effect 固有でもよいが、呼び出し方法は統一する

完了条件:

- どのエフェクトでも同じ入口から preset を扱える
- preset の追加導線が effect ごとに別実装にならない

### Phase 2.5: Preset Browser Bridge

- effect preset browser の card grammar を fixed する
- preset のメタ情報を preview / type / source / compatibility で読めるようにする
- save/load の入口を inspector から引けるようにする

完了条件:

- preset browser と effect inspector の表示語彙が一致する

### Phase 3: Appearance Section 標準化

- appearance section の標準項目を定義する
- 現在は個別に埋もれている設定を appearance 配下へ寄せる
- `Histogram / Edge Mode / Preview Hint / Range Mode` のような補助 UI を共通表現に寄せる
- `edge handling` は blur / sharpen / matte の共通語彙にする
- `histogram` や `graph` は表示専用コンポーネントとして分離する
- appearance がない effect でも空 section を出すかどうかをルール化する

完了条件:

- 標準エフェクトで appearance section の意味が揃う
- `Appearance` が「何でも置く箱」ではなくなる

### Phase 3.5: Visual Widgets Catalog

- histogram / curve / edge mode / preview hint の共通 widget をカタログ化する
- 同じ widget を複数 effect で再利用できるようにする
- 今後の effect 追加時に widget を流用しやすくする

完了条件:

- 似た見た目の補助 UI が個別実装で増殖しない

### Phase 4: OFX / Third-party Bridge

- OFX パラメータのうち、標準 UI 契約に乗るものを自動分類する
- サードパーティ製エフェクトでも header / preset / appearance の枠を維持する
- カスタム UI のあるプラグインは fallback として扱う
- custom parameter は `Fallback` section に逃がす
- OFX 側に UI 情報が足りないときは safe default を使う
- 外部プラグインの破壊的なレイアウト変更はしない

完了条件:

- OFX エフェクトでも共通 UI の骨格が崩れない
- 外部エフェクトであっても `Preview / Preset / Appearance` が見える

### Phase 4.5: Third-Party Fallback Policy

- 未分類パラメータの表示規則を固定する
- プラグインが独自 UI を持つ場合の折りたたみルールを決める
- 互換性優先で、見せ方の統一を優先しすぎない

完了条件:

- 変換できない plugin でも UI が破綻しない

### Phase 5: Inspector / Property Editor Harmonization

- Inspector と Property Editor で別の見え方になっている部分を揃える
- effect stack の各項目に共通の affordance を持たせる
- UI 文法のズレを最小化する
- effect stack item の hover / selection / active state を統一する
- property row に特殊 effect だけの独自ボタンを増やさない

完了条件:

- Inspector 側と property 側で同じエフェクトが同じ意味で見える
- 主要エフェクトで操作導線の差が目立たない

---

## 対象範囲

- `ArtifactInspectorWidget`
- `ArtifactPropertyWidget`
- `EffectStack`
- `OFXHost`
- preset browser

### Inspector Entry Points

`ArtifactInspectorWidget` 側で最初に触る場所。

- effect rack の表示行
- effect add / remove / reorder の文脈
- rack context menu
- current effect の summary / state row

`ArtifactPropertyWidget` 側は、ここで分類された section を受けて表示する。
つまり、この milestone の主導権は `Inspector` に置き、`Property` は受け皿として段階的に揃える。

### Property Editor Role

`ArtifactPropertyWidget` は、effect-specific UI を増やす場所ではなく、分類済み section を整えて見せる場所にする。

担当する役割:

- `Appearance` と `Advanced` の見出し表示
- `Fallback` section の折りたたみ
- preset 関連 controls の共通表示
- 既存 property row の視線誘導を壊さない整理

扱わないこと:

- effect ごとの独自 affordance を増やすこと
- Inspector と別の意味を持つ section 名を作ること
- plugin 固有の操作を property row に直接埋めること

---

## 実装順

1. descriptor / section classification
2. inspector への共通 header 接続
3. preset browser との bridge
4. appearance widget catalog
5. OFX fallback policy
6. property editor の見え方整流

### Implementation Focus for Inspector

- `ArtifactInspectorWidget::Impl::updateEffectsList()`
  - effect rack item を descriptor-aware にする入口候補
- `ArtifactInspectorWidget::Impl::refreshRackButtons()`
  - common header の toggle / preset / appearance affordance を置く候補
- `ArtifactInspectorWidget::Impl::showRackContextMenu()`
  - preset / preview / appearance の共通操作をまとめる候補
- `ArtifactInspectorWidget::Impl::handleAddEffectClicked()`
  - effect type に応じて descriptor 初期値を入れる候補
- `ArtifactInspectorWidget::Impl::setEffectEnabledById()`
  - header の enabled state と同期する候補

`ArtifactPropertyWidget` は次の段階で追従する。

- section classifier の表示
- appearance / fallback の折りたたみ
- preset related controls の共通化

---

## 実装タスク

### Phase A: Contract Freeze

- [ ] effect UI descriptor の最小項目を固定する
- [ ] section classification の列挙を定義する
- [ ] header / preview / preset / appearance / advanced / fallback の境界を文書化する
- [ ] 標準エフェクトの代表サンプルを 3 つ選ぶ

### Phase B: Inspector Bridge

- [ ] `ArtifactInspectorWidget` から descriptor を読める経路を作る
- [ ] effect stack item に共通 header を表示する
- [ ] preview toggle の位置を統一する
- [ ] preset save/load の入口を 1 か所へ寄せる
- [ ] `updateEffectsList()` を descriptor-aware にする
- [ ] `showRackContextMenu()` から共通操作を呼べるようにする
- [ ] `setEffectEnabledById()` と header state を同期する

### Phase C: Preset Integration

- [ ] preset browser の card grammar を effect preset 用に固定する
- [ ] factory preset と user preset の見た目を揃える
- [ ] recent preset の導線を共通化する
- [ ] effect 固有 preset UI を共通導線へ接続する

### Phase D: Appearance Harmonization

- [ ] appearance section に入れる標準項目を列挙する
- [ ] edge handling の共通語彙を固定する
- [ ] histogram / graph / matte preview の widget を共通化する
- [ ] empty appearance の扱いを決める
- [ ] `Gaussian Blur / Sharpen / Curves / Levels / Glow` の表示項目を対応付ける
- [ ] `Fallback` に落ちた項目の表示ルールを決める

### Phase E: OFX / Third-party Bridge

- [ ] OFX パラメータの自動分類ルールを定義する
- [ ] custom UI の fallback 表示を決める
- [ ] 未分類項目の折りたたみ挙動を決める
- [ ] 外部プラグインのレイアウト崩れを避ける

### Phase F: Property Editor Alignment

- [ ] Property Editor 側の section 見え方を揃える
- [ ] effect stack の hover / selection / active state を統一する
- [ ] 特殊 effect だけの独自ボタンを減らす
- [ ] Inspector と Property Editor の文法差を縮める
- [ ] `Appearance / Advanced / Fallback` の見出しを共通化する
- [ ] section 折りたたみの既定状態を決める
- [ ] preset controls を property row と衝突させない

### Phase F.5: Property Surface Cleanup

- [ ] property row のラベルと effect header の優先順位を決める
- [ ] fallback parameter の表示幅を決める
- [ ] section 間の余白と階層感を揃える
- [ ] effect-specific row decoration の乱立を抑える

完了条件:

- Property Editor が effect ごとのバラバラな補助 UI を吸収できる
- `Appearance / Advanced / Fallback` が同じ見出し体系で読める
- Inspector と Property Editor の情報の重なりが整理される

---

## リスクと留意点

- 共通化しすぎると、個別エフェクトの個性や必要な例外が隠れる
- OFX はメタデータ品質がばらつくため、推定精度に依存しすぎない
- preset と appearance を一緒に扱うと、保存対象の境界が曖昧になる
- 既存 UI の再配置で、ユーザーが「どこに行ったか」迷いやすい
- UI の共通化が進みすぎると、各 effect の独自性が埋もれる

---

## 非対象

- 個別エフェクトのアルゴリズム刷新
- GPU 実装の拡張そのもの
- すべてのエフェクトの UI を一度に作り直すこと

---

## 成功条件

- ユーザーがエフェクトごとに UI の探し方を覚え直さなくてよい
- `Preview / Preset / Appearance` が共通の場所にある
- OFX を含む外部エフェクトにも同じ UI 文法を適用できる
- 個別エフェクトの差分は「中身」であり「枠」ではなくなる
- 新しい effect を追加したときに UI 設計を毎回ゼロから考えなくてよい

---

## 関連

- `docs/planned/MILESTONE_EFFECT_SYSTEM_IMPROVEMENT_2026-03-28.md`
- `docs/planned/MILESTONE_OFX_PLUGIN_SUPPORT_2026-04-18.md`
- `docs/planned/MILESTONE_PRESET_BROWSER_STARTER_FLOW_2026-05-31.md`
