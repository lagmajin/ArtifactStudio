# Top-Level Widget Architecture Migration

**ステータス:** In Progress

**日付:** 2026-07-13  
**対象:** `Artifact` アプリケーションシェル / Qt ADS workspace

## 目的

`ArtifactMainWindow` の `QMainWindow` 継承を廃止し、通常のトップレベル `QWidget` と Qt ADS を中心とした単一のレイアウト管理へ移行する。

現在併存している次の二つの状態管理を一本化する。

1. `QMainWindow` の toolbar / status bar / `saveState()` / `restoreState()`
2. Qt ADS の dock / tab / splitter / floating / `saveState()` / `restoreState()`

最終的には、ウィンドウ位置・サイズだけをトップレベル `QWidget` が所有し、workspace 配置は Qt ADS だけが所有する。

## 前提

- Composition Editor の描画・floating 時の描画問題は解決済みとする。
- render surface readiness、swapchain、描画初期化経路の変更は本マイルストーンの対象外とする。
- Qt ADS の floating container が別 top-level windowになる仕様は維持する。
- 新しい signal / slot 接続経路は追加しない。既存のイベントバス、サービス、明示的メソッド呼び出しを再利用する。
- QtCSS / `setStyleSheet()` は追加しない。
- 子リポジトリおよび Qt ADS 本体は変更しない。

## 現状の問題

### 1. レイアウト所有者が二重になっている

`ArtifactMainWindow` は `QMainWindow` でありながら、実際の編集領域は `ads::CDockManager` が管理している。

一方、保存・復元では次の状態が別々に扱われている。

- `QWidget::saveGeometry()` / `restoreGeometry()`
- `QMainWindow::saveState()` / `restoreState()`
- `CDockManager::saveState()` / `restoreState()`
- workspace JSON 内の dock 可視状態

この構造では、復元順序、初回レイアウト、toolbar領域、ADS splitter状態のどれが最終決定権を持つかが不明確になる。

### 2. WorkspaceManager が `QMainWindow` と `QDockWidget` に依存している

`ArtifactWorkspaceManager` は `QMainWindow*` を受け取り、`QDockWidget` をタイトルで検索している。しかし、実際の dock 本流は Qt ADS の `CDockWidget` である。

これにより次の責務ずれがある。

- dock 列挙の型が実際の workspace と一致しない
- 表示名が永続IDとして使われる
- workspace manager が具体的なウィンドウ実装を知りすぎている

### 3. `ArtifactMainWindow` に責務が集中している

現在の `ArtifactMainWindow` は少なくとも次を担当している。

- OS top-level window lifecycle
- menu bar
- toolbar / tool options
- status bar
- Qt ADS dock登録と操作
- workspace mode
- dock state保存・復元
- floating surface補助処理
- application settings反映
- keyboard event routing

継承型の変更だけではなく、先に workspace 責務を分離する必要がある。

## 目標アーキテクチャ

```text
ArtifactMainWindow : QWidget
└─ QVBoxLayout
   ├─ ArtifactMenuBar
   ├─ MainToolbarHost
   │  ├─ ArtifactToolBar
   │  └─ Workspace selector
   ├─ ArtifactToolOptionsBar
   ├─ ArtifactWorkspaceWidget
   │  └─ ads::CDockManager
   │     ├─ Project View
   │     ├─ Composition Viewer
   │     ├─ Timeline
   │     ├─ Property Editor
   │     └─ Other dock surfaces
   └─ QStatusBar
```

`QMenuBar`、`QToolBar`、`QStatusBar` は `QMainWindow` の専用領域としてではなく、通常の子widgetとしてlayoutに配置する。

## コンポーネント責務

### ArtifactMainWindow

`ArtifactMainWindow` はトップレベルアプリケーションシェルに限定する。

担当する責務:

- `closeEvent()` / `showEvent()` / key event
- window geometry
- native title bar設定
- 終了確認
- application-level shortcut routing
- root layout構築
- application settingsを各子コンポーネントへ明示的に反映

担当しない責務:

- dock列挙
- dock表示・activation
- dock state保存・復元
- splitter配置
- dock titleからの検索

### ArtifactWorkspaceWidget

Qt ADS の所有と操作を集約する `QWidget` を導入する。

想定する公開面:

```cpp
class ArtifactWorkspaceWidget : public QWidget {
public:
  QByteArray saveLayoutState() const;
  bool restoreLayoutState(const QByteArray &state);

  void addDockedWidget(...);
  void addDockedWidgetTabbed(...);
  void addDockedWidgetFloating(...);
  void setDockVisible(const QString &dockId, bool visible);
  void activateDock(const QString &dockId);
  bool closeDock(const QString &dockId);
  QStringList dockIds() const;
};
```

既存の `ArtifactMainWindow` にある Qt ADS 操作APIは段階的にこのクラスへ移す。

移行中は `ArtifactMainWindow` に薄い転送メソッドを残してよいが、新規コードは `ArtifactWorkspaceWidget` を利用する。

### ArtifactWindowChrome

menu bar、toolbar、tool options、status barの生成と配置をまとめる内部コンポーネントとする。

独立した公開モジュールにする必要がなければ、`ArtifactMainWindow.cppm` 内の private implementation として保持し、`.ixx` の依存増加を避ける。

### ArtifactWorkspaceManager

`QMainWindow*` 依存を除去し、永続化データとworkspace操作の調停に限定する。

想定スナップショット:

```cpp
struct WorkspaceSnapshot {
  QByteArray windowGeometry;
  QByteArray dockState;
  WorkspaceMode workspaceMode;
  int schemaVersion = 1;
};
```

保存対象:

| 状態 | 所有者 | 永続化形式 |
|---|---|---|
| ウィンドウ位置・サイズ | `ArtifactMainWindow` | `QWidget::saveGeometry()` |
| dock / tab / splitter / floating | `ArtifactWorkspaceWidget` | `CDockManager::saveState()` |
| workspace mode | `ArtifactWorkspaceManager` | enum / stable text |

廃止対象:

- `QMainWindow::saveState()` / `restoreState()`
- `QDockWidget` の子孫探索
- dock titleを永続キーにした検索
- 同一セッション内での重複レイアウト復元

## Dock識別子

表示名と永続IDを分離する。

- `windowTitle`: ローカライズ・UI表示用
- `dockId` / `objectName`: 永続化と操作用

workspace preset、visibility、activation、tab group復元は安定した `dockId` を使用する。

既存presetとの互換が必要な期間だけ、旧titleからdock IDへの明示的な変換表を持つ。恒久的なtitle fallbackにはしない。

## 中央Workspaceの扱い

現在の `ArtifactCentralDock` は `AllDockWidgetFeatures` を持つため、中央アンカー自体がclose・floating可能な構成になっている。

移行時は次のどちらかを選択する。

### 推奨: 通常dockによるデフォルト配置

特別な中央widgetを廃止し、Composition Viewerを含む通常のADS dock群からデフォルトレイアウトを構築する。

利点:

- 中央だけ異なるライフサイクルを持たない
- presetと通常dock操作が同じ規則になる
- workspace全体をADS stateだけで表現できる

### 代替: 固定中央アンカー

中央アンカーを維持する場合は次を禁止する。

- close
- float
- auto-hide
- stable ID変更

restore失敗時にも必ず再生成されるworkspaceアンカーとして扱う。

## 状態復元契約

起動時の順序を固定する。

1. `ArtifactMainWindow` とroot layoutを生成
2. `ArtifactWorkspaceWidget` と全必須dockを登録
3. window geometryを復元
4. ADS dock stateを一度だけ復元
5. workspace modeと論理UI状態を反映
6. 復元できなければデフォルトADS配置を適用
7. windowを表示

ADS restoreは全dock登録後にのみ呼ぶ。

終了時の順序:

1. window geometryを取得
2. ADS dock stateを取得
3. workspace modeを取得
4. 一つのsnapshotとして保存

## 旧レイアウト互換

レイアウトschema versionを更新する。

- 旧 `QMainWindow state` は新構成へ変換しない
- 旧geometryは読み取れる場合のみ継承する
- 既存ADS dock stateはdock ID互換が維持できる場合のみ復元する
- 復元失敗は部分適用せず、デフォルト配置へフォールバックする
- 旧キーは新schemaで正常起動・保存できた後に削除する

## 実装段階

### M-TLW-1 状態管理の一本化

- startup / shutdownの保存復元経路を列挙する
- ADS stateをworkspace配置の唯一の正本にする
- `QMainWindow::saveState()` / `restoreState()` の利用を停止する
- workspace JSONから重複する状態復元を除去する

