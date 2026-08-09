# Neurodiversity Accessibility (2026-08-08)

**最終更新:** 2026-08-09
**状態:** 部分実装（N-1 Calm UIテーマのトークン適用済み。輝度上限・点滅検出は未完了）

## 概要

発達障害・ニューロダイバーシティ（ADHD、自閉スペクトラム症、ディスレクシア、その他認知特性）のユーザーが DCC 作業を持続可能にするための機能群。DCC 業界初の本格的ニューロダイバーシティ対応を目指す。

## 現状

`Calm` UIテーマのプリセットと設定ダイアログ導線を実装済み。`ArtifactAutoSaveManager` / `ArtifactUndoHistoryWidget` は不注意や作業記憶の弱さを**副産物的に**補完しているが、アクセシビリティ設定とは無関係に動作している。

## 実装進捗

- ✅ N-1 の低刺激 UI テーマトークン（無彩色ベース＋鈍青アクセント）を `DccStylePreset::CalmStyle` として追加
- ✅ 既存の Palette ベーステーマ再適用経路に接続。QtCSS の追加や新規シグナルは不要
- ✅ General Settings の UI Theme から `Calm` を選択・保存可能
- ✅ N-2 のフォーカスモード（`Ctrl+Shift+F`）でメニューバー、ツールバー、オプションバー、ステータスバー、非中央ドックを一時的に非表示化し、元の可視状態を復元
- ⏳ ビューポート輝度上限、フレーム間の高速明暗変化検出、ギズモ／コンテンツ描画への適用は次段階

## カテゴリ別計画

---

## カテゴリ A: ADHD 支援

### A-1. フォーカスモード / Zen モード

**目的**: 注意散漫を防ぐため、ビューポート以外の全パネルを非表示にする。AE の `~` キー最大化とは異なり、完全にクロームを排除する。

**仕様**:
- ショートカット（デフォルト: `Ctrl+Shift+F`）でトグル
- 全ドック・ツールバー・ステータスバーを `hide()`。ビューポートのみ表示
- ビューポートはウィンドウ全体を占有、背景色は無彩色に
- マウスを画面端に移動で一時的にツールバーを表示（オプション）
- フォーカスモード中でもコマンドパレット（`Ctrl+Shift+P`）は使用可能
- 再度ショートカットで元のレイアウトを復元

**実装**:
- `ArtifactMainWindow` に `enterFocusMode()` / `exitFocusMode()` 追加
- ドックの可視性スナップショットを保存し復元
- 設定でフォーカスモード中の背景色・輝度上限を指定可能に

**ファイル**:
- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- `Artifact/src/Widgets/ArtifactStatusBar.cppm`

---

### A-2. タスクタイマー / ポモドーロ通知

**目的**: DCC 作業は没入しすぎて数時間気づかないことが多い。定期的な休憩を促す。

**仕様**:
- 設定で作業時間（デフォルト: 25分）＋休憩時間（デフォルト: 5分）を指定
- 作業開始時にタイマー起動（手動または自動）
- 残り5分でステータスバーにカウントダウン表示
- 時間経過でソフトな通知（ビューポート端に半透明オーバーレイ「休憩をお勧めします」）
- 休憩モード中は全入力をブロック（強制モード）または通知のみ（ソフトモード）

**実装**:
- 設定: `focusTimerEnabled`, `focusTimerWorkMinutes`, `focusTimerBreakMinutes`, `focusTimerForceBreak`
- `QTimer` ベースのカウントダウン
- オーバーレイは CompositionRenderController の描画パスに追加

