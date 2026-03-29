# Repository Pull Workflow

このリポジトリは、親リポジトリと複数の submodule を含む。  
他 PC でも安全に最新化するには、`git pull` だけで済ませず、親と submodule の整合を確認しながら更新する必要がある。

この手順書は、`最新を安全に取る側` のための運用をまとめたもの。

関連:

- `docs/REPO_PUSH_WORKFLOW.md`

## 目的

この手順で防ぎたい事故は次の通り。

- 親 repo は更新されたが submodule が古いまま
- submodule が detached HEAD のまま壊れる
- `.gitmodules` と実体が食い違う
- ある PC では開けるが、別 PC では checkout できない
- fork 管理の外部 repo を誤って upstream 扱いする

## 基本原則

- 親 repo を更新したら、必ず submodule も同期する
- `--recursive` を基本にする
- submodule 内でローカル変更があるなら、更新前に必ず確認する
- detached HEAD は異常ではないが、作業中に気づかず上書きしない
- submodule を勝手に commit しない。まず参照 SHA に合わせる

## 一番安全な標準手順

通常はこれを使う。

```bash
git status --short --branch
git pull --rebase --autostash
git submodule sync --recursive
git submodule update --init --recursive
git submodule status --recursive
```

### 意味

- `git status --short --branch`
  先にローカル変更や branch を確認する
- `git pull --rebase --autostash`
  親 repo を安全寄りに更新する
- `git submodule sync --recursive`
  `.gitmodules` の URL 変更をローカル設定へ反映する
- `git submodule update --init --recursive`
  親 repo が指す commit に submodule を合わせる
- `git submodule status --recursive`
  取得状態を確認する

## PowerShell での推奨手順

Windows ではこの順が扱いやすい。

```powershell
git status --short --branch
git pull --rebase --autostash
git submodule sync --recursive
git submodule update --init --recursive
git submodule status --recursive
```

## クリーンな別 PC を最新化する初回手順

```bash
git clone <parent-remote-url> ArtifactStudio
cd ArtifactStudio
git submodule sync --recursive
git submodule update --init --recursive
git submodule status --recursive
```

### 確認ポイント

- 親 repo が期待 branch にいる
- submodule が missing になっていない
- `-` や `+` が大量に出ていない
- private fork を参照する submodule が認証エラーになっていない

## 日常更新の手順

普段の更新はこれで足りる。

```bash
git status --short --branch
git pull --rebase --autostash
git submodule update --init --recursive --jobs 8
git submodule status --recursive
```

`--jobs 8` は並列更新用。環境に応じて省略可。

## ローカル変更がある場合

更新前に必ず確認する。

```bash
git status --short --branch
git submodule foreach --recursive git status --short
```

### 判断基準

- 親だけ変更している
  まず親 repo を整理する
- submodule に変更がある
  その変更が必要なものか確認する
- 不明な変更が submodule にある
  先に stash または別 branch へ退避する

### 安全策

```bash
git stash push -u -m "pre-pull-parent"
git submodule foreach --recursive "git stash push -u -m pre-pull-submodule || true"
```

その後に通常の pull 手順を行う。

## detached HEAD の扱い

submodule は `git submodule update` 後に detached HEAD になることがある。  
これは通常動作。

ただし、その状態で submodule に直接作業を始めるのは危険。

作業が必要なら:

```bash
cd <submodule>
git switch <working-branch>
git pull --rebase
```

更新だけしたいなら、detached HEAD のまま親の参照 SHA に合わせておけばよい。

## URL や fork 参照が変わった場合

`.gitmodules` が更新されていると、古い PC では URL が食い違うことがある。

その場合は必ずこれを挟む。

```bash
git submodule sync --recursive
git submodule update --init --recursive
```

## トラブル時の確認コマンド

```bash
git status --short --branch
git submodule status --recursive
git config --file .gitmodules --get-regexp path
git config --file .gitmodules --get-regexp url
```

submodule ごとの現在位置確認:

```bash
git submodule foreach --recursive "git branch --show-current; git rev-parse --short HEAD"
```

## よくある壊れ方

### 1. 親だけ pull して submodule を更新していない

症状:

- ビルドが通らない
- include や module の不整合が出る
- あるファイルだけ古い

対応:

```bash
git submodule update --init --recursive
```

### 2. `.gitmodules` は更新されたがローカル URL が古い

症状:

- fetch で古い remote に行こうとする
- submodule update が失敗する

対応:

```bash
git submodule sync --recursive
git submodule update --init --recursive
```

### 3. submodule 内に作業途中の変更が残っている

症状:

- update で止まる
- rebase できない
- 想定外の差分が残る

対応:

- stash
- 別 branch に退避
- 本当に必要な変更か確認

## この repo 向けの注意

- `ArtifactWidgets`
- `libs/DiligentEngine`
- `third_party/*`

これらは、親 repo から見れば参照先であり、他 PC で最新化する時は `親が指す commit` に合わせるのが原則。

特に外部依存は、手元で勝手に branch を進めない方が安全。

## 推奨ワンライナー

毎回同じ手順にしたいならこれでよい。

```bash
git pull --rebase --autostash && git submodule sync --recursive && git submodule update --init --recursive
```

PowerShell でもそのまま使える。

## 更新後の最終確認

```bash
git status --short --branch
git submodule status --recursive
```

期待する状態:

- 親 repo に不要な差分がない
- submodule が missing になっていない
- 参照 SHA が揃っている
- 他 PC でも同じ commit 構成を再現できる

## 運用メモ

push 側が正しく `submodule を先に push` していないと、pull 側で解決不能になることがある。  
その場合は pull 手順の問題ではなく、公開順序の問題なので、`docs/REPO_PUSH_WORKFLOW.md` 側の修正が必要。
