# Milestone: 3D Compositing（コンポジション内ライブ 3D シーン） (2026-07-08)

> 状態: DRAFT（新規・未実装・専用マイルストーン未作成を確認済み）

---

## 1. 概要

コンポジションビューポート上で、インポート済み 3D モデル・カメラ・ライトを「レイヤーとして」配置し、3D 空間で合成（ライブ 3D コンポジット）する機能。Nuke の 3D コンポジット相倖。

## 2. なぜ必要か（AE ライクなモーショングラフィックスとして）

- `MILESTONE_3D_MODEL_IMPORT_AND_CONTENTS_VIEWER_2026-03-29.md` でモデル import は存在するが、それを comp 内でカメラ/レイヤーと 3D 的に合成する経路がない。
- ロゴ 3D・プロダクト 3D・空間演出など、モーショングラフィックスで 3D と 2D の混在合成は必須。
- AE は「ネイティブ 3D レイヤー + カメラ」でこれをカバーしている。

## 3. 参照元ツール

- **Nuke** — 3D ビュー / ScanlineRender / カメラ・ジオメトリ・ライトのコンポジット。
- **After Effects** — 3D レイヤー・カメラ・ライト・レイヤー距離順ソート。

## 4. 現状（ソース確認・2026-07-08）

- grep `Live3D|3D Comp|Scanline|3D コンポジット` → 0 hit。専用マイルストーンなし。
- 関連基盤:
  - `MILESTONE_3D_MODEL_IMPORT_AND_CONTENTS_VIEWER_2026-03-29.md`（import 済み）。
  - `MILESTONE_3D_VIEWPORT_ORBIT_PAN_PREVIEW_MODE_2026-06-07.md`（viewport 操作）。
  - `MILESTONE_LIGHT_LINKING_2026-03-31.md` / `MILESTONE_ENVIRONMENT_MAP_2026-03-28.md`（ライト/IBL 基盤）。
  - Diligent レンダラ（`libs/DiligentEngine`）による 3D 描画パス。

## 5. スコープ（提案 Phase）

- **Phase 1 — 3D レイヤーとしての配置**
  - import モデルを comp レイヤーとして登録し、transform を既存 2D レイヤーと統合。
  - カメラレイヤー追加と viewport からの射影。
- **Phase 2 — 3D 合成パス**
  - Z 深度に基づくレイヤーソート、ライト/IBL の適用、Diligent での 3D→comp 合成。
- **Phase 3 — 深度・マスク連携**
  - 深度チャンネル出力、3D マット、Depth of Field 等の後処理接続。

## 6. リスク / 未確認事項

- 既存 2D レイヤーブレンドパイプライン（`LayerBlendPipeline`）との責務分離が必要。
- パフォーマンス（3D 再評価コスト）の設計。

## 7. 関連文書

- `docs/planned/MILESTONE_3D_MODEL_IMPORT_AND_CONTENTS_VIEWER_2026-03-29.md`
- `docs/planned/MILESTONE_3D_VIEWPORT_ORBIT_PAN_PREVIEW_MODE_2026-06-07.md`
- `docs/planned/MILESTONE_LIGHT_LINKING_2026-03-31.md`
- `docs/planned/MILESTONE_ENVIRONMENT_MAP_2026-03-28.md`
