# コンポジションエディタ 不足機能分析 — 2026-06-03

**分析手法**: ドキュメント調査（COMPOSITION_EDITOR_CONTRACT, FEATURE_AUDIT_MOTION_DESIGN, MILESTONE類）＋  
実ソースコード検証（ArtifactCompositionRenderController.cppm 8476行, ArtifactCompositionEditor.cppm 4580行, ArtifactCurveEditorWidget.cppm 818行, ArtifactTextGizmo.cppm 210行, TransformGizmo.cppm, ShapeGroup.cppm 等）

---

## 1. ドキュメントと実際のコードの差分（実際は実装されていたもの）

以下の機能はドキュメント上「未実装／未完成」とされていたが、実コードではすでに動作している。

| 機能 | ドキュメント | 実際のコード | エビデンス（ソース行） |
|------|------------|------------|----------------------|
| **Motion Path overlay** | コメントアウト中 | ⭕ 実装済み | `motionPathCache_` にキャッシュ→描画。`buildMotionPathSamples()` ＋ キーフレームヒットテスト＋ドラッグ編集完備 |
| **Effect Hitbox View** | 提案段階 | ⭕ 実装済み | `drawEffectHitboxOverlay()` がレイヤーbounds / mask範囲 / matte範囲を色分け。`showEffectHitboxOverlay_` toggle完備 |
| **Render Debounce** | 未実装（仮説） | ⭕ 実装済み | `renderDebounceTimer_` (16ms single-shot) + `onRenderDebounceTimeout()` 存在。プロパティ編集中のGPU飽和対策として動作 |
| **Pen Tool / Mask編集** | M-CE-MASK-2 進行中 | ⭕ 実装済み | 頂点作成・ドラッグ移動・Bezier handle編集・セグメント上ヒットテストまで実装 |
| **Text box resize** | Phase 1 未完了 | ⭕ 実装済み | `TransformGizmo.cppm` にtext layer専用paragraph box resize。undoに`maxWidth`/`boxHeight`含む。side/corner drag時にopposite edge維持 |
| **Density Heatmap overlay** | ドキュメントなし | ⭕ 実装済み | `showDensityHeatmapOverlay_` toggle + `densityHeatmapColor()` 関数で色分け表示 |

---

## 2. 接続漏れ・未配線（コードはあるが使われていない）

### a. TextGizmo — 完全に未接続
- `ArtifactTextGizmo.cppm` (210行) は完全なクラス定義（`draw()` / `hitTest()` / `handleMousePress/Move/Release` / `cursorShapeForViewportPos`）
- **しかしどのコントローラからもimportされていない**
- 実際のテキスト編集は `TransformGizmo.cppm` 側のparagraph box resizeで代替。TextGizmo独自のrange selectorハンドルは無効

### b. Speed Graph — `sampleSpeedGraph()` は実装済み、接続確認が主題
- `ArtifactCurveEditorWidget.cppm:35` に `sampleSpeedGraph()` の実装あり
- `ArtifactCurveEditorWidget.cppm:686` の `setSpeedGraph(...)` から `sampleSpeedGraph(...)` を呼んで `CurveTrack` に変換している
- `ArtifactTimelineWidget.cppm:3902` の `showSpeedGraph()` から graph mode を切り替えている
- Value Graph（値グラフ）の描画・編集ロジックも動作する
- したがって、ここは「未実装」ではなく、**表示切替とデータ流入の整合確認**が主題

### c. Shape Operators — インターフェースのみ、実装ゼロ
- `ShapeGroup.cppm` に `addOperator()` / `operatorAt()` / `removeOperatorAt()` のインターフェースはある
- **以下の ShapeOperator 実装クラスが1つも存在しない**：
  - `TrimPaths`（start/end/offset パスアニメーション）
  - `Repeater`（コピー＋トランスフォームオフセット）
  - `MergePaths`（パスブール演算: Merge/Add/Subtract/Intersect/Exclude）
  - `OffsetPaths`（パス拡大縮小）
  - `Pucker&Bloat`（パス膨張収縮）
  - `Twist`（パスねじれ変形）

### d. Text Animator — Timeline keyframe 未配線
- `ArtifactTextLayer` に `animators_` ベクター、追加/削除/個数管理、JSON保存復元、property path接続は完了
- Inspectorからのproperty編集は動作する
- **タイムラインのキーフレームトラックとしてアニメータープロパティを露出するコードがない**
- Phase 4（`MILESTONE_TEXT_ANIMATOR_INTEGRATION_2026-04-27.md`）の未完了部分

---

## 3. 実装が途中・不安定なもの

### a. Mask/Roto 編集の残課題
Pen Tool自体は動く。未完成な操作：

