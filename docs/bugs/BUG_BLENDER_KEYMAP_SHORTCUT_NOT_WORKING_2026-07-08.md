# BUG: Blender ライクショートカット設定システムが押下しても機能しない

> 作成: 2026-07-08 / 状態: 診断済み（修正済み）
> 対象: `ArtifactCore/src/UI/InputOperator.cppm`（KeyMap / InputBinding / InputOperator）
> 症状: 文字列で登録したショートカット（例: "Ctrl+S"）を押しても何も起きない。

---

## 症状

- ユーザーが構築した Blender ライクなショートカット設定システム（`KeyMap` + `InputOperator`、`Input.Operator` モジュール）で、キーを押しても対応アクションが実行されない。
- イベントは各ウィジェット（AssetBrowser / CompositionEditor / Timeline / Inspector）から `InputOperator::processKeyPress(this, event->key(), event->modifiers())` 経由で正しく届いている（配送経路は正常）。

---

## 根本原因: `addBinding(QString)` が修飾キーを破棄し、キーコードに埋め込む

### 1. 文字列オーバーロードがモディファイアを落とす

`ArtifactCore/src/UI/InputOperator.cppm:404-409`:

```cpp
InputBinding* KeyMap::addBinding(const QString& keySequence,
                                Action* action,
                                const QString& description) {
    QKeySequence seq(keySequence);
    return addBinding(seq[0], InputEvent::Modifiers(), action, description);  // ← 修飾キーを捨てる
}
```

- `QKeySequence seq("Ctrl+S")` の `seq[0]` は、**修飾キーのビットを上位に含む結合値**（例: `Qt::CTRL | Qt::Key_S`）を返す。
- この結合値が `key` 引数として `addBinding(int key, ...)` に渡され、`binding->setKeyCode(key)` により `keyCode_` にセットされる。
- 同時に `InputEvent::Modifiers()`（空）が `modifiers` 引数として渡るため、`requiredModifiers_` も空のまま。

結果: バインディングは `keyCode_ = (Ctrlビット | Key_S)`、`requiredModifiers_ = 空` で保存される。

### 2. 照合時に生キーと結合値が一致しない

`ArtifactCore/src/UI/InputOperator.cppm:81-105`（`InputBinding::matches`）:

```cpp
if (event.keyCode != keyCode_) return false;   // event.keyCode は修飾ビットなしの生キー
```

- 実際の `QKeyEvent::key()` は修飾ビットを含まない `Qt::Key_S` を返す（修飾キーは `event->modifiers()` 側）。
- よって `event.keyCode (Key_S) != keyCode_ (Ctrl|Key_S)` → **常に false → マッチしない**。

### 3. `findBinding` のルックアップキーも不一致

`ArtifactCore/src/UI/InputOperator.cppm:436-440` および登録側 `:398`:

```cpp
// 登録: keyBindings_[{key, static_cast<int>(modifiers)}]   // key = Ctrl|Key_S, modifiers = 0
// 照合: keyBindings_.find({key, static_cast<int>(mods)})    // key = Key_S, mods = Ctrl
```

- 登録時は `{Ctrl|Key_S, 0}`、照合時は `{Key_S, Ctrl}` で探すため、**マップ検索もヒットしない**。

### 4. 第2の潜在的欠陥: `matches()` が `modifiers_` を比較しない

`ArtifactCore/src/UI/InputOperator.cppm:385-386`:

```cpp
binding->setKeyCode(key);
binding->setModifiers(modifiers);   // modifiers_ をセット
```

- `addBinding(int key, Modifiers)` は `modifiers_` をセットするが、`matches()` は `requiredModifiers_` / `forbiddenModifiers_` をチェックするだけで **`modifiers_` を比較しない**。
- したがって `requiredModifiers_` を設定する経路がない限り、キー単体の一致だけで発火してしまう（広すぎるマッチ）。文字列オーバーロードでは `requiredModifiers_` が設定されないため #1 の「発火しない」バグと表裏。

---

## 呼び出し経路（確認済み）

1. ウィジェット `keyPressEvent`/`eventFilter` → `InputOperator::processKeyPress(this, event->key(), event->modifiers())`（`cppm:644`）。
2. `getWidgetKeyMap(widget)`（`cppm:612`）でウィジェットに紐づく KeyMap を名前検索。
3. `findMatchingBinding(keyMap, key, mods)`（`cppm:58`）→ `KeyMap::findBinding(key, mods)`（`cppm:436`）。
4. `findBinding` が `keyBindings_` から `{key, mods}` を検索 → **文字列登録の場合ヒットせず null を返す**。
5. `processKeyPress` は `false` を返し、イベントは通常の Qt ショートカット処理へ流れる（または無視される）。

※ `registerWidgetKeyMap`（`cppm:596`）は KeyMap 名を widget property に保存し、`getWidgetKeyMap` で名前検索する。この経路自体は正常。

---

## 修正案

### 修正 A（必須）: `addBinding(QString)` で修飾キーを分離して渡す

`ArtifactCore/src/UI/InputOperator.cppm:404-409` を以下に修正:

```cpp
InputBinding* KeyMap::addBinding(const QString& keySequence,
                                Action* action,
                                const QString& description) {
    QKeySequence seq(keySequence);
    if (seq.isEmpty()) {
        return nullptr;
    }
    const int combined = seq[0];
    const int key = combined & ~int(Qt::KeyboardModifierMask);   // 純キーのみ
    const auto mods = toInputModifiers(
        Qt::KeyboardModifiers(combined & int(Qt::KeyboardModifierMask)));  // 修飾キーのみ
    return addBinding(key, mods, action, description);
}
```

### 修正 B（推奨）: `addBinding(int, Modifiers)` で `requiredModifiers_` を設定

`ArtifactCore/src/UI/InputOperator.cppm:385-386` 付近:

```cpp
binding->setKeyCode(key);
binding->setModifiers(modifiers);
binding->setRequiredModifiers(modifiers);   // matches() が比較するフィールドを設定
```

### 修正 C（任意）: `findBinding` の厳密一致を緩和

`KeyMap::findBinding(key, mods)`（`:436`）は完全一致。NumLock/CapsLock 等の余剰モディファイアを無視するなら、マスク比較へ変更する（優先度低）。

---

## 影響範囲・制約

- 変更ファイル: `ArtifactCore/src/UI/InputOperator.cppm`（既存行の編集のみ）。
- CRLF 維持: `edit` ツールで該当箇所のみ書き換え（行末を維持）。
- `InputEvent` / `toInputModifiers` の変換が正しければ、`event->key()`（生キー）と `event->modifiers()`（修飾）の分離は `processKeyPress` 側で既に正しく行われているため、修正 A/B のみで解決する。
- 修飾キーなしの単キーバインド（例: "Space"）は `seq[0]` にビットがないため、従来通り動作していた（報告の「一部は効く」場合の説明）。

---

## 未確認事項

- ユーザー設定システムが `addBinding(QString)` 経由で登録しているか、`addBinding(int, Modifiers)` 経由か（設定ダイアログ `ShortcutSettingPage` の実装を要確認）。文字列経由の場合は本バグが確定。
- `toInputModifiers` が `Qt::KeyboardModifiers` の各ビットを `InputEvent::Modifiers` に正しく写像しているか（マスク定数 `Qt::KeyboardModifierMask` の存在を要確認）。
