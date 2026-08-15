# External Renderer Design

> 2026-04-22 作成

**最終更新:** 2026-08-15
**ステータス:** snapshot／farm transport の基盤実装済み、独立 renderer process の end-to-end 実装待ち

## Update 2026-08-15

現行コードを再確認した。`ArtifactOffscreenRenderer2D` は Diligent のオフスクリーン GPU ターゲットを初期化し、フレームを描画する独立 renderer 部品として存在する。`RenderFarmSharedBuffer`／IPC にはフレームの共有・検証・読み出し基盤があり、Render Queue／Project 側にもジョブ管理経路がある。

- ただし、これらは「独立 renderer process が job snapshot を受け取り、composition を再構成し、範囲をレンダーして結果・診断を返す」E2E の証拠ではない。
- 現行の確認範囲では、親子プロセス間の外部 renderer job schema、CLI 起動、snapshot 復元、進捗／失敗の完全な戻し、再実行・キャンセル契約は未完了。
- 判定は **オフスクリーン描画と transport の基盤は実装済み、独立 renderer process の end-to-end は未達** を維持する。

内蔵レンダラはそのまま維持しつつ、クラッシュしやすいオフラインレンダリングだけを別プロセスへ切り出すための設計案。

目的は「UI 本体を巻き込まずに、失敗しやすい処理を隔離する」こと。

---

## 背景

今のアプリは、内蔵レンダラを起動しなければ大きな問題は出にくい。
一方で、オフラインレンダリングは長時間実行・高負荷・素材依存・GPU 依存が重なりやすく、クラッシュやハングの隔離価値が高い。

そこで方針を分ける。

- 内蔵レンダラ: 既存の UI 内で継続
- 外部レンダラ: 別プロセスのジョブ実行器として新設

---

## 目標

- UI 本体とレンダラーを分離する
- レンダー失敗時に親プロセスを落とさない
- レンダー入力を live object ではなく snapshot にする
- CLI からも起動できるようにする
- 将来のバッチレンダーやキュー処理に流用できる形にする

---

## 非目標

- 内蔵レンダラの全面置き換え
- ライブな layer / effect オブジェクトの IPC 共有
- GPU コンテキスト共有を前提にした複雑な接続
- 最初から分散レンダリングやクラスタ対応を入れること

---

## 基本構成

### 親プロセス

- UI から render job を生成する
- job を JSON か同等の純データへシリアライズする
- 子プロセスを起動する
- 進捗、ログ、結果、失敗を受け取る

### 子プロセス

- render job を読み込む
- snapshot から composition / layer / effect / frame range を再構成する
- 指定フレームをレンダリングする
- 出力ファイルと診断情報を返す

---

## データ境界

外部レンダラーへ渡すのは live object ではなく、次のような純データに限定する。

- composition ID / name
- frame range
- output path
- size / pixel ratio / format
- render quality preset
- layer snapshot
- effect snapshot
- mask / matte / blend 情報
- asset resolve 情報
- preview / offline のモード
- diagnostic options

重要なのは、`ArtifactAbstractLayer` や `ArtifactAbstractEffect` の実体をそのまま渡さないこと。

---

## Job Schema

最初の job は最小構成でよい。

```json
{
  "version": 1,
  "jobId": "uuid",
  "mode": "offline",
  "composition": {
    "id": "composition-id",
    "name": "Scene A",
    "frameStart": 0,
    "frameEnd": 240,
    "fps": 30
  },
  "output": {
    "path": "D:/renders/scene_a",
    "format": "png",
    "width": 1920,
    "height": 1080
  },
  "quality": {
    "preset": "final"
  },
  "snapshot": {
    "layers": [],
    "effects": [],
    "assets": []
  },
  "diagnostics": {
    "logLevel": "info",
    "saveCrashTrace": true
  }
}
```

この schema は将来拡張してよいが、初期は安定優先で固定する。