| 操作 | 状態 |
|------|------|
| Segment上へのvertex insert（線分クリック→途中に頂点追加） | ✗ 未実装 |
| Smooth / Corner 切替 | ✗ 未実装 |
| Open / Closed path 切替 | ✗ 未実装 |
| Selected handle / tangent mode の明示UI表示 | ✗ 未実装 |
| Handle drag の undo 粒度 | △ 改善余地あり |
| Mask 編集中の Inspector 表示（count / path count / enabled state） | ✗ 未実装（M-CE-MASK-4） |

### b. Group Layer — mask未完了
- Offscreen RT → blit のOption A実装は存在
- 以下が未完了：
  - **グループレベルのマスク適用**（`drawMaskedTextureLocal` 未接続）
  - **TextureManager pooling**（per-group簡易キャッシュのみ。cross-group再利用なし）
  - Unit test / visual test 未作成

---

## 4. まったくコードがない機能（ソースコード検索で0ヒット）

| 機能 | カテゴリ | 備考 |
|------|---------|------|
| **Motion Sketch** | モーション | マウスパス→キーフレーム一括生成がない |
| **Auto-Orient** | モーション | パスに沿う向き自動補正がない |
| **Roving Keyframes** | キーフレーム | キーフレームのtime-addressableな連続化がない |
| **Source Text Keyframe** | テキスト | 時間変化するテキスト内容（`setSourceText`）がない。編集はインラインのみ |
| **Audio Scrubbing** | プレビュー | Scrub時のリアルタイム音声previewがない。PlaybackEngineへの導線のみ |
| **Layer Styles** | レイヤー品質 | DropShadowのみ。Bevel / InnerShadow / Stroke / Satin がない |
| **Track Matte drag UX** | 編集 | `LayerMatteReference`参照のUI操作がない |
| **RAM Preview queue UI** | プレビュー | スタンドアロンqueue widgetがない |
| **A/B Wipe ビューア** | 比較 | A/B chipはあるがwipe比較UIがない |
| **Color Profile Embed** | カラー | ICC読み書きexportロジックがない |
| **Render Farm orchestration** | レンダリング | RPC低レイヤーのみ。master/slaveスケジューラがない |
| **Render Queue checkpoint/retry** | レンダリング | チェックポイント保存/リトライロジックがない |

---

## 5. Puppet Tool — プレースホルダーのみ

- `ArtifactToolBar.cppm:45` にtoolbarボタンのicon登録はある
- Mesh deformer / pin / bone / warp のコア実装コードは存在しない
- ドキュメントでも「toolbar placeholderのみ」と確認済み

---

## 6. 総合不足マップ

```
┌─────────────────────────────────────────────────────────────┐
│                   不足機能 一覧マップ                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  P0（コードはあるが使えていない）                            │
│  ├─ TextGizmo 未接続（210行の実装が無視されている）          │
│  ├─ Speed Graph は実装済み、接続確認が必要                  │
│  └─ Segment上 vertex insert 未実装                          │
│                                                             │
│  P1（インターフェースはあるが実装がない）                    │
│  ├─ Shape Operators 6種 すべて未実装                         │
│  ├─ Text Animator timeline keyframe 未配線                  │
│  ├─ Group Layer mask 未接続                                 │
│  └─ Mask Inspector 表示（count/path/enabled）未実装          │
│                                                             │
│  P2（そもそもコードがない）                                  │
│  ├─ Motion Sketch / Auto-Orient / Roving Keyframes          │
│  ├─ Source Text Keyframe                                    │
│  ├─ Audio Scrubbing                                         │
│  ├─ Layer Styles 拡充                                       │
│  ├─ Track Matte drag UX                                     │
│  └─ Puppet Tool 実装                                        │
│                                                             │
│  P3（インフラ系）                                            │
│  ├─ RAM Preview queue UI                                    │
│  ├─ A/B Wipe ビューア                                       │
│  ├─ Color Profile Embed                                     │
│  ├─ Render Farm / Queue checkpoint                          │
│  └─ Scopes（Waveform不足）                                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 7. 補足：既知の不具合（修正済み含む）

| 不具合 | 状態 | 対応日 |
|--------|------|--------|
| GPU Texture Cache pathのBGRAチャンネル順問題 | ✅ 修正済み | 2026-05-10 |
| LOD downscaleとマスクラスタライゼーションのスケール不整合 | ✅ 修正済み | 2026-05-10 |
| ゴーストオーバーレイ（render-key cache再利用 + gizmo overlay競合） | ✅ 修正済み | 2026-05-08 |
| `ImageF32x4_RGBA` の内部BGRA順問題 | ⚠️ 注意事項 | 新規floatデータ扱う箇所で再発可能性あり |

---

*分析日: 2026-06-03*
*調査対象: ArtifactStudio 親リポジトリ (Artifact/, ArtifactCore/)*
