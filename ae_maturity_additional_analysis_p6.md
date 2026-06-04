# AE-Like 成熟度追加分析 — パート6（パート1-5 + 既存1-31 以外の問題）

**調査対象**: Application/AI/WebUI/ActiveQt/PowerShell/Script/Network/Reactive/Platform/UI/Diagnostics/Widgets 細部  
**制約**: ソースコードのみ。

---

## P0 — 即時修正必須

### 1. AudioPreviewWidget の signal/slot サイクル無限再帰
- **発生箇所**: `Artifact/src/Widgets/AudioPreviewWidget.cppm:496-511`
- **内容**: `engine::positionChanged` → `waveform::setPosition` (emit) → `engine::setPosition` (emit) のループ。タイマー/ドラッグで無限再帰してクラッシュ。
- **影響**: アプリ落ち。

### 2. BuiltinScriptVM の timeout が谎り — future がブロッキング解体
- **発生箇所**: `ArtifactCore/src/Script/Engine/BultinScriptVM/BuiltinScriptVM.cppm:61-70`
- **内容**: `std::async` の `future` を timeout 経路で `get()` せずにスコープ抜け。C++ 規格でデストラクタは完了までブロックするため、タイムアウトが機能せず呼び出し元が永久停止。
- **影響**: スクリプト評価が UI スレッドを永久に停止させる。

### 3. PowerShellWidget のコマンド注入
- **発生箇所**: `Artifact/src/Widgets/PowerShellWidget.cppm:124`
- **内容**: ユーザー入力をそのまま `powershell.exe -Command` に渡し、エスケープ/サニタイズなし。
- **影響**: プロジェクトファイルやネットワーク経由の入力が任意コマンド実行に至る危険。

---

## P1 — 高重大度

### 4. AudioPreviewWidget の QAudioSink がリロードでリーク
- **発生箇所**: `Artifact/src/Widgets/AudioPreviewWidget.cppm:244`
- **内容**: `loadFile()` で `new QAudioSink` するが古いインスタンスを delete しない。
- **影響**: 音声ファイル切替でメモリリーク。

### 5. CollabPresenceWidget の QLayoutItem リーク
- **発生箇所**: `Artifact/src/Widgets/CollabPresenceWidget.cppm:143-145`, `121-126`
- **内容**: `removeItem()` した後 widget だけ `delete` し `QLayoutItem` を解放しない。`clearUsers()` も同様。
- **影響**: コラボ表示を更新するたびレイアウト管理ポインタがリーク。

### 6. BuiltinScriptVM でタイムアウト経路の data race
- **発生箇所**: `ArtifactCore/src/Script/Engine/BultinScriptVM/BuiltinScriptVM.cppm:61-69`
- **内容**: `requestCancel()` した直後にバックグラウンドスレッドが `evaluator_` を触り続け、`setVariables(backup)` と競合。
- **影響**: 未定義動作/クラッシュ。

### 7. AIClient::shutdown の data race
- **発生箇所**: `Artifact/src/AI/AIClient.cppm:656-657`
- **内容**: `localAgent.reset()` と再代入が `mutex` なしで行われ、並行 `postMessage` が中途半端な状態を読む。
- **影響**: AI 連携が停止不能/クラッシュ。

### 8. InputOperator::KeyMap::removeBinding で emit→delete の UAF
- **発生箇所**: `ArtifactCore/src/UI/InputOperator.cppm:371-376`
- **内容**: `bindingRemoved(binding)` emit 直後に `delete binding`。スロット側がポインタを保持するとダングリング。
- **影響**: クラッシュ。

### 9. ActionManager::unregisterAction も同様に emit→delete の UAF
- **発生箇所**: `ArtifactCore/src/UI/InputOperator.cppm:222-228`
- **内容**: 解放後に保持したポインタが無効化。
- **影響**: クラッシュ。

### 10. ProfilerOverlayWidget の paintEvent で負の depth が巨大アロケート
- **発生箇所**: `Artifact/src/Widgets/Diagnostics/ProfilerOverlayWidget.cppm:251`
- **内容**: `std::string(static_cast<std::size_t>(s.depth * 2), ' ')` で `depth` が負なら `size_t` ラップで巨大確保、OOM。
- **影響**: プロファイラ描画時にアプリ停止。

