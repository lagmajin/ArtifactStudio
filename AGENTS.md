# AGENTS

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

QtCSS / `setStyleSheet()` は原則として新規追加しないこと。見た目の調整は `QPalette`、owner-draw、`QProxyStyle`、既存の theme token で解決し、QtCSS は移行不能な例外に限って使うこと。

サブモジュール（例: `ArtifactWidgets` / `libs/DiligentEngine` / `third_party/*`）は、ユーザーが明示的に依頼した場合を除き変更・コミット・push しないこと。

サブモジュールに修正が必要な場合は、まず親リポジトリ側で代替可能か確認し、不可なら「fork 運用」または「パッチ運用」を提案してから進めること。

## Shell Command Execution Rules

* **Keep commands short:** PowerShell often fails with long/complex chains. Keep commands short and simple.
* **Script files for complex logic:** If a command involves loops, conditions, or is very long, **write it to a temporary script file first** (e.g., `_temp.ps1`), execute it, and then delete it.
* **Use absolute paths:** Always use full absolute paths (e.g., `X:\dev\artifactstudio\...`). Do not rely on `cd`.
* **Run sequentially:** Avoid chaining multiple commands with `&&` or `;`. Run them one by one to catch errors easily.
