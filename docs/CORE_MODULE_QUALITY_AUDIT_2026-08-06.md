**最終更新:** 2026-08-06

# コアモジュール品質調査メモ

**結論: プロダクション級ではない。** 機能の幅は商用DCC並みだが、基盤の堅牢性はプロトタイプ〜アルファ相当。特に**保存したプロジェクトが復元できない**という致命的欠陥がある。
（静的解析のみ。AGENTS.md に従いビルド・テストは未実行）

---

## 規模

| | ファイル | LOC |
|---|---|---|
| ArtifactCore | 1,301 | 217,984 |
| Artifact（実質のコアドメイン） | 1,034 | 428,809 |
| ArtifactWidgets | 112 | 9,781 |
| **テスト** | **7** | **657（0.1%）** |

---

## P0 — 出荷不可レベル

### 1. プロジェクト保存→読込のラウンドトリップが壊れている（実測確認済み）

`ArtifactProjectImporter.cppm:385` が呼ぶファクトリが**常に nullptr を返す**:

```cpp
// ArtifactAbstractLayer.cppm:4453  (static、仮想でない)
ArtifactAbstractLayerPtr ArtifactAbstractLayer::fromJson(const QJsonObject &obj) {
  Q_UNUSED(obj);
  return ArtifactAbstractLayerPtr();   // ← 常に空
}
```
→ **`.artifact` / `.json` を開くと全コンポジションのレイヤー数が 0**。実働ファクトリ `createArtifactLayerFromJson` は IPC 経路からしか呼ばれていない。

さらに `Importer.cppm:369` は `ArtifactCompositionInitParams params;` をデフォルト構築するだけで、保存済みの `width/height/frameRate/frameRange/backgroundColor` を一切読まない → **常に 1920×1080 / 30fps / 100 フレーム**に化ける。`guideSet` は書くだけで読む箇所が存在しない。レイヤー ID も復元されず、親子関係・トラックマットが全滅する。

### 2. 保存が無言で拒否される

`ArtifactProjectManager.cppm:841` — Error 診断が1つでもあると保存中止。`ProjectDiagnostic.cppm:38` により**オフライン素材1件で `DiagnosticSeverity::Error`**。UI通知はなく `qWarning` のみ（`ArtifactFileMenu.cppm:364`）。ユーザーからは「保存ボタンが無反応」に見える。
加えて `setDirty(false)` は全コードベースに 0 件（`setDirty(true)` は 19 件）→ 保存後も永久に未保存扱い。

### 3. テストとCIが実質存在しない

- テスト 657 行 / 本番 656,574 行 = **0.1%**。内容は Levenshtein距離・パス取得・文字列変換・スクリプトエンジン初期化のみ。
- Composition / Layer / Render / Image / Memory / Serialization のテストは**ゼロ**。
- CI は `.github/workflows/` に i18n チェックと serialization 監査の Python スクリプト2本のみ。**ビルドCIもテストCIもない。**

### 4. 所有権モデルの体系的破綻

- 生 `Impl*` メンバ **153ファイル** vs `unique_ptr<Impl>` 38ファイル。
- うち **114ファイルが `= delete` を一切持たない** → コピーすれば二重解放。例: `LayerStrip.ixx:41` は `Impl* impl_` + デストラクタありでコピー未禁止。
- **リーク確定 14クラス**（デストラクタで `delete impl_` していない）:
  `CLAPHost, ColorSpace, CompressQuality, ImageYUV420, RendererQueue, RenderStatics, BroadcastColorsCS, ChromaKeyCS, EmbossCS, GlowCS, LeaveColorCS, NegateCS, ScatterCS, SimpleChokerCS`
  DirectCompute 系は Impl 内に GPU リソース（`RefCntAutoPtr<ITexture>` 等）を抱えるため、実効的に**エフェクト適用ごとに VRAM リーク**。

### 5. スレッド安全性の前提が破綻している

- `ArtifactRenderQueueService.cppm:6073` で `std::vector<std::thread> renderWorkers` によるマルチスレッドレンダリング。
- 一方 `ArtifactAbstractComposition`（4,837行）は **mutex / atomic ゼロ**。`ArtifactAbstractLayer`（9,841行）は mutex 2個（composition ポインタとプロパティキャッシュのみ）で、transform / effects / masks / dirty flags / variants は無防備。
- スレッド所有権を記述したコメントも見当たらない。

### 6. God Class / 巨大ファイル

| ファイル | 行数 |
|---|---|
| `ArtifactCompositionRenderController.cppm` | **35,127** |
| `ArtifactCompositionEditor.cppm` | 13,045 |
| `ArtifactAbstractLayer.cppm` | 9,841 |
| `ArtifactRenderQueueService.cppm` | 7,709 |

