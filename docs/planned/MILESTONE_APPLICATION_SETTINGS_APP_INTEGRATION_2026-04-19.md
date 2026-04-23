# MILESTONE_APPLICATION_SETTINGS_APP_INTEGRATION_2026-04-19

`ApplicationSettingDialog` とアプリ本体 (`ArtifactMainWindow` / `AppMain`) の連携を整理し、設定を「保存するだけ」ではなく「実際にアプリへ反映できる」状態へ持っていくためのマイルストーン。

## Goal

- Preferences で編集した値を `ArtifactAppSettings` に保存する
- 保存した値を起動時に本体へ反映する
- 設定変更後に、再起動なしで主要 UI が追従する
- メニューバー、QADS タブ、テーマ、プレビュー系のような「見た目」と「挙動」の境界を整理する

## Scope

- `Artifact/src/Widgets/Dialog/ApplicationSettingDialog.cppm`
- `Artifact/include/Widgets/Dialog/ApplicationSettingDialog.ixx`
- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- `Artifact/include/Widgets/ArtifactMainWindow.ixx`
- `Artifact/src/AppMain.cppm`
- `ArtifactCore/src/Application/ArtifactAppSettings.cppm`
- `ArtifactCore/include/Application/ArtifactAppSettings.ixx`
- `Artifact/src/Widgets/ArtifactMenuBar.cppm`
- `Artifact/include/Widgets/Menu/ArtifactMenuBar.ixx`
- `Artifact/src/Widgets/Dock/DockStyleManager.cppm`

## What This Milestone Covers

### 1. Settings Model As The Single Source Of Truth

- `ArtifactAppSettings` に UI 関連の設定を集約する
- `QSettings` や個別 widget の ad-hoc 状態を減らす
- 設定キーを `General / UI / Render / Preview` のように整理する

### 2. Preferences UI Editing

- `ApplicationSettingDialog` の各ページで値を編集できるようにする
- 数値、スイッチ、テーマ選択、フォント設定などをここで一元編集する
- 変更した値を `Apply` / `OK` で保存する

### 3. Live Application Sync

- 設定変更後に main window が即追従する
- メニューバーの文字サイズ
- QADS タブの文字サイズ
- テーマ、アクセント、サーフェス色
- 必要ならプレビューや render center の見た目も再適用する

### 4. Startup Sync

- `AppMain` 起動時に `ArtifactAppSettings` を読み込む
- `applyDCCTheme()` と同様に UI 設定も本体に流す
- layout restore や workspace restore と衝突しないよう順序を整理する

## Non-Goals

- Preferences を完全な設定エンジンにすること
- すべての widget を一気に live bind すること
- QSS ベースの再設計に戻すこと
- 新しい中央集権イベントバスを導入すること

## Recommended Order

### Phase 1: Settings Keys And Persistence

- `ArtifactAppSettings` に UI 用の設定キーを追加する
- フォント、表示スケール、必要なら色設定を持たせる
- 既定値を決める

### Phase 2: Preferences Editing UI

- `ApplicationSettingDialog` の既存ページに設定項目を追加する
- 値の保存・読み込みを実装する
- `Apply` で本体へ反映できるようにする

### Phase 3: Main Window Apply Path

- `ArtifactMainWindow` に UI 設定の再適用口を作る
- メニューバーと dock tab の font を再計算して反映する
- 必要なら関連 widget を repolish する

### Phase 4: Startup And Theme Consistency

- `AppMain` の初期化順に settings 反映を組み込む
- theme 適用と font 適用の順序を整理する
- 既存の layout restore と干渉しないように確認する

## Current Status

- `ApplicationSettingDialog` は既に複数ページ構成になっている
- `ArtifactAppSettings` は既にアプリ設定の保存基盤として機能している
- theme 反映は `AppMain` で本体へ流している
- メニューバーと QADS タブはそれぞれ個別に font 調整が入っている
- いまの課題は、これらの設定更新を「編集 UI -> 設定保存 -> 本体再適用」の1本の経路に揃えること

## Validation Checklist

- Preferences で変更した値が保存される
- 再起動後も設定が維持される
- メニューバーの文字サイズが設定に追従する
- QADS タブの文字サイズが設定に追従する
- theme 更新と font 更新が競合しない
- 設定変更後にアプリ全体の再起動が不要になる