**ファイル**:
- `Artifact/src/Settings/AccessibilitySettings.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- `Artifact/src/Widgets/ArtifactStatusBar.cppm`

---

### A-3. ガイド付き操作 / ワンステップアクション

**目的**: 多段階の複雑な操作を作業記憶に頼らず実行できるよう、事前定義されたアクションシーケンスを1クリックで実行する。

**仕様**:
- よくある操作をプリセット登録:
  - 「平面レイヤーを追加して中央配置」
  - 「選択レイヤーにガウスぼかし(radius=5)を追加」
  - 「選択レイヤーを複製してX方向に50pxオフセット」
- ユーザーがカスタムシーケンスを記録・保存可能（簡易マクロ）
- コマンドパレットから実行
- 実行前に「この操作は Ctrl+Z で取り消せます」の確認表示（オプション）

**実装**:
- アクションシーケンスを JSON で定義（`ArtifactAppSettings` に保存）
- `ActionManager` の既存取引を逐次実行するランナー
- 記録モード: ユーザー操作をキャプチャしてシーケンス化（Phase 2）

**ファイル**:
- `ArtifactCore/src/Action/ActionManager.cppm`
- `Artifact/src/Widgets/CommandPalette/ArtifactCommandPaletteWidget.cppm`

---

### A-4. 注意散漫防止のアニメーション抑制

**目的**: ADHD ユーザーにとって、UI のアニメーションは意図せず注意を奪う。

**仕様**:
- 設定 `reduceMotion` が有効な場合:
  - ドックのスライドアニメーション → 即時表示切替
  - パネル折りたたみ → モーフィングなし
  - グラフエディタの補間線 → 静的描画（アニメーションしない）
  - ツールチップ → フェードインなしで即時表示
  - 通知オーバーレイ → 0.5秒以内に固定表示
- 既存の `QPropertyAnimation` / `QTimer::singleShot` 遅延表示を条件分岐

**実装**:
- OS の `prefers-reduced-motion` を `AccessibilitySettings` に取り込み
- 各アニメーション箇所に `if (reduceMotion) { immediate; return; }` を追加
- グローバルな `QApplication::setEffectEnabled` と併用

**ファイル**:
- 散布的（各 Widget のアニメーション箇所）
- `Artifact/src/Settings/AccessibilitySettings.cppm`

---

## カテゴリ B: 自閉スペクトラム症 (ASD) 支援

### B-1. 感覚過負荷の抑制（低刺激テーマ）

**目的**: 高彩度・高コントラストの UI が感覚過負荷を引き起こすユーザー向けに、色刺激を最小化したテーマを提供する。

**仕様**:
- プリセットテーマ「Calm」: 無彩色ベース＋低彩度アクセント
  - 背景: `#2A2A2E`（暗灰）
  - テキスト: `#C0C0C4`（淡灰）
  - アクセント: `#6A8CA0`（鈍青）
  - 選択色: `#506070`（彩度抑制）
  - ギズモ色: デフォルトより彩度 -40%
- ビューポート背景輝度上限: 設定可能（デフォルト: 120 cd/m² 相当 sRGB 値）
- 全色を `adjustColorForDeficiency` と同様のパイプラインで彩度抑制フィルタ適用
- 高速点滅コンテンツ（1秒間に3回以上の明暗反転）の自動検出と警告オーバーレイ

**実装**:
- DCC テーマシステムに「Calm」を追加
- `adjustColorForDeficiency` と同様の彩度抑制関数 `desaturateForSensory`
- フレーム間の輝度変化を CompositionRenderController の描画ループで検出

**ファイル**:
- `ArtifactCore/src/Application/AppSettings.cppm`
- `Artifact/src/Settings/AccessibilitySettings.cppm`
- `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm`
- テーマ定義 JSON

---

### B-2. 予測可能な UI レイアウト（パネルロック）

**目的**: ドラッグによる意図しないパネル移動がストレスの原因になるユーザー向け。レイアウトを固定し、変更不可にする。

**仕様**:
- 設定 `lockLayout` が有効な場合:
  - ドックのドラッグ移動を無効化
  - スプリッターのリサイズを無効化
  - パネル自動整列・自動リサイズを無効化
- レイアウトの保存・読み込み（「ロックされたレイアウト」として）
- リセット機能（既定のクリーンレイアウトに戻す）

**実装**:
- `QDockWidget::setFeatures` で移動・クローズ機能を制御
- `QSplitter` のハンドル無効化
- `QMainWindow::saveState` / `restoreState` でレイアウト永続化

**ファイル**:
- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- `Artifact/src/Settings/AccessibilitySettings.cppm`

---

### B-3. 音フィードバックの細粒度制御

**目的**: 特定の音が不快なユーザー向け。操作確認音を完全に OFF にするのではなく、音量・周波数特性を個別に調整できるようにする。

