# Copilot Instructions

## Project Guidelines
- The application uses Diligent Engine for direct DX12 rendering.
- Uses parent-child repository structure: **ArtifactStudio (parent) → Artifact, ArtifactCore, ArtifactWidgets (children)**

## 🚫 CRITICAL: Git Workflow for Parent-Child Repositories

**MUST read and follow: `.github/GIT_WORKFLOW_PARENT_CHILD.md`**

### Golden Rule
1. **Edit child repos ONLY when explicitly requested by user**
2. **Child commit → Child push → Parent gitlink update → Parent push**
3. **NEVER commit parent before all children are pushed**
4. **NEVER push parent without updating child gitlinks**
5. **Check before every push**:
   - All child repos have clean working trees or their changes are committed/pushed
   - Parent's gitlinks point to the latest child commits
   - Commit messages follow the format: `feat_FeatureName` or `fix_IssueName` (underscores, not spaces)

### Pre-Push Checklist (MANDATORY)
Before running `git push origin main`:
```bash
# [ ] Confirm which repositories were modified
git status -s

# [ ] If child repos modified, ensure they're all pushed
git -C Artifact push origin main
git -C ArtifactCore push origin main  
git -C ArtifactWidgets push origin main

# [ ] Update parent gitlinks
git add Artifact ArtifactCore ArtifactWidgets
git commit -m "Bump_[RepoName]_to_[description]"

# [ ] Finally, push parent
git push origin main

# [ ] Verify parent push succeeded
git log --oneline origin/main -1
```

### Repository Structure
```
ArtifactStudio/                      (PARENT)
├── main (primary development branch)
├── Artifact/                        (CHILD submodule)
│   ├── main
│   └── recover/2026-03-25-wip (backup)
├── ArtifactCore/                    (CHILD submodule)
│   ├── main
│   └── recover/2026-03-25-wip (backup)
├── ArtifactWidgets/                 (CHILD submodule)
│   └── main
└── skinny-canopy (worktree, do not modify parent/child relationship)
```