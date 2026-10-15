# Native Dock タブ操作強化マイルストーン

**最終更新:** 2026-10-16
**ステータス:** In Progress

## 目的

既定の `NativeDockSurface` のドックタブを、QADS と比較して日常操作に十分な水準へ引き上げる。対象は `Composition Viewer`、`Project`、`Inspector` などの**ドック／パネルタブ**であり、ドキュメントタブの保存・複製・未保存表示は含めない。

加えて、Visual Studio のタブエリアに相当する要素のうち、ドックパネルのタブへ適用できるものを M5 で扱う。Visual Studio はタブ配置として Top / Left / Right を提供し**下端は提供しない**ため、下端配置は本アプリ独自の拡張として、VS 準拠の要素と区別して設計・実装する。

タイムラインは既定で下領域に置かれ、コンポジションごとに 1 つのタブ（`timeline::<compositionId>`、タブ名はコンポジション名）になる。このコンポジション対応タブへ未保存の印を出す要望を M6 で扱う。ロックは対象外とし、`Pin＋Lock` の分離は `MILESTONE_DOCK_ENHANCEMENT_PACK_2026-09-13.md` の 7 に委ねる。

## 現状と比較の基準

- 現行コードにはタブの選択、同一バー内の並べ替え、別領域へのドラッグ、ダブルクリック／領域外ドロップによるフロート化、閉じる、ピン、横スクロール、長名省略、ドロップ予告がある。`Artifact/include/Widgets/Dock/NativeDockSurface.ixx` の実装を基準とする。
- QADS の公式説明には、タブが多い場合のタブメニュー、ドック領域単位のドラッグ、フローティング領域内へのドッキング、キャンセル可能なドラッグ、挿入順を示すプレビューがある。これらは比較対象であり、全機能を同時に移植する要求ではない。
- Visual Studio の公式ドキュメントでは、タブ配置は `Set tab layout` で **Top / Left / Right** を選べる（下端は無い）。タブ関連の公式機能には、複数行タブとピン留めタブの別行表示、タブドロップダウンでの非表示タブのイタリック表示、タブの色分け、閉じたタブの復元、プレビュータブ、未保存マーク（大きいドット）がある。このうちドックパネルのタブへ適用するのは**配置・複数行・ピン行・一覧表示・復元**であり、プレビュータブとプロジェクト基準の色分けはドキュメント専用のため対象外とする。未保存マークは M5 の対象外とし、コンポジションに 1 対 1 で対応するタブに限った例外として M6 で扱う。
- 現行 Native の chrome 描画はタブバー上端を前提にしている。`DockTabBar::paintEvent` の選択タブ contour は下辺を開けて描き、`DockTabSurface::paintEvent` の `contentTop` は `tabBar()->geometry().bottom() + 1` を基準に外枠を組み立てる。タブ一覧ボタンは `Qt::TopRightCorner` に固定されている。下端配置はこの 3 箇所の反転を伴う。
- タイムライン系のドックはコンポジション単位で生成される。`timeline::<compositionId>`、`dopesheet::<compositionId>`、`animation-timeline::<compositionId>`、`audio-mini::<compositionId>` があり、タイムラインのタブ名は解決時点のコンポジション名を使う（`Artifact/src/AppMain.cppm` の `timelineDockTitle` / `timelineDockObjectId` 系）。
- 未保存の状態は現状プロジェクト全体でしか持たない。`UndoManager::hasUnsavedChanges()` は単一の `version_` と `savedVersion_` の比較で、`ArtifactProject::isDirty()` もプロジェクト単位。コンポジションごとの dirty は無く、`UndoCommand` の基底はコンポジションの scope を問い合わせる口を持たない（`compositionId_` は各サブクラスの private メンバ）。`Composition Viewer` の編集コンテキストタブは、このプロジェクト全体の値を `artifactDocumentDirty` として表示している。
- `NativeDockSurface` に登録済みドックの `setDockTitle` API を追加した。タイトル変更は `titles_`、パネル widget、現在のタブ文字列、浮動ウィンドウ見出し、Auto-Hide サイドバー／フライアウト、`ArtifactMainWindow` の public wrapper を更新する。実装経路は静的確認のみ。
- `AGENTS.md`、`docs/design/composition-viewport/README.md` のタブ責務、`MILESTONE_DOCK_ENHANCEMENT_PACK_2026-09-13.md` は「未保存マークを Dock タブに出さない」と定めている。タイムラインのコンポジションタブは `timeline::<compositionId>` の Dock タブであるため、M6 の実施には規則の扱いを決める必要がある。
- Native は任意のコンテナルート split tree と浮動ウィンドウ内の局所split treeを作れる。各パネルのローカル／コンテナ split path・比率と、組み込み5領域の上下・左右 splitter 比率は任意のレイアウト項目として保存し、旧JSONではルートタブ面・既定サイズへフォールバックする。浮動グループには保存可能な安定IDを追加した。旧JSONは従来のタブID列で復元する。
- Pin は close 抑止、Lock は移動・float 抑止として分離し、lock 状態も任意の portable layout field へ保存する。

