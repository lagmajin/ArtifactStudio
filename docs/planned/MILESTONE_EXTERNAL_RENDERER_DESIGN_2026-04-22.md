# External Renderer Design

> 2026-04-22 作成

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

### Phase 2

- 1 フレーム PNG 出力を実装する
- ログと終了コードを親へ返す

### Phase 3

- frame range 対応
- 進捗表示
- キャンセル対応

### Phase 4

- 複数 backend 対応
- キャッシュ / リトライ / 再開
- 必要なら UI からの起動導線を追加

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
