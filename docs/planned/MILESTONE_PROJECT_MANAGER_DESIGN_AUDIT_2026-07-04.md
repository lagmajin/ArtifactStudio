# マイルストーン: プロジェクトマネージャ 機能監査 (2026-07-04)

**最終更新:** 2026-08-15

> 作成: 2026-07-04
> 元依頼: 「次はプロジェクトマネージャーを」

## 監査サマリー

`ArtifactProjectManagerWidget.cppm`（4,272行）はプロジェクト全体の comp / footage / asset をツリー表示する左ペインの核。M-PV-1/2 完了済みで基本操作（選択/リネーム/Asset Browser 同期/sync chip）は揃っている。



---

## 🔴 P0: ビュー・表示系

### ビューモード

| 機能 | 参照元 | 状態 |
|---|---|---|
| **List View（リスト表示）** | 全アプリ | ⚠️ ツリー表示のみ。詳細情報付きの表形式リストがない |
| **Thumbnail Grid View（サムネイルグリッド）** | Resolve/Unity | ❌ サムネイル＋ファイル名のグリッド表示。Asset Browser にはあるが Project 側にない |
| **Tile View（タイル表示）** | Premiere/Resolve | ❌ 中サイズサムネイル＋メタデータのタイル表示 |
| **Freeform View（自由配置）** | Premiere Icon View | ❌ アイテムをキャンバス上に自由配置できるビュー |
| **ビューモード切替ボタン** | Premiere/Resolve | ❌ List/Icon/Tile/Freeform をツールバーから即切替 |

### カラム・メタデータ表示

| 機能 | 参照元 | 状態 |
|---|---|---|
| **メタデータ列の表示/非表示カスタマイズ** | Premiere/Resolve | ❌ Name/Type/Size/Date/Duration/FrameRate/Resolution/Comment を選択表示 |
| **カラム幅のドラッグ調整** | Premiere/Final Cut | ⚠️ |
| **カラムの並び替え（ドラッグで入れ替え）** | Resolve | ❌ |
| **カラムヘッダクリックでソート** | Premiere/Final Cut | ❌ |
| **セカンダリソート（Shift+クリック）** | Premiere | ❌ Name ソート内で Type ソートなど多段ソート |
| **メディア使用回数カラム** | Premiere/Resolve | ❌ プロジェクト内で何回使われているかを表示 |
| **オフライン/オンライン状態カラム** | Premiere | ⚠️ |
| **フレームレートカラム** | Premiere/Resolve | ❌ |
| **ビデオ/オーディオコーデックカラム** | Premiere | ❌ |

### 検索・フィルタ

| 機能 | 参照元 | 状態 |
|---|---|---|
| **プロジェクト全体検索（全 Bin 横断）** | Premiere/Avid | ❌ |
| **タイプフィルタ（Comp/Footage/Audio/Solid/Shape 切替）** | AE | ⚠️ 部分的 |
| **ラベル色フィルタ** | Premiere/Resolve | ❌ |
| **使用状況フィルタ（使用中/未使用）** | Premiere | ❌ |
| **オフラインフィルタ** | Premiere | ❌ |
| **日付フィルタ（今日/今週/今月）** | Resolve | ❌ |
| **評価フィルタ（★1-5）** | Premiere/Resolve | ❌ |
| **フィルタプリセット保存** | Resolve | ❌ よく使うフィルタ条件を名前付き保存 |
| **Find Bin（検索結果を仮想 Bin に）** | Avid | ❌ 検索結果を保存して後で再利用 |

---

## 🔴 P0: 組織・構造系

