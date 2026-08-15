# レイヤー自動命名規則の実装

**最終更新:** 2026-08-15
**マイルストーン**: M-LA-1 Layer Auto-Naming Convention
**作成日**: 2026-04-10
**見積もり**: 8-12h
**優先度**: Low (細かいUX改善)

## 2026-08-15 現行コード照合

- ✅ 各 layer class に既定名があり、Layer Factory は `ArtifactLayerInitParams::name()` を初期名として適用する。画像／動画／SVG／3D などの import は source の base name を渡す経路がある。
- ✅ Camera／Light／Shape／Text／Clone／Group／Adjustment などは型ごとの既定名を持ち、Timeline では inline rename、F2、context menu、専用 Rename command／Undo が利用できる。
- ✅ PreCompose については `PreComposeManager` に default name prefix と auto-naming flag があり、生成された precomp 名の制御経路が存在する。
- ⚠️ 画像・テキスト・シェイプ等を横断する共通の重複回避（`_1`／`_2`）、自動命名 ON／OFF 設定、手動リネーム後の連番継続は現行コード上で確認できない。命名は主に呼び出し側の初期名と各 class の固定既定名に分散している。
- ⏳ ソース名／色情報を使った統一規則、複数同時生成時の連番最適化、カスタム命名プリセット、設定保存、全経路の runtime／Undo QA は未完了。

## Update 2026-08-15

- 現行コードでは型別既定名、import source の basename、PreCompose の auto-naming、Timeline／F2 rename と Undo を確認できる。
- 全 layer type 共通の重複回避、auto-naming 設定、手動 rename 後の連番継続、命名 preset、全生成経路の runtime／Undo QA は未完了または未確認。

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
