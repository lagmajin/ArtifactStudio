# レイヤー自動命名規則の実装
**マイルストーン**: M-LA-1 Layer Auto-Naming Convention
**作成日**: 2026-04-10
**見積もり**: 8-12h
**優先度**: Low (細かいUX改善)

## 概要

After Effects のようなプロフェッショナルな動画編集ソフトでは、レイヤーが増えると管理が大変になる。
新規レイヤー作成時に、ソースファイル名やレイヤー種別に基づいて自動的に適切な名前を付けることで、ワークフローを効率化する。

## 機能仕様

### 自動命名規則
- **画像/動画ファイル**: ファイル名から拡張子を除去 (例: `background.jpg` → `background`)
- **テキストレイヤー**: `Text Layer` + 連番 (既存と重複しないようインクリメント)
- **シェイプレイヤー**: `Shape Layer` + 連番
- **ソリッドレイヤー**: `Solid` + 色情報 + 連番 (例: `Solid_Red_1`)
- **Nullレイヤー**: `Null` + 連番
- **ライト/カメラ**: `Light 1`, `Camera 1` など

### 実装要件
- `ArtifactAbstractLayer::createLayer()` 呼び出し時に自動命名を適用
- 重複名チェック (同じ名前が存在する場合 `_1`, `_2` を付加)
- ユーザー設定で自動命名をON/OFF可能
- 手動で名前変更した場合は次回も連番を継続

### 実装場所
- `Artifact/src/Composition/ArtifactAbstractComposition.cppm` のレイヤー追加関数
- 新規設定項目: `Preferences > General > Auto-naming for new layers`

### AEとの差別化
- よりスマートな命名: ファイルパスからフォルダ名も考慮 (例: `footage/walking_man.mp4` → `walking_man`)
- 複数レイヤー同時作成時の連番最適化
- 命名規則のカスタマイズ可能 (プリセット保存)

## 技術的考慮
- ファイルパス解析は `QFileInfo` を使用
- 連番管理はコンポジションごとに独立
- undo/redo 対応

## テストケース
- 同じ名前のファイルを複数インポートした場合の命名
- テキストレイヤーの連番生成
- 自動命名OFF時の動作
- 手動命名後の自動命名継続性