### Bin / フォルダ

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Bin の中に Bin（ネスト階層）** | Premiere/Avid/Resolve | ⚠️ |
| **Smart Bin（条件付き仮想フォルダ）** | Premiere/Resolve | ❌ 「Type=Comp AND 未使用」のような条件で自動収集するスマートフォルダ |
| **Bin に色ラベル** | Premiere/Resolve | ❌ |
| **Bin の並び替え（ドラッグ＆ドロップ）** | Premiere | ⚠️ |
| **Bin のロック（編集防止）** | Avid | ❌ |
| **Bin をタブとして開く** | Premiere | ❌ ダブルクリックで Bin を別タブで開く |
| **最近使った Bin** | Premiere | ❌ 最近開いた Bin の履歴 |
| **デフォルト Bin テンプレート** | Premiere | ❌ 新規プロジェクト作成時のデフォルト Bin 構造テンプレート |

### コレクション・グループ

| 機能 | 参照元 | 状態 |
|---|---|---|
| **Collection（手動コレクション）** | AE | ❌ 任意のアイテムをテーマ別に集めた手動コレクション |
| **Folder Track / Stack** | Final Cut/Avid | ❌ アイテムをストーリー的にグループ化 |
| **Keyword Collection（キーワードタグ）** | Final Cut | ❌ タグ付けによる動的コレクション |
| **Favorite / Reject マーキング** | Final Cut | ❌ F=お気に入り、Delete=却下の簡易トリアージ |

---

## 🟡 P1: プロジェクト管理

| 機能 | 参照元 | 状態 |


---

## 🟡 P1: 一括操作・バッチ処理

| 機能 | 参照元 | 状態 |
|---|---|---|
| **一括リネーム（連番/置換/正規表現）** | AE/Premiere | ❌ |
| **一括再リンク（複数メディアの一括パス修正）** | Premiere | ❌ |
| **一括解釈変更（FPS/Pixel Aspect/Alpha 設定）** | AE | ❌ |
| **一括置換（クリップ A→クリップ B に全使用箇所を置換）** | Premiere | ❌ |
| **一括削除＋未使用ファイルも削除** | Premiere | ❌ |
| **選択アイテムから新規 Comp 作成** | AE | ⚠️ Pre-compose はあるが、複数 footage から comp 生成する導線は弱い |
| **バッチエクスポート（選択アイテムを個別レンダリング）** | AE/Resolve | ❌ |

---

## 🟡 P1: インポート・メディア管理

| 機能 | 参照元 | 状態 |
|---|---|---|
| **インポートダイアログ（解釈オプション付き）** | AE | ⚠️ InterpretFootage dialog あり |
| **ドラッグ＆ドロップインポート** | Premiere/Final Cut | ⚠️ |
| **フォルダ構造を維持したインポート** | Premiere | ❌ OS のフォルダ階層を Bin 構造として再現 |
| **インポートキュー（バックグラウンド処理）** | Resolve | ❌ 大量ファイルのインポート中も作業継続 |
| **インポートステータス表示** | Resolve | ❌ 成功/失敗/処理中の進捗表示 |
| **Proxy（プロキシ）生成・管理** | Premiere/Resolve | 📋 計画中 |
| **Proxy アタッチ/デタッチ状態表示** | Premiere/Resolve | ❌ |
| **オフラインメディアの一覧表示** | Premiere | ❌ |
| **メディアの再リンクダイアログ** | Premiere | ❌ ファイル選択＋自動検索＋全候補表示 |
| **メディアキャッシュ管理（キャッシュクリア/場所指定）** | AE/Premiere | ⚠️ |

---

## 🔵 P2: メタデータ・解析

| 機能 | 参照元 | 状態 |
|---|---|---|
| **メタデータパネル（選択アイテムの全情報表示）** | Premiere/Resolve | ⚠️ File Details はあるが簡易 |
| **メタデータの一括編集** | Premiere | ❌ 複数選択して共通フィールドを一括変更 |
| **カスタムメタデータフィールド** | Premiere/Resolve | ❌ Scene/Shot/Take/Description 等のカスタム列追加 |
| **EXIF / XMP メタデータ読み取り** | Premiere/Lightroom | ❌ |
| **使用状況レポート（どの comp で何回使われているか）** | Premiere/Nuke | ❌ |
| **依存関係マップ（ビジュアルグラフ）** | Nuke/Resolve | ❌ |
| **重複ファイル検出** | Premiere/Resolve | ❌ |

