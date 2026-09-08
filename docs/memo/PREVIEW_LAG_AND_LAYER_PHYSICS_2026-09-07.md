# プレビューもたつき + レイヤー物理 Gap メモ

**最終更新:** 2026-09-07（String追加調査・Fix1-3実装を追記）

## 0. Stringアロケーション追加調査（2026-09-07）

- `ZeroString`（`ArtifactCore/include/Core/ArtifactString.ixx:453`、SSO 23byte）は存在するがASCII寄り・Qt相互運用なし。Qt境界では使えず、内部キーのみに限定。
- 実装済み（ビルド未確認）：
  - Fix1: namespace/SHA毎frame再計算の排除＋`id().toString()`保持＋namespace毎mkpathスキップ（`ArtifactPlaybackService.cppm`）。
  - Fix2: `drawGlyphText`/`drawGlyphs`の重複code point解決排除（`PrimitiveRenderer2D.cppm`）。unique表＋2pass目はconst参照acquire。
  - Fix3: RAM fallback summaryの組立て前比較（`ArtifactCompositionRenderWidget.cppm`）、OVR定数タグ化＋mask文簡素化（`ArtifactCompositionRenderOverlay.cppm`）。
## 5. 支配項の再検証（2026-09-07）

- readbackはgated済み：`shouldCaptureRamPreview`（`ArtifactCompositionRenderController.cppm:38674-38828`）が`!ready && pendingBuild`等を要求。定常再生では発火しない。無条件という前回仮説は誤り。
- uploadはcontent-cache済み：`PrimitiveRenderer2D.cppm:63-82 computeImageContentKey`（4KB fingerprint）＋`m_spriteTexCache`。静止画の再uploadなし。前回仮説は誤り。
- 残る真の支配項は毎tickフル評価（`goToFrame`→physics/collision/modulation→8pass→Present）。static-skipは設計済みだが要ビルド検証のため未着手。
- 安全微修正として`PrimitiveRenderer2D.cppm`のdraw系qCDebug 22箇所を`isDebugEnabled()`ゲート化（有効時挙動不変）。

## 6. 毎tick評価の等価early-out（2026-09-07、ビルド未確認）

- `Physics2D::hasBodies()` 追加（`Physics2D.ixx/.cppm`）。`getBodies()` 全コピー回避の軽量空判定。
- `updateCompositionRigidWorld`：空worldのstep省略。空worldはcontact不可→`contactEvents`は空のまま→`step`内のclearもno-opで等価。catch-up最大8×substep分を削減。
- `evaluateJointConstraints`：enable/disableループ後に joints空＋bodies空＋signatures空ならreturn。以降の全分岐はbody/joint必須（cleanup対象なし、proxyはjoint経路由来のみ、per-ownerは除去分岐のno-op群のみ）。stale signatureはガード不成立→全経路で自己修復。
- `evaluateJointBreaks`：joints空ならreturn（ループ本体は`hasLayerJoint`必須）。
- 見送り：`evaluateLayerCollisionPairs`（`isActiveAt`がframe依存で等価ガード不可）、`evaluateRigidBodyContacts`（per-stepカウンタリセットがあるため不可）、layer `goToFrame`（`processAtFrame`はstatefulで等価省略不可）。
- 注意：frameChangedラムダはDirectConnectionでworker thread実行。既存からpointer/stateHashに触れており、今回のキャッシュ読取も同一ハザード分類（新規分類の追加なし）。不整合は既存のmismatch-dropで吸収。

## 1. 平面レイヤーでも重い理由（確定分）

プレビュー経路：`ArtifactPlaybackEngine.cppm:356 runPlaybackLoop` → `ArtifactPlaybackService.cppm:728 publishFrame / 928 goToFrame` → `ArtifactAbstractComposition.cppm:1205,1423 setFramePosition` → `ArtifactPreviewCompositionPipeline.cppm:549 render` → `ArtifactIRenderer.cppm:3270 flush / 3768 present` → `ArtifactDiligentEngineRenderWindow.cppm:1664 Present`

