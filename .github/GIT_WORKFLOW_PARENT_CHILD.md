# Git Workflow: Parent-Child Repository Structure

このドキュメントは、**ArtifactStudio (親) → Artifact, ArtifactCore, ArtifactWidgets (子)** の親子リポジトリ構成での安全なコミット・プッシュ手順を定義します。

AI エージェントは **必ずこの手順を守ること**。

---

## 🚫 **禁止事項（MUST NOT）**

1. **子リポジトリを直接編集後、親をプッシュしない**
   - 親のgitlink が古いままになり、リポジトリが不整合状態になります

2. **子リポジトリで変更後、親に反映せずプッシュしない**
   - ローカルでは動作するが、他の開発者が pull したときに子が古いコミットを参照します

3. **親と子のコミットメッセージを無関係にしない**
   - 親子のコミット履歴が乖離し、ロールバック時に対応が困難になります

4. **複数の子リポジトリを同時に変更して親に push しない**
   - 1つの子で失敗した際、全体をロールバックできなくなります

5. **親の push に失敗しても、子は push したままにしない**
   - 親がリモートでは古いままで、子だけ新しくなり、次の pull で競合が多発します

---

## ✅ **安全な手順**

### **ケース 1: 子リポジトリのみ変更した場合**

#### Step 1: 子リポジトリで確認
```bash
cd X:\dev\ArtifactStudio\Artifact  # (または Core/Widgets)
git status
git diff
```

#### Step 2: 子リポジトリでコミット
```bash
git add .
git commit -m "feat_MyFeature_description"  # 親と同期しやすい名前にすること
```

#### Step 3: 子リポジトリで状態確認
```bash
git log --oneline -3
git branch
```
✅ **親と子が同じブランチ（`main`）にいることを確認**

#### Step 4: 子リポジトリを push
```bash
git push origin main
```

#### Step 5: 親リポジトリで gitlink 更新
```bash
cd X:\dev\ArtifactStudio
git status
# 出力: modified:   Artifact (new commits)
git add Artifact
git commit -m "Bump_Artifact_to_[commit_hash_or_description]"
git push origin main
```

#### Step 6: 親の push 成功を確認
```bash
git log --oneline -3
git remote -v
```

---

### **ケース 2: 複数の子リポジトリを変更した場合**

**各子リポジトリごと** に `ケース 1` を実行してください。

```bash
# Artifact 完了 → 親に反映
# ArtifactCore 完了 → 親に反映  
# ArtifactWidgets 完了 → 親に反映
```

**同時に複数の子をコミット・プッシュしない**。1つずつ確認・反映してください。

---

### **ケース 3: 親リポジトリのみ変更した場合**

```bash
cd X:\dev\ArtifactStudio
git status  # Artifact/Core/Widgets に修正がないことを確認
git add .
git commit -m "chore_ParentRepo_description"
git push origin main
```

---

## ⚠️ **トラブル対応**

### **子をコミット後、親をプッシュする前に親の push に失敗した場合**

```bash
# 失敗時の親の HEAD
git log --oneline -3  # ローカル ≠ リモート

# 対応:
# 1. 原因を特定（権限、競合など）
# 2. git pull origin main（競合確認）
# 3. 競合があれば解決して再 push

git push origin main
```

### **親の push に失敗したが、子は既に push した場合**

```bash
# 状態確認
git -C Artifact log --oneline origin/main -3
git log --oneline -3

# 問題: 親がリモートで古いままなので、
#      他の開発者が pull したときに子も古いコミットを参照

# 対応:
# 1. 親の push を成功させる（git pull → merge/rebase → push）
# 2. 親がリモートで最新になったら、他の開発者に通知
```

### **子で revert/rebase したが、親に反映する前に push した場合**

```bash
# 子リポジトリ
git log --oneline origin/main -3  # リモートの状態
git log --oneline -3               # ローカルの状態

# ローカルのハッシュ ≠ リモートのハッシュ の場合、
# 親の gitlink が古いコミットを指していない確認後に
# 親を push してください
```

---

## 📋 **チェックリスト（AI エージェント向け）**

**毎回の push 前に、このチェックを実行してください：**

```bash
# [ ] 1. 子リポジトリの状態確認
git -C Artifact status
git -C ArtifactCore status
git -C ArtifactWidgets status

# [ ] 2. すべてのローカルコミットが stage されたか確認
git status  # (親リポジトリ)

# [ ] 3. 親のコミット前に子が全て push されたか確認
git log --oneline -1
# コミットメッセージが "Bump_*" であれば OK

# [ ] 4. 親の push 実行
git push origin main

# [ ] 5. 親の push が成功したか確認
git log --oneline origin/main -1  # ローカルの HEAD と同じハッシュ？
```

---

## 🔗 **参考資料**

- [Git Submodules ドキュメント](https://git-scm.com/book/ja/v2/Git-Tools-%E3%82%B5%E3%83%96%E3%83%A2%E3%82%B8%E3%83%A5%E3%83%BC%E3%83%AB)
- [Submodule push の注意](https://git-scm.com/docs/git-push#Documentation/git-push.txt---recurse-submodulesnbspnbspcommit-check)

---

## 🎯 **要約**

| 操作 | 順序 | 確認 |
|-----|-----|------|
| 子を編集 | 1 | `git status` で確認 |
| 子をコミット | 2 | `git commit` |
| 子を push | 3 | `git push origin main` |
| 親の gitlink を更新 | 4 | `git add [child]` |
| 親をコミット | 5 | `git commit -m "Bump_..."` |
| 親を push | 6 | `git push origin main` ✅ 成功を確認 |

**この順序を守ること。逆順や同時実行は禁止。**
