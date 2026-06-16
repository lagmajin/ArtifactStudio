# App Debugger / Frame Debug Bundle Persistence Worklog - 2026-06-17

## Goal

`App Debugger` の `Frame Debug` 系を、単発の snapshot 表示ではなく、再起動後も追える診断面に寄せる。

## Current Status

- `FrameDebugSnapshot` / `FrameDebugViewWidget` / `FramePipelineViewWidget` / `FrameResourceInspectorWidget` / `FrameStateDiffWidget` は既に接続済み
- 今回は `FrameDebugBundle` を **起動時復元 + 自動保存 + 外部更新追従** まで進めた
- まだ手動の保存/読込 UI は追加していない

## What Changed

### App Debugger bundle persistence

- `Artifact/src/Widgets/Diagnostics/AppDebuggerWidget.cppm`
  - `FrameDebugBundle` を `Temp/ArtifactStudio/frame-debug-bundle.json` に自動保存するようにした
  - `ARTIFACT_FRAME_DEBUG_BUNDLE_FILE` があれば保存先を上書きできるようにした
  - 起動時に bundle を復元するようにした
  - refresh 時に bundle JSON を再読込して、外部更新に追従するようにした
  - `Export` 面に bundle の保存先と bundle 存在状態を明示するようにした
  - `Overview` / `Capture` / `Export` の tool tip に bundle path と更新時刻を追加した
  - `shareableSummary` を追加して、コピーしやすい要約を JSON より先に出すようにした
  - 保存先を project root 基準の `AppDataLocation/FrameDebug/<project>/frame-debug-bundle.json` に寄せた

### Frame Debug view wording

- `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm`
  - 冒頭に read-only inspector であることを明示した
  - tooltip で `App Debugger` に戻って比較・履歴を追う導線を補足した

## Notes

- QtCSS の新規追加はしていない
- `QColorDialog` の新規追加はしていない
- 新しい公開 signal/slot は追加していない
- `QImage` の新規採用はしていない

## Verification Status

- ビルド未実施
- 手元の変更確認のみ

## Resume Checklist

1. 必要なら `Frame Debug` に bundle の明示的な export/import 導線を足す
2. `App Debugger` の export text を bundle 共有向けにさらに整える
3. 必要なら `FrameDebugBundle` を保存先固定のプロジェクト領域へ移す

## Related

- `docs/planned/MILESTONE_APP_FRAME_DEBUG_VIEW_2026-04-20.md`
- `docs/planned/MILESTONE_APP_DEBUGGER_REPORT_SHARE_BUNDLE_2026-04-24.md`
- `docs/planned/MILESTONE_APP_DEBUGGER_SESSION_HISTORY_COMPARISON_2026-04-24.md`