### Example

```json
{
  "version": 1,
  "jobId": "7b7f0a57-2f8f-46ea-8c9b-71f2f4adf4f2",
  "mode": "offline",
  "composition": {
    "id": "comp_main",
    "name": "Main Shot",
    "frameStart": 0,
    "frameEnd": 120,
    "fps": 30,
    "resolution": { "width": 1920, "height": 1080 },
    "alphaMode": "premultiplied"
  },
  "output": {
    "path": "D:/renders/main_shot",
    "format": "png",
    "width": 1920,
    "height": 1080
  },
  "quality": {
    "preset": "final",
    "previewQuality": "high"
  },
  "snapshot": {
    "layers": [
      {
        "id": "layer_bg",
        "parentId": null,
        "name": "Background",
        "kind": "solid",
        "visible": true,
        "locked": false,
        "shy": false,
        "opacity": 1.0,
        "blendMode": "normal",
        "transform": {
          "position": [0, 0],
          "scale": [1, 1],
          "rotation": 0
        },
        "range": { "in": 0, "out": 120 },
        "notes": "",
        "tags": ["base"]
      },
      {
        "id": "layer_title",
        "parentId": null,
        "name": "Title",
        "kind": "text",
        "visible": true,
        "locked": false,
        "shy": false,
        "opacity": 0.92,
        "blendMode": "normal",
        "transform": {
          "position": [240, 120],
          "scale": [1, 1],
          "rotation": 0
        },
        "range": { "in": 12, "out": 96 },
        "effects": [
          {
            "id": "fx_shadow",
            "displayName": "Drop Shadow",
            "pipelineStage": "rasterizer",
            "enabled": true,
            "properties": {
              "offsetX": 8,
              "offsetY": 8,
              "blur": 24,
              "opacity": 0.35
            }
          }
        ]
      }
    ],
    "effects": [
      {
        "id": "fx_grade",
        "displayName": "Hue / Saturation",
        "pipelineStage": "rasterizer",
        "enabled": true,
        "properties": {
          "hue": 0,
          "saturation": 0.1,
          "lightness": 0.0
        }
      }
    ],
    "assets": [
      {
        "id": "asset_logo",
        "sourcePath": "D:/project/assets/logo.png",
        "resolvedPath": "D:/project/assets/logo.png",
        "hash": "sha256:...",
        "status": "ready"
      }
    ]
  },
  "diagnostics": {
    "logLevel": "info",
    "saveCrashTrace": true,
    "captureFrameDebug": true
  }
}
```

This example is intentionally small. いま必要なのは「どんな情報を snapshot に入れるか」の共通認識であって、全機能の網羅ではない。

---

## Snapshot Shape

`snapshot` は live object の代替ではなく、レンダラーが読むための固定データにする。

### composition

- id
- name
- frame range
- fps
- resolution
- background / alpha mode

### layers

各 layer は最低限、次を持つ。

- layer id
- parent id
- name
- visible / locked / shy
- opacity
- blend mode
- transform
- timing range
- content kind
- notes / tags

### effects

各 effect は最低限、次を持つ。

- effect id
- display name
- pipeline stage
- enabled flag
- ordered properties
- preset / variant info

### masks / mattes

- mask path list
- matte references
- mask mode / invert / opacity
- matte blend mode

### assets

- asset id
- source path
- resolved path
- hash / version tag
- loading status

### render hints

- quality preset
- color management hints
- cache hints
- diagnostic flags

## Type Sketch

実装時は、次のような型のまとまりに落とすと扱いやすい。

