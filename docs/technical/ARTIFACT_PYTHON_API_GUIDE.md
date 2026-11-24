# Artifact Python API ガイド

**最終更新:** 2026-11-20

Artifact の埋め込み Python から、現在の project／composition／layer／effect／playback／render 機能を操作するための API リファレンスです。API は `artifact` module に登録されます。

## 実行方法

アプリの `Script > Macros` は、scripts workspace の `macros` フォルダー直下にある `.py` ファイルを列挙します。ファイルを選ぶと Artifact の埋め込み Python engine が実行します。AE Utility Pack の既存スクリプトも同じ API bridge を使います。

`artifact.core.system` を含む純 Python の Core API は、外部 Python fallback でも各実行時に登録されます。project／layer／effect など C++ callback に接続する `artifact.*` API は in-process bridge を必要とし、external fallback では利用できません。

スクリプトから直接利用できるのは登録済み関数です。`app`、`project`、`selected_layers` などの型付き C++ object model や、汎用 Python REPL があるという意味ではありません。

## はじめてのスクリプト

```python
import artifact

project = artifact.project_info()
artifact.log(f"Project: {project.get('name', '(none)')}")

for layer in artifact.get_layers():
    artifact.log(f"Layer: {layer['name']} ({layer['id']})")
```

`artifact.log(message, level="info")` は `print` を使ってログへ出します。レベルは `info`、`warn`、`error`、`debug` を受け取ります。

## API 一覧

### Project

| API | 用途 | 戻り値 |
|---|---|---|
| `project_new(name="Untitled")` | project を作成 | 成功時 `True` |
| `project_open(path)` | project file を開く | 成功時 `True` |
| `project_save(path=None)` | 現在の project を保存。省略時は現在の path | 成功時 `True` |
| `project_info()` | project／current composition の情報を取得 | dict。project がない場合は空 dict |
| `project_current_comp_id()` | current composition ID を取得 | string |

`project_info()` の composition 情報には `width`、`height`、`fps`、`current_comp_id` が含まれる場合があります。

### Layers

| API | 用途 | 戻り値 |
|---|---|---|
| `get_layers()` | 現在の composition の全 layer | `id`、`name`、`visible`、`locked` を持つ dict の list |
| `get_selected_layers()` | 選択 layer | `id`、`name` を持つ dict の list |
| `create_layer(name, layer_type="solid")` | layer を作成 | 成否 bool。type は `solid`、`text`、`null` |
| `delete_layer(name)` | 名前で layer を削除 | 成否 bool |
| `duplicate_layer(name, new_name=None)` | 名前で layer を複製 | 成否 bool |
| `move_layer(name, index)` | layer を指定 index へ移動 | 成否 bool。負数／整数でない index は失敗 |
| `get_layer_property(name, property_name)` | 対応 property を読む | 値、未対応時 `None` |
| `set_layer_property(name, property_name, value)` | 対応 property を設定 | 成否 bool |
| `rename_selected_layers(...)` | 選択 layer を一括命名 | 成否 bool |
| `clean_selected_layers(...)` | 選択 layer の指定項目を整理 | 成否 bool |
| `trim_comp_to_content(...)` | layer 内容に合わせて comp range を調整 | 成否 bool |

現在の `get_layer_property`／`set_layer_property` が扱う property は `visible`、`locked`、`solo`、`shy`、`name` です。API は layer 名で対象を特定するため、同名 layer がある composition では一意性に依存できません。

### Effects

| API | 用途 |
|---|---|
| `get_available_effects()` | 使用可能な effect の ID／表示名一覧 |
| `add_effect(layer_name, effect_type)` | layer に effect を追加 |
| `get_effects(layer_name)` | layer の effect 一覧 |
| `remove_effect(layer_name, effect_index)` | index の effect を削除 |
| `get_effect_param(layer_name, effect_index, param_name)` | effect parameter を取得 |
| `set_effect_param(layer_name, effect_index, param_name, value)` | effect parameter を設定 |

effect index は `get_effects()` が返す順序に対応します。利用可能な effect ID は固定値と決めつけず、`get_available_effects()` で取得してください。

### Playback と Render

| API | 用途 |
|---|---|
| `current_frame()` / `set_current_frame(frame)` | 現在 frame の取得／移動 |
| `play()` / `pause()` / `stop()` | playback 制御 |
| `set_render_settings(start=..., end=..., output_path=..., format=...)` | 後続の `render()` の既定値を設定 |
| `render(start=0, end=None, output_path=None, format="png")` | 現在の composition を Render Queue に追加して開始 |
| `render_current_frame(output_path=None)` | 現在 frame だけを Render Queue に追加して開始 |
| `for_each_frame(start, end, callback)` | inclusive な frame 範囲で frame を移動し callback を呼ぶ |

