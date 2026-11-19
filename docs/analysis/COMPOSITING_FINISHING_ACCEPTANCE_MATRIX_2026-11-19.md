# 合成仕上げ 受入れマトリクス — Track Matte / Keyer / Motion Blur（2026-11-19）

**最終更新:** 2026-11-19

**ステータス:** 静的契約は概ね整備 / runtime 受入れ未実施

**目的:** `docs/analysis/REPORT_DCC_GAP_UPDATE_2026-08-15.md` §優先ギャップ 3（「Matte / Keyer / Motion Blur の制作操作を完成させる」）を、実コード・既存契約テスト・R&D 結果に基づく 1 枚の受入表へ落とし込む。項目1（連番）の `IMAGE_SEQUENCE_ACCEPTANCE_MATRIX_2026-11-17.md` と同じ判定凡例を使う。

**前提:** ビルド・テスト・実機はユーザー明示指示まで実行しない。「静的」はコード/テスト存在の確認、「RT-未」は実素材・実アプリ未受入れを意味する。

## 根拠となる正本文書（重複定義を避け参照）

- `docs/technical/MATTE_BLEND_COMPOSITING_ORDER_2026-07-03.md` … 下→上／マット→ブレンドの評価順、34 ブレンドモード
- `docs/technical/BLEND_MASK_COMPOSITION_CONTRACT_2026-05-08.md` … accum/layer フォーマット、straight/premultiplied 境界、FrameDebugSnapshot 診断
- `docs/planned/MILESTONE_TRACK_MATTE_DRAG_LINK_UX_2026-06-01.md` … Alt-drag、cycle 拒否、Undo、dangling reference
- `docs/analysis/KEYING_EFFECTS_RD_2026-10-26.md` … キーイング 39 件契約テストと残ギャップ
- `docs/planned/MILESTONE_PHYSICAL_MOTION_BLUR_2026-06-07.md` … Motion Blur 統合先（旧 03-29 は SUPERSEDED）
- `docs/analysis/INSIGHT_ARCHIVE_2026-09-01.md` 2026-08-25 項 … velocity ターゲット未生成バグの修正
- `docs/analysis/PROFESSIONAL_VS_AUDIENCE_GAP_2026-10-04.md` … 2D velocity/depth AOV 欠如（A-13）
- `docs/analysis/COLOR_CHANNEL_ORDER_CONSISTENCY_AUDIT_2026-10-16.md` … Track Matte の BGRA→論理 RGB 修正

## 判定凡例

- **静的-済**: コード／契約テストで確認済み（本セッションではテスト未実行）
- **静的-経路**: 接続は確認済みだが専用テストなし／ビルド未反映
- **既知ギャップ**: 実装側に残る不具合・未実装（受入れても失敗が確定している）
- **RT-未**: 実素材・実アプリで未受入れ

---

## A. Track Matte / Mask / Blend

| # | ケース | 期待値 | 根拠 | 状態 |
|---|---|---|---|---|
| A-1 | 評価順 | 下→上、レイヤーサーフェス→マット→ブレンドの順 | MATTE_BLEND_COMPOSITING_ORDER §4 | 静的-済 |
| A-2 | MatteType 4 種 | Alpha / Luma / InverseAlpha / InverseLuma がそれぞれ正しい係数で適用 | 同 §2.2 | 静的-済 |
| A-3 | 複数マット参照 | 直列適用（stackMode=0 デフォルト）、順序依存が安定 | 同 §2.2 | 静的-経路／RT-未 |
| A-4 | Alt-drag で matte link | self-reference／cycle 拒否、Undo/Redo、Inspector badge 同期 | MILESTONE_TRACK_MATTE_DRAG_LINK_UX §Update 2026-08-15 | 静的-済／RT-未（実機 drop affordance） |
| A-5 | マット source を削除・移動 | dangling reference がクリア／再解決され fallback 描画になる | 同 §残る確認課題 | 静的-経路／RT-未（全経路） |
| A-6 | straight/premultiplied 境界 | layer 入力は straight、accum は premultiplied 風で一貫 | BLEND_MASK_COMPOSITION_CONTRACT §Render Contract | 静的-経路／RT-未 |
| A-7 | BGRA ingress | マットの色解析だけ論理 RGBA 化、layer への scalar mask は成分順維持 | COLOR_CHANNEL_ORDER_CONSISTENCY_AUDIT 2026-10-16 | 静的-済 |
| A-8 | ブレンド失敗時 | Normal リトライ → 直接スプライト描画のフォールバック | MATTE_BLEND_COMPOSITING_ORDER §2.3 | 静的-済 |
| A-9 | 診断 | FrameDebugSnapshot に render path／maskContract／dispatch 数を記録 | BLEND_MASK_COMPOSITION_CONTRACT §Smoke Harness | 静的-済 |
| A-10 | 実コンポジションで複数 matte + blend 併用 | AE と同じ合成順・複数マット条件で一致 | 8/15 レポート §3 | RT-未 |

## B. Keyer（Chroma / Luma / Difference / IBK）