```text
RenderJob
  - version
  - jobId
  - mode
  - composition : RenderCompositionSnapshot
  - output : RenderOutputSpec
  - quality : RenderQualitySpec
  - snapshot : RenderSceneSnapshot
  - diagnostics : RenderDiagnosticsSpec

RenderCompositionSnapshot
  - id
  - name
  - frameStart
  - frameEnd
  - fps
  - resolution
  - alphaMode

RenderSceneSnapshot
  - layers : [RenderLayerSnapshot]
  - effects : [RenderEffectSnapshot]
  - assets : [RenderAssetSnapshot]

RenderLayerSnapshot
  - id
  - parentId
  - name
  - kind
  - visible
  - locked
  - shy
  - opacity
  - blendMode
  - transform : RenderTransformSnapshot
  - timing : RenderTimingSnapshot
  - effects : [RenderEffectSnapshot]
  - masks : [RenderMaskSnapshot]
  - mattes : [RenderMatteSnapshot]
  - notes
  - tags

RenderTransformSnapshot
  - position
  - scale
  - rotation
  - anchor
  - skew
  - opacity

RenderTimingSnapshot
  - inFrame
  - outFrame
  - startOffset
  - timeScale

RenderEffectSnapshot
  - id
  - displayName
  - pipelineStage
  - enabled
  - properties
  - preset
  - variant

RenderMaskSnapshot
  - maskId
  - name
  - mode
  - inverted
  - opacity
  - pathCount

RenderMatteSnapshot
  - matteId
  - targetLayerId
  - type
  - blendMode
  - enabled
  - opacity

RenderAssetSnapshot
  - id
  - sourcePath
  - resolvedPath
  - hash
  - status

RenderOutputSpec
  - path
  - format
  - width
  - height

RenderQualitySpec
  - preset
  - previewQuality
  - colorManagementHints
  - cacheHints

RenderDiagnosticsSpec
  - logLevel
  - saveCrashTrace
  - captureFrameDebug
```

---

## Execution Flow

1. 親が current composition から render job を生成する
2. job を JSON へ書き出して子プロセスを起動する
3. 子が job を読み、snapshot を復元してレンダリングする
4. 子が終了コード / ログ / 出力パスを返し、親が UI に結果を表示する

この流れの間、親は live object を編集し続けてもよいが、実行中の子には影響しない。

---

## IPC 方針

最初は単純でよい。

- 親 -> 子: JSON ファイル or stdin
- 子 -> 親: stdout / stderr / 終了コード
- 追加で必要なら progress ファイルや named pipe を使う

最初から複雑な双方向 RPC にしない。
ログと進捗の可視化が必要になってから段階的に強化する。

### 返却情報

- 終了コード
- human readable error
- render summary
- output files
- optional diagnostics bundle

### 将来の拡張候補

- progress event stream
- cancel request
- pause / resume
- remote queue bridge
- headless batch mode

---

## Build Layout

最初は同一 repo 内で、UI 本体とは別ターゲットとして置くのがよい。

### 推奨構成

- `Artifact` : 既存 UI アプリ
- `ArtifactRenderer` : 外部レンダラー実行器
- `ArtifactCore` : 共有の低レベル基盤
- 必要なら `ArtifactWidgets` は UI 側に残す

### CMake 方針

- `ArtifactRenderer` 用に独立した `CMakeLists.txt` を持たせる
- 共有コードはライブラリ化して両方から参照する
- UI 専用の widget / service 依存は `ArtifactRenderer` から切る
- まずは build 分離だけを行い、repo 分割は後回しにする

### 期待効果

- 依存関係が見えやすくなる
- 外部レンダラーの crash 調査がしやすくなる
- `artifact-renderer.exe` を CLI 実行しやすくなる
- 将来の batch / headless モードへつなぎやすい

---

## 実行モデル

### 推奨

- `artifact-renderer.exe --job job.json`
- 1 job = 1 process を基本にする
- 1 フレーム単位のスモールジョブも将来的に扱えるようにする

### メリット

- クラッシュの分離が簡単
- 再起動が容易
- 実験的な renderer backend を試しやすい
- バッチ処理や CI に流用しやすい

### 注意点