- ① worker→GUI backpressure が frame clock：`ArtifactPlaybackEngine.cppm:643-646` `BlockingQueuedConnection`（skipMode None時）。GUI/合成/render の超過がそのまま `sleep_until:423` ループを止める。
- ② 静止画でも毎tickフル `goToFrame`：`ArtifactPlaybackService.cppm:928` → physics catch-up `ArtifactAbstractComposition.cppm:1435,1461`。dirty-skipなし。
- ③ 再生中はRAM cacheを使わない既定値：`ramPreviewPlaybackFallbackWhilePlaying_=false`（`ArtifactPlaybackService.cppm:194`）。`ArtifactCompositionRenderWidget.cppm:523` / `ArtifactCompositionRenderController.cppm:34137` で `playing-policy-disabled` になり常時フル合成。
- ④ gradient平面が毎frame CPU `QImage` 生成：`ArtifactSolidImageLayer.cppm:686-697` `makeSolidGradientImage` inside `draw()`。`currentFillImage():713` キャッシュ不使用。solid色は `drawSolidRectTransformed:702` で安い。→ 今回修正（下記）。
- ⑤ `QImage→GPU` upload：`ArtifactIRenderer.cppm:4298`、`ArtifactCompositionViewDrawing.cppm:1943,1954`。
- ⑥ `renderMutex_` 直列化：`ArtifactCompositionRenderWidget.cppm:474`（render丸ごと保持、resize/pan/zoomと競合 `:415,:684,:729`）。
- ⑦ per-frame log/string：`ArtifactCompositionRenderWidget.cppm:536-545`（毎frame QString構築、出力のみ差分抑制）、`ArtifactCompositionRenderController.cppm:34166`（120frame毎+理由変化時 — 許容範囲）、`ArtifactPlaybackEngine.cppm:555`（120毎 — 許容範囲）。
- ⑧ 1rectでも固定8pass + readback + vsync：`buildFrameRenderPassPlan:10258`、`readback...:3723,3737`、`Present:1664`。
- ⑨ msタイマー量子化：`ArtifactCompositionPlaybackController.cppm:120-124` `int(1000/fps)` 切捨て（60fps→16ms、66.6fps相当に加速）。→ 今回 `lround` に修正（下記）。
- ⑩ 広域invalidate：`ArtifactPlaybackService.cppm:856` `LayerChangedEvent` → RAM+GPU cache破棄。

## 2. レイヤー物理の残Gap

核：`Physics2D.ixx:69-415`（Box2D v3ラッパ）+ `ArtifactAbstractLayer.cppm:4424-4745` + `ArtifactAbstractComposition.cppm:1422-1839`。

- joint不足（weld/wheel/pulley/chainなし）：`Physics2D.ixx:376-387`、`ArtifactAbstractComposition.cppm:1772-1783`。UIも0-5のみ `ArtifactAbstractLayer.cppm:9204-9205`。
- マテリアル固定：`density=1.0,friction=0.3` 直書き `ArtifactAbstractLayer.cppm:4608-4650`。per-layer friction/density UIなし。
- `setFixedRotation(false)` 強制 `ArtifactAbstractLayer.cppm:4658`。bullet/CCD・sleep閾値のper-layer露出なし（LODのみ `PhysicsSystem.cppm:755-769`）。
- 静的コライダーは床1枚のみ（`setStaticFloor` が旧床破棄 `Physics2D.cppm:216-219`）。壁/天井/静的polygonなし。
- concave/複合shape不可（凸hull+8頂点 `ArtifactAbstractLayer.cppm:4616-4630`）。
- rigid timelineキャッシュなし（seekで `resetRigidBodySimulation` `ArtifactAbstractComposition.cppm:1438-1439`）。
- 3D除外・1layer1joint制限（`ArtifactAbstractComposition.cppm:1630,1685`、`Physics2D.cppm:456-474`）。
- 旧spring `ArtifactLayerPhysics.ixx:188-229` とBox2Dの二重経路併存。

## 3. 今回修正（簡単・安全なものだけ）

1. `ArtifactSolidImageLayer.cppm:685-698` — gradient分岐で毎frame `makeSolidGradientImage` していたのを `currentFillImage()` 再利用に変更。不透明度は `drawSpriteTransformed(..., opacity()*weight)` 側で乗算（source-override経路 `666-675` と同一方式）。CPU生成+uploadの毎frame実行を撲滅。
2. `ArtifactCompositionPlaybackController.cppm:120-124` — `int(1000/fps)` 切捨てを `lround` に変更（60fps 16→17ms）。`PreciseTicker::Duration` が `milliseconds`（`PreciseTicker.ixx:14`）のためμs化は見送り。

## 4. 見送り（メモのみ・要設計判断）

- `BlockingQueuedConnection→Queued` 化（毎frame意味が変わる）。
- `ramPreviewPlaybackFallbackWhilePlaying_` 既定値変更（再生ポリシー変更）。
- `renderMutex_` 分割、8pass固定の早期out、readback/vsync条件化。
- `PreciseTicker` の `microseconds` 化（`.ixx` 変更＋全呼び出し影響）。
- physics の joint/マテリアル/静的障害物/複合shape追加（UI+保存形式に波及）。
- per-frame QString summary（`ArtifactCompositionRenderWidget.cppm:536`）の完全撤去（安いので後回し）。
