# マイルストーン: CPP/HPP 段階削減と Module 移行

> 2026-06-12 作成

**最終更新:** 2026-08-15
**Status:** module／ixx／cppm 移行は大きく進行中、旧 h／hpp と third-party／互換層が多数残り、全廃は未達

## 目的

`Artifact` 側の新規実装を `ixx/cppm` 中心へ寄せ、`cpp/hpp` の追加を止めていく。

このマイルストーンは、一気に全部を置き換えるためのものではない。
まずは安全に移せる単位を順番に `ixx/cppm` 化し、旧 `cpp/hpp` を薄い互換層として整理しながら、最終的に削除できる状態へ持っていく。

## 背景

このリポジトリではすでに module ベースの実装が増えている一方で、旧来の `cpp/hpp` も多く残っている。

無理に一括変換すると、次の問題が出やすい。

- `#include` の置き場所崩れ
- `module;` と `export module` の責務混在
- 循環 import
- 既存利用側の壊れ方が広い

そのため、削除より先に「移行順」を固定するのが重要。

## 方針

- 末端の UI / menu / dialog から移す
- service はその次に移す
- layer / composition / project の中核は最後に回す
- 旧ファイルは、利用側が全部切り替わるまで残す
- 1ファイルずつ小さく移す

## Non-Goals

- 一括で `cpp/hpp` を全廃すること
- 挙動変更を伴う大改修
- 新しい設計ルールの追加
- 無理な循環解消のための責務再編

## Phase 1: 移行しやすい末端から

### 対象

- `Artifact/src/Widgets/Timeline/*`
- `Artifact/src/Widgets/Render/*`
- `Artifact/src/Widgets/Native/NativeHelper.cpp`
- `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cpp`

### 目的

依存が少なく、挙動の局所性が高い実装から `ixx/cppm` 化して、移行テンプレートを固める。

### 完了条件

- 末端 widget が `ixx/cppm` で自然に読める
- `#include` が module 境界を壊していない
- 旧 `hpp` が不要になったものから削除できる

## Phase 2: service / helper 層

### 対象

- `Artifact/src/Service/*`
- `Artifact/src/Tool/*`
- `Artifact/src/Controller/*`
- `Artifact/src/GUI/*`

### 目的

UI から呼ばれる薄いロジックを module 化し、API 境界を整理する。

### 完了条件

- UI 側からの呼び出しが `import` に寄る
- 互換 wrapper が残っていても責務が薄い

## Phase 3: layer / composition / project

### 対象

- `Artifact/src/Layer/*`
- `Artifact/src/Composition/*`
- `Artifact/src/Project/*`
- `Artifact/src/Application/ArtifactApplicationManager.cpp`

### 目的

参照点が多い中核を、先に固めた移行パターンに沿って段階的に分割する。

### 完了条件

- 循環 import が局所化されている
- `ArtifactAbstractLayer` / `ArtifactAbstractComposition` 周辺の責務が崩れていない
- 旧 `cpp/hpp` の削除候補が明確になる

## 移行順の目安

1. `Artifact/src/Widgets/Timeline/TimelineScaleWidget.cpp`
2. `Artifact/src/Widgets/Timeline/ArtifactTimelineNavigatorWidget.cpp`
3. `Artifact/src/Widgets/Timeline/ArtifactTimelineLabel.cpp`
4. `Artifact/src/Widgets/Native/NativeHelper.cpp`
5. `Artifact/src/Widgets/Render/GridRenderer.cpp`
6. `Artifact/src/Widgets/Render/ArtifactCompositionViewerFooter.cpp`
7. `Artifact/src/Service/ArtifactClipboardService.cpp`
8. `Artifact/src/Service/ArtifactEffectService.cpp`
9. `Artifact/src/Service/ArtifactAudioService.cpp`
10. `Artifact/src/Layer/ArtifactLayerSetting.cpp`

## 実施ルール

- `module;` の後に `#include` を置かない
- 同一ファイルで自己 `import` しない
- ポインタ / 参照だけの依存は前方宣言を優先する
- 旧 `hpp` は利用側が全部移るまで残す
- 1ファイルごとに差分を小さく保つ

## 2026-08-15 現行実装監査

- `ArtifactCore`／`Artifact` の CMake は多数の `FILE_SET CXX_MODULES` と module 専用 target／implementation reference を運用しており、移行基盤は確立している。
- 一方、`Artifact`／`ArtifactCore`／`ArtifactWidgets` には `.h`／`.hpp` が現在も多数存在する。`TimelinePlayheadDraw.hpp`、画像／mesh／互換ヘッダ、third-party shader／codec headers などは一括削除対象にできない。
- したがって本マイルストーンは「新規実装を module 中心にする」段階は進行中だが、Phase 1〜3 の完了や旧 API／header の削除候補確定は未確認。
- module hygiene の静的検査 target は別マイルストーンで整備済みだが、ビルド・検査の実行結果はこのターンでは取得していない。

## 参照資料

- `docs/planned/REPO_WIDE_MODULE_HYGIENE_AUDIT_2026-06-11.md`
- `docs/planned/MILESTONE_CORE_MODULE_HYGIENE_BUILD_STABILIZATION_2026-04-21.md`
- `docs/planned/MILESTONE_TIMELINE_FEATURE_IMPLEMENTATION_2026-04-03.md`
