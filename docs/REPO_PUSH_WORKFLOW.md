# Repository Push Workflow

このリポジトリは、親リポジトリ 1 つと複数の submodule で構成されている。  
push の基本方針は **「submodule を先に確定し、最後に親リポジトリを push する」**。

## 対象構成

- 親リポジトリ
- `Artifact`
- `ArtifactCore`
- `ArtifactWidgets`
- `libs/DiligentEngine`
- `third_party/verdigris`

注意:

- `third_party/Qt-Advanced-Docking-System` は現在の `.gitmodules` に載っていないため、通常の submodule と同じ扱いにしない。
- もしこのディレクトリを fork 版ライブラリとして使うなら、別途 submodule として正式登録するか、独立した repo として運用方針を決める。

## 安全な push 順序

### 1. まず親と各 submodule の差分を切り分ける

確認コマンド:

```bash
git status --short --branch
git submodule status --recursive
```

見るポイント:

- 現在の branch 名
- detached HEAD になっていないか
- 親リポジトリの変更
- 各 submodule 内の変更
- `.gitmodules` の変更有無
- submodule の gitlink が更新されているか

branch / HEAD の確認は各 repo で行う。

```bash
git branch --show-current
git rev-parse --short HEAD
```

### 2. submodule を先にコミットして push する

各 submodule は独立した repo として扱う。

手順:

1. submodule に入る
2. まず `git pull --rebase` で upstream を取り込む
3. 変更をコミットする
4. その submodule の remote に push する

この順番を守る理由:

- 親リポジトリは submodule の commit SHA だけを持つ
- 親を先に push すると、参照先 SHA が remote に存在しない状態を作りやすい
- その状態は clone / checkout / CI で壊れる

### 3. 親リポジトリ側で submodule pointer を更新する

submodule の push が終わったら、親リポジトリに戻って pointer 更新を commit する。

確認コマンド:

```bash
git diff --submodule
git status --short
```

ここで見たいのは、親の index に入るのが「実ファイル」ではなく「submodule の SHA 更新」になっていること。

### 4. 親リポジトリを最後に push する

親リポジトリは、submodule の commit がすでに remote にある状態で push する。

これで以下が成立する:

- 親の gitlink が指す SHA が remote 上に存在する
- clone 直後に `git submodule update --init --recursive` が通る
- 再現性が保てる

## Push 前の再現確認

可能なら、別の clone で再現できるか確認する。

確認例:

```bash
git clone <remote-url> <temp-dir>
git submodule update --init --recursive
```

見るポイント:

- クリーン clone でエラーが出ない
- submodule が欠けていない
- `.gitmodules` と実体の参照先が一致している

この確認が通れば、親と submodule の同期が崩れていない可能性が高い。

## `Qt-Advanced-Docking-System` の扱い

このディレクトリは、親の index に gitlink として見えている場合があるが、`.gitmodules` には登録されていないことがある。  
その場合は **壊れた submodule 状態** とみなす。

対応は 3 択:

- その変更を使わないなら、親から切り離す
- fork 版ライブラリとして使うなら、`.gitmodules` に正式登録する
- 独立 repo として扱うなら、親の submodule 管理から外して別運用にする

重要:

- `.gitmodules` にない gitlink を「なんとなく submodule」として push しない
- 親 repo のみを push しても、他人の環境では再現できない

## 最終チェック

push 前に次を確認する。

```bash
git status --short --branch
git submodule status --recursive
```

期待する状態:

- 親リポジトリの変更が意図どおり
- submodule はそれぞれ push 済み
- `.gitmodules` と gitlink の整合が取れている
- 不明な nested repo が残っていない

## 実運用の判断基準

迷ったら次の順で判断する。

1. その変更は親か submodule のどちらに属するか
2. その commit は remote に先に存在しているか
3. 親が指す SHA は他人の環境で再現できるか
4. `.gitmodules` に載っていない repo を混ぜていないか
5. 各 repo で `pull --rebase` 済みか
6. 可能なら別 clone で `submodule update --init --recursive` を通したか

この 6 点を満たしていれば、親と submodule を壊さずに push できる。
