# M-UI-OFFLINE-1: オフライン・不可視 UI 操作／キャプチャ基盤

**最終更新:** 2026-11-13
**ステータス:** In Progress（Windows 不可視 window 起動、widget / completed GPU frame capture の入口を追加。runtime は未検証）

## 目的

Windows のデスクトップ操作に依存せず、Artifact の実 UI を不可視で起動し、ローカルのスクリプト／AI エージェントから入力・状態取得・画像キャプチャ・ログ／デバッグ情報取得を行える経路を整える。外部サービスや画面上のウィンドウ表示を前提にしない。

## 現状

- `tools/capture_ui_test.py` と `ArtifactUiTest` は固定 fixture の起動・UI スクリーンショット・結果ファイルをサポートする。
- `--components-no-window` は `WA_DontShowOnScreen` と `ArtifactVirtualInput` を使い、Components の限定された統合確認をウィンドウ無しで実行する。
- `ARTIFACT_STARTUP_SCREENSHOT` は visible なトップレベルウィンドウを要求し、通常の UI capture は不可視起動に再利用できない。
- `ArtifactStartupScreenshot` は `QScreen::grabWindow` / widget capture 系のため、ウィンドウを表示しない条件での安定したキャプチャ契約がない。
- `ArtifactUiTest` は QApplication と `ArtifactMainWindow` を生成する。通常アプリ全体を offscreen QPA へ切り替えられるか、GPU viewport が swapchain なしに描画できるかは未確認。

## 進捗（2026-11-13）

- `Artifact.exe --ui-headless --ui-mcp-port <port>` を追加し、main window と transient top-level widgets に `WA_DontShowOnScreen` / `WA_ShowWithoutActivating` を適用する。
- headless mode の `ui.screenshot` は widget capture のみ許可し、native desktop grab を拒否する。`ui.quit` で制御 endpoint から通常の shutdown を要求できる。
- `ui.compositionCapture` は `CompositionRenderController::readbackPresentedFrameImage()` を明示実行し、completed GPU composition frame を取得する。desktop/window fallback は行わず、frame未生成時は明確に失敗する。readbackが不可視起動でも有効かはruntime未検証。
- `ui.state` は Composition editor / renderer 初期化状態、frame serial、last / average frame time、last frame execution thread id を返す。状態取得ではGPU readbackを行わない。
- `tools/run_ui_offline.py` は Artifact を `CREATE_NO_WINDOW` で起動し、Qt の `windows` platform plugin と exe 隣接 `platforms` directory を明示し、APPDATA / LOCALAPPDATA を run directory に隔離し、localhost control port とログを用意する。設定未保存時に既定有効となる startup screenshot は無効化し、明示 capture だけを保存する。既存 `tools/ui_control_client.py` が widget 操作・状態取得・capture に使える。
- Qt 公式仕様では内部 `QEvent::Show` は widget が visible になる直前に届けられ、`QWidget::grab()` は widget と子 widget を pixmap へ描画する。このため `Polish` / `Show` での不可視属性設定と widget capture はAPI上成立する設計だが、Windows QPAでのruntime結果は未確認。
- `ui.compositionCapture` は明示要求時のみ同期 GPU readback を行う診断境界で、frame loop からは呼ばない。
- これらのソース変更はまだ build/runtime 検証していない。hidden native child、QWidget grab の可否、Diligent viewport capture の実動作は未確認。

起動例（Windows PowerShell）:

```powershell
python tools/run_ui_offline.py --exe out/build/x64-Debug/Artifact/Artifact.exe -- --lang en
```

ランチャーに表示された port を使い、別 terminal から操作する:

```powershell
python tools/ui_control_client.py --port 49152 list --filter layer
python tools/ui_control_client.py --port 49152 click --target layerListWidget
python tools/ui_control_client.py --port 49152 screenshot --output .\ui-capture.png
python tools/ui_control_client.py --port 49152 composition-capture --output .\composition-frame.png
python tools/ui_control_client.py --port 49152 quit
```

`list` / `state` で UI 構造とフォーカスを確認し、`click` / `drag` / `key` / `type` / `wheel` で Qt 内の仮想入力を送り、`screenshot` で widget tree、`composition-capture` で完了済みGPU Composition frameを PNG 化する。ランチャーを Ctrl+C で止めると `ui.quit` を送り通常終了を試みる。run directory は既定で `%TEMP%` 内に作られ、ログと隔離 app data を残す。

## 方針

1. 既存 `Artifact.exe --ui-mcp-port` と `tools/ui_control_client.py` を再利用し、不可視起動 runner を提供する。将来 `ArtifactUiTest` の JSONL action stream を追加してもよい。
2. UI 構築と Qt event loop は維持し、トップレベル window は表示せず `WA_DontShowOnScreen` で非表示 surface にする。最初は `windows` QPA plugin の不可視 window を使い、`offscreen` plugin は Qt platform capability と GPU 使用条件を調査して別途選ぶ。
3. UI 入力は既存の `ArtifactVirtualInput`／Qt event 経路を通す。objectName / accessible name を使ったクリック、キー、テキスト入力、待機、状態取得をスクリプトから要求できるようにし、OS のマウス・キーボード注入を使わない。
4. キャプチャを種類別にする。
   - Qt widget tree: `QWidget::render` などの widget 描画をメモリ画像へ出力。
   - Composition CPU/software surface: 既存 renderer capture API を直接利用。
   - GPU viewport: offscreen render target / readback が既に成立する経路のみ対応し、swapchain依存や GPU readback 制約を明記する。
   - desktop/window capture はこの基盤の要件に含めない。
