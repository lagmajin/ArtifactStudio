# HieroPlayer 機能ギャップ分析（2026-09-22）

**最終更新:** 2026-09-22

The Foundry HieroPlayer（Nuke ファミリーのデスクトップレビューツール）の公式情報をもとに、
ArtifactStudio とのギャップを整理する。目的は「レビュー・比較・注釈」周辺の機能取り込み候補の特定。

## 参照元（Web）

- Foundry 製品ページ: https://www.foundry.com/products/nuke-family/hiero-player
- 機能比較表: https://learn.foundry.com/hiero/17.0/content/timeline_environment/workflow/feature_comparison.html
- Viewer Tools: https://learn.foundry.com/hiero/content/timeline_environment/usingviewer/viewer_tools.html
- Timeline Playback Tools: https://learn.foundry.com/hiero/17.0/content/timeline_environment/usingviewer/playback_tools.html
- Annotations: https://learn.foundry.com/hiero/content/timeline_environment/annotations/annotations.html
- Comparing Versions: https://learn.foundry.com/hiero/17.1v1/content/timeline_environment/versioning/comparing_versions.html

## HieroPlayer の機能棚卸し（公式ドキュメントより）

### レビュー / 比較
- **A/B バッファ比較**: 同一トラックの2バッファ。B バッファは edit に影響しない独立参照（B 側に点線枠表示）
- **比較モード**: wipe、difference、blend、stack、Onion Skin
- **バージョン比較**: ファイル名規約による version scanning、`V` キーのバージョンセレクタ
- **Clipping 警告**: under（青）/over（赤）exposure の false-color 警告
- **リアルタイムスコープ**: histogram / waveform / vector。ROI 矩形でスコープ範囲を限定可能
- **Viewer 調整**: gain（linear 入力前）、gamma（表示変換後）、saturation。出力に影響しない表示専用
- **Layers / Channels 表示**: motion vectors、depth 等のレイヤー切替、RGB/Alpha/Luma チャンネル
- **Viewer masks**: 16:9、1.85:1 等のアスペクトマスク。Guides（title/action safe、format center）
- **カラーサンプル**: ソースファイルの RGBA 生値バー（表示変換前）
- **See through missing media** / **Obey Alpha** 切替

### 再生 / ナビゲーション
- **JKL**: J=逆、K=停止/再生、L=正。K+J/K+L で1フレーム step、K+ドラッグで jog（回転モーション検知あり）
- **再生モード**: Play All Frames / Skip Frames / Play All Frames Buffering
- **In/Out マーカー**: ドラッグ調整、Ctrl で同時移動。タイムコード欄は `+/-20` 等の数式入力を受理
- **Playhead A/B indicator**: どのトラックを見ているかプレイヘッドにタブ表示
- **再生キャッシュ**: Viewer 下の緑バーで可視化、一時停止/再開ボタン
- **Image Quality / proxy scale**: 1:1〜1:16、Auto（ズーム連動）
- **Viewer ごとの audio latency 補正**、**Broadcast Monitor 出力**

### 注釈 / コラボレーション
- **Annotations**: Viewer 上にブラシ／シェイプ描画 + コメント。対象粒度は フレーム / フレームレンジ / In-Out / クリップ / シーケンス
- Annotations Panel: コメント投稿・返信（Notes）、Markdown 一部対応、編集履歴（Edited 表示）
- タイムスライダー／タイムラインに青マーカーで位置表示、ドラッグで範囲調整
- **書き出し**: Quick Export で焼き込み、Custom Export で PNG/JPEG として /annotations フォルダへ
- **Sync Review**（共同レビューセッション）対応
- タグ付け（カスタムメタデータ）によるフィルタ、**Version shots / timeline snapshot**、dailies playlist、shot manager 連携

## ArtifactStudio 側の既存資産（コード・ドキュメントで確認済み）

| 領域 | 既存実装 | 根拠 |
| --- | --- | --- |
| A/B 比較（Contents Viewer） | compare page、source A/B、wipe / split / difference、swap、比較 shortcut、設定保存 | `MILESTONE_REVIEW_COMPARE_ANNOTATION_2026-03-28.md` 監査（2026-08-15） |
| JKL シャトル | JKL 文法、連打倍速（JJ=2x…）、K+J/K+L スロー（±0.5x） | `MILESTONE_PLAYBACK_CONTROL_DESIGN_AUDIT_2026-07-04.md` |
| RAM preview / キャッシュ可視化 | ScrubBar の cache 表示・clear、PlaybackClock の負方向再生・loop | 同上 |
| オニオンスキン | ペイントレイヤー向け `setShowOnionSkin` 等 | `ArtifactCompositionRenderController` |
| チャンネル分離表示 | Color/Alpha/R/G/B/Depth/Normal/Velocity/UV 等 | `ViewportChannelDisplayMode` |
| In/Out・work area | `ArtifactWorkAreaControlWidget`、loop/in/out は Playback Service 経由 | AGENTS.md Transport 規約 |
| レビュー系の計画書 | Review Workspace（M-FE-7-1〜7-6）、Compare/Annotation（Phase 1〜4） | `docs/planned/MILESTONE_REVIEW_WORKSPACE_2026-04-03.md`（Phase 1 のみ部分実装、残り pending） |

## ギャップ一覧（HieroPlayer 基準 → ArtifactStudio）

