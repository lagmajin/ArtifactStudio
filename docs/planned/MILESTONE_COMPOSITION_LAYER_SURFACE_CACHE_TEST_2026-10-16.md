# Composition Layer Surface Cache 独立テスト

**最終更新:** 2026-10-16
**ステータス:** Blocked（ArtifactCore 実装 unit の重複 IFC 参照）

## 実装状況（2026-10-16）

- 独立 target `ArtifactCompositionLayerSurfaceCacheTest` と、Composition Editor の実描画経路を使う MCP GUI fixture を追加。
- `CompositionRenderController` に opt-in cache hit／miss／invalidation／entry build counters と cold-path JSON snapshot API を追加。診断無効時はこれらのカウンターを更新しない。
- `cache.runRegression` は同一 view の cache reuse と effect edit 後の対象 entry invalidation/rebuild、および別 layer 編集の非干渉を検証する。
- CMake configure は成功。`EnvironmentVariable.cppm` の旧手書き IFC 参照を除き、同じ split target が生成する primary-interface 参照に一本化したことで、その C7684 は解消し `ArtifactCoreEnvironment` target はビルド成功。
- standalone Debug target build では GPU 4 unit の IFC 依存を manifest に追加して解消。また Animation Transform 2D／3D、AnimationDynamics、Channel で MSVC による duplicate IFC が出たため、古い明示参照を除き生成済み module dependency metadata を使うよう整理し、これらのエラーを解消。
- 続いて `EncoderSetting.cppm` で同型の duplicate IFC が残る。ArtifactCore の既存手書き IFC mapping 全体に関わるため、独立 test executable の compile/runtime は未確認。

## 目的

FBX アニメーション独立ビューアと同様に、製品アプリ本体とは別プロセスで起動できるテスト用 GUI を用意し、Composition View の Layer Surface Cache の再利用と invalidation を再現・記録する。

## 対象と境界

- 対象は `ArtifactCompositionRenderController.cppm` の `LayerSurfaceCacheEntry` と `buildLayerSurfaceCacheKey()` を使うレイヤー単位の加工済み surface cache。
- Composition-space GPU cache、GPU texture cache、composition result cache、RAM preview cache は対象外。これらは別の cache として扱う。
- 製品の描画・入力経路を置き換えず、実際の Composition View と同じ controller 経路を使う。
- テスト fixture と診断はテスト用 executable に閉じ、ユーザープロジェクトや製品用データを変更しない。

## テスト窓と操作

独立 executable `ArtifactCompositionLayerSurfaceCacheTest` を追加し、FBX Viewer と共通の `ArtifactTestMcpServer.h` / `ArtifactTestWindowCapture.h` を利用する。MCP で fixture 準備、描画待機、cache state／診断取得、操作、capture を行えるようにする。

Fixture は小さく決定的な静止画系 composition とし、Surface Cache が使われる mask または rasterizer effect 付き layer を含める。effect API の現状と適用可能な layer 型を実装前に確認し、テスト専用機能や新しい製品 UI は追加しない。

## 受入シナリオ

1. 初回描画で対象 layer の cache miss と surface 生成が記録される。
2. composition／frame／view／effect 条件を変えずに再描画し、cache hit が記録される。出力 capture が初回と一致する。
3. 対象 layer の mask または effect を変更し、該当 layer の entry が無効化されて再生成される。capture は変更前と異なる。
4. 別 layer を変更した場合、対象外 layer の entry が不要に破棄されないことを確認する。
5. 同じ layer で full／interactive draft の切替を行い、cache slot／signature が条件に応じて分離されることを確認する。
6. executable 起動から MCP 操作、診断 JSON、PNG capture 保存までを単独で完了できる。

## 観測要件

- hit／miss／invalidation／surface rebuild を layer ID と cache slot（full／draft）ごとに記録する。
- 診断はテスト用 opt-in とし、通常の描画 hot path で無効時の文字列構築やログ出力を発生させない。
- JSON 結果にはシナリオ、frame、対象 layer、期待値、観測値、capture path、失敗理由を残す。
- capture 比較は同一 backend／同一条件の画像同士で行い、backend 間の pixel parity はこのテストの合否に含めない。

## 実装前確認と完了条件

- `CompositionRenderController` の cache hit／miss と invalidation の既存観測手段を調べ、テストから安全に読める方法を選ぶ。
- hit／miss を既存ログだけで安定して判別できない場合、opt-in の低コストな診断カウンターを検討する。ホットパス規則に従い、無効時はカウンター／ログの追加負荷を避ける。
- 対象 executable の CMake option は既存 standalone test viewer と同じく opt-in とする。
- ビルド・実行・GPU runtime 検証は、この AGENTS.md の制約に従いユーザーが明示するまで行わない。
