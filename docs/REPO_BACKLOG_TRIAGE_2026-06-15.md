# Repository Backlog Triage — 2026-06-15

**Purpose**: `docs/planned/*` と `docs/analysis/*` を突き合わせ、いま「未着手 / 着手途中 / 完了だが整理未了」のどれに該当するかを1枚で判定する。
**Inputs**:
- `docs/analysis/COMPOSITION_EDITOR_GAP_ANALYSIS_2026-06-03.md`
- `docs/analysis/WIDGET_GAP_ANALYSIS_2026-06-03.md`
- `docs/analysis/FEATURE_AUDIT_MOTION_DESIGN_2026-06-02.md`
- `docs/MILESTONE_ROADMAP_CURRENT.md` (2026-06-03)
- `docs/planned/MILESTONES_BACKLOG.md` (2026-06-13)
- `docs/worklog/CRITICAL_RENDER_AND_PROJECT_HEALTH_WORKLOG_2026-06-04.md`

---

## 1. 全体ステータス

| 区分 | 件数（概算） | 備考 |
|------|------|------|
| 完了（verified 2026-04-14 帯） | 約 15 | M-DIAG-1〜4 / M-PV-1〜2 / M-AS-4 / M-UI-23 / M-UI-3 / M-UI-5 / M-CE-1 / M-CE-CRIT-2 / M-AI-1 / M-AI-2 / M-AB |
| コードはあるが未接続（P0/P1） | 6 | TextGizmo / Speed Graph sample / Mask vertex insert / Shape Operators 6種 / Text Animator timeline / Group mask |
| 進行中（worklog で実装済み、ビルド未） | 2 | M-CE-CRIT-1 / M-APP-5 (Project Health / Problem View) |
| 着手可能だが未着手 | 多数 | M-RAM-3 / M-LE-1 / M-TL-15 / Shape Operators 一式 / M-UI-6 / M-UI-7 残課題 / M-IR-8 / M-WKR-1 / M-FE-9 ほか |
| 後回し（依存が重い） | 一式 | Render Farm / Puppet / Auto-Orient / Motion Sketch / Roving Keyframes / OIIO pipeline / 3D Gizmo / Software Render Pipeline |

---

## 2. 再トリアージ表（手を動かす候補順）

### Priority A — ビルド未確認の進行中案件を締める

| # | 案件 | ソース / 詳細 | 状態 | 次の作業 |
|---|------|-------------|------|----------|
| A-1 | Critical Render / Media Stability Program | `MILESTONE_CRITICAL_RENDER_MEDIA_STABILITY_2026-04-30.md` / worklog `2026-06-04` | 接続済み・ビルド未 | `Artifact` を MSVC でフルビルド → エラーを最小修正 → smoke case 起動 |
| A-2 | Project Health / Problem View Wiring | `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_PHASE1_EXECUTION_2026-05-12.md` | 接続済み・ビルド未 | A-1 と並走、build 通ったあと Problem View / Health Dashboard / App Debugger 3面で表示確認 |

### Priority B — コードはあるが未接続（1回で効果が大きい）

| # | 案件 | エビデンス | 影響 | 見積目安 |
|---|------|----------|------|----------|
| B-1 | `sampleSpeedGraph()` 実装 | `ArtifactCurveEditorWidget.cppm:815` コメントのみ | Curve Editor の Value/Speed 切替が実データで動く | S |
| B-2 | `ArtifactTextGizmo` を Composition Editor に接続 | `ArtifactTextGizmo.cppm` 210行がいずれの controller からも未 import | text 専用 range selector ハンドルが復活 | S〜M |
| B-3 | Pen Tool: 線分上 vertex insert | `COMPOSITION_EDITOR_GAP_ANALYSIS_2026-06-03.md` §3.a | mask/roto の編集体験の底上げ | S |
| B-4 | Mask Inspector 表示（count/path/enabled） | gap analysis §3.a | マスクの現状把握が UI で可能に | S |

