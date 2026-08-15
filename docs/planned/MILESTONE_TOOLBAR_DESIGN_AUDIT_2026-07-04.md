# マイルストーン: ツールバー 機能監査 (2026-07-04)

**最終更新:** 2026-08-15

## Update 2026-08-15

- `ArtifactToolBar` は選択／手／ズーム／移動／回転／スケール／カメラ／アンカー／シェイプ／ペン／テキスト／ブラシ／クローン／消しゴム等の tool action、zoom／grid／guide／view 操作、workspace mode 連携、tool options bar、compact／text-under-icon 表示を実装済み。
- action の status tip、tool button の accessible name／description、More tools menu、workspace ごとの可視性調整も確認できる。前回表の ToolOptionsBar、折りたたみ表示、workspace/context 連携は部分的に進んでいる。
- ただし tool preset、長押しグループ、追加／削除／並替カスタマイズ、直前 tool 切替、左右／上下 docking、選択依存の自動切替、Space 長押しの専用 toolbar 契約、double-click 設定 dialog、custom toolbar 保存は未確認または未実装。

> Photoshop / Premiere / C4D / Blender ツールバー比較。

## 🔴 P0

| 機能 | 参照元 | 状態 |
|---|---|---|
| **ツールプリセット切替ドロップダウン** | Photoshop | ❌ |
| **ツールオプションバー（選択ツールの詳細設定）** | Photoshop/Premiere | ⚠️ ToolOptionsBar あり |
| **ツールのグループ化（長押しで展開）** | Photoshop | ❌ |
| **ツールバーのカスタマイズ（ツール追加/削除/並替）** | Photoshop/Blender | ❌ |
| **直前のツールに戻る（切り替え）** | Photoshop | ❌ |

## 🟡 P1

| 機能 | 参照元 | 状態 |
|---|---|---|
| **ツールショートカットのツールチップ表示** | Photoshop | ⚠️ |
| **ツールバーのドッキング位置切替（左右/上下）** | Photoshop | ❌ |
| **ツールバーの折りたたみ（アイコンのみ→テキスト付き）** | C4D | ❌ |
| **コンテキスト依存ツール切替（選択に応じて自動切替）** | C4D | ❌ |
| **クイックツール（Space 長押しで一時ツール切替）** | Blender | ❌ |
| **ツールのダブルクリックで設定ダイアログ** | Photoshop | ❌ |

## 🔵 P2

| 機能 | 参照元 | 状態 |
|---|---|---|
| **カスタムワークスペースツールバー保存** | Photoshop/Blender | ❌ |
| **ツールバーの色/アイコンサイズカスタマイズ** | - | ❌ |
| **ペンタブレット用ダブルツール（筆圧で切替）** | Photoshop | ❌ |