| # | ケース | 期待値 | 根拠 | 状態 |
|---|---|---|---|---|
| B-1 | 4 キーヤーの基本アルゴリズム | 39 件契約テストが通る（2026-10-27 に 39/39 pass） | KEYING_EFFECTS_RD §R&D conclusions 12 | 静的-済（ビルド済・pass 記録あり） |
| B-2 | RGBA/BGRA、straight/premultiplied | 論理 RGB で等価、partial alpha で有限出力 | 同 §43,51 | 静的-済／RT-未（GPU parity） |
| B-3 | GPU モフォロジ半径 >8 | CPU フォールバックへルーティング | 同 §28,71 | 静的-済（wrapper 選択は未実行確認） |
| B-4 | clean plate 読込／クリア | 空 path で plate をクリアし source を通过、invalid 置換で旧 plate 保持 | 同 §26,100 | 静的-済 |
| B-5 | 実グリーン画面 + 対応 clean plate | 不均一照明・髪の細線・holdout で満足なマット | 同 §R&D conclusions 4 | RT-未（推奨受入セット未作成） |
| B-6 | CPU/GPU ピクセル parity | 同一入力で CPU と GPU 出力が一致 | 同 §5 | RT-未 |
| B-7 | IBK 2 個目／copy ID の保存復元 | 各エントリが別 ID・別設定で復元される | 同 §24,81 | **既知ギャップ**（catalog 未登録、`__copy` 正規化なし） |
| B-8 | clean plate の project packaging | plate がコピー／相対化され、移動後も読める | 同 §27,73 | **既知ギャップ**（`cleanPlatePath` 未収集） |
| B-9 | 出力 alpha メタデータ | Opaque タグ source を key しても Opaque のままにしない | 同 §29,76 | **既知ギャップ**（descriptor/content 不一致） |
| B-10 | 非有限ピクセル入力 | finite 出力または明示拒否 | 同 §35,85 | **既知ギャップ**（未定義） |
| B-11 | Chroma Despill Mode 3 | 公開値 3 が effect まで届く | 同 §50 | **既知ギャップ**（typed setter が 0-2 に clamp） |
| B-12 | IBK Preview Mode 2 | 「Premultiplied RGB」表示が alpha を二重適用しない | 同 §54 | **既知ギャップ**（未決定） |

## C. Motion Blur

| # | ケース | 期待値 | 根拠 | 状態 |
|---|---|---|---|---|
| C-1 | velocity ターゲット生成 | タイムラインの Motion Blur 有効時に velocity が生成され `MotionBlurPass::apply` が走る | INSIGHT 2026-08-25（修正済み） | 静的-経路／RT-未（ビルド後未確認） |
| C-2 | OFF 時 | velocity ターゲットを作らずメモリ増加がない | 同 | RT-未 |
| C-3 | 3D レイヤー移動 | ON でブラーが乗る、OFF で乗らない | 同「次の確認」 | RT-未 |
| C-4 | shutter angle / phase / samples | AppSettings + ApplicationSettingDialog から設定可能 | 同 | 静的-済 |
| C-5 | per-layer MSAA 併用 | Motion Blur 有効時は per-layer MSAA を除外（単一サンプル depth 前提） | 同 | 静的-済 |
| C-6 | レイヤー単位 transform multi-sample | 前後フレーム transform を lerp して多重描画 | MILESTONE_MOTION_BLUR §2026-07-25 監査 | **既知ギャップ**（未完了） |
| C-7 | 2D velocity / 2D depth AOV | 2D レイヤーにも velocity を供給 | 10-04 レポート A-13 | **既知ギャップ**（AOV 11 項目すべて 3D 前提） |
| C-8 | 速度境界・深度遮蔽 | 実フレームで破綻しない | 8/15 レポート §3 | RT-未 |
| C-9 | Render Queue 出力一致 | Preview と Render Queue でモーションブラー結果が一致 | 8/15 レポート §優先ギャップ 3 | RT-未 |

---

## 集約: 何が「まだ足りない」か

1. **評価順・基本 UI・4 キーヤーのアルゴリズム・velocity 生成条件は静的に充足**。旧レポートの「UI も中身も無い」という表現は 2026-08 以降の実装で古くなっている。
2. **真の残課題は 2 層**:
   - **runtime 受入れ**（A-3/5/6/10, B-2/5/6, C-1/2/3/8/9）— ビルド・実素材・実機許可が必要
   - **実装側の既知ギャップ**（B-7〜B-12, C-6/7）— 受入れ以前に直すべき項目。特に B-7/B-8 は保存・移動でユーザーに被害が出る可能性が高く優先度が高い
3. **序盤の費用対効果が高いのは A-4/A-5 の実機確認と C-1/2/3 の ON/OFF 比較**。どちらも新規実装なしで受入れだけ実施できる。

## 実行に必要な条件

- ビルド／テスト実行の明示許可
- 受入素材: 不均一照明のグリーン画面＋clean plate、半透明・髪-edge を含む still、複数 matte を積んだコンポジション、3D レイヤー移動クリップ
- 既存ハーネス: `ArtifactSoftwareRenderTestWidget`（Preview／Render Queue 比較、`KEYING_EFFECTS_RD` §39 件契約テスト）

## 次のアクション（方針案）

1. **既知ギャップ B-7/B-8 の修正**は Artifact 子リポジトリの変更を伴う。AGENTS.md の「子リポジトリは明示依頼なしに変更しない」に従い、着手前にユーザー確認を取る。
2. runtime 受入れはビルド許可後に A→B→C の順で実行し、失敗を source／decode／matte／blend／key／velocity／composite で分類する。
3. 3D lookdev の縦統合（項目3）は、本表の C-8（深度遮蔽）と Material→render pass 契約が交差するため、C の受入れ結果を先に参照する。