### 11. 複数の Script/ScriptVM モジュールが空シェル
- **発生箇所**: `ArtifactCore/src/Script/Engine/Syntax/ASTNode.cppm`, `Func/ExprIntrinsics.cppm`, `Enviroment/EnvironmentManager.cppm`, `UI/ViewportOperator.cppm`, `Network/NetworkRPCServer.cppm`
- **内容**: モジュール宣言のみで中身が空。`EnvironmentManager` は本体がコメントアウト。
- **影響**: Script/Network/UI の機能自体が空虚。

### 12. BuiltinManager.cppm にテスト forward decl が混入
- **発生箇所**: `ArtifactCore/src/Script/Engine/Func/BuiltinManager.cppm:2`
- **内容**: `class tst_QList;` が本番コードに存在。
- **影響**: テスト依存の静的初期化/ODR 干渉リスク。

---

## P2 — 中程度/保守負債

### 13. FrameDebugViewWidget に未使用 import
- **発生箇所**: `Artifact/src/Widgets/Diagnostics/FrameDebugViewWidget.cppm:18`
- **内容**: `Widgets.Utils.CSS` がインポートされているが使用箇所なし。
- **影響**: 未整理コード。

### 14. ActiveContextService::goToFrame の int64→int トランケーション
- **発生箇所**: `Artifact/src/Application/ActiveContextService.cppm:115`
- **内容**: `static_cast<int>(frame)`。2^31 フレーム超のコンポジションで符号反転/範囲外。
- **影響**: 長尺シーク不能。

### 15. NLE/Core.cppm の free function が ODR 違反
- **発生箇所**: `ArtifactCore/src/NLE/Core.cppm:42`
- **内容**: `findClip`/`findTrack` 等が匿名名前空間にあるが `inline` ではない。複数 TU にインクルードされると ODR。
- **影響**: リンク/最適化で不定動作。

### 16. ShaderNode::link の出力→出力リンクガード不足
- **発生箇所**: `ArtifactCore/src/ShaderNode/ArtifactShaderNode.cppm:103`
- **内容**: `from->isOutput` をチェックせず、output→output のリンクが通ってしまいコード生成で失敗。
- **影響**: ノードグラフの不正接続が静的に成功し実行時エラー。

### 17. CollabPresenceWidget 以外でも QLayoutItem リークパターン
- **発生箇所**: `CollabPresenceWidget.cppm` 一括
- **内容**: `removeItem()` → `delete widget()` が複数個所で同一パターン。`QLayoutItem` の delete 忘れ。
- **影響**: 徐々にメモリリーク。

### 18. InputOperator の forbiddenModifiers チェックが常に false
- **発生箇所**: `ArtifactCore/src/UI/InputOperator.cppm:517`
- **内容**: `(event.modifiers & forbiddenModifiers_) != InputEvent::Modifiers()`。`Modifiers()` がゼロ初期化なら常に真になり forbidden が機能しない。
- **影響**: 修飾キー制約が常に無視。

### 19. AssetDirectoryModel のツールチップ連結スタイル
- **発生箇所**: `Artifact/src/Asset/AssetDirectoryModel.cppm:376`
- **内容**: `tooltip += "\nContent Ratio:";` の `"\n"` が `const char*`。暗黙変換で動作するがスタイル不安定。
- **影響**: ランタイムでは問題ないが保守性。

---

## 新規合計（パート6 のみ）

| カテゴリ | P0 | P1 | P2 |
|----------|----|----|----|
| 無限再帰/ブロッキング | 2 | 0 | 0 |
| コマンド注入 | 1 | 0 | 0 |
| メモリリーク (QAudioSink/QLayoutItem) | 0 | 2 | 1 |
| Use-after-free (emit→delete) | 0 | 2 | 0 |
| data race (UI/Network/Script/Platform) | 0 | 3 | 0 |
| 空シェル/スタブ (Script/Network/UI) | 0 | 6 | 0 |
| トランケーション/ODR | 0 | 1 | 1 |
| スタイル/ハイジーン | 0 | 0 | 3 |

---

*分析日: 2026-06-03*  
*派生元: 既存 ae_maturity_*.md パート1-5 の重複を除外*