### Priority C — 設計は揃っている、段階実装

| # | 案件 | ソース | 依存 / 注意点 |
|---|------|------|--------------|
| C-1 | Timeline Right Pane Keyframe Edit Refinement | `MILESTONE_TIMELINE_RIGHT_PANE_KEYFRAME_EDIT_REFINEMENT_2026-05-23.md` | T-Painter 正規経路のみ触る |
| C-2 | Timeline Ripple Edit Phase 1 | `MILESTONE_TIMELINE_RIPPLE_EDIT_PHASE1_EXECUTION_2026-06-04.md` | T-Painter 経路・undo 単位に留意 |
| C-3 | Visual Density Monitor | `MILESTONE_VISUAL_DENSITY_MONITOR_2026-06-03.md` | 既存 debug/overlay surface に乗せる |
| C-4 | Mask Keyframe Foundation | `MILESTONE_MASK_KEYFRAME_FOUNDATION_2026-05-10.md` | M-UI-7 と分離して先に scalar だけ |
| C-5 | Render Preflight Phase 2（= Problem View 連動） | `MILESTONE_RENDER_PREFLIGHT_2026-06-02.md` | A-1/A-2 と語彙を揃える |
| C-6 | Group Layer Mask 接続 | `COMPOSITION_EDITOR_GAP_ANALYSIS_2026-06-03.md` §3.b | TextureManager pooling まで含めると重いので接続だけ先に |

### Priority D — 着手可能だが依存あり・後段

| # | 案件 | メモ |
|---|------|------|
| D-1 | Shape Operators 6種（TrimPaths / Repeater / MergePaths / OffsetPaths / PuckerBloat / Twist） | interface のみ。コアな 1〜2 個を薄く実装してから残り展開 |
| D-2 | M-IR-8 ImmediateContext Boundary / De-direct | Diligent 境界のため最小変更で `RenderCommandBuffer` 経由に寄せる |
| D-3 | M-WKR-1 Background Utility Worker Process | job contract → in-process runtime の順 |
| D-4 | M-RE-1 External Renderer Design | 設計段階。実装は後段 |

### Priority E — 再着手しない（実装済み or 構造的問題のため整理優先）

`MILESTONE_ROADMAP_CURRENT.md` の Priority C と一致:
- Motion Path overlay / Effect Hitbox View / Render Debounce / Pen Tool / Text box resize / Audio Waveform（実装済み扱い）
- Render Farm / Puppet / Auto-Orient / Motion Sketch / Roving Keyframes / Software Render Pipeline（依存重）
- 既存の「DRAFT な新規効果」群（Luminescence / Quantum Glitch / Glow Variants / Aperture / 等）はバックログには載せず、look-dev 用メモとして維持

---

## 3. 直近 1 セッションで進めるときの安全順

1. `Artifact` フルビルド（worklog の Resume Checklist step 1）
2. コンパイルエラー最小修正
3. `Problem View` / `Health Dashboard` / `App Debugger` / `Render preflight` の表示確認
4. B-1（Speed Graph sample）から着手（範囲が局所、効果大）
5. 残りは worklog `2026-06-04` のフォーマットで再開ログを残す

---

## 4. 注意点 / 禁止事項（AGENTS.md 準拠）

- QtCSS / `QColorDialog` / 新規 signal-slot / `QImage` の追加は禁止
- サブモジュール（`Artifact` / `ArtifactCore` / `ArtifactWidgets`）への直接編集・push は明示依頼時のみ
- ビルド・テストは明示依頼時のみ
- C++20 modules の `.ixx` purview 側に `#include` を置かない
- 既存の `ArtifactTimelineTrackPainterView` を正規名として扱う

---

## 5. 更新履歴

- 2026-06-15: 初版作成。再開用 worklog `2026-06-04` の `Resume Checklist` を起点に再構成。