`render()` と `render_current_frame()` は queue 作成と開始要求を行います。`True` は render の完了や最終ファイルの検証を保証しません。`for_each_frame()` は frame を変更するため、処理後に必要なら元の frame に戻してください。

### Environment

`artifact.environment.get(name, default=None)`、`has(name)`、`names()` は、アプリが継承した `ARTIFACT_*` 環境変数を読み取ります。書き込み API はありません。

### OS / System

`artifact.core.system` は ArtifactCore の system helper です。

| API | 用途・契約 |
|---|---|
| `get_engine_version()` / `get_platform()` | engine version と Python platform 名 |
| `resolve_path(path)` | 絶対パスへ解決 |
| `path_exists(path)` / `is_file(path)` / `is_directory(path)` | path の存在・種類を確認 |
| `list_directory(path, max_entries=10000)` | 1階層を名前順で列挙。上限は10,000 entries。各要素は `name`、`path`、`is_file`、`is_directory` |
| `read_text(path, encoding="utf-8", max_bytes=16777216)` | text file を読む。既定の最大読取量は16 MiB、指定可能な上限は256 MiB。超過時は `ValueError` |
| `write_text(path, content, encoding="utf-8", overwrite=False)` | text file を書く。既存ファイルは既定で保護 |
| `create_directory(path, parents=True, exist_ok=True)` | directory を作成 |
| `delete_file(path)` | file を削除。directory の再帰削除は提供しない |
| `copy_file(source, destination, overwrite=False)` / `move_file(...)` | file をコピー／移動。destination は既定で上書きしない |
| `start_process(args, cwd=None, env=None, show_window=False)` | argv list で子 process を開始し PID を返す。shell 経由では起動しない。`env` は string-to-string dict |
| `process_status(pid)` | 起動中／終了／管理外を返す。終了時は exit code を含む |
| `terminate_process(pid)` / `forget_process(pid)` | 起動した process を終了要求／終了済み handle を解放 |

Filesystem API は `~` を展開します。相対 path は現在の process working directory 基準です。file I/O error は Python の標準例外として呼び出し元へ返ります。`read_text()` と `write_text()` は通常ファイルだけを扱い、書き込み先 symlink は拒否します。`delete_file()` は通常ファイルか symlink entry だけを削除し、directory と special file は拒否します。子 process の標準入力・出力・エラー出力は破棄されます。`env` は現在の環境への追加・上書きとして子 process にだけ渡り、Artifact process 自体の環境は変更しません。`cwd` は `~` 展開されません。

```python
import artifact.core

system = artifact.core.system
print(system.list_directory(system.resolve_path(".")))

pid = system.start_process(["python", "tools/generate_report.py"])
print(system.process_status(pid))
```

この API はファイルや process を変更できます。path の許可範囲を制限する sandbox はありません。`start_process` は argv list の各要素を一つの引数として扱い、shell の解釈をしません。stdout/stderr を取得せず、実行完了を待ちません。Windows では `show_window=False` が console window を抑止します。戻り値の PID を `process_status()` で確認してください。process handle は現在の Python interpreter 内のメモリ上で最大128個まで保持し、終了後に `forget_process()` で解放してください。external fallback はスクリプト実行ごとに interpreter を作り直すため、process handle は同じスクリプト内でのみ追跡できます。アプリ終了後に子 process がどう扱われるかは保証されません。利用者が信頼するスクリプトだけを実行してください。

## 例: 選択 layer の名前と可視状態を出力

```python
import artifact

for layer in artifact.get_selected_layers():
    name = layer["name"]
    visible = artifact.get_layer_property(name, "visible")
    artifact.log(f"{name}: visible={visible}")
```

## 注意点

- API は登録済み bridge の範囲に限られ、全アプリ機能を呼び出せる汎用 object model ではありません。
- `Script > Macros` から起動した Python はアプリの現在状態を変更できます。破壊的な操作は project を保存し、対象を確認してから実行してください。
- API に sandbox や実行時間制限があるとは限りません。信頼できるスクリプトだけを実行してください。
- Python REPL、型付き `app`／`project` object、headless CLI は別途未完了の項目です。詳細は [Python API & Scripting milestone](../planned/MILESTONE_PYTHON_API_SCRIPTING_2026-03-30.md) を参照してください。

## 実装参照

- `Artifact/src/Export/Python/ArtifactPythonAPI.cpp`
- `ArtifactCore/src/Script/Python/CorePythonAPI.cpp`
- `Artifact/src/Widgets/Menu/ArtifactScriptMenu.cpp`
- `Artifact/src/AppMain.cpp`（Python engine bootstrap と API 登録）