5. 操作結果を JSONL で返す。各 action に成功／失敗、対象 object、観測値、画像 path、ログ末尾を含め、失敗時にアプリを確実に終了する。
6. 常時稼働 server ではなく、1 セッション 1 プロセスを基本とし、プロジェクトと app data は一時ディレクトリへ分離する。ユーザーデータを暗黙に開かない。

## 段階

### Phase 0: 能力調査

- QApplication / Qt platform plugin の初期化順、`ArtifactMainWindow` の表示前依存、既存仮想入力 helper、widget render capture を確認する。
- `windows` QPA + hidden window と `offscreen` QPA の差、Diligent device / swapchain 要求、子 widget と native widget の例外を記録する。
- 操作対象 API は既存 accessibility/objectName を再利用し、必要な安定名の不足分だけを対象限定で補う。グローバル signal/slot は追加しない。

### Phase 1: 不可視 UI fixture runner

- 既存 `ArtifactUiTest` に不可視起動の汎用 fixture を加え、widget tree のキャプチャと状態 query を提供する。
- CLI から JSONL action を渡し、`ArtifactVirtualInput` で pointer/key/text event を配送する。
- `capture_ui_test.py` に逐次 action file と per-action artifact manifest を追加し、既存 fixture CLI と共存させる。

### Phase 2: 汎用操作と再現性

- `find`, `click`, `type`, `key`, `wait`, `read`, `capture`, `quit` の限定 schema を定める。任意コード実行 API は作らない。
- UI 状態、locale、DPI、window geometry、renderer/backend、起動引数を capture manifest に含める。
- action log と画像を一つの run directory にまとめ、同一 fixture の再実行と差分比較を可能にする。

### Phase 3: GPU viewport capture lane

- swapchain を使わない Diligent offscreen surface が実アプリの Composition 経路に適用可能か調べる。
- 成立する renderer / layer / effect 範囲だけを宣言し、非対応条件は明示的に失敗させる。GUI全体が描けると偽らない。
- 画像評価は対象機能ごとに計画し、golden image の基準、許容差、色管理、ロケールを定義してから評価する。

## 受け入れ条件

- Windows desktop にトップレベル window が表示されない状態で、実 UI widget tree が生成される。
- OS マウス／キーボードを動かさず、objectName 指定の入力 action が既存 Qt event handler に到達し、状態変化を構造化出力で確認できる。
- widget capture が画面依存なしで PNG として保存できる。GPU capture は対応 lane と制約を manifest に記録する。
- action failure、timeout、クラッシュ時に exit code・ログ・失敗 action が保存され、子プロセスが残らない。
- 一時プロジェクト／app data 以外へ書き込まず、`ja` / `en` を含む locale 指定が保存済み設定や OS 言語に依存しない。
- 既存 `capture_ui_test.py` fixture と Component no-window の挙動を維持する。

## 制約・未決事項

- 「offline」は操作・キャプチャ・デバッグ制御がローカルで完結する意味とする。アプリの外部通信機能を全て無効化するかは別途明示設定が必要。
- `Qt::WA_DontShowOnScreen` は不可視化の候補であり、Qt platform plugin / native child / graphics surface すべてで完全に画面外になる保証は未検証。
- Qt widget tree の描画と GPU Composition viewport の描画は別経路である。前者が動いても後者の swapchain-free capture は証明されない。
- Qt software offscreen と Diligent GPU offscreen は別 lane とし、両者の parity を仮定しない。
- UIテストは `ja` / `en` と設定画面で有効な追加 locale を明示実行する。UI操作用の多言語 fixture では基準画像がない locale を英語との厳密 pixel diff に使わない。
- ビルド、CMake、テスト、GUI起動による検証はユーザーの明示指示後に行う。

## 参照

- `tools/capture_ui_test.py`
- `Artifact/src/AppMain.cpp` (`ARTIFACT_UI_TEST_COMPONENTS_NO_WINDOW`)
- `Artifact/src/Widgets/ArtifactStartupScreenshot.cpp`
- `Artifact/src/Render/ArtifactOffscreenCompositionRenderer.cpp`
- `docs/technical/STARTUP_FLAGS_CONTRACT_2026-09-29.md`
- `docs/technical/HOT_PATH_RULES.md`
- [Qt QWidget::grab / render](https://doc.qt.io/qt-6/qwidget.html#grab)
- [Qt QShowEvent delivery timing](https://doc.qt.io/qt-6/qshowevent.html)
- [Qt::WA_DontShowOnScreen](https://doc.qt.io/qt-6/qt.html)