**仕様**:
- 音カテゴリごとの独立した音量設定:
  - UI 操作音（クリック、ホバー）
  - 完了通知音（レンダー完了、エクスポート完了）
  - エラー音
  - タイマー通知音（Phase A-2）
- 各カテゴリ: 音量 0-100%、音程 ±12 半音、波形（サイン/三角/ノイズ）
- マスターボリューム（既存）
- 「全音 OFF」クイックトグル

**実装**:
- 既存の `QSoundEffect` 使用箇所をカテゴリタグ付きに拡張
- 設定 UI にカテゴリ別スライダー追加

**ファイル**:
- `Artifact/src/Settings/AccessibilitySettings.cppm`
- `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`

---

## カテゴリ C: ディスレクシア支援

### C-1. 読みやすいフォント

**目的**: ディスレクシアのユーザーが UI テキストを読みやすくする。

**仕様**:
- フォント選択（ドロップダウン）:
  - システムデフォルト
  - OpenDyslexic（バンドルまたはシステムにインストール済みを検出）
  - Atkinson Hyperlegible（Braille Institute の無料フォント）
  - 任意のシステムフォント
- フォントサイズ（既存の `fontScalePercent` と連動）
- 適用範囲: プロパティパネル、タイムライン、レイヤーパネル、メニューバー、ツールチップ
- プロポーショナルフォント／等幅フォントの選択（タイムラインの数値読みやすさのため）

**実装**:
- `QApplication::setFont` + ウィジェット単位の `setFont` 上書き
- フォントファイルがバンドルされていない場合は OS 標準フォントにフォールバック
- OpenDyslexic は OFL ライセンスでバンドル可能

**ファイル**:
- `Artifact/src/Settings/AccessibilitySettings.cppm`
- `Artifact/src/AppMain.cppm`

---

### C-2. テキスト読み上げ (TTS)

**目的**: 長いプロパティ名、エラーメッセージ、レイヤー名を読み上げることで、テキスト処理負荷を軽減する。

**仕様**:
- Qt の `QTextToSpeech` を使用（Windows: SAPI5, macOS: NSSpeechSynthesizer, Linux: speech-dispatcher）
- 読み上げ対象:
  - ホバーしたプロパティ名と現在値
  - エラーダイアログの全文
  - レイヤー名（レイヤーパネルで選択時）
  - ツールヒント
- 読み上げ速度・ピッチ・音量を個別設定
- ショートカット（デフォルト: `Ctrl+Shift+T`）で読み上げトグル
- プロパティ値の単位（px, %, deg）も読み上げ

**実装**:
- `ArtifactTTS` サービスクラス（シングルトン）
- 各ウィジェットの `setAccessibleDescription` 内容を読み上げキューに送る
- 設定ダイアログに TTS 専用セクション追加

**ファイル**:
- `Artifact/src/Service/ArtifactTTSService.cppm`（新規）
- `Artifact/include/Service/ArtifactTTSService.ixx`（新規）
- `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`

---

### C-3. 行間・文字間隔の調整

**目的**: テキスト密度が高すぎるとディスレクシアのユーザーが読み飛ばしやすい。

**仕様**:
- 設定 `textDensity` : Compact / Normal / Relaxed / Very Relaxed
  - Compact: 現状維持
  - Normal: 行間 1.2x
  - Relaxed: 行間 1.5x、文字間隔 +10%
  - Very Relaxed: 行間 2.0x、文字間隔 +20%
- 適用範囲: プロパティパネル、タイムラインレイヤー名、ツールチップ、エラーメッセージ
- `QFont::setLetterSpacing` と `QFont::setWordSpacing` を使用（Qt 5.13+）

**実装**:
- `AccessibilitySettings` に `textDensityLevel` 追加
- 影響を受ける全テキスト要素に `applyTextDensity(font)` ヘルパー

**ファイル**:
- `Artifact/src/Settings/AccessibilitySettings.cppm`
- 散布的（各ウィジェット）

---

## カテゴリ D: 全般

### D-1. 取り消し操作の透明性

**目的**: 「取り返しのつかない操作をしてしまった」という不安を軽減する。

