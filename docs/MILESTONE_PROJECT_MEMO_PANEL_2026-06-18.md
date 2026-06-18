# マイルストーン: Project Memo Panel

> 2026-06-18 作成

## 目的

タイムコード（フレーム）に紐づいたメモを管理・表示するドックパネルを追加する。

---

## 背景

制作中に「ここを直す」「ここで要相談」といったメモを残したい場面は多いが、現状は外部ツールに頼るか、レイヤー名に書き込むしかない。タイムラインと連動したメモパネルがあれば、レビュー作業や共同制作が効率化する。

---

## 概念

- **Project Memo**: `{ timecode, frame, text, color, createdAt }`
- **現在のコンポジション単位**: コンポジション切り替えでメモリストも切り替わる
- **ダブルクリックでジャンプ**: メモをダブルクリックすると該当フレームへ移動

---

## フェーズ設計

### Phase 1: Model

**目的:** メモデータを保持するモデルを実装する。

**作業項目:**
- `Artifact/src/Widgets/ArtifactProjectMemoModel.cppm` 作成
  - `ArtifactProjectMemo` 構造体
  - 追加/削除/編集/リスト取得
  - 現在の `CompositionId` に応じたセグメント管理
- 必要に応じて `ArtifactProjectMemoModel.ixx` も作成

**完了条件:**
- プログラムからメモの CRUD が可能
- コンポジション切り替えでメモリストが切り替わる

**完成度:**
- [ ] モデルクラス作成
- [ ] CRUD 実装
- [ ] コンポジション連携

---

### Phase 2: Widget

**目的:** メモを表示・編集する UI を実装する。

**作業項目:**
- `Artifact/src/Widgets/ArtifactProjectMemoWidget.cppm` 作成
  - `QListView` または `QTreeWidget` ベース
  - 追加/削除/編集ボタン
  - ダブルクリックで `FrameChangedEvent` 発行
  - 色選択は `FloatColorPicker` を利用
- `W_OBJECT` / `W_OBJECT_IMPL` 追加

**完了条件:**
- UI からメモの追加/編集/削除ができる
- メモをダブルクリックすると該当フレームにジャンプする

**完成度:**
- [ ] Widget クラス作成
- [ ] リスト表示
- [ ] 編集 UI
- [ ] ジャンプ機能

---

### Phase 3: Dock registration

**目的:** メインパネルとして登録する。

**作業項目:**
- `Artifact/src/AppMain.cppm`
  - `#import` 追加
  - `addDockedWidgetTabbed` または `addDockedWidget` で「Project Memo」として登録
  - ワークスペースモードに応じた表示/非表示設定（必要なら）

**完了条件:**
- アプリ起動後、「Project Memo」パネルがドッキング可能
- レイアウト保存/復元が ADS によって行われる

**完成度:**
- [ ] AppMain に登録
- [ ] レイアウト確認

---

## Non-Goals

- プロジェクトファイルへの永続化（初版はメモリ上のみ）
- 他ユーザーとのリアルタイム共有
- コメントへの返信スレッド
- メモの検索/フィルタ（後続改善で検討）

---

## 技術方針

- データは `ArtifactProjectMemoModel` に集約し、Widget は View のみ担当
- コンポジション切り替えは `CurrentCompositionChangedEvent` を購読
- フレームジャンプは `FrameChangedEvent` を発行
- `setStyleSheet()` は使わず、既存の theme token / `QPalette` / `QProxyStyle` を使用

---

## 進捗サマリー

| Phase | 状態 |
|---|---|
| Phase 1 | 未着手 |
| Phase 2 | 未着手 |
| Phase 3 | 未着手 |

**総合完成度:** 0%