- 毎回 process spawn のコストはある
- 進捗更新の設計が必要
- 失敗時の再実行戦略が必要

---

## レンダリング責務の分割

### 親プロセスが持つもの

- UI 状態
- プロジェクト操作
- 直近の playback state
- render job 生成
- 出力先の選択
- 進捗 UI

### 子プロセスが持つもの

- snapshot の復元
- layer/effect の評価
- actual render loop
- image / frame output
- crash logging
- job summary / diagnostics

---

## 失敗時の扱い

- 子プロセスが非 0 で終了したら job failed とする
- timeout も失敗として扱う
- 例外メッセージと終了コードは親へ返す
- crash dump は子プロセス側で保存する
- 親は UI を壊さず、再試行可能な状態を保つ

---

## ロールアウト案

### Phase 1

- job schema を定義する
- snapshot を JSON 化する
- CLI で job を読むだけの子プロセスを作る

Status:

- `2026-05-23`
  - `ArtifactRenderer` target を追加し、`artifact-renderer.exe --job job.json` の入口を作成
  - v1 job schema の最小検証を追加
  - `ArtifactRenderer/examples/minimal-job.json` を追加
  - `--validate-only` / `--dump-summary` で、レンダリング前に job の読み取りと snapshot 件数を確認できるようにした
  - まだ実レンダリング、進捗 IPC、Render Queue からの起動導線は未接続

### Phase 2

- 1 フレーム PNG 出力を実装する
- ログと終了コードを親へ返す

Status:

- `2026-05-23`
  - `artifact-renderer.exe --job job.json` の通常実行で先頭フレーム PNG を 1 枚書き出す Phase 2 stub を追加
  - 現段階の PNG は snapshot 復元後の実コンポジットではなく、job / composition 由来の診断パターン
  - stdout / stderr に JSON line の `renderStarted` / `renderCompleted` / `renderFailed` を返す
  - PNG 以外の output format は Phase 2 未対応として非 0 終了にする

### Phase 3

- frame range 対応
- 進捗表示
- キャンセル対応

Status:

- `2026-05-23`
  - `frameStart <= frame < frameEnd` の PNG sequence stub を追加
  - 各フレーム完了ごとに stdout JSON line の `renderProgress` を返す
  - `--cancel-file path` または `diagnostics.cancelFile` の sentinel file でキャンセルできるようにした
  - キャンセル時は `renderCanceled` を stderr に返し、終了コード `7` にする
  - まだ親 UI からの cancel request 発行、progress 表示接続、実 snapshot composite は未接続

### Phase 4

- 複数 backend 対応
- キャッシュ / リトライ / 再開
- 必要なら UI からの起動導線を追加

Status:

- `2026-05-23`
  - `quality.backend` と `--backend` を追加し、CLI 契約として backend selector を受け取れるようにした
  - Phase 4 stub では `auto` / `cpu` / `diagnostic` を診断 PNG backend として扱い、未実装 backend は明示的に非 0 終了にする
  - `diagnostics.cacheMode: "resume"` または `--resume` で既存 PNG を skip/cache hit として扱う
  - `diagnostics.retryCount` または `--retry-count` で PNG 書き込み失敗時の retry 回数を指定できるようにした
  - progress JSON line に `backend` / `cacheHit` / `attempts` を追加
  - まだ本物の GPU/CPU backend 切替、永続 cache index、親 UI 起動導線は未接続

### Phase 5

- 親プロセスが stdout stream だけに依存せず完了結果を読めるようにする
- job JSON / CLI から result artifact の出力先を指定できるようにする
- 外部プロセス起動導線の前に、結果取得の契約を固定する

Status:

- `2026-05-23`
  - `--summary-file` / `diagnostics.summaryFile` を追加し、最終 `renderCompleted` / `renderFailed` / `renderCanceled` を JSON file として保存できるようにした
  - `--event-log` / `diagnostics.eventLogFile` を追加し、stdout/stderr と同じ render event を JSON Lines で保存できるようにした
  - event log は実行開始時に truncate し、各 event emit 時に追記する
  - まだ `ArtifactRenderQueueService` からの `QProcess` 起動、UI progress 反映、cancel sentinel 作成は未接続

