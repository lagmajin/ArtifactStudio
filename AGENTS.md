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

`QColorDialog` の新規使用は禁止。色選択 UI は `FloatColorPicker` または既存の承認済みカラー picker を使い、既存箇所を変更する場合も `QColorDialog` を増やさず統一を優先すること。

新規のシグナル＆スロット接続は絶対禁止。特にグローバルなシグナルや中央集権的なイベント配線を導入しないこと。新しい公開シグナル／スロットが本当に必要な場合は設計レビューを経て、既存のイベント経路やサービスを再利用する方針とする。

`QImage` の新規採用は原則禁止。描画・合成・転送の本流では使わず、ホットパスでは `ImageF32x4_RGBA` などの GPU/バッファ寄り表現を優先すること。既存の `QImage` は入出力、互換フォールバック、Qt API との境界に限って維持し、そこ以外では増やさないこと。
変換は暗黙にしない。`QImage` 化、CPU ダウンロード、GPU アップロードは必ず明示関数を通し、API 境界で自動変換しないこと。

### AI がはまりやすい箇所の簡易チェック
- Qt 型は「他のヘッダに入っていそう」でも、使うファイル側で直接 `#include` する。
- モジュールの `.ixx` と `.cppm` は責務を分ける。公開宣言は自己完結、実装は実装ファイルに閉じる。
- `W_OBJECT` を追加・変更したら、対応する `W_OBJECT_IMPL(...)` と `W_SIGNAL(...)` の整合を必ず確認する。
- リンクエラーは「宣言だけ増えて実装がない」「実装はあるがビルド対象外」「wobject 実体不足」の順で疑う。
- `QApplication`、`QRegularExpression`、`QPainterPath`、`QMetaObject` 系は特に include 漏れを疑う。

module purview（`module X;` の後）に `#include` を絶対に追加しないこと。`#include` はグローバルモジュールフラグメント（`module;` と `module X;` の間）にのみ置く。誤って purview 側に置くと、MSVC が TBB / CRT ヘッダのパースに失敗し、`profiling.h` の構文エラーや `iosfwd` の再定義エラーを引き起こす。やむを得ず purview に include が必要な場合は `import <header>;` 形式を用いる。

C++20 modules の再発防止ルール:
- `module X;` の同一ファイル内で `import X;` は禁止。自己 import は Ninja dyndep を壊しやすい。
- `Artifact.Layer.Abstract` のような public layer module では、他モジュール所有の型を前方宣言しない。`ArtifactAbstractComposition` とその `Ptr` / `WeakPtr` alias は `Artifact.Composition.Abstract` 側だけで定義する。
- これらの違反は `check_module_hygiene` ターゲットで機械検査する。

### C++20 modules 循環参照（Circular Dependency）

このプロジェクトは大規模な C++20 modules を使用しており、`.ixx`（インターフェース）と `.cppm`（実装）の両方が Ninja dyndep スキャン対象となるため、**モジュール間の循環参照が発生しやすい**。以下の点に注意すること。

- `.ixx` に不必要な `import` を追加しない。特に、インターフェース上でポインタ／参照としてしか使わない型は、**global module fragment 内で前方宣言**し、module import を避ける（例: `ArtifactIRenderer*` → `namespace Artifact { class ArtifactIRenderer; }`）。
- `export import` で別モジュールを丸ごと再エクスポートすると、そのモジュールの全依存が透過的に伝播して循環の温床になる。必要な型だけを `import` して前方宣言で済ませることを検討する。
- `.ixx` と `.cppm` は同じモジュール名を名乗るため、**両方のファイルで宣言された `import` の和集合**がそのモジュールの依存セットになる。実装 `.cppm` 側の `import` だけで済む依存は `.ixx` 側に書かない。
- 循環が発生した場合、以下の優先順位で解消を試みる:
  1. `.ixx` の不要な `import` を削除／前方宣言に置き換え
  2. `export import` を通常の `import` に変更し、再エクスポートをやめる
  3. 循環の原因となっているクラスを別モジュールに分離（粒度を細かくする）
  4. どうしても解消できない場合は、`.ixx` を廃して従来の `.hpp` + non-module `.cppm` に戻す（最終手段）

### モジュール / Qt 再発防止の簡易ルール

- AI はビルドエラーを見て場当たり的に `import` や `#include` を増やさないこと。
- `.ixx` には宣言成立に必要な最小限の `import` だけを書くこと。実装でしか使わない依存は `.cppm` / `.cpp` に閉じること。
- ポインタ、参照、スマートポインタで保持するだけの型は、まず forward declaration で済ませられないか確認すること。
- 値保持、継承、メンバー呼び出し、delete、signal/slot 接続まで行う型は完全型が必要だと考えること。
- `std::unique_ptr<Impl>` をヘッダで持つ場合、デストラクタはヘッダ inline にせず `.cppm` / `.cpp` 側で定義すること。
- Qt 型は「他ヘッダ経由で見えるはず」と考えず、使うファイル側で直接 `#include` すること。
- 特に `QApplication`、`QStatusBar`、`QPainterPath`、`QRegularExpression`、`QMetaObject`、各種 Event 型は include 漏れを優先的に疑うこと。
- `module X;` 以降に `#include` を追加しないこと。`#include` は global module fragment (`module;`) 側にのみ置くこと。
- 循環参照が疑われる場合、まず `.ixx` の不要な `import` を疑い、実装側へ移せないか確認すること。
- `export import` は依存を広く伝播させるため、エラー回避目的で安易に追加しないこと。

アイコンを追加・差し替えする場合は、既存 `Material` 系の参照を増やさず、`Artifact/App/Icon/Studio/` に収めるオリジナル SVG を優先すること。見た目はソリッド寄り、太めのシルエット、高コントラスト、16px でも読めることを優先し、細い線や装飾過多は避けること。アイコン未設定のメニューアクションは、必要ならこの方針で新規アイコンを補完すること。

`Artifact/App/Icon/Studio/filemenu_*.svg` は承認済みの VS-like File Menu アイコンとして扱い、AI はユーザーが明示的に依頼しない限り編集・置換・再生成しないこと。Composition / Edit など他メニューのアイコンを作る場合は、この File Menu アイコン群の太めのシルエット、ソリッド寄りの形状、高コントラスト、16px 可読性を参照する。

サブモジュール（例: `ArtifactWidgets` / `libs/DiligentEngine` / `third_party/*`）は、ユーザーが明示的に依頼した場合を除き変更・コミット・push しないこと。

サブモジュールに修正が必要な場合は、まず親リポジトリ側で代替可能か確認し、不可なら「fork 運用」または「パッチ運用」を提案してから進めること。

GitLab は指示がない限りバックアップ専用として扱うこと。push は許可するが、削除・force push・履歴改変・リポジトリ再作成は行わない。正規運用の基準は GitHub 側とする。

ビルドやテストは、ユーザーが明示的に依頼した場合のみ実施すること。勝手に重い確認を走らせず、必要なら先に一言確認する。

結果報告はできる限り簡潔にすること。不要な前置きや冗長な説明は避け、変更点・影響・未確認事項を短くまとめる。
