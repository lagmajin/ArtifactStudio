# Dock Empty State マイルストーン

**最終更新:** 2026-09-13

**ステータス:** Not Started

## 目的

全ドックを閉じた／中央エリアが空のときに、真っ黒な面ではなく「次の一手」だけを示す空状態を表示する。`+`追加導線（`MILESTONE_DOCK_PANEL_ADD_MENU_2026-08-15.md`）の発見性を補完し、レイアウト復旧の迷子をなくす。

## 背景

- `+`によるパネル追加・再表示、Recent／Favorite（IDベース）、default state復元は実装済み（runtime検証pending）。
- 一方、全ドックclose時はガイドがなく、初心者は復旧導線に辿り着けない。
- 前例として `Artifact.Widgets.CompositionEmptyOverlay`（`Artifact/src/Widgets/CompositionEmptyOverlay.cppm`）、`ArtifactWelcomeWidget`、`docs/design/composition-empty-state/README.md` の言語（外枠なし・単一主操作・Project View配色／密度）がある。これを流用し、新規デザイン言語は作らない。

## スコープ

- 中央ワークスペースが空（visible dock数==0、またはcentral area空）のときのみ表示する `ArtifactDockEmptyState` widgetの新設。
- 主操作1つ「パネルを追加」→ 既存`+`メニュー（`ArtifactMenuBar` corner `+`／`View > Window Panels > パネルを追加／再表示`）と同一の列挙・activate経路を呼ぶ。重複生成しない。未登録IDを出さない。
- 副操作1つ「デフォルトレイアウトに戻す」→ 既存default state restore APIを再利用。
- 左右／下部の空areaは対象外とする（中央のみ。広げる場合は別途要求が必要）。

### 非スコープ

- 新パネル本体、QADS置換、preset再設計、パネル内部構成変更、サブモジュール変更。
- Welcome／Composition空状態の文言・導線の改変（再利用はするが変更しない）。
- 常時バッジ、`Selected`集約、タブ色分けの追加。
- 新規EventBusイベント、新規グローバルsignal/slot、新規`QSettings`キー、新規ショートカットの追加。

## UI方針

- `EmptyCompositionOverlay`と同型：外枠なしカード、`QVBoxLayout`縦積み、`QPalette`＋`currentDCCTheme()` tokenのみ。QtCSS／`setStyleSheet()`／`QColorDialog`を使わない。
- アイコンは `Artifact/App/Icon/Studio/` の既存オリジナルSVGのみ。新規Material参照なし。
- レスポンシブは `updateResponsiveLayout()` を移植：`width<420／height<300`でcompact化（icon／説明文省略、ボタン高さ34／36切替）。
- 通常時の常設要素を増やさない。Empty State自体もタイトル＋説明1行＋主ボタン＋副リンクまでに抑える。
- tooltip／accessibleName／descriptionを設定（Add Menu Phase 4と同一方針）。
- Dockタブ／ドキュメントタブ分離を維持：未保存マークを出さない、ドキュメント操作を混在させない。
- VP周辺モック（`docs/design/composition-viewport/README.md`）の採用範囲に触れない。キャンバス内流用なし。

## 責務境界

| 責務 | 所有者 |
|---|---|
| visible判定 | `DockLayoutRegistry`問合せのみ（書かない） |
| 追加・activate | 既存DockManager facade／`ArtifactMainWindow` API |
| Recent／Favorite | 既存`Workspace/RecentDockIds`境界（Empty Stateは書かない） |
| 文言・配置 | 新規`ArtifactDockEmptyState` widget内のみ |
| 個別パネルの編集内容 | 各パネル本体（触らない） |

ボタン接続は `EmptyCompositionOverlay` と同型の `std::function` 注入とし、MainWindow側の既存ハンドラに接続する。新規signal/slot配線は作らない。

## 実装フェーズ

### Phase 1: 表示widget新設

- [ ] `ArtifactDockEmptyState` widget新設（表示のみ、コールバック2つ：addPanelRequested、restoreDefaultRequested）
- [ ] `centralWorkspaceLayout`への重ね置き／切替接続。QADS private APIに触らない
- [ ] `Impl*`生所有（`Impl`所有にshared／unique_ptr新規採用しない）、デストラクタで`delete`

### Phase 2: 表示条件配線

- [ ] MainWindow側でvisible数0↔復帰のshow/hide配線。既存visibility／top-level同期経路に相乗り
- [ ] portable JSON／registryへの書き込みをしないことを確認
- [ ] `+`メニューと同一覧・同一activateであることを確認

### Phase 3: 密度とアクセシビリティ

- [ ] 狭幅（width<420／height<300）での崩れを確認
- [ ] ダーク／ライトの既存token視認性を確認
- [ ] キーボード（Escで閉じない、フォーカスはボタンへ）、screen readerを確認

## 受け入れ条件

- [ ] 全dock closeでガイドが出る、1つでも表示で消える
- [ ] 「パネルを追加」が既存`+`と同一一覧・同一activate動作
- [ ] 未登録IDを出さない、重複生成しない、保存復元を壊さない
- [ ] QtCSS、QColorDialog、新規グローバルsignal/slot、`QImage`新規、ホットパス確保を導入しない
- [ ] `DockStyleManager`公開APIにQADS型を再流出させない
- [ ] `git -C Artifact diff --check`通過（コード変更時）

## 検証方針

ビルド、CMake、テスト、runtime確認は実装後にユーザー許可を得て実施する。本マイルストーン作成時点ではコード変更と検証は行わない。

## 関連文書

- `docs/planned/MILESTONE_DOCK_PANEL_ADD_MENU_2026-08-15.md`
- `docs/planned/MILESTONE_INDEPENDENT_DOCK_MANAGER_2026-08-13.md`
- `docs/planned/MILESTONE_DOCK_WORKSPACE_DESIGN_AUDIT_2026-07-04.md`
- `docs/design/composition-empty-state/README.md`
- `docs/design/composition-viewport/README.md`
- `docs/WIDGET_MAP.md`