### Phase 6

- Render Queue から外部 renderer process を起動できるようにする
- 既存の queue status / progress 経路に外部 renderer events を流し込む
- 外部 renderer は当面 PNG sequence stub のみを担当し、既存 internal CPU/GPU path は維持する

Status:

- `2026-05-23`
  - Render Backend に `external` を追加
  - `ArtifactRenderQueueService` が `renderBackend == external` の job で `artifact-renderer.exe --job job.json` を起動する分岐を追加
  - 外部 process 用 job JSON / summary / event log / cancel sentinel を temp work dir に生成する
  - 外部 process の JSON line `renderProgress` / `renderCompleted` を既存 queue progress に反映する
  - queue cancel / shutdown 時は cancel sentinel を作り、必要なら外部 process を terminate / kill する
  - 現段階では external backend は PNG sequence のみ対応。実 snapshot composite ではなく外部 renderer 側の diagnostic PNG stub を使う

### Phase 7

- 外部 renderer に live object ではなく composition snapshot を渡す
- 最初の復元対象を solid layer に絞り、未対応 layer は診断 fallback のまま維持する
- snapshot transport の shape を親子で確認できるようにする

Status:

- `2026-05-23`
  - `ArtifactRenderQueueService` の external job JSON に `composition->toJson()` の object と `layers` 配列を含めるようにした
  - `ArtifactRenderer` の job summary に transported layer count を追加
  - 外部 renderer が `snapshot.layers` の `LayerType::Solid` JSON を読み、背景色 + solid rectangle を PNG に描けるようにした
  - `examples/minimal-job.json` に composition snapshot と solid layer の例を追加
  - 未対応 layer / solid layer がない job は従来の diagnostic pattern に fallback する

### Phase 8

- external solid renderer の座標系を composition space と output space で揃える
- solid layer snapshot の基本 transform を反映する

Status:

- `2026-05-23`
  - `snapshot.composition.width/height` と output width/height の比率で canvas scale を適用
  - solid layer の position / anchor / rotation / scale / opacity を反映
  - `examples/minimal-job.json` に anchor / rotation fields を追加

### Phase 9

- text layer snapshot の最小復元を追加する
- 外部 renderer の結果 JSON に painted / unsupported layer counts を載せる

Status:

- `2026-05-23`
  - `snapshot.layers` の `LayerType::Text` を外部 renderer 側で rasterize する経路を追加
  - `text.value` / font / alignment / wrap / box size / opacity を反映し、必要なら shadow / stroke も簡易合成する
  - frame result と progress event に painted / unsupported layer count を追加
  - `examples/minimal-job.json` に text layer の例を追加

### Phase 10

- SVG layer snapshot の最小復元を追加する
- job file の相対 source path を job directory 基準で解決する

Status:

- `2026-05-23`
  - `snapshot.layers` の `svg.sourcePath` を持つ `LayerType::Shape` を外部 renderer 側で rasterize する経路を追加
  - relative `svg.sourcePath` を job JSON の directory 基準で解決するようにした
  - `examples/minimal-job.json` に SVG layer の例と `examples/minimal-shape.svg` を追加

### Phase 11

- image layer snapshot の最小復元を追加する
- image layer の source path と寸法を job に含める

Status:

- `2026-05-23`
  - `snapshot.layers` の `LayerType::Image` を外部 renderer 側で rasterize する経路を追加
  - `ArtifactImageLayer::toJson()` に `image.sourcePath` / `image.fitToLayer` / `image.width` / `image.height` を追加
  - `examples/minimal-job.json` に image layer の例と `examples/minimal-image.ppm` を追加

### Phase 12

