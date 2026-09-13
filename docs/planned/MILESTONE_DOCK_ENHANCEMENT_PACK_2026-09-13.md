# Dock Enhancement Pack — 1/2/4/5/7/9

**最終更新:** 2026-09-13

**ステータス:** Not Started

## 目的

ドック運用の迷子・手戻りを減らす6件（1, 2, 4, 5, 7, 9）を1パックとして起案する。いずれも既存facade／registry／portable JSON／`+`導線の再利用を原則とし、新規レイアウトエンジン・QADS置換・パネル本体実装は行わない。

## 現状確認（事実）

- `WorkspaceMode`は10種実装済み：`Default / Import / Layout / Animation / VFX / Compositing / Text / Export / Debug / Audio`（`Artifact/src/Core/ArtifactWorkspaceModes.cppm`、`workspaceModeInfos()`）。`kWorkspaceModeCount = 10`。
- Ctrl+1〜4＝Default／Animation／Compositing／Audioのみ対応。残り6modeはメニュー経由。Shift+Space＝focused dockのimmersive切替あり（`MILESTONE_DOCK_WORKSPACE_DESIGN_AUDIT_2026-07-04.md`）。
- portable JSON＝versioned envelope、旧配列互換、入力検証あり。`DockLayoutDocument`／registryにarea／tab group／visible／pinned／floating geometry記録（`MILESTONE_INDEPENDENT_DOCK_MANAGER_2026-08-13.md`）。
- `+`追加導線＝登録済みDockのみ列挙、既存dockの`show()／raise()／setAsCurrentTab()`で再表示、IDベースRecent／Favorite（`MILESTONE_DOCK_PANEL_ADD_MENU_2026-08-15.md`）。
- Command Palette＝`ArtifactCommandPaletteWidget`（`Artifact/src/Widgets/CommandPalette/ArtifactCommandPaletteWidget.cppm`）にMRU・fuzzy検索あり。Dock再表示コマンドは未登録。
- pin＝closable抑止として実装済み。Lock（移動・float禁止）はなし。

## 共通制約

- QtCSS／`setStyleSheet()`新規禁止、`QColorDialog`禁止、新規グローバルsignal/slot禁止。
- Dockタブ／ドキュメントタブ分離を維持。未保存マークをDockタブに出さない。
- 固定キー禁止・ローカルバインド必須：新規ショートカットは`ShortcutBindings`に`ShortcutId`＋既定キー＋表示名＋永続化キー＋所有コンテキストを登録し、`matches()`で判定する。`event->key() ==`直書き・フォールバック禁止。
- 独自コンテナ優先、新規ホットパス確保なし、`QImage`新規なし。
- QADS private APIに依存しない。`ArtifactMainWindow`公開APIを急激に変えない。

---

## 1. Workspace Presetの仕上げ

### 課題

10modeあるのにクイック切替は4つだけ。Color／Review用途の置き場所が不明確。

### 方針

- Phase 1で既存10modeのvisibility ruleを棚卸しし、Color／Review用途をExport／Compositing rule拡張で吸収できるか、新mode追加が必要かを判定する。`kWorkspaceModeCount`変更は判定後に最小化する。
- Ctrl+1〜4は維持し、Ctrl+5〜0で残りmodeへ拡張する場合は`ShortcutBindings`へ正式登録する（GlobalではなくWorkspaceコンテキスト）。

### 受入

- [ ] 10modeの表示rule一覧が文書化される
- [ ] Color／Reviewの扱い（既存mode拡張 or 新mode）が決定される
- [ ] 追加ショートカットは設定画面表示・保存／再読込・空バインド無効化・競合検出を確認

### 非スコープ

- レイアウトエンジン再設計、QADS置換。

---

## 2. 壊れたレイアウトのSafe復旧

### 課題

portable JSON／ADS state破損時にレイアウトが崩れたまま起動する。

### 方針

- `DockLayoutDocument`の既存version検証・入力検証を入口に使い、失敗時に「前回正常／工場出荷／現状維持」の3択ダイアログを出す。
- 「前回正常」＝直近の正常portable JSONバックアップ、「工場出荷」＝起動時標準ADS state（既存default state capture再利用）。

