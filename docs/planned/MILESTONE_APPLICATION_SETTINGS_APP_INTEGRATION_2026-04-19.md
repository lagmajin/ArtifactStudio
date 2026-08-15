# MILESTONE_APPLICATION_SETTINGS_APP_INTEGRATION_2026-04-19

ステータス: Settings model／Preferences／主要 UI 反映基盤実装済み（startup order・全設定 live sync・runtime 検証待ち、静的確認 2026-07-29）
最終更新: 2026-08-15

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

---

## Next Execution Slice

Phase 1 は、保存対象を `General / UI / Render / Preview` に分けて、まず UI 系から固める。

### Phase 1A の着手点

1. `ArtifactAppSettings` に UI 用の設定キーを追加する
2. フォント、表示スケール、必要なら色設定を持たせる
3. 既定値を UI / theme / preview の順で決める
4. `QSettings` と widget の ad-hoc state を settings model に寄せる

### Phase 1 完了条件

- Preferences で変更した値が保存される
- 再起動後も設定が維持される
- 保存対象のキー境界が読める

### Phase 2A の着手点

1. `ApplicationSettingDialog` の既存ページに設定項目を追加する
2. 値の保存・読み込みを実装する
3. `Apply` で本体へ反映できるようにする
4. 編集 UI と settings model の責務を分ける

### Phase 2 完了条件

- Preferences で編集した値が settings model に入る
- `Apply` / `OK` で保存できる
- UI の編集内容が本体へ渡る

### Phase 3 への前提

- main window の再適用口は settings model が固まってから作る
- theme と font の適用順序は startup sync の前に整理する

## 2026-08-15 現行コード監査

- `ArtifactAppSettings` の schema は General／UI／Render／Preview に加え、Import、ProjectDefaults、Accessibility、ContentsViewer、Timeline、Viewport、AI などの設定を一元登録している。
- `ApplicationSettingDialog` の各ページには Import、Preview、Project Defaults、Memory & Performance、Composition View、Audio Scrubbing、Shortcuts、Plugins、AI、Accessibility などの編集面がある。
- Main Window／Menu Bar／Contents Viewer／Timeline などが `ArtifactAppSettings::instance()` を参照する利用箇所を確認した。
- ただし設定変更を全 UI が即時反映する live sync、起動時の theme／font／layout restore の順序、全設定の再起動後保持はコード検索だけでは完全には証明できず、runtime 検証待ち。

判定: **Settings schema、Preferences 編集面、主要 UI の参照経路は実装済み。全設定の live sync、startup order、runtime 保存／復元検証は pending。**
