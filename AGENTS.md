# AGENTS

## 🚫 CRITICAL: Parent-Child Repository Git Workflow

This project uses a **parent-child submodule structure**:
- **Parent**: `ArtifactStudio` (main branch)
- **Children**: `Artifact`, `ArtifactCore`, `ArtifactWidgets` (all main branch)

**Before any commit/push operation**, read and follow: [`.github/GIT_WORKFLOW_PARENT_CHILD.md`](.github/GIT_WORKFLOW_PARENT_CHILD.md)

### Golden Rules (MUST FOLLOW)
1. **Always commit child repos first**, then push
2. **NEVER push parent without updating child gitlinks** with `git add Artifact ArtifactCore ArtifactWidgets`
3. **Child push → Parent gitlink update → Parent push** (this order is mandatory)
4. **Check before every push**:
   ```bash
   git status -s                    # Confirm which repos modified
   git -C Artifact push origin main
   git -C ArtifactCore push origin main
   git -C ArtifactWidgets push origin main
   git add Artifact ArtifactCore ArtifactWidgets
   git commit -m "Bump_[RepoName]_to_[description]"
   git push origin main
   git log --oneline origin/main -1  # Verify success
   ```

5. **Never edit child repos unless explicitly requested by user**

---

AI が UI 名称やウィジェット責務で迷ったら、まず [docs/WIDGET_MAP.md](docs/WIDGET_MAP.md) を参照してください。

特にタイムライン周辺は、UI 上の呼び方とコード上のクラス名がずれやすいので、名称確認を先に行うこと。

制作パス系の実装マイルストーンは [Artifact/docs/MILESTONE_M11_SOFTWARE_RENDER_PIPELINE_2026-03-11.md](Artifact/docs/MILESTONE_M11_SOFTWARE_RENDER_PIPELINE_2026-03-11.md) を参照してください。

分野別の小さめなバックログは [docs/MILESTONES_BACKLOG.md](docs/MILESTONES_BACKLOG.md) を参照してください。

Project View 専用の実装段階は [Artifact/docs/MILESTONE_PROJECT_VIEW_2026-03-12.md](Artifact/docs/MILESTONE_PROJECT_VIEW_2026-03-12.md) を参照してください。

Asset 系の統合段階は [Artifact/docs/MILESTONE_ASSET_SYSTEM_2026-03-12.md](Artifact/docs/MILESTONE_ASSET_SYSTEM_2026-03-12.md) を参照してください。

`ArtifactIRenderer` の整理段階は [Artifact/docs/MILESTONE_ARTIFACT_IRENDER_2026-03-12.md](Artifact/docs/MILESTONE_ARTIFACT_IRENDER_2026-03-12.md) を参照してください。

`DiligentEngine` / DX12 周辺は、AI にとって読み違えやすいシビアなコードとして扱ってください。

特に `D3D12` / `Diligent` backend / render path の低レベル実装を変更する場合は、推測で広く触らず、関連箇所を十分に読んで変更範囲を最小化すること。

挙動が断定できない場合は、先に現状の責務と依存関係を確認してから編集すること。

`ArtifactCore` 専用のバックログは [ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md](ArtifactCore/docs/MILESTONES_CORE_BACKLOG.md) を参照してください。

Text 系の Core 整備段階は [ArtifactCore/docs/MILESTONE_TEXT_SYSTEM_2026-03-12.md](ArtifactCore/docs/MILESTONE_TEXT_SYSTEM_2026-03-12.md) を参照してください。

Property 系 UI の残骸と再整理方針は [Artifact/docs/PROPERTY_EDITOR_AUDIT_2026-03-11.md](Artifact/docs/PROPERTY_EDITOR_AUDIT_2026-03-11.md) を参照してください。

QtCSS / `setStyleSheet()` は絶対に新規追加しないこと。見た目の調整は `QPalette`、owner-draw、`QProxyStyle`、既存の theme token で解決し、QtCSS は移行不能な例外に限って使う（例外は設計レビュー必須）。

新規のシグナル＆スロット接続は絶対禁止。特にグローバルなシグナルや中央集権的なイベント配線を導入しないこと。新しい公開シグナル／スロットが本当に必要な場合は設計レビューを経て、既存のイベント経路やサービスを再利用する方針とする。

サブモジュール（例: `ArtifactWidgets` / `libs/DiligentEngine` / `third_party/*`）は、ユーザーが明示的に依頼した場合を除き変更・コミット・push しないこと。

サブモジュールに修正が必要な場合は、まず親リポジトリ側で代替可能か確認し、不可なら「fork 運用」または「パッチ運用」を提案してから進めること。