### 受入

- [ ] 破損時に起動不能・真っ黒にならない
- [ ] 選択肢ごとの復元結果がportable JSON round-tripで壊れない
- [ ] 存在しないDock IDを安全に無視する

### 非スコープ

- 自動修復の推測実行（ユーザー選択なしの書換えはしない）。

---

## 4. Immersiveの対象記憶

### 課題

Shift+Space最大化はその場限りで、次回も同じDockから始めたい。

### 方針

- 最後にimmersive化したdock IDを既存Workspace settings境界に保存し、次回Shift+Space押下時に同Dockから復帰する。既存immersive保存・復元経路の拡張のみ。
- registryへの新規永続モデル追加は最小化し、portable JSONのvisible／pinned互換を壊さない。

### 受入

- [ ] 再起動後も前回immersive対象を復元できる
- [ ] 対象Dockが未登録の場合は安全に無視し、focused dockへfallbackする
- [ ] 既存close／float／tab操作を壊さない

---

## 5. タブオーバーフロー一覧 `>>`

### 課題

タブが多いとはみ出して選べない。

### 方針

- ADS内部title-bar private APIに依存せず、`ArtifactMenuBar` corner `+`と同手法（既存MainWindow chrome内のボタン＋登録済みDock再列挙）で`>>`ポップアップを実装する。
- 一覧選択は既存表示／activate API（重複生成なし）を呼ぶ。

### 受入

- [ ] 狭幅でもタイトル・既存操作を圧迫しない
- [ ] 未登録Dockを出さない
- [ ] キーボード操作・Esc閉じ・tooltip／accessible nameあり

---

## 7. Pin＋Lockの分離

### 課題

pin（閉じられない）だけで、誤ドラッグ・誤floatを防げない。

### 方針

- 現行pin（closable抑止）は維持し、`Lock（移動・float禁止）`を追加する。`DockLayoutRegistry`に`locked` flagを追加し、portable JSONへversion互換で保存・復元する。
- 操作は既存facade経由のfeature mappingに閉じる。

### 受入

- [ ] pinとlockを独立にon/offできる
- [ ] 旧JSON（lockedなし）を後方互換で読む
- [ ] lock中は移動・float・tab引抜きを抑止し、解除で復帰する

---

## 9. コマンドパレット連携

### 課題

パネル追加が`+`／Viewメニューからしかできない。

### 方針

- `+`の登録Dock列挙ロジックをdescriptor化し、Command Paletteから`パネル名`で検索→追加・activateできるようにする。既存`show()／raise()／setAsCurrentTab()`再利用、重複生成なし。
- Palette側は既存MRU（`CommandPalette/MruActions`）に相乗りし、新規settingsキーを増やさない。Dock Recent／Favorite境界も既存のまま。

### 受入

- [ ] `Ctrl+P → パネル名`で未配置は追加、配置済みはactivateされる
- [ ] 未登録Dockを出さない
- [ ] 新規グローバルsignal/slotを追加しない

---

## 実装順（推奨）

2 Safe復旧 → 1 Preset仕上げ → 5 タブ一覧 → 9 パレット連携 → 7 Lock → 4 Immersive記憶

## 検証方針

ビルド、CMake、テスト、runtime確認は実装後にユーザー許可を得て実施する。本マイルストーン作成時点ではコード変更と検証は行わない。

## 関連文書

- `docs/planned/MILESTONE_DOCK_PANEL_ADD_MENU_2026-08-15.md`
- `docs/planned/MILESTONE_INDEPENDENT_DOCK_MANAGER_2026-08-13.md`
- `docs/planned/MILESTONE_DOCK_WORKSPACE_DESIGN_AUDIT_2026-07-04.md`
- `docs/planned/MILESTONE_WORKSPACE_MANAGER_2026-03-29.md`
- `docs/planned/MILESTONE_WORKSPACE_PRESETS_2026-04-10.md`
- `docs/planned/MILESTONE_DOCK_EMPTY_STATE_2026-09-13.md`
- `docs/WIDGET_MAP.md`