---

## 🔵 P2: UI/UX 品質

| 機能 | 参照元 | 状態 |
|---|---|---|
| **ホバープレビュー（サムネイル拡大ポップアップ）** | AE/Resolve | ✅ M-PV-2 実装済み |
| **インラインリネーム（クリック→即編集）** | AE/Final Cut | ✅ M-PV-1 実装済み |
| **エフェクト/プリセットのドラッグ＆ドロップ適用** | Premiere/Resolve | ❌ |
| **アイテムを右クリック→「コンポジションで開く」** | AE | ⚠️ |
| **選択アイテム数をステータスバーに表示** | Premiere | ❌ |
| **Bin のスクロール位置記憶（戻った時に復元）** | Premiere | ❌ |
| **サムネイルサイズスライダー** | Resolve | ❌ Bin 内のサムネイルサイズ調整 |
| **ズームスライダー（アイコンサイズ変更）** | Premiere Icon View | ❌ |
| **プレビューエリア（選択アイテムの拡大プレビュー）** | Premiere/Resolve | ❌ Bin 上部に選択アイテムの拡大プレビュー＋簡易再生 |

---

## 🔵 P2: 他アプリ由来の特殊機能

| 機能 | 参照元 | 概要 |
|---|---|---|
| **Outliner の表示フィルタ（全タイプ/選択中/可視/アクティブ）** | Maya/Blender | シーン内のオブジェクトをフィルタ表示切替 |
| **Hierarchy with Parenting Lines** | Maya/Blender | 親子関係をインデント＋接続線で表示 |
| **Collections（レイヤーグループをコレクション化）** | Blender | 複数オブジェクトを Collection にまとめて一括操作 |
| **Asset Browser 統合（ライブラリから D&D 配置）** | Blender | Asset Browser と Project のシームレス統合 |
| **Content Browser のタグシステム** | Unreal | アセットにタグ付け＋タグクラウド表示 |
| **Content Browser のお気に入りフォルダ** | Unreal | よく使うフォルダをサイドバーにピン留め |
| **Content Browser のアセット監査（参照カウント/メモリサイズ）** | Unreal | 各アセットの使用状況とメモリ使用量を一覧 |
| **Smart Collections（動的条件コレクション）** | Figma | 「★付き」「最近更新」「未使用」等の動的コレクション |
| **Component / Variant 管理** | Figma | アセットのバリアント（解像度違い/カラーバリエーション）を管理 |
| **Project Versioning / History** | Figma/Premiere | プロジェクトのバージョン履歴と差分表示 |
|---|---|---|
| **複数プロジェクトをタブで開く** | Premiere/Resolve/Blender | ❌ |
| **プロジェクト間のアイテムコピー/移動** | Premiere | ❌ |
| **最近使ったプロジェクト一覧（スタート画面）** | AE/Premiere/Resolve | ❌ |
| **プロジェクトテンプレート** | AE/Premiere/Resolve | ❌ プリセット comp 構成で新規プロジェクト作成 |
| **プロジェクト設定パネル（解像度/FPS/デュレーション）** | AE | ⚠️ |
| **プロジェクトの依存関係解析** | Resolve/Nuke | ❌ 全 comp の参照関係をグラフ表示 |
| **プロジェクト健全性チェック** | Premiere/Resolve | 📋 計画中 (M-APP-5) |
| **オートセーブ＋バージョン履歴** | Premiere/AE | ⚠️ |
| **プロジェクトのバックアップ/アーカイブ** | Premiere | ❌ 全メディア込みでプロジェクトを圧縮保存 |
| **プロジェクトサイズ概算** | Premiere | ❌ |
| **Consolidate / Transcode（メディア収集＋変換）** | Premiere/Resolve | ❌ 使用メディアのみを収集して別フォルダに複製 |
以下、AE / Premiere / Resolve / Nuke / Blender / Maya / C4D / Houdini / Unity / Unreal / Figma / Final Cut / Avid / Photoshop の **14 アプリ群**から不足機能を収集。