3,000行超が 20ファイル、1,000行超が 70ファイル。`ArtifactAbstractLayer` は基底クラス1つに **public メソッド約250個**（transform / timeline / 剛体・軟体物理 / 破砕 / audio / effects / modifiers / components / script / mask / matte / variant / thumbnail / LOD）。型安全性も放棄している:

```cpp
virtual void setComposition(void *comp);   // ArtifactAbstractLayer.ixx:334
void *composition() const;
void *QueryInterface(const std::type_index &ti);
```

---

## P1 — 中程度

- **エラー処理が場当たり的**: 217k LOC に対し `try` 50 / `catch` 62 / `assert` わずか **6**。`std::expected` は 0件、`std::optional` 27件。`catch(...)` 30件中 9件が空ブロック。失敗通知はほぼ `qDebug/qWarning`（74ファイル）で、呼び出し側は握りつぶす。
- **スタブの残存**: `FloatImage.cppm` は全メソッド空（`width()` は常に 0）。`RenderWorker::run()` 空。`Blend2D_CS::Process()` 空。`ChromaKeyCS::loadShaderBinaryFile()` 空でシェーダが載らない。
- **BVH が未接続**: `Render/BVH.ixx` に実装があるのに `Hittable.ixx:57` の `HittableList::hit` は線形 O(n) 走査のまま。
- **`MultiChannelImage` のコピーが浅い**（`MultiChannelImage.ixx:78`）— `SharedPtr<VideoChannel>` をメンバワイズコピーするためバッファを共有。ドキュメント記載なし。
- **ビルド設定が緩い**: `/WX-` で警告をエラー化しない、`/W4` 指定なし（既定 /W3）、clang-tidy / sanitizer / 静的解析なし。
- **CMake が脆い**: `GLOB_RECURSE` + 個別ファイル除外リスト25件 + force-module リスト40件のハードコード。再現性とスケールに不安。`ArtifactCore` が `../Artifact/include` を PUBLIC include している（レイヤリング違反）。
- **ヘッダ肥大**: 125個の `.ixx` が `<regex> <random> <any> <filesystem>` 等を丸ごとコピペした GMF を持つ（`LayerStrip.ixx` は1クラスのために32ヘッダ）。ビルド時間に直撃。
- **ドキュメント**: 792 の公開ヘッダのうち Doxygen コメントがあるのは 112（14%）。

---

## 良い点（正当に評価できる部分）

- **C++20 modules をこの規模で成立させている**こと自体が技術的達成。モジュール衛生の機械検査（`check_module_hygiene`）も用意されている。
- 保存は `QSaveFile`（一時ファイル+rename）でアトミック、3世代バックアップ、20世代オートセーブ、クラッシュ復旧あり。読み込み側に DoS 上限（レイヤー10万件等）も設定済み。
- `SchemaMigration.ixx` に BFS ベースのスキーマ移行機構が**実装済み**（ただし未接続）。
- GPU リソースは Diligent `RefCntAutoPtr` で RAII 管理されている。
- `ImageBuffer::imagePixelBytes` のオーバーフロー安全乗算、`DeepImageBuffer` の `static_assert(sizeof(DeepSampleGpu)==40)` による HLSL stride 保証など、要所に堅い実装がある。
- `noexcept` が 1,472箇所と広範に付与されている。
- ColorTransferFunction / MultiChannelImage(16ch AOV) / MFR+RenderFarm など、機能面は商用製品水準。

---

## プロダクション級にするための最短順序

1. **`ArtifactProjectImporter` を `ArtifactAbstractComposition::fromJson` 経由に切り替える**（既に正しい実装が存在する）。これだけで保存/読込が生き返る。併せてレイヤー ID 復元（`ArtifactLayerSetting::setId` は呼び出し 0件）を通す。
2. **保存/読込のラウンドトリップ回帰テストを1本書く** — 現状これがないから 1. が誰にも気づかれなかった。
3. `validateBeforeSave` のオフライン素材を Error → Warning に降格し、失敗をユーザーに通知する。`setDirty(false)` を保存成功時に呼ぶ。
4. 生 `Impl*` 114件に `= delete` を機械的に付与（スクリプトで一括可能）。リーク14件のデストラクタ修正。
5. ビルドCI（`/W4` + 警告差分チェック）とテストCIを GitHub Actions に追加。
6. その後に God Class 分割・スレッド安全性設計（レンダースレッドはドメインモデルの不変スナップショットのみ触る、等の契約明文化）。

**優先度1〜3は数時間〜1日規模で、体感品質が最も大きく変わる。**