**仕様**:
- Undo スタックの上限を大幅に増加（デフォルト: 1000、設定で変更可）
- 破壊的操作の前に「この操作は Ctrl+Z で取り消せます」をステータスバーに表示
- Undo 履歴ウィジェットで過去の操作を一覧可能（既存 `ArtifactUndoHistoryWidget` をデフォルト visible に）
- 自動セーブ間隔を短縮可能（デフォルト: 5分 → 1分に設定可）

**実装**:
- 既存 `UndoManager` / `ArtifactAutoSaveManager` の設定項目追加

**ファイル**:
- `ArtifactCore/src/Undo/UndoManager.cppm`
- `Artifact/src/Project/ArtifactAutoSaveManager.cppm`

---

### D-2. エラーメッセージの平易化

**目的**: 技術的なエラーメッセージが読めずにパニックになるのを防ぐ。

**仕様**:
- 設定 `simpleErrorMessages` 有効時:
  - エラーダイアログに「何が起きたか」＋「どうすればいいか」の2行表示
  - 例: `QImage::scaled: Image is a null image` → 「画像を読み込めませんでした。ファイルが破損している可能性があります。別のファイルをお試しください。」
- エラーメッセージと対処法をペアで定義する辞書（JSON）
- 未定義のエラーは原文のまま + 「このエラーについて開発者に報告する」リンク

**実装**:
- メッセージ辞書: `resources/accessibility/error_messages.json`
- `QErrorMessage` の `qtHandler` をカスタムハンドラで上書き

**ファイル**:
- `Artifact/src/AppMain.cppm`
- `resources/accessibility/error_messages.json`（新規）

---

## Phase 一覧と優先順位

| Phase | カテゴリ | 内容 | コスト | 効果範囲 | リスク |
|-------|---------|------|--------|---------|--------|
| N-1 | ASD | 低刺激テーマ「Calm」 | 低 | ASD + 感覚過敏全般 | 極小 |
| N-2 | ADHD | フォーカスモード | 中 | ADHD + 全ユーザー | 小 |
| N-3 | ADHD | アニメーション抑制 | 低 | ADHD + 前庭障害 | 極小 |
| N-4 | 全般 | 取り消し操作の透明性 + エラー平易化 | 低 | 全認知障碍 | 極小 |
| N-5 | ディスレクシア | 読みやすいフォント | 中 | ディスレクシア | 小 |
| N-6 | ディスレクシア | TTS 読み上げ | 中 | ディスレクシア + 弱視 | 小 |
| N-7 | ASD | パネルロック | 低 | ASD | 極小 |
| N-8 | ADHD | タスクタイマー | 低 | ADHD | 極小 |
| N-9 | ADHD | ガイド付き操作 | 高 | ADHD + 初心者全般 | 中 |
| N-10 | ディスレクシア | 行間・文字間調整 | 低 | ディスレクシア | 極小 |
| N-11 | ASD | 音フィードバック制御 | 低 | ASD + 聴覚過敏 | 極小 |

Phase N-1〜N-4 までで DCC 業界初の本格的ニューロダイバーシティ対応としてリリース可能。Phase N-5 以降は継続改善。

## 変更対象ファイル一覧

| ファイル | Phase |
|----------|-------|
| `Artifact/src/Settings/AccessibilitySettings.cppm` | N-2,3,5,7,8,10,11 |
| `Artifact/include/Settings/AccessibilitySettings.ixx` | N-2,3,5,7,8,10,11 |
| `Artifact/src/Widgets/ArtifactMainWindow.cppm` | N-1,7 |
| `Artifact/src/Widgets/ArtifactStatusBar.cppm` | N-2,8,4 |
| `Artifact/src/Widgets/Render/ArtifactCompositionRenderController.cppm` | N-1,8 |
| `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm` | N-2,3,5,6,7,8,10,11 |
| `Artifact/src/AppMain.cppm` | N-5, D-2 |
| `Artifact/src/Service/ArtifactTTSService.cppm`（新規） | N-6 |
| `Artifact/include/Service/ArtifactTTSService.ixx`（新規） | N-6 |
| `ArtifactCore/src/Undo/UndoManager.cppm` | N-4 |
| `Artifact/src/Project/ArtifactAutoSaveManager.cppm` | N-4 |
| `resources/accessibility/error_messages.json`（新規） | D-2 |