---

## 📊 優先度マトリクス

| 優先 | カテゴリ | 件数 | 代表機能 |
|---|---|---|---|
| 🔴 | ビューモード | 5 | List/Thumbnail/Tile/Freeform 切替 |
| 🔴 | メタデータ列 | 9 | カラム表示カスタマイズ/ソート/使用回数 |
| 🔴 | 検索・フィルタ | 9 | プロジェクト全体検索/Smart Bin/フィルタプリセット |
| 🔴 | Bin 構造 | 7 | Smart Bin/Bin 色ラベル/Bin テンプレート |
| 🟡 | プロジェクト管理 | 10 | 複数タブ/テンプレート/依存関係解析/Consolidate |
| 🟡 | 一括操作 | 7 | 一括リネーム/再リンク/置換/解釈変更 |
| 🟡 | インポート管理 | 10 | フォルダ構造維持/キュー/プロキシ/再リンク |
| 🔵 | メタデータ | 7 | カスタムフィールド/EXIF/依存関係マップ |
| 🔵 | UI/UX | 9 | プレビューエリア/サムネイルサイズ/選択数表示 |
| 🔵 | 特殊機能 | 10 | Collections/タグ/Parenting Lines/Version History |

---

## 関連文書

- `Artifact/docs/MILESTONE_PROJECT_VIEW_2026-03-12.md` — Project View 実装段階 (M-PV-1〜5)
- `docs/planned/MILESTONES_BACKLOG.md` — 全体バックログ
- `docs/planned/MILESTONE_PROJECT_ASSET_WORKFLOW_2026-03-27.md` — プロジェクト・アセットワークフロー
- `docs/planned/MILESTONE_MULTI_PROJECT_EDITING_2026-03-27.md` — マルチプロジェクト編集
- `docs/planned/MILESTONE_LAYER_GROUP_SYSTEM_2026-03-27.md` — レイヤーグループ
- `docs/planned/MILESTONE_PROJECT_TEMPLATE_GALLERY_2026-06-16.md` — プロジェクトテンプレート
- `Artifact/src/Widgets/ArtifactProjectManagerWidget.cppm` — メイン実装 (4,272行)
- `Artifact/include/Widgets/ArtifactProjectManagerWidget.ixx` — インターフェース
- `docs/WIDGET_MAP.md` — ウィジェット責務

## Static Audit (2026-08-15)

現行ソースを再確認したところ、旧監査から進展がある。`ArtifactProjectManagerWidget` / `ArtifactProjectView` には Tree と Tile の切替、プロジェクト横断の検索欄、タイプフィルタ、列ヘッダのソート、列幅調整、インラインリネーム、複数選択、選択アイテムのプレビュー／メタデータ表示、依存関係表示、欠損 footage の再リンク補助、proxy 状態表示、drag & drop 導線が実装されている。Tile はサムネイルとメタデータを描画し、密度調整も持つ。

ただし、List / Thumbnail Grid / Freeform の独立した表示モード、列の表示・並び替え設定、セカンダリソート、Smart Bin / 保存フィルタ / コレクション／タグ、Bin ロック・タブ・履歴、バッチ操作、インポートキューと進捗、proxy 生成管理、EXIF/XMP・カスタムメタデータ、依存関係グラフ、バージョン／アーカイブ管理は確認できない。検索・タイプフィルタ・Tile 表示は実装済みでも、全項目の runtime UX、設定保存、性能、大量データ時の挙動は未検証である。

判定: **部分実装。** M-PV 系の基本導線と Tree/Tile の現行基盤は確認できるが、監査表の P0/P1 機能群は多数未実装または runtime 未検証である。