この段階では `ArtifactMainWindow : QMainWindow` を維持してよい。

### M-TLW-2 Workspace責務の抽出

- `ArtifactWorkspaceWidget` を導入する
- `CDockManager` の所有権を移す
- dock追加・表示・activation・state APIを移す
- `ArtifactWorkspaceManager` の `QMainWindow` / `QDockWidget` 依存を除去する
- 移行用の薄い転送APIを必要最小限だけ残す

### M-TLW-3 Window chromeの通常layout化

- `QVBoxLayout` をroot layoutとして導入する
- menu barを通常widgetとして配置する
- toolbarとtool optionsを固定chrome領域へ配置する
- `QStatusBar` を通常widgetとして配置する
- `addToolBar()`、`setMenuBar()`、`statusBar()` への依存を除去する

### M-TLW-4 QWidgetトップレベル化

- `ArtifactMainWindow` の基底を `QMainWindow` から `QWidget` へ変更する
- event handlerの基底呼び出しを `QWidget` へ変更する
- `QMainWindow` includeと公開依存を除去する
- AppMainとIPC側のトップレベル探索がobject nameまたは明示参照で動作することを確認する

### M-TLW-5 互換コード撤去

- 旧転送APIを削除する
- 旧layout schema keyを整理する
- title-based dock lookupを削除する
- `QMainWindow` 前提の文書とコメントを更新する

## 変更対象候補

- `Artifact/include/Widgets/ArtifactMainWindow.ixx`
- `Artifact/src/Widgets/ArtifactMainWindow.cppm`
- `Artifact/include/Core/ArtifactWorkspaceManager.ixx`
- `Artifact/src/Core/ArtifactWorkspaceManager.cppm`
- `Artifact/src/AppMain.cppm`
- `Artifact/src/Application/ArtifactProjectBundleIpc.cppm`
- workspace menu / preset呼び出し側

新規 `.ixx` / `.cppm` はCMake再スキャン範囲を広げるため、M-TLW-1では追加しない。M-TLW-2で新規モジュールが必要かを確定し、既存実装ファイル内のprivate componentで代替可能か先に検討する。

## 非対象

- Composition Editorの描画初期化
- render surface readiness
- Diligent / D3D12 backend
- Qt ADS forkまたはpatch
- dock UIの全面的な見た目変更
- Project View / Property Editor / Timeline内部レイアウトの変更
- 新しいグローバルイベント経路

## 完了条件

- `ArtifactMainWindow` が `QWidget` を直接継承している
- `QMainWindow::saveState()` / `restoreState()` を使用していない
- workspace配置の正本がADS state一つになっている
- window geometryとdock stateの所有者が分離されている
- `ArtifactWorkspaceManager` が `QMainWindow` と `QDockWidget` に依存していない
- dock操作が安定したdock IDを使用している
- menu、toolbar、tool options、status barが通常layout内で表示される
- 旧layoutデータが存在しても安全にデフォルト配置へフォールバックできる
- dock / floating / preset切替後も同じworkspace state契約で保存・復元できる

## 実装状況 (2026-07-13)

- `ArtifactMainWindow` は `QWidget` 継承へ移行済み
- menu / toolbar / tool options / ADS / status barは通常のroot layoutで構成済み
- `QMainWindow::saveState()` / `restoreState()` は保存復元経路から除去済み
- `ArtifactWorkspaceManager` の旧 `QDockWidget` 列挙・可視状態保存を除去済み
- 関連メニュー、Timeline、Composition Editorの `QMainWindow` castを移行済み
- `git diff --check` は通過
- ビルド・ランタイム検証は未実施（AGENTS.mdの実行禁止規約による）

## 関連文書

- [`docs/WIDGET_MAP.md`](../WIDGET_MAP.md)
- [`docs/planned/MILESTONE_WORKSPACE_MANAGER_2026-03-29.md`](MILESTONE_WORKSPACE_MANAGER_2026-03-29.md)
- [`docs/planned/MILESTONE_WORKSPACE_PRESETS_2026-04-10.md`](MILESTONE_WORKSPACE_PRESETS_2026-04-10.md)
- [`Artifact/docs/INVESTIGATION_QADS_UPSTREAM_COMPARISON_2026-06-23.md`](../../Artifact/docs/INVESTIGATION_QADS_UPSTREAM_COMPARISON_2026-06-23.md)
