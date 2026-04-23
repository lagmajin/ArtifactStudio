# Layer Solo View (Diligent) Milestone

`Layer View (Diligent)` を、単体レイヤーの検査ビューから「ソロ表示で編集補助にも使えるビュー」へ寄せるためのマイルストーン。

## Goal

- 選択レイヤーを Diligent ベースで安定して表示する
- ソロ表示中でも zoom / fit / pan / selection 追従が破綻しないようにする
- mask / roto / overlay の入口を整理して、後から編集機能を載せやすくする
- software 側の診断ビューと見え方を近づける
- 単体表示だけでなく、検査と比較に使える情報量を持たせる

## v1 Requirements

Layer View は単体表示窓ではなく、次の 3 つを担う。

- `Edit`
  - そのレイヤー単体を編集する
- `Inspect`
  - そのレイヤーがどう処理されているかを見る
- `Impact`
  - そのレイヤーが誰に影響しているかを見る

v1 では、少なくとも次の表示・切替を備える。

- `Final / Source` 切替
- `Pivot / Bounds` overlay
- `Alpha` 表示
- `Stage` を 1 個選択して表示
- `Before / After`
- `cache / debug` 文字列
- `Visible / Lock / Solo / Active` の状態表示
- `Matte / Mask` の簡易状態表示
- `Effect Stack` の要約表示
- `Impact` 視点の依存先 / 被依存先の表示
- `Compare` モードでの差分確認

この要件は、編集導線・解析導線・影響確認導線を分離しつつ、1 つの Layer View に集約するための基準とする。

## Scope