参考: [QADS 公式 README](https://github.com/githubuser0xFFFF/Qt-Advanced-Docking-System)、[Visual Studio: Customize window layouts and personalize tabs](https://learn.microsoft.com/en-us/visualstudio/ide/customizing-window-layouts-in-visual-studio?view=visualstudio)、[既存の QADS 比較調査](../../Artifact/docs/INVESTIGATION_QADS_UPSTREAM_COMPARISON_2026-06-23.md)、[ドック／ドキュメントタブの責務](../../AGENTS.md)。

## 実装段階

### M1 — タブへのアクセスと操作

1. タブバーの空き領域に、全パネルを名前で選べる一覧を追加する。省略・スクロールされたタブも選択できるようにする。
2. タブの右クリックメニューに、対象パネルの `閉じる`、`フロート化`、`元の領域へ戻す`、`ピン／解除` を、適用可能な場合だけ表示する。閉じる操作は既存の visibility と pinned の契約を通す。
3. メニューとタブ一覧はマウスおよび標準のキーボード操作で使えるようにする。製品操作の固定キーは追加しない。
4. タブに常時表示する操作は原則として名前と閉じるボタンに絞り、ピン操作をメニューへ移す。既存のピン状態は維持する。

**受け入れ条件:** 多数のタブでも目的のパネルへ到達できる。メニューの対象がクリックしたタブと一致し、選択中タブを誤って操作しない。ピン中の閉じる制約を保つ。

**進捗（2026-09-22）:** タブ一覧ボタンと、クリックしたタブを対象とする右クリックメニューを実装した。ドック済みタブではフロート化、グループフロート化、ピン／解除、閉じるを提供し、浮動タブでは元の領域へ戻す、ピン／解除、閉じるを提供する。ピンは常時タブ操作から外し、閉じるボタンだけを残した。ビルドおよび実機確認は未実施。

**進捗（2026-10-15・同一タブグループを閉じる）:** QADS 4.5.0 のタブコンテキストメニューに合わせ、ドック済み・浮動タブの右クリックへ `Close other panels` を追加した。対象は同じタブグループ内で現在開いており、closable かつ Pin されていないパネルだけとし、まとめて既存のレイアウト Undo 経路へ記録する。静的確認のみで、複数タブ・NoTab・Pin・Undo／Redo の実機確認は未実施。

**進捗（2026-10-15・閉じる機能フラグ）:** QADS の per-dock closable flag に合わせ、パネルごとの `Allow closing` を右クリックメニューへ追加した。無効時はドック済み・浮動タブの閉じるボタンを隠し、メニューの `Close panel` を無効にする。Auto-Hide フライアウトの閉じるボタンも同じ closable／Pin 判定を通す。Pin は引き続き閉じる操作の抑止、closable は閉じる機能自体の可否として独立する。状態は optional `closable` JSON field に保存し、旧レイアウトは true として扱う。コード差分の静的確認のみで、全閉じ経路・保存復元・Undo／Redo・実機UIは未確認。

**進捗（2026-10-15・中クリックで閉じる）:** QADS の `MiddleMouseButtonClosesTab` と同じく、タブ上の中クリックで closable かつ Pin されていないパネルを閉じる設定を追加した。既定は QADS の base config と同じく無効。右クリックメニューから有効化でき、portable layout JSON と既存 Undo 経路に含める。コード差分の静的確認のみで、中クリック、旧JSON、保存復元・Undo／Redoの実機挙動は未確認。

**進捗（2026-10-15・Auto-Hideボタンの領域切替）:** QADS の `AutoHideButtonTogglesArea` に合わせ、既定はアクティブパネルだけをAuto-Hideし、設定有効時またはCtrl押下時はタブ領域の開いているパネルをまとめてAuto-Hideする。設定は右クリックメニューと任意JSON項目 `autoHideButtonTogglesArea` で保持し、旧レイアウトは既定の false として読む。グループ内でAuto-Hideできないパネルが開いている場合は領域操作を無効にする。コード静的確認のみで、ボタン操作・Ctrl操作・旧JSON・Undo／Redoの実機確認は未実施。

**進捗（2026-10-15・Auto-Hide設定API）:** `ArtifactMainWindow` に `setAutoHideButtonTogglesArea()` / `autoHideButtonTogglesArea()` を追加し、Native では現在の surface 設定、QADS 互換 backend では `CDockManager` の `AutoHideButtonTogglesArea` config flag へ委譲する。コード差分の静的確認のみで、両 backend の呼び出し時期と実操作は未確認。

**進捗（2026-10-15・タブメタデータAPIの façade 公開）:** Native surface に既存の `dockIcon` / `setDockIcon` と `dockTabToolTip` / `setDockTabToolTip` を `ArtifactMainWindow` から呼べるようにした。QADS 互換 backend は `CDockWidget::icon` / `setIcon` と `setTabToolTip`、タブ widget の tooltip 参照へ委譲する。QADS の公開 API へ繋がるようになったが、実表示・backend 別コンパイル・タブ面移動後の反映は未確認。

**進捗（2026-10-15・per-dock機能 API の façade 公開）:** Native に実装済みだった closable / movable / floatable / focusable / Auto-Hide可否を `ArtifactMainWindow` から取得・設定できるようにした。QADS 互換 backend では `DockWidgetClosable` / `DockWidgetMovable` / `DockWidgetFloatable` / `DockWidgetFocusable` / `DockWidgetPinnable` へ対応づける。無効な dock ID は getter で false、setter は false を返す。静的確認のみで、全機能の制限時挙動、Undo／Redo、両 backend の実機動作は未確認。

**進捗（2026-10-15・フォーカス枠機能）:** QADS の `DockWidgetFocusable` に合わせ、パネル単位の `Show focus highlight` を追加した。無効にすると、そのパネルがアクティブな間だけ Native のタブ枠・パネル枠のフォーカス表示を抑制する。既定は true。optional `focusable` JSON field と既存 Undo 経路に含め、旧レイアウトは true として扱う。静的確認のみで、浮動・Auto-Hide・切替後の表示、保存復元・Undo／Redo は未確認。

**進捗（2026-10-15・ドックグループのドラッグ）:** ドック済み／浮動タブバーの余白から、現在のタブグループ全体をドラッグできる経路を追加した。浮動ウィンドウにタブ面が一つだけある場合は、独自ヘッダーからもそのグループをドラッグできる。ドロップ中は既存の一時プレビューを使い、タブ挿入位置、ドック面の中央／四辺 split、コンテナ外周 split、浮動面の中央／四辺 split、Auto-Hide サイドバーの挿入位置を表示する。split 時は先頭パネルで新しい面を作って残りを同じ面へ移し、順序と選択タブを維持する。浮動グループを Auto-Hide へ移した場合、ドック復帰時は元のタブ順を保って通常ドック面へ戻す。ドロップ先がなければ、ドック済みグループは浮動化し、既に浮動しているグループはウィンドウを移動する。独自ヘッダーのドラッグは複数 split 面を持つ浮動ウィンドウでは無効。コードの静的確認のみで、実機での各オーバーレイ、複数パネル split、Undo／Redo は未確認。

**進捗（2026-10-15・タブツールチップAPI）:** QADS の `CDockWidget::setTabToolTip` に合わせ、Native surface に `dockTabToolTip` / `setDockTabToolTip` を追加した。値は panel widget の `toolTip` に置き、通常・浮動タブと Auto-Hide サイドバーへ反映する。アプリ側のメタデータなので portable layout には重複保存しない。静的確認のみで、各surface間移動後の表示と呼び出し側統合は未確認。

**進捗（2026-10-15・パネルアイコンAPI）:** QADS の `CDockWidget::setIcon` / `icon` に合わせ、Native surface に `setDockIcon` / `dockIcon` を追加した。アイコンは通常・浮動タブ、タブ一覧、Auto-Hide サイドバーへ反映する。panel widget の `artifactDockIcon` dynamic property に保持し、アプリ側のメタデータとしてレイアウトへは重複保存しない。静的確認のみで、実表示・HiDPI・浮動タイトルバーと Auto-Hide flyout の表示は未確認。

**進捗（2026-10-15・動的タイトルAPI）:** QADS の DockWidget window-title 更新に合わせ、Native surface と `ArtifactMainWindow` に `setDockTitle` を追加した。タブ文字列、panel widget の window title、浮動ウィンドウのタイトル／見出し、Auto-Hide サイドバー／表示中フライアウトを同期する。タブ選択・グループ移動・表示復帰時にも、浮動ウィンドウの見出しを現在のタブへ合わせる。既存の `dockDisplayTitle`、一覧メニュー、閉じたパネル復元表示も更新済みタイトルを参照する。静的確認のみで、実際の改名、浮動グループのタブ切替、Undo／Redo・保存後の再起動は未確認。

**進捗（2026-10-16・Area Title Bar Actions）:** QADS の `CDockWidget::setTitleBarActions()` / `titleBarActions()` に対応する `ArtifactMainWindow::setDockTitleBarActions()` / `dockTitleBarActions()` を追加した。Native は選択中パネルの QAction を既存 Area Title Bar に表示し、下端タブ配置では上部タイトル行へ表示する。QAction は呼び出し側所有のまま guarded reference で保持し、QADS互換 backend は upstream API へ委譲する。追加シグナル接続は行わない。コード静的確認のみで、ボタン表示・切替・削除済み QAction の実機挙動は未確認。

**進捗（2026-10-16・タブ一覧ボタンの動的表示）:** QADS の `DockAreaDynamicTabsMenuButtonVisibility` に対応する設定APIを追加した。有効時は複数タブの表示名が省略される場合だけパネル一覧ボタンを表示し、タブ面／タブバーのリサイズ後と既存のタブ状態更新経路で再評価する。既定はQADSと同じ無効。QADS互換backendはグローバル設定フラグへ委譲する。静的確認のみで、長いタブ名・多数タブ・リサイズ時の表示切替は未確認。

**進捗（2026-10-16・無効なAreaボタンの非表示設定）:** QADS の `DockAreaHideDisabledButtons` に対応する設定APIを追加した。有効時は既存ルールで使用不可の Close group／Float group／Auto-Hide ボタンを非表示にし、無効時は従来どおり無効状態で残す。QADS互換backendはグローバル設定フラグへ委譲する。静的確認のみで、機能フラグ・Pin・Lock状態ごとのボタン表示は未確認。

**進捗（2026-10-15・浮動タイトルの選択同期）:** QADS の既定 `FloatingContainerHasWidgetTitle` に合わせ、浮動タブ面の現在タブに応じてウィンドウタイトルと独自ヘッダー見出しを更新する。マウス／キーボード選択と既存のプログラム操作経路から更新し、新しいシグナル接続は追加しない。コード静的確認のみで、タブ切替後の実表示と複数split面のウィンドウタイトル規則は未確認。

**進捗（2026-10-16・浮動コンテナのパネルアイコン）:** QADS の `FloatingContainerHasWidgetIcon` に対応する設定APIを追加した。有効時は浮動タブ面の選択パネルのアイコンをウィンドウアイコンへ反映し、アイコン未設定時と無効時はアプリのウィンドウアイコンを使う。既定はQADSと同じく無効で、設定切替・タブ選択・パネルアイコン更新時に既存経路で同期する。QADS互換backendはグローバル設定へ委譲する。静的確認のみで、各OSのタイトルバー表示は未確認。

**進捗（2026-10-16・タブ閉じるボタン表示）:** QADS既定の `ActiveTabHasCloseButton` に合わせ、Nativeも既定では選択中タブだけに閉じるボタンを表示する。`AllTabsHaveCloseButton` と `RetainTabSizeWhenCloseButtonHidden` の設定APIも追加し、非選択タブのボタン表示と非表示時のタブ幅保持を切り替えられるようにした。Pinは閉じるボタンを無効にする従来動作を維持する。QADS互換backendは同名config flagへ委譲する。静的確認のみで、タブ選択・並べ替え時の表示と幅変化は未確認。

**進捗（2026-10-16・タブ名の省略設定）:** QADS `DisableTabTextEliding` に対応する設定APIを追加した。有効時はタブ名を省略せず、既存スクロールボタンで横移動する。タブ一覧ボタンの動的表示が同時に有効な場合、省略設定時は文字の省略を、非省略設定時はタブ領域からのはみ出しを判定する。既定は省略表示。QADS互換backendは同名config flagへ委譲する。静的確認のみで、長い名前・タブ数・横スクロール・一覧ボタンの実機動作は未確認。

**進捗（2026-10-16・非選択タブ名の表示）:** QADS `ShowTabTextOnlyForActiveTab` に対応する設定APIを追加した。有効時はアイコンが設定されているタブに限り、非選択タブの文字列を隠してアイコンのみ表示し、選択時に保存済みタイトルを再表示する。アイコンなしのタブ名は表示したまま。既定は無効。QADS互換backendは同名config flagへ委譲する。静的確認のみで、タブ選択・タイトル改名・アイコン設定変更時の表示は未確認。

**進捗（2026-10-16・常時タブ表示）:** QADS `AlwaysShowTabs` に対応する `ArtifactMainWindow` 設定APIを追加した。既定では、浮動コンテナ内にタブ面が一つだけあり、表示タブが一つで、そのパネルにタイトルバー操作がない場合はタブバーを隠す。下端配置のタブバーは表示タブが複数ある場合だけ表示する。有効時は表示タブがあれば常に表示する。QADS互換backendは同名config flagへ委譲する。静的確認のみで、単一／複数面の浮動コンテナ、下端タブ、タイトルバー操作の実機表示は未確認。

**進捗（2026-10-16・ダブルクリックで分離）:** QADS `DoubleClickUndocksWidget` に対応する `ArtifactMainWindow` 設定APIを追加した。既定はQADSの `DefaultBaseConfig` と同じ有効で、無効化するとタブ上のパネル分離とNative拡張の空き領域からのグループ分離を止める。QADS互換backendは同名config flagへ委譲する。設定切替後のタブ操作は実機未確認。

**進捗（2026-10-16・タブ一覧ボタン）:** QADS `DockAreaHasTabsMenuButton` に対応する設定APIを追加した。既定では表示し、無効化するとドック済み・浮動・Auto-Hide の各タブ面からパネル一覧ボタンを隠す。`DockAreaDynamicTabsMenuButtonVisibility` と同時に使う場合も、一覧ボタン設定が有効な時だけ動的な省略判定を適用する。QADS互換backendは同名config flagへ委譲する。実機での切替表示は未確認。

**進捗（2026-10-16・Area Close ボタン）:** QADS `DockAreaHasCloseButton` / `DockAreaCloseButtonClosesTab` に対応する設定APIを追加した。既定はボタン表示・領域全体を閉じる動作とし、個別設定で非表示またはアクティブパネルだけ閉じる動作へ切り替える。ドック済み／浮動面へ反映し、Native の Pin・closable・close-protected 制約は両方の閉じ方で維持する。QADS互換backendは同名config flagへ委譲する。実機表示・単体／グループ閉じ・Undo／Redoは未確認。

**進捗（2026-10-16・Area Undock ボタン）:** QADS `DockAreaHasUndockButton` に対応する設定APIを追加した。既定は表示し、無効化するとドック済みタブ面の `Float tab group` ボタンを隠す。パネル単位の floatable 制約やドラッグ分離の可否は変更しない。QADS互換backendは同名config flagへ委譲する。表示切替と浮動後のグループ操作は未確認。

**進捗（2026-10-16・単一パネル面のタイトルバー抑制）:** QADS `CDockAreaWidget::HideSingleWidgetTitleBar` に対応し、`ArtifactMainWindow::setDockAreaHideSingleWidgetTitleBar()` / `dockAreaHideSingleWidgetTitleBar()` を追加した。Native ではタブ面ごとに保持し、単一パネルかつタイトルバー操作がない場合に上部タイトル／アクション行とタブバーを隠す。`AlwaysShowTabs` が有効なら抑制しない。設定は任意 `hideSingleWidgetTitleBar` JSON 項目に保存し、分割・浮動配置の復元後に適用する。QADS互換backendは対応する `CDockAreaWidget` フラグへ委譲する。静的確認のみで、実機表示、面移動、保存復元は未確認。

**進捗（2026-10-16・単一メインドック時のタイトルバー抑制）:** QADS `CDockManager::HideSingleCentralWidgetTitleBar` に対応し、`ArtifactMainWindow::setDockAreaHideSingleCentralWidgetTitleBar()` / `dockAreaHideSingleCentralWidgetTitleBar()` を追加した。Native はメインドックコンテナ全体の開いているパネル数を見て、一つだけでタイトルバー操作がない時にその面の上部行とタブバーを隠す。ドック面の開閉・移動後も全面を再評価し、`AlwaysShowTabs` を優先する。設定はグローバルで portable layout JSON には含めない。QADS互換backendは同名 config flag へ委譲する。静的確認のみで、1／複数パネル切替の実機表示は未確認。

**進捗（2026-10-16・分割挿入の均等化設定）:** QADS `CDockManager::EqualSplitOnInsertion` に対応する `ArtifactMainWindow::setDockEqualSplitOnInsertion()` / `dockEqualSplitOnInsertion()` を追加した。既定は QADS と同じ無効で、設定変更は既存splitterを変更せず次の挿入から適用する。有効時は同方向の連結split branchをdock面の葉数に応じて配分し、直交する親splitの比率は維持して新しい枝だけ均等化する。QADSの multi-child splitter とは木構造が異なるが、同方向の面の面積配分を近づける。QADS互換backendは同名 config flag へ委譲する。挿入比率の実機確認は未実施。

**進捗（2026-10-16・ドロップ先オーバーレイ判定の同期）:** Native の `dragEnter` / `dragMove` で、先にその位置のドック面／コンテナ外周オーバーレイを選び、表示中 overlay mode と同じ判定でグループドロップ可否を確認する。外周エッジ帯とドック面矢印が重なる時も、表示プレビューと確定先が食い違わないようにした。コード静的確認のみで、実際の重なり位置でのD&Dは未確認。

**進捗（2026-10-16・Splitter の非opaque resize）:** QADS `CDockManager::OpaqueSplitterResize` に対応する `ArtifactMainWindow::setDockOpaqueSplitterResize()` / `dockOpaqueSplitterResize()` を追加した。既定は QADS `DefaultNonOpaqueConfig` と同じ無効で、ドラッグ中は `QSplitter` の rubber band を表示し、release 時にサイズを確定する。有効時は従来どおりドラッグ中に内容をリサイズする。設定変更は既存のルート／ローカル／浮動 splitter にも反映し、新しい splitter にも引き継ぐ。QADS互換backendは同名 config flag へ委譲する。実機でのハンドル操作は未確認。

**進捗（2026-10-16・Auto-Hide ボタンのチェック表示）:** QADS `CDockManager::AutoHideButtonCheckable` に対応する `ArtifactMainWindow::setAutoHideButtonCheckable()` / `autoHideButtonCheckable()` を追加した。既定は QADS と同じ無効で、切替時は既存のドック面ボタンにも適用する。QADS互換backendは同名 auto-hide config flag へ委譲する。チェック状態の実機表示は未確認。

**進捗（2026-10-16・Auto-Hide サイドバーのアイコン表示）:** QADS `CDockManager::AutoHideSideBarsIconOnly` に対応する `ArtifactMainWindow::setAutoHideSideBarsIconOnly()` / `autoHideSideBarsIconOnly()` を追加した。有効時はアイコン付きタブのタイトルを隠し、アイコンのないタブはタイトルを表示する。設定切替、パネルタイトル・アイコン・ツールチップ更新後も同じ描画情報へ同期する。QADS互換backendは同名 auto-hide config flag へ委譲する。実機表示は未確認。

**進捗（2026-10-16・Auto-Hide のホバー開閉）:** QADS `CDockManager::AutoHideShowOnMouseOver` に対応する `ArtifactMainWindow::setAutoHideShowOnMouseOver()` / `autoHideShowOnMouseOver()` を追加した。有効時はサイドバータブへの hover から500ms後にフライアウトを開き、タブ／フライアウトから離れて500ms後に閉じる。移動先がフライアウト内なら閉じない。QBasicTimer の既存 timerEvent 経路を使い、新規signal／slot接続は追加していない。QADS互換backendは同名 auto-hide config flag へ委譲する。実機での遷移確認は未実施。

**進捗（2026-10-15・移動／フロート機能フラグ）:** QADS の `DockWidgetMovable` と `DockWidgetFloatable` に合わせ、パネルごとの `Allow moving` と `Allow floating` を分けた。通常の領域移動・split・タブ並べ替えは movable、分離・浮動グループへの合流は floatable で判定し、両方を止める既存 Lock は維持する。`movable` / `floatable` は optional JSON field で、旧レイアウトは true として読む。静的確認のみで、各フラグの組み合わせ、全ドラッグ先、Undo／Redo・復元の実機挙動は未確認。

**進捗（2026-10-15・Auto-Hide機能フラグ）:** QADS の pinnable capability に合わせ、`Allow Auto-hide` をパネル単位で追加した。既存の `Pin panel` は close 保護の意味を維持し、Auto-Hide への新規配置・ドラッグ先だけを別フラグで制御する。現在 Auto-Hide 中のパネルは、フラグを無効にしても戻す操作を妨げない。optional `autoHideable` field は旧レイアウトで true として読む。静的確認のみで、全ドロップ経路、保存復元・Undo／Redo の実機挙動は未確認。

### M2 — ドラッグとドロップの予測可能性

1. 同一タブバー内の挿入位置、別タブバーへの合流、五つの領域への移動を区別するプレビューを表示する。空の領域にも移動先が分かるようにする。
2. ドラッグを `Esc` など Qt 標準のキャンセル経路で終了した場合は元の配置を保つ。キャンセルと領域外ドロップを区別し、キャンセルをフロート化として扱わない。
3. 無効な移動先、同じ位置への移動、ドラッグ元パネルの消失を安全に処理する。
4. QADS の `CDockAreaWidget::allowedAreas()` に合わせ、対象 DockArea が許可する候補だけをオーバーレイに表示し、プレビューと確定判定で同じ許可集合を使う。既存レイアウトでは全方向を許可する。

**受け入れ条件:** プレビューと確定後の位置が一致する。キャンセル後にパネル数・順序・可視状態が変わらない。DPI 変更時と浮動ウィンドウからの操作も確認する。

**進捗（2026-10-16・DockArea許可方向）:** `ArtifactMainWindow::setDockAreaAllowedDropAreas()` / `dockAreaAllowedDropAreas()` を追加し、対象パネルが属するタブ面単位でQADS `CDockAreaWidget::setAllowedAreas()` に相当する許可集合を設定・取得できるようにした。Native の一時オーバーレイは許可された候補だけを表示し、同じマスクでカーソル判定を行う。任意JSON項目 `allowedDropAreas` に保存し、旧JSON・範囲外の値は全方向許可へフォールバックする。QADS互換backendでは対応する `CDockAreaWidget` へ委譲する。Auto-Hideで元のsplit leafが再生成される復帰経路にも許可集合を引き継ぐ。コード静的確認のみで、コンテキスト切替後の設定、オーバーレイ操作、保存復元、両backendの実機確認は未実施。

**進捗（2026-09-22）:** タブ上のドラッグでは、挿入位置を縦線で表示し、左半分／右半分に対応した位置へ確定するようにした。`Esc` で終了したドラッグはキャンセルとして記録し、領域外へ放した場合だけをフロート化として扱う。表示されていない上・下・左・右領域もウィンドウ端のドロップ対象として予告表示し、そこへドックを作れるようにした。ビルドおよび実機確認は未実施。

**進捗（2026-10-15・同位置への並べ替え）:** 同じタブ面内のドロップが移動前と同じ挿入位置になる場合、タブを抜き差しせず履歴 snapshot も作らないようにした。対象タブは選択状態にする。コード静的確認のみで、Qt drag/drop の実操作と Undo 履歴は未確認。

**進捗（2026-10-15・ダブルクリックで領域フロート）:** QADS 4.5.0 の動作に合わせ、既存のタブ上ダブルクリックは単一パネルのフロート化として維持し、ドック済みタブバーの空白部分をダブルクリックすると同じタブ面の全パネルを一つの浮動グループへ移すようにした。全メンバーの floatable／Lock 条件を検査し、既存のグループフロートと Undo 経路を使う。浮動中のタブバー空白では再フロートしない。コード差分の静的確認のみで、実際のダブルクリック・グループ順序・Undo／Redo は未確認。

**進捗（2026-10-15）:** 対象ドック面に一時的な中央＋四方向の配置オーバーレイを表示し、中央はタブ統合、四方向は対象面との局所splitへ接続した。中央オーバーレイへの合流はQADS 4.5.0に合わせてタブ先頭へ挿入し、タブバー上のドロップは挿入線の位置へ入れる。空のsplit葉はパネル移動・フロート化時に隣の枝へ畳む。splitterの現在寸法から比率を保存し、split path／比率の復元とUndoスナップショットに接続した。浮動ウィンドウも独立したsplit treeを持ち、対象面へのオーバーレイとグループ単位の保存・復元を行う。選んだ方向へのsplitに失敗した場合、別の固定領域へ成功扱いで移動するフォールバックを除き、プレビューと結果の不一致を防ぐ。コード差分は静的確認のみで、ビルド・ドラッグ操作・保存復元・Undo／Redoの実機確認は未実施。

**進捗（2026-10-15・外周ターゲット）:** QADS 4.5.0の DockArea overlay と Container overlay の二段判定を確認した。Native の中央＋四方向候補に加えて、コンテナ外周の一時ターゲットを追加した。外周方向の候補先に既存パネルがある場合はその領域の外縁タブ面をsplitし、空なら固定ルート領域へ追加する。外周ボタンの矩形内外を問わず、表示中の外周ターゲット帯でプレビューと同じ処理を実行する。表示中の外周アイコンはタブバーと重なる位置でも外周候補として扱う。split位置と比率は既存の `splitPath`／`splitRatios` 保存・復元経路へ渡す。プレビューも選択タブ面の分割側を表示し、通常のタブバー上は挿入プレビューを優先する。ルートは既存の5固定領域のままで、QADSの任意形状のコンテナsplit treeとはまだ差がある。コード差分は静的確認のみで、ビルドおよびドラッグ操作の実機確認は未実施。

**進捗（2026-10-15・一時オーバーレイとコンテナルート split）:** ドラッグ中だけドック先のオーバーレイを表示し、ドラッグ離脱・ドロップで消す経路を維持した。外周帯では対象アイコン上だけでなく帯の上にいる間も対応する方向を選択表示し、プレビューと確定方向を一致させる。ドック面と外周の候補が同方向で重なる場合はドック面の局所splitを優先し、方向が異なる場合はコンテナ外周splitを優先する（QADS 4.5.0 の二段overlay判定に合わせる）。この外周splitはメイン領域だけでなく浮動コンテナにも適用し、浮動中のsplit tree全体を新しいsplitの片側へ配置する。コンテナ外周の候補は既存の五固定領域内に押し込まず、現在のルート全体を新しい splitter の片側へ入れて分割する。ローカル split path と別にコンテナ split path／比率を保存し、レイアウト復元時に両階層を再構築する。Auto-Hide から戻る際、元の動的ルート葉が消えていれば保存したコンテナ path から葉を復元する。静的差分確認のみで、ビルド、D&D の表示・確定、レイアウト再読込、Auto-Hide 復帰は未確認。

**進捗（2026-10-15・Auto-Hide ドロップ）:** Auto-Hide サイドバーへのドラッグ中、配置先バーを一時オーバーレイし、タブ順序の挿入線を表示する。同じバー内で並べ替える場合は、ドラッグ元を除いた後の index に補正してプレビュー位置とのずれを防ぐ。ドック済みパネルと浮動パネルのドロップをサイドバー登録へ接続し、タブのクリック／キーボード操作でフライアウトを開閉、右クリックで通常ドックへ戻す操作や四辺への Auto-Hide 配置を選べるようにした。Auto-Hide 化前のタブグループ・順序・split pathを保持し、復帰時は同じタブグループへ戻し、元のsplit葉が空になっていた場合は保存した経路を使って枝を再構成する。レイアウト保存・復元にも元のグループ情報を含める。静的差分確認のみで、ビルド、実際のドラッグ／クリック、保存復元、Undo／Redo は未確認。

**進捗（2026-10-15・一時ドック先オーバーレイと領域ボタン）:** ドラッグ中にドック先を一時オーバーレイする仕組みは、対象タブ面の中央／四方向とコンテナ外周の候補に分けて実装済み。QADS の領域操作に近づけるため、ドック済みタブ面の右上に Auto-Hide、Close group、Float group、タブ一覧を並べた。Auto-Hide は辺領域でアクティブタブをその辺へ移動し、中央面では表示しない。Auto-Hide 不可またはロック中は無効。右クリックの Auto-hide tab group to からは開いているグループメンバーを選択した辺へまとめて移し、全メンバーに元グループ／順序／split path／比率を保存して、復帰時に再結合できるようにする。一括操作は既存Undoバッチで一履歴にまとめる。Float は面内の全パネルが floatable かつ unlocked の場合に有効、Close は開いている全パネルが closable・unpinned で中央ワークスペース保護対象でない場合に有効とする。Auto-Hideボタンのアクティブパネル／領域切替は `AutoHideButtonTogglesArea` とCtrl操作で選べる。コード差分の静的確認のみで、ビルド・ドラッグ操作・各ボタン・グループ復帰の実機確認は未実施。

### M3 — タブグループと浮動領域

1. 単一タブとグループ全体の移動操作を分け、グループ移動の対象を明示する。
2. 複数タブを持つ浮動領域への合流と、そこからの再ドックを設計・実装する。単一パネルの既存フロート操作との互換性を保つ。
3. グループ単位の閉じる・移動は、各パネルのピンと可視状態を尊重する。

**着手条件:** `DockLayoutDocument`、`DockLayoutRegistry`、Native の保存・復元契約でグループ ID、順序、浮動ジオメトリを表現できることを先に確認する。表現できない場合は保存形式を先に設計し、この段階の UI 実装を保留する。

**受け入れ条件:** グループを移動・浮動化・再ドックしても各パネルの実体は再生成されず、選択タブと順序が保存・再起動後に復元される。

**進捗（2026-09-22）:** 同じドック領域のタブを一つの浮動ウィンドウへまとめて移す操作と、浮動ウィンドウ内の全タブを元の領域へ戻す操作を追加した。ドック済みタブを浮動タブバーへドロップして、既存の浮動グループへ合流する経路も追加した。浮動タブの順序、選択中タブ、共有ジオメトリを `floating-tabs:` グループとして保存・復元する。実機での再起動復元、DPI、浮動タブからの操作は未確認。

**進捗（2026-10-15・浮動グループID）:** 浮動グループに別個の `floatingGroupId` を持たせ、タブの順序・メンバー列（旧 `tabGroup`）からグループ同一性を分離した。任意 JSON 項目として保存するため既存データは読み込み可能で、ID がない旧レイアウトは従来のメンバー列を使う。レイアウト復元時は同じIDのパネルを一つの浮動ウィンドウへまとめ、浮動中のパネル同士の移動でも残ったウィンドウのIDを維持する。コード差分は静的確認のみで、欠落パネルを含む復元、Undo／Redo、実際の浮動ウィンドウ間ドラッグは未確認。

**進捗（2026-10-15・ルート splitter 比率）:** 従来はパネルの split tree 比率だけを保存し、既定の上／中央／下と左／中央／右を分けるルート splitter の比率が保存対象外だった。version 1 のトップレベル任意フィールド `rootSplitterRatios` に6つの正規化比率を保存し、復元時に適用する。旧 JSON、破損した比率、未レイアウト時の空サイズは既定値を維持する。静的確認のみで、ウィンドウサイズ変更後の比率、空領域の表示切替、再起動後の実寸は未確認。

**進捗（2026-10-15・Pin／Lock分離）:** QADS の panel feature flags に近づけるため、Pin は close 制御のまま維持し、別の Lock を追加した。Lock 中はタブドラッグ・並べ替え・移動・split・Auto-Hide・float／再ドックを拒否し、コンテキストメニューから解除できる。Lock は optional `locked` JSON field に保存し、旧 layout は unlocked として読む。静的確認のみで、Qt のタブバー標準並べ替え抑止、各ドロップ経路、保存復元・Undo／Redo、全対応言語の UI は未確認。

### M4 — レイアウト履歴と回復

1. タブ並べ替え、領域移動、フロート化、再ドックを既存の `LayoutSnapshotCommand`／Undo 経路に接続する。
2. 旧レイアウト、不在パネル、破損したグループ参照を読み込んだ場合の復元規則を定める。
3. 既定レイアウトへの復帰後もパネルが重複・消失しないことを確認する。

**受け入れ条件:** 操作単位で Undo／Redo が対称に動作する。保存・再起動・旧データ読み込みを通して配置と可視状態が一貫する。

**進捗（2026-09-22）:** Native surface のフロート化、グループフロート化、再ドック、領域移動、タブグループ移動、タブ並べ替え、右クリックからのピン／閉じるを、既存の `LayoutSnapshotCommand` に渡すコールバックへ接続した。グループ復帰は一つの履歴単位として扱う。復元時は履歴記録を抑止する。J用の既存Debug構成で `Artifact` ビルドを試行したが、変更対象外の `ArtifactCoreEnvironment` が `EnvironmentVariable` IFC の曖昧な解決で停止したため、今回の対象のコンパイルおよびUndo／Redo実機確認は未実施。

### M5 — タブエリアの VS 風整理と上下配置

タブバーをペイン上端だけでなく**下端にも置けるようにする**。既定は上端とし、既存レイアウトの見た目を変えない。あわせて VS のタブエリア要素のうちドックパネルへ適用できるものを段階的に取り込む。

#### M5-1 — タブ配置の切替（上端／下端）

1. タブ領域ごとに、タブバーの配置を上端／下端で切り替えられるようにする。
2. 導線は、タブの右クリックメニュー（`タブを下に置く` / `タブを上に置く`）とタブ一覧ボタンのメニューに置き、**対象タブバーを明示**して適用する。選択中タブを誤って対象にしない。
3. 配置はタブ領域単位の属性として保存し、再起動・レイアウトのリセット・既存の Undo／Redo 経路で往復させる。`saveLayoutState`／`restoreLayoutState` は領域単位の属性を持たないため、保存契約を先に決める（M5 の着手条件を参照）。
4. キーボードから実行する場合だけ `ShortcutBindings` に専用 `ShortcutId`（Dock ローカルコンテキスト）を追加する。既定キーは空とし、固定キーを実装へ直書きしない。
5. 下端は VS の `Set tab layout` に存在しない本アプリ独自の拡張であるため、メニュー文言とドキュメントで VS 準拠要素と区別する。

**進捗（2026-09-24）:** タブ右クリックとタブ一覧メニューに、対象領域名を明示した上下配置の選択肢を追加した。配置は `DockLayoutDocument.areaTabPositions` に任意 JSON object として保存し、version 1 のまま欠落・不明値を上端扱いにする。Undo／Redo は既存のレイアウト snapshot 経路で往復させる。浮動タブグループは固定領域属性とグループ識別子が未整備のため対象外。ビルド・保存復元・Undo／Redo の実機確認は未実施。

#### M5-2 — 下端配置時の QADS Area Title Bar

1. `DockTabBar::paintEvent` の選択タブ contour は現在 `rect.bottom()` を開けて描いている。下端配置では開く辺を**上辺へ反転**する。
2. `DockTabSurface::paintEvent` の `contentTop` は `tabBar()->geometry().bottom() + 1` 基準。下端配置では `tabBar()->geometry().top() - 1` 基準へ切り替え、外枠 outline を上方向へ組み立てる。
3. タイトル下線は文字の近傍に保ち、下端配置でもペイン接合部と重ならない位置へ上下対称に置く。
4. QADS の `TabsAtBottom` ではタブ列だけを下へ移し、Area Title Bar のボタン列は上部に残す。Native も下端配置時は独立した上部アクション行を保ち、現在パネル名を表示する。浮動タブ面は従来どおり OS／独自ウィンドウ見出しを使う。
5. 独立アクション行の高さを `SE_TabWidgetTabContents` へ反映し、ページ widget と重ならないようにする。`PE_FrameTabWidget` 抑止は維持し、owner-draw の上下枠と実際のタブ位置を一致させる。
6. タブ形状そのものは `QStyleOptionTab::position` が `South` を返すことで自動反転する想定とし、反転しない箇所だけを 1〜4 で補う。
7. D&D の挿入予告、領域判定（`areaAt`）、タブ並べ替えは `tabRect()` と領域矩形基準で配置非依存。反転後も予告位置と確定位置が一致することを確認する。

**受け入れ条件（M5-1 / M5-2）:** 上下どちらの配置でも、選択タブの接続表現、フォーカス枠、タイトル下線が破綻しない。下端配置では上部アクション行とアクティブパネル名を維持し、本文と重ならない。切替後もタブ選択・並べ替え・別領域への D&D・フロート化・再ドック・ピン／閉じるの結果が変わらない。配置は保存・再読込・リセット・Undo／Redo で往復し、旧データは上端へフォールバックする。

#### M5-3 — タブ一覧と復元

1. タブ一覧メニューで、省略・スクロールされたタブを選べるようにし、一覧上の非表示項目をイタリックで示す（VS の "Show invisible tabs in italic in tab dropdown menu" 相当）。
2. 直前に閉じたドックパネルを元のタブ領域・元の位置へ戻す操作を追加する。パネル実体は再生成せず、既存の visibility とピンの契約を通す。ドキュメントの未保存復元とは混ぜない。
3. ピンの別行表示は複数行タブの導入を前提とするため M5-4 の従属項目とし、既存のピン契約（閉じる抑止）は変更しない。

**進捗（2026-09-24）:** タブ一覧で非表示パネルを斜体表示し、一覧から再表示できるようにした。直前に閉じたパネルを復元する項目も追加した。非表示化では既存パネルをタブ領域・順序ごと保持し、復元では同一インスタンスを再表示する。復元操作は既存レイアウト Undo 経路へ記録する。実機確認は未実施。

#### M5-4 — 複数行タブ（設計先行）

1. VS の複数行タブとピンの別行表示は、単一行＋スクロールボタンの `QTabBar` では表現できない。タブバーを自前レイアウトへ置き換えるか、行数分の `QTabBar` を並べるかを決めてから着手する。置き換えはドラッグ並べ替え・D&D・タブ一覧・ピン行・chrome 描画の全経路へ波及する。
2. 方式決定までは着手せず、現行の1行＋横スクロールを維持する。

**設計判断（2026-09-24）:** 今回は現行の1行＋横スクロールを維持し、複数行タブとピン別行は実装しない。`DockTabSurface` は単一の `DockTabBar` を `QTabWidget::setTabBar()` で使い、イベントフィルター、タブ index、閉じるボタン、ドラッグ並べ替えを `QTabWidget` の index に直結している。行ごとに `QTabBar` を増やす方式では、各行 index と全体 index の対応、選択同期、ドラッグ挿入、閉じる操作、浮動グループの順序を新たに管理する必要がある。自前レイアウト方式も同じ編集契約の置き換えを伴うため、M5 の安全な小段階には含めず、別設計で global tab identity・行割当・ピン契約・アクセシビリティ・保存復元を定義する。M5-4 は設計判断のみで、複数行表示の受入は未完了。

**受け入れ条件（M5-3 / M5-4）:** 一覧から選んだパネルと実際に切り替わるタブが一致する。復元したパネルが元の領域・順序・可視状態へ戻り、重複や消失が起きない。

**着手条件・未確認（M5）:**

- `QTabWidget::setTabPosition(QTabWidget::South)` のタブ形状、`QStyleOptionTab::position`、スクロールボタンと一覧ボタンの位置は未確認。owner-draw chrome が上端前提のため、実機で contour と外枠の一致を確認する。
- 下端配置を `Bottom` ドック領域へ適用すると、ウィンドウ下端の領域タブとステータス行が近接する。混同しない余白・区切りは要判断（`Insight.md` に記録）。
- 保存契約: `kDockLayoutDocumentVersion`（現在 `1`）は `DockLayoutDocument::fromJson` と `restoreLayoutState` の両方で厳密一致し、不一致時は entries を破棄する。M5-1 は version 据え置きの任意 `areaTabPositions` object を採用した。未知キーは読み飛ばし、欠落・不明値は上端へ戻す。旧 JSON 配列形式も上端へ戻す。
- 浮動タブグループの順序には引き続き `floating-tabs:` の並び文字列を使う。グループ同一性は任意の `floatingGroupId` に分離済みで、旧レイアウトだけが並び文字列を識別子として使う。
- `QTabBar` のスクロールボタンと省略表示（`setElideMode(Qt::ElideRight)`）は下端配置でも維持し、一覧ボタンと重ならないことを確認する。

**進捗（2026-09-24）:** M5-1〜M5-3 のコード実装を追加した。`DockTabBar` と `DockTabSurface` は South の選択 contour／フォーカス外枠／タイトル下線へ反転した。初期実装ではボタン列も `BottomRightCorner` へ移動していた。

**進捗（2026-10-15・QADS型の下端タブ配置）:** QADS 4.5.0 の `CDockAreaWidget::createTitleBar()` / `CDockAreaTitleBarPrivate::createTabBar()` を確認し、TabsAtBottom 時はタブ列だけを下へ配置し、Area Title Bar のボタン列を上部に残す構造へ合わせた。Native もドック済みタブ面に独立した上部アクション行を設け、アクティブパネル名を表示する。ページ領域は `SE_TabWidgetTabContents` の調整でアクション行の下から始める。コード差分の静的確認のみで、QStyle geometry、表示、切替、D&D、保存復元、Undo／Redo は実機未確認。

**進捗（2026-10-15・Area Title Barからのグループドラッグ）:** 下端タブ配置時に上部へ分離したタイトル行のパネル名部分からも、同じタブグループをドラッグできるようにした。既存の一時ドック先オーバーレイ、split／タブ統合、キャンセルと領域外フロートの経路を再利用する。コード静的確認のみで、実際のドラッグ、キャンセル、領域外ドロップ、Undo／Redo は未確認。

### M6 — コンポジションのタブの未保存表示

既定で下領域に置かれるタイムラインの、**コンポジションに 1 対 1 で対応するタブ**（`timeline::<compositionId>`、タブ名はコンポジション名）へ未保存の印を出す。ロックは対象外とし、`Pin＋Lock` の分離は `MILESTONE_DOCK_ENHANCEMENT_PACK_2026-09-13.md` の 7 が担当する。

1. 対象はコンポジション対応のタイムラインタブだけにする。同じコンポジションから生成される `<名前> Dope Sheet` / `<名前> Animation Timeline` / `<名前> Audio Mini`、および Layer Solo などの編集コンテキストタブは対象外とする。
2. 印はタブ名の隣の小さな状態マークとし、タブ名そのものは変更しない。常時表示する操作はタブ名と閉じるボタンのままとし、印だけを足す。
3. 状態が変化したときだけ更新する。コンポジションの改名、タブの並べ替え、フロート化、再ドック、領域移動、レイアウトの保存・復元によって印が消えないようにする。
4. 保存・上書き保存・プロジェクト読み込みの完了で解除する。既存の未保存経路（`UndoManager` の saved version、`ArtifactProject` の dirty）から取得し、新しいグローバル signal／slot を追加しない。
5. `setDockTitle` を使い、Composition タブの名称変更を生成側から接続する。`titles_` の参照箇所は複数あるため、タブ文字列・浮動見出し・一覧・復元表示に更新漏れがないことを確認する。
6. 未保存の判定粒度は現状プロジェクト全体しかないため、表示する意味を先に決める（下記「要判断」）。暫定案を採る場合も、コンポジション固有の未保存と誤認させない文言・tooltip を付ける。

**受け入れ条件:** タブの印と実際の未保存状態が一致し、保存後に消える。改名・並べ替え・フロート化・再ドック・領域移動・レイアウト保存／復元・リセットを経ても印と状態が維持される。対象外のタブに印が出ない。ドキュメントタブの `artifactDocumentDirty` と二重管理にならない。

**要判断（M6 の着手条件）:**

- **未保存の粒度:** (a) 暫定案 = 既存のプロジェクト全体の dirty を使う。タブ文字列と印の更新だけで実装でき、変更範囲は `NativeDockSurface` とタイムラインタブの生成側に閉じる。ただし未編集のコンポジションのタブにも同じ印が出るため、「このコンポジションが未保存」という意味にはならない。(b) 本来案 = コンポジション単位の dirty を新設する。`UndoCommand` の基底（`Artifact/include/Undo/UndoManager.ixx` の `class UndoCommand`）はコンポジションの scope を問い合わせる口を持たず、`compositionId_` は各サブクラスの private メンバに散在するため、基底へ仮想アクセサを追加して該当サブクラスで override するか、コンポジション単位の revision を別に持つ必要がある。正確だが変更範囲は Undo コアへ広がる。
- **規則の改訂:** `AGENTS.md`、`docs/design/composition-viewport/README.md` のタブ責務、`MILESTONE_DOCK_ENHANCEMENT_PACK_2026-09-13.md` は「未保存マークを Dock タブに出さない」と定めている。タイムラインのコンポジションタブは Dock タブであるため、実施には**例外の明示**（コンポジションに 1 対 1 で対応するタブに限る）か、当該タブを編集コンテキストタブとして扱う再分類が必要。規則そのものは AI 側で書き換えず、ユーザーの決定を待つ。

**進捗（2026-09-22）:** 未着手。実装案のみ記録（未保存表示の要望を反映、ロックは対象外）。

**進捗（2026-10-15）:** QADS 4.5.0 の `CDockWidget::NoTab` に相当する Native API を追加した。開閉状態を `artifactDockOpen` としてタブ見出しの表示状態から分離し、`noTab` を任意レイアウト項目として保存・復元する。`ArtifactMainWindow::setDockNoTab()` から切替可能。JSON の省略時は従来どおりタブを表示する。静的確認のみで、ビルド・実機確認は未実施。

## 実装上の制約

- 変更対象は主に `Artifact/include/Widgets/ArtifactNativeDockSurface.ixx`、必要に応じて `Artifact/src/Widgets/ArtifactMainWindow.cppm` とドックレイアウト契約（`Artifact/include/Widgets/ArtifactDockManager.ixx`）。子リポジトリの変更は別途明示依頼を受けてから行う。
- 既存のサービス／コマンド経路を使用し、新規 signal／slot、QtCSS、`QColorDialog` を導入しない。
- パネルのドラッグ・タブ UI と Composition Viewer 内の描画、入力、ギズモは分離する。採用モックのキャンバス内表現を根拠に変更しない。
- タブバー配置の切替は `QTabWidget::setTabPosition` と owner-draw chrome の上下反転を組で扱う。`DockTabBar`／`DockTabSurface`／`DockSurfaceStyle`／タブ一覧ボタンの corner の 4 箇所を同時に更新し、片側だけ反転した状態を残さない。
- ドックレイアウトの保存形式を変更する場合は、既存データが読めなくなる挙動を避ける。`kDockLayoutDocumentVersion` の厳密一致と entries 破棄の現行仕様を前提に、任意フィールド追加を第一候補として扱う。
- ドックタブとドキュメントタブのメニュー項目を混在させない。未保存マーク、複製、色分け、プレビュータブをドックタブへ持ち込まない。M6 の未保存表示はこの規則に対する明示的な例外であり、規則を変更する場合は `AGENTS.md` 側の記述もユーザーの承認を得て同時に更新する。
- M6 のタブ名更新は、`NativeDockSurface` のタイトル保持とタイムラインタブの生成・改名の経路を一組で扱う。タブ文字列だけを更新して浮動ウィンドウのタイトルやタブ一覧が古いままになる状態を残さない。
- ビルド、CMake、テスト、実機確認はユーザーの明示指示後に実施する。実装レビュー時には静的確認と実機未確認の範囲を分けて報告する。

## 着手順

M1 → M2 → 保存契約の確認 → M3 → M4 → M5-2（描画の上下反転のみ）→ 保存契約の決定 → M5-1（配置の切替と保存）→ M5-3 → M5-4（設計先行）。M6 は M5-2 で入れるタイトル更新 API と表示器を再利用できるため、M5-1 の後、M5-3 と並行して着手する（粒度と規則の決定後）。各段階で既存機能の回帰を確認してから次へ進む。