### 🔴 P0: 静止画にも効く「見る精度」系（開発優先方針と整合）
| # | HieroPlayer 機能 | ArtifactStudio 現状 | 提案 |
|---|---|---|---|
| 1 | gain / gamma / saturation の表示専用調整 | 未実装（M-VP-DCC-1 の「Viewer exposure controls」P1 と同一課題） | M-VP-DCC-1 に統合。表示変換後段の GPU pass として実装 |
| 2 | Clipping 警告（over/under exposure の false color） | 未実装 | 1 と同じ表示 pass で警告色 overlay に |
| 3 | スコープ（histogram / waveform / vector）+ ROI | 未実装 | ヒストグラムから開始。GPU reduce または小さな readback + 専用描画。QImage 禁止に注意 |
| 4 | カラーサンプル（生 RGBA 値の常時バー） | 未実装 | カーソル下 1px readback で実現可能 |
| 5 | OCIO ベースの表示色空間切替 | OCIO 基盤は計画内（M-FE-7-2 の setLUT/setOCIOConfig は未実装） | 表示変換を renderer final output 段に固定 |
| 6 | アスペクトマスク（16:9 等） | safe margin は既存、マスクは未実装 | 既存 safe-area overlay の派生として軽量に追加可能 |

### 🟡 P1: 比較・レビューの強化（既存 compare 基盤の延長）
| # | HieroPlayer 機能 | ArtifactStudio 現状 | 提案 |
|---|---|---|---|
| 7 | 独立 B バッファ（edit に影響しない参照側） | Contents Viewer の A/B はパス切替型 | M-FE-7-3 の CompareSession 設計に B 独立概念を取り込む |
| 8 | バージョンスキャン（v001/v002 → `V` で切替） | 未実装 | Asset System の metadata/status 基盤へ version token 解釈を追加 |
| 9 | Annotations（描画 + コメント + パネル + タイムラインマーカー + PNG 焼き込み） | 計画のみ（M-FE-7-5 未着手） | composition 座標系保存・Pen/Arrow/Rect/Text・export の順で実装 |
| 10 | approved / needs work / rejected のレビュー状態 | 未実装（Phase 2 pending） | review record 契約を先に固定（既存文書が同旨を指摘済み） |
| 11 | Playhead A/B indicator / See through missing media | 未実装 | 比較実装の後で検討 |

### ⚪ P2: 再生・運用系（動画依存のため優先方針上は後回し寄り）
| # | HieroPlayer 機能 | ArtifactStudio 現状 | 提案 |
|---|---|---|---|
| 12 | Skip Frames / Buffering の再生モード選択 | RAM preview 範囲指定は ⚠️、Skip Frames は ❌ | 動画対応再開時に Playback Service API として追加 |
| 13 | K+ドラッグ jog（回転検知）、Jog Wheel / Shuttle Slider UI | JKL キーは済、ドラッグ jog・ダイヤルは ❌ | Playback Control 設計監査の残件に既出。ShortcutBindings 登録必須 |
| 14 | Viewer ごとの audio latency 補正 | 未実装 | M-AU 系と連携。動画優先度復帰後 |
| 15 | Broadcast Monitor（SDI/NDI 外部出力） | 未実装 | 大規模。当面対象外 |
| 16 | タグ/メタデータ フィルタ、dailies playlist、shot manager | Asset metadata 基盤はあるがタグ/プレイリスト/ショット管理は未実装 | M-FE-7-4 Shot Management と Asset System の接続として設計 |
| 17 | Sync Review（共同レビュー） | 未実装 | ネットワーク基盤が必要。長期候補 |

## 推奨アクション

1. **短期（静止画優先方針と整合）**: P0 の 1〜6 を「Viewer Inspection Controls」として束ね、M-VP-DCC-1 に統合して重複計画を避ける → **決定済み（2026-09-22）**: P1-5（既存 exposure controls）を核に、P1-10 Clipping 警告 / P1-11 スコープ+ROI / P1-12 カラーサンプルバー / P1-13 OCIO 表示色空間切替 / P1-14 アスペクトマスク として `docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md` へ統合した
2. **中期**: P1 の 7〜10 は `MILESTONE_REVIEW_WORKSPACE_2026-04-03.md` / `MILESTONE_REVIEW_COMPARE_ANNOTATION_2026-03-28.md` の未完成 Phase を HieroPlayer 仕様で具体化する形が効率的（新規マイルストーン乱立を避け既存文書を更新）
3. **長期**: P2 は動画対応の優先度が戻った段階で再評価（開発優先方針 2026-07-27 に従う）

## 注意事項

- HieroPlayer の UI 再現ではなく機能概念の取り込みに留めること（商標・デザインの模倣は避ける）
- AGENTS.md 制約: 新規 signal/slot 禁止・ShortcutBindings 必須・QImage/QPainter 新規禁止・ホットパスのアロケーション規則をスコープ/警告表示の実装にも適用すること
- 本分析は Web 上の公式ドキュメントと静的コード検索に基づく。実装状態の詳細は各マイルストーン文書の監査記録を正とする

## 関連ドキュメント

- `docs/planned/MILESTONE_REVIEW_WORKSPACE_2026-04-03.md`
- `docs/planned/MILESTONE_REVIEW_COMPARE_ANNOTATION_2026-03-28.md`
- `docs/planned/MILESTONE_PLAYBACK_CONTROL_DESIGN_AUDIT_2026-07-04.md`
- `docs/planned/MILESTONE_VIEWPORT_DCC_PARITY_2026-09-22.md`
- `docs/analysis/VIEWPORT_DCC_PARITY_C4D_HOUDINI_MAYA_2026-09-22.md`