- `Artifact/src/Widgets/Render/ArtifactRenderLayerEditor.cppm`
- `Artifact/src/Widgets/Render/ArtifactRenderLayerWidgetv2.cppm`
- `Artifact/src/Widgets/Render/ArtifactSoftwareRenderInspectors.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `ArtifactCore/src/UI/RotoMaskEditor.cppm`
- `ArtifactCore/include/UI/RotoMaskEditor.ixx`

## Non-Goals

- ここで完全なマスクペイントツールを完成させること
- 3D レイヤー編集へ拡張すること
- composition editor の全面再設計

## Background

現状の `ArtifactRenderLayerEditor` は、Diligent レイヤー単体の確認導線としては薄く、実質は `Play / Stop / Screenshot` を持つ wrapper に近い。
一方で、マスク編集の実体は composition 側の `ArtifactCompositionRenderController` と Core の `RotoMaskEditor` に分かれている。

この milestone は、Layer Solo View を「表示だけの検証窓」から、選択レイヤーの状態確認と編集補助の入口へ育てるための整理に使う。

## Phases

### Phase 1: View Responsibility Split

- 目的:
  - `ArtifactRenderLayerEditor` を「ソロビューの外枠」に固定する
  - `ArtifactLayerEditorWidgetV2` を「Diligent 描画と入力処理の核」として扱う
  - software 系の検証ウィジェットと役割が混ざらないようにする

- 作業項目:
  - `ArtifactRenderLayerEditor.cppm`
    - toolbar は `Play / Stop / Screenshot` のみに整理する
    - wrapper が renderer state を直接持たない前提を明文化する
    - `setTargetLayer()` / `setClearColor()` / `resetView()` の転送責務を維持する
  - `ArtifactRenderLayerWidgetv2.cppm`
    - view の state を `renderer_` 側に寄せる
    - `zoomLevel_` / `isPanning_` / `running_` の意味を整理する
    - `targetLayerId_` と preview 用の一時 state を分ける
    - render loop を「表示中だけ回す」前提へ寄せる
  - `ArtifactSoftwareRenderInspectors.cppm`
    - `Software Layer Test` を Diligent ソロビューと混同しないように役割を固定する
    - ソロ表示が必要な診断経路は software 側に閉じる
  - `docs`
    - `Layer Solo View` と `Software Layer Test` の使い分けを追記する

- 完了条件:
  - `ArtifactRenderLayerEditor` が薄い wrapper として説明できる
  - `ArtifactLayerEditorWidgetV2` が Diligent 表示の責務を持つと説明できる
  - software 診断ビューと Layer Solo View の役割差が docs で明文化される
  - 将来の mask / roto 入口を追加しても、責務が崩れない形になる

### Phase 1b: V1 View Surface

- 目的:
  - v1 に必要な表示軸を先に固定する
  - `Edit / Inspect / Impact` を同じ Layer View 内で切り替えられる土台を作る
  - 実装面は `ArtifactLayerEditorWidgetV2` に一本化する

- 作業項目:
  - `ArtifactLayerEditorWidgetV2.cppm`
    - `DisplayMode` / `EditMode` の内部 state を追加する
    - `paintEvent()` に表示モード別の描画分岐を実装する
    - `Final / Source` の表示切替を追加する
    - `Pivot / Bounds` overlay を描画する
    - `Alpha` 表示モードを追加する
    - `Before / After` の比較表示を追加する
    - `cache / debug` の短い文字列を表示できるようにする
    - `setEditMode()` / `setDisplayMode()` / `setPan()` を実装する
    - `zoom()` と `resetView()` の内部 state を renderer と同期させる
  - `ArtifactCompositionRenderController.cppm`
    - `Final / Source` の前提になる stage 選択の情報を供給する
    - overlay で使う bounds / pivot の情報を layer から受け取る
  - `docs`
    - `Edit / Inspect / Impact` の意味を明確化する

- 完了条件:
  - 1 レイヤーを選んだ時、見ている対象が `Final` か `Source` か分かる
  - pivot / bounds / alpha / stage / before-after / debug 情報を Layer View で確認できる
  - `Inspect` と `Impact` の情報が編集 UI と混ざらない
  - `paintEvent()` に mode 別の見た目が実装されている
  - `setEditMode()` / `setDisplayMode()` / `setPan()` が no-op ではない
  - v1 の見た目と表示切替は `ArtifactLayerEditorWidgetV2` に集約されている

### Phase 2: Solo Preview Stability

- current composition / current layer の追従を安定させる
- zoom / pan / fit の状態復元を整理する
- hidden tab での無駄な render loop を減らす
- layer の選択変更で target が古くならないようにする
- inspect HUD の情報更新を render loop から切り離す

### Phase 2b: Inspect HUD Expansion

- layer name / type / size / opacity / blend mode を読めるようにする
- visible / lock / solo / active / hidden の state を 1 画面に出す
- source size / bounds / pivot / cache state をまとめる
- selection 中のレイヤーの最小診断文字列を表示する

### Phase 2c: Compare and Impact Surface

- Final / Source / Before / After の見比べを固定化する
- matte / mask / effect stack の要点を短く出す
- このレイヤーがどこに影響しているかを要約する
- 依存先 / 被依存先の簡易一覧を出す

### Phase 3: Mask / Roto Entry Bridge

- layer selection から mask editor を開く導線を作る
- mask overlay の表示順を gizmo と衝突しないように整理する
- `RotoMaskEditor` と Layer Solo View の責務境界を決める
- 既存の `LayerMask / MaskPath` を直接編集できる最小導線を Layer Solo View に載せる

### Phase 3b: Light Effect Inspection

- effect stack の 1 本目だけでも見えるようにする
- before / after の要約を effect 単位で出せるようにする
- local adjustment / partial application の当たりを見やすくする
- 影響が大きい effect を強調する

### Phase 4: Editing Parity

- software test widget と Diligent view の見え方を近づける
- visible / opacity / mask / selection の差分を縮める
- diagnostic 用のログと操作導線を追加する
- impact / compare / debug の見え方を software 側と揃える

## Recommended Order

1. `Phase 1`
2. `Phase 1b`
3. `Phase 2`
4. `Phase 3`
5. `Phase 4`

## Current Status

- 2026-03-26 時点では、Diligent Layer Solo View は「表示と操作の核」を詰める前段階
- mask 編集は composition 側に実体があり、Layer Solo View にはまだ直接つながっていない
- そのため、まずは view の責務整理と mask 入口の設計から入るのが安全
- 2026-03-27 時点で、`ArtifactLayerEditorWidgetV2` の `paintEvent()` / `setEditMode()` / `setDisplayMode()` / `setPan()` / `resetView()` / `fitToViewport()` / `zoomAroundPoint()` を実装し、`ArtifactRenderLayerEditor` に mode 切替 UI も足し始めた
- 2026-03-27 時点で、`ArtifactLayerEditorWidgetV2` に `Final / Source / Compare` を見分けやすくする mode badge を追加した
- 2026-03-27 時点で、`layerInfoText` を追加し、`Inspect` 用に layer 名 / 2D or 3D / opacity / source size / bounds を 1 画面で読めるようにした
- 2026-03-27 時点で、`Vis / Lock / Solo / Active` の状態を layer info に追加し、`Hidden / OutOfRange / Transparent / Ready` の簡易 state も読めるようにした
- 2026-03-27 時点で、`Playback / Frame` を inspect HUD に追加し、ソロビューが再生系の state に追従しているか分かるようにした
- 2026-03-27 時点で、Mask edit mode で `LayerMask / MaskPath` を直接編集する最小導線を入れ始めた

## Future Outlook Notes

Layer Solo View を AE 寄りにする観点で、今後の不足は次の 3 点に集約される。

- コンテキスト表示
  - 自分のレイヤーはフル表示
  - 他レイヤーは半透明またはワイヤー表示
  - マットはアウトライン表示
  - ただし AE は「完全に消える / フル表示しかない」寄りで、比較用の見せ方としては弱い
- データフロー可視化
  - このレイヤーがどこに影響するか
  - どのマットを使っているか
  - 誰に使われているか
  - 現状は一切見えないので、Impact 視点の不足が大きい
- エフェクト前後の比較
  - before / after
  - エフェクトごとの段階表示
  - パイプラインが見えないため、inspect 用の比較導線が不足している
- エフェクト部分適用の可視化
  - `Rect` / `Mask` などの局所適用をどこに効かせているか見せる
  - Layer Solo View の `Impact` と直結させる
  - 将来的に Track Matte や Shape 適用に伸ばせる前提を作る
- 情報密度
  - `Play / Stop / Screenshot` だけでは検査ビューとして薄い
  - `Inspect / Compare / Impact` を前面に出した方が、レイヤー単体の価値が上がる
  - 最低でも state badge と要約 HUD が必要

## Phase 1 Notes

- `ArtifactRenderLayerEditor` は実装上すでに wrapper に近いので、Phase 1 ではその前提を壊さずに整理する
- `ArtifactLayerEditorWidgetV2` は Diligent 描画と入力処理の実体なので、ここに責務を寄せる
- 先に「何をどこが持つか」を固定してから、Phase 2 以降で追従・mask・parity を足す

## Phase 1b Notes

- 現在の `ArtifactLayerEditorWidgetV2` は `paintEvent()` と `setEditMode()` / `setDisplayMode()` / `setPan()` が未実装なので、まずここを埋める
- `Final / Source` は layer の生描画と処理後結果の切替として扱い、`Before / After` は同一レイヤーの比較表示として扱う
- `cache / debug` は UI の主役にしないが、render state の判定に使える短文で出せるようにする
- `Pivot / Bounds` は selection / transform の可視化としてまとめ、overlay の責務から外しすぎない
- wrapper 側に v1 の表示ロジックを増やさず、表示面は V2 に閉じる
- 2026-03-27 時点で、上記の state hooks と HUD overlay を一度入れた。`Final / Source` の実画像差分と wrapper 側の mode 切替 UI も追加済み。次は `Inspect / Impact` の情報密度を詰める
- 2026-04-21 時点で、`Inspect HUD`, `Impact surface`, `Compare` を明示的な v1 要件として扱うと、Layer Solo View の役割がかなり分かりやすくなる