- shape layer snapshot の最小復元を追加する
- shape layer の shape geometry と style を job に含める

Status:

- `2026-05-23`
  - `snapshot.layers` の `LayerType::Shape` を外部 renderer 側で `QPainterPath` に戻して rasterize する経路を追加
  - `examples/minimal-job.json` に shape layer の例を追加

### Phase 13

- precomp layer snapshot の最小復元を追加する
- nested composition snapshot を job に含める

Status:

- `2026-05-23`
  - `ArtifactCompositionLayer::toJson()` に `composition.sourceId` を追加
  - render job の `snapshot.composition.compositions` に nested composition snapshot を積むようにした
  - 外部 renderer 側で `LayerType::Precomp` を再帰的に rasterize する経路を追加
  - `examples/minimal-job.json` に precomp layer と nested composition の例を追加

### Phase 14

- video layer snapshot の最小復元を追加する
- video layer の source / proxy / size metadata を job に含める

Status:

- `2026-05-23`
  - `ArtifactVideoLayer::toJson()` を `LayerType::Video` と namespaced `video.*` keys に揃えた
  - 外部 renderer 側で `video.proxyPath` / `video.sourcePath` を試し、読める場合は静止 surface として描画するようにした
  - 読めない場合は簡易プレースホルダを返すようにした

### Phase 15

- audio layer snapshot の最小復元を追加する
- audio layer の source / volume / mute metadata を job に含める

Status:

- `2026-05-23`
  - `ArtifactAudioLayer::toJson()` の `audio.sourcePath` / `audio.volume` / `audio.muted` を外部 renderer 側で読み込める状態にした
  - 外部 renderer 側で `LayerType::Audio` を診断 placeholder として描画し、source / volume / muted 状態が見えるようにした
  - `examples/minimal-job.json` に audio layer の例を追加

---

## 既存コードとの関係

- 内蔵レンダラは今のまま維持する
- `FrameDebugSnapshot` 系は snapshot 生成の参考になる
- `CompositionRenderController` の責務はなるべく壊さない
- 外部レンダラーは新しい実行器として別に立てる

---

## まとめ

この案は「外部レンダラーを新しく作る」ための土台であって、今の UI を壊す計画ではない。

- 内蔵レンダラは現状維持
- オフラインレンダリングだけ別プロセス化
- 入力は snapshot ベース
- まずは 1 job = 1 process の単純なモデルから始める

ここから先は、job schema と snapshot 化の深さを決めれば実装へ進める。


---

## Static audit follow-up (2026-07-25)

ArtifactRenderQueueService に外部 renderer job JSON の生成、専用作業ディレクトリ、子プロセス起動、stdout／stderr の JSON Lines 受信、進捗・summary・cancel・失敗処理が実装されている。RenderFarmMaster には worker、RPC server、remote worker 設定、retry／checkpoint の基盤もある。

一方、設計書の snapshot schema と実際の job schema の完全一致、外部子プロセス側の再構成範囲、CLI 単独起動、クラッシュ復旧の実行時検証、RPC の実運用は未確認である。したがって親子プロセス分離の基盤は実装済み、schema／CLI／運用検証は継続課題として記録する。

## 現行コード監査 (2026-08-15)

- Render Queue に外部 renderer job の JSON 生成、専用作業ディレクトリ、子プロセス起動、JSON Lines の stdout／stderr 受信、進捗・summary・cancel・失敗処理がある。
- `RenderFarmMaster`／RPC server には snapshot／job contract、worker、retry、checkpoint、remote worker 管理の基盤がある。
- ただし、設計上の snapshot schema と実際の payload の完全一致、全 layer／effect の外部再構成、CLI 単独実行、クラッシュ後の成果物復旧、実ネットワーク運用は未検証。

判定: **外部 renderer の親子境界と job transport は実装済み。独立 process の完全な renderer parity と end-to-end 運用は pending。**
