# M-OCIO-1 OpenColorIO 統合 Milestone

作成日: 2026-06-16
ステータス: Draft
対象: `Artifact/src/Color/ArtifactColorScienceManager.cppm`,
      `Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm`,
      `Artifact/src/Widgets/Color/ColorPicker/*`,
      `Artifact/src/Color/ColorNodeGraph*`,
      `Artifact/src/Effect/ArtifactColorLUT*`,
      `Artifact/src/Project/ArtifactProjectManager.cppm`,
      `ArtifactCore/include/Color/ColorACES.ixx`,
      `ArtifactCore/include/Color/ColorGamutConversion.ixx`,
      `ArtifactCore/include/Color/AutoColorMatch.ixx`,
      `ArtifactCore/include/Color/ColorSpace.ixx`
位置づけ: 既存 ACES 基盤に **OCIO config / 役割 / 表示 / ビュー / ワーキングスペース** の 5 ロールを追加し、制作現場の color pipeline を一括管理する foundation。
参照:
- `docs/analysis/REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.8
- `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` (P2 Color Management)
- `docs/planned/MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md`
- `docs/planned/MILESTONE_COLOR_GRADING_WORKSPACE_2026-03-30.md`
- `docs/planned/MILESTONE_COLOR_MANAGEMENT_QUICK_2026-04-10.md`
- `docs/planned/MILESTONE_APP_COLOR_CORRECTION_RACK_2026-05-18.md`
- `docs/planned/MILESTONE_LUT_BROWSER_2026-06-16.md` (LUT Library companion)
- `docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md` (linear canonical)

---

## 1. 目的

`REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.8:

> Color (DaVinci Resolve / Baselight / Nuke)
> - Color page (Qualifier / Power Window): 0 hit
> - Color managed output (ACES): 0 hit
> - HDR / Dolby Vision mastering: 0 hit
> - LUT calc (programmatic): 0 hit
> - Vector scope / Waveform scope / Parade scope: 0 hit
> - Color warper (wheels + log): 0 hit
> - LUT export (write): 0 hit
> - ACES IDT / ODT config: 0 hit
> - OpenColorIO (OCIO) config: 0 hit
> - OpenColorIO transforms (LMT): 0 hit

DaVinci Resolve / Baselight / Nuke / Houdini などのプロダクションパイプラインは **OpenColorIO (OCIO)** を介して color management を統一する。OCIO は

- 役割 (role) ベースの色空間定義
- 表示 / ビュー / ワーキングスペース切替
- 複数 config (`config.ocio`) の切替
- Look Modification Transform (LMT) の適用
- 共通 IDT / ODT / LMT プリセット

を提供する。

ArtifactStudio は `ColorACES.ixx` / `ColorGamutConversion.ixx` を持つが、**OCIO config の読み込みも、役割の概念も、LMT も無い**。制作現場で「ACEScg で作業 / sRGB で表示 / Rec.2020 で納品」ができない。

> 重要: `ArtifactCore` 側に OCIO wrapper を入れる。実 OCIO ライブラリは **外部依存** (`OpenColorIO` 公式)。`third_party/` 配下を直接触らず、CMake の `find_package(OpenColorIO)` を活用する前提で設計する。サブモジュール（`ArtifactWidgets`）には触らない。

---

## 2. 現状整理 (2026-06-16 基準)

### 2.1 既存資産

- `ArtifactCore/include/Color/ColorACES.ixx` — `class ColorACES`（ACES 変換の最小実装）
- `ArtifactCore/include/Color/ColorGamutConversion.ixx` — gamut 変換
- `ArtifactCore/include/Color/AutoColorMatch.ixx` — 自動色補正
- `ArtifactCore/include/Color/ColorSpace.ixx` — color space enum
- `Artifact/src/Color/ArtifactColorScienceManager.cppm` — ディレクトリ走査 (`scanLutDirectory`)
- `Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm` — panel
- `MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md` — 既存構想

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| OCIO config 読み込み | 0 hit | 制作現場の色管理に乗れない |
| 役割 (role) 概念 | なし | scene_linear / display / input 区別が無い |
| 表示 / ビュー / ワーキングスペース | なし | 切替不可 |
| IDT / ODT / LMT | なし | カメラ入力 → 作業 → 出力の変換が繋がらない |
| LUT write | なし | 3D LUT 出力で他ツールへ渡せない |
| 既存 ACES との接続 | 部分的 | `ColorACES` のみ。OCIO と並走 |
| Project 保存 | なし | 設定が project に乗らない |
| Diagnostics | なし | 不正 config 検出なし |

### 2.3 既存 milestone との関係

- `MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md` — 既存下位 layer。本 milestone はその上に **OCIO 統合** を載せる
- `MILESTONE_LUT_BROWSER_2026-06-16.md` — LUT Library。本 milestone は OCIO config と library を並走
- `MILESTONE_APP_COLOR_CORRECTION_RACK_2026-05-18.md` — color correction rack UI
- `MILESTONE_COLOR_GRADING_WORKSPACE_2026-03-30.md` — workspace
- `MILESTONE_RENDER_FORMAT_CONTRACT_2026-05-16.md` — linear canonical。OCIO の `scene_linear` 役割と一致

---

## 3. 設計の柱

### 3.1 OCIO Config Model

`ArtifactCore/include/Color/OCIOConfig.ixx` を新規追加:

```cpp
namespace ArtifactCore {

// 役割 (role): scene_linear / display / input / rendering / color_picking 等
enum class OCIORole {
    SceneLinear,
    Display,
    Input,
    Rendering,
    ColorPicking,
    // OCIO config に登録された任意 role
    Custom
};

struct OCIORoleInfo {
    OCIORole kind;
    QString name;        // config 上の名前
    QString colorSpace;  // 役割が指す color space
};

struct OCIOView {
    QString name;                    // "ACES - sRGB"
    QString displayColorSpace;       // 表示色空間
    QString viewTransform;           // view transform 名
    QList<QLookTransform> looks;     // LMT 一覧
};

class OCIOConfig {
public:
    // 読み込み
    static std::shared_ptr<OCIOConfig> load(const QString& path);  // config.ocio
    static std::shared_ptr<OCIOConfig> builtinACES();              // built-in ACES

    // query
    QString name() const;
    int versionMajor() const;
    int versionMinor() const;
    QString defaultDisplay() const;
    QString defaultView(const QString& display) const;
    QList<OCIOView> views(const QString& display) const;
    OCIORoleInfo roleInfo(OCIORole kind) const;
    QString colorSpaceName(const QString& key) const;

    // 変換
    QMatrix4x4 matrixFromTo(const QString& srcColorSpace, const QString& dstColorSpace) const;
    // または 1D/3D LUT
};

} // namespace ArtifactCore
```

- 実 OCIO ライブラリがあれば `OCIO::Config::CreateFromFile` を内部で利用
- ない場合は **built-in ACES config** を `builtinACES()` で提供し、interface だけ揃える

### 3.2 OCIO Manager (singleton)

`ArtifactCore/include/Color/OCIOManager.ixx`:

```cpp
class OCIOManager {
public:
    static OCIOManager& instance();

    // 現在 config
    void setActiveConfig(const QString& configPath);
    std::shared_ptr<OCIOConfig> activeConfig() const;

    // role 取得
    QString sceneLinear() const;     // 通常 "ACES - ACEScg"
    QString display() const;          // "sRGB - Display"
    QString inputDefault() const;      // カメラ raw の default input

    // 変換ヘルパ
    QMatrix4x4 sceneToDisplay(const QString& display,
                              const QString& view,
                              const QString& looks) const;
    QMatrix4x4 sceneToOutput(const QString& outputCS) const;
    QMatrix4x4 inputToScene(const QString& inputCS) const;

    // 永続化
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject& obj);
};
```

- シングルトン。`setActiveConfig` で切替
- project JSON に保存

### 3.3 Composition 設定

`Artifact/src/Composition/ArtifactCompositionSettings.cppm` に **OCIO role override** を追加:

```cpp
// 既存
QString workingSpace;        // 既存 working color space
QString displaySpace;         // 既存 display color space

// 新規
QString inputSpace;            // カメラ / 素材の input
QString viewTransform;         // "ACES - sRGB"
QStringList looks;             // LMT 一覧
```

- 既定値は OCIOManager から取得
- composition 単位で override 可能

### 3.4 UI 露出

`ArtifactColorSciencePanel` に **`OCIO` タブ** を追加:

- Config path 選択
- Display / View 切替
- Looks (LMT) 適用 / 解除
- Scene Linear / Display / Input 役割の現在値表示
- Project View の footer に **working space 名** を表示

### 3.5 Render 経路統合

`ArtifactCompositionRenderController` の **view transform 適用** を追加:

1. layer composite 結果を `OCIOManager::sceneLinear()` で保持 (linear canonical)
2. `OCIOManager::sceneToDisplay(display, view, looks)` で **view transform 適用**
3. swap chain へ出力

- `RENDER_FORMAT_CONTRACT_2026-05-16.md` の linear canonical と整合
- 既存 `colorSpace` 設定は **scene linear = working space** として扱う

### 3.6 IDT / ODT / LMT

- **IDT (Input Device Transform)**: カメラ raw → scene linear。`OCIOManager::inputToScene(inputCS)` で対応
- **ODT (Output Device Transform)**: scene linear → display。`sceneToDisplay()` で対応
- **LMT (Look Modification Transform)**: scene linear 内で適用。`looks[]` として composition に保存

OCIO 標準の preset を built-in で用意:

- `Rec.1886 / Rec.709`
- `Rec.1886 / Rec.2020`
- `ACES / P3-D60`
- `ACES / sRGB`

### 3.7 LUT Write

`ArtifactCore/include/Color/LUTWriter.ixx` を新規追加:

```cpp
class LUTWriter {
public:
    // .cube 1D / 3D 書き出し
    static bool writeCube(const QString& path,
                          int size3D,
                          const QString& sourceCS,
                          const QString& targetCS);

    // .3dl 書き出し
    static bool write3dl(const QString& path,
                         int size3D,
                         const QString& sourceCS,
                         const QString& targetCS);

    // OCIO processor で生成
    static QByteArray generateCube(OCIOProcessor processor,
                                   int size3D,
                                   const QString& sourceCS,
                                   const QString& targetCS);
};
```

- `ArtifactColorSciencePanel` の `Export LUT` ボタンから起動
- 出力サイズは 17 / 33 / 65 から選択

### 3.8 Color Interop ID

- アプリ間で共有できる色空間識別子を導入し、OCIO role / working space / display preset の参照名として扱う。
- 例:
  - `scene_linear`
  - `display_srgb`
  - `display_rec709`
  - `working_acescg`
  - `input_camera_log`
- まずは project JSON と preset JSON のキーとして扱い、後から外部アプリ連携の交換フォーマットへ拡張する。

### 3.8 不変条件 (Guardrails)

- 既存 `ColorACES.ixx` / `ColorGamutConversion.ixx` は **温存**
- 実 OCIO ライブラリは **外部依存**。CMake `find_package(OpenColorIO)` で optional
- ライブラリ不在でも `builtinACES()` で **mock 動作** する
- `QImage` / `setStyleSheet` 流入禁止
- 新規 signal-slot 接続は `configChanged / roleChanged` の 2 個に限定
- `RENDER_FORMAT_CONTRACT_2026-05-16.md` の linear canonical と整合 (scene linear = working space)
- 既存 project は OCIO 未設定でも開ける (fallback: `scene_linear = sRGB`)

### 3.9 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 枠に:

- `ocio.config.missing` (severity=error, config path が見つからない)
- `ocio.config.invalid` (severity=error, parse 失敗)
- `ocio.role.unmapped` (severity=warning, 役割に color space が未割当)
- `ocio.view.missing` (severity=warning, display / view が見つからない)
- `ocio.lut.size-mismatch` (severity=info, export サイズが OCIO processor と不整合)

---

## 4. フェーズ計画

### Phase 1: Core data + manager (P0, 1〜2 セッション)

- `ArtifactCore/include/Color/OCIOConfig.ixx` 新規
- `ArtifactCore/include/Color/OCIOManager.ixx` 新規
- `ArtifactCore/src/Color/OCIOManager.cppm` 実装
- `builtinACES()` で built-in mock 動作
- `toJson / fromJson` 実装

**Done criteria:**
- `setActiveConfig(path)` で `activeConfig()` が更新
- role / view / look query が動作
- library 不在でも mock で `sceneToDisplay` が成立
- 永続化が round-trip

### Phase 2: Composition 設定 + Render 統合 (P0, 1〜2 セッション)

- `ArtifactCompositionSettings` に OCIO role override 追加
- `ArtifactCompositionRenderController` の render 経路に view transform 適用
- 既存 `colorSpace` 設定との整合

**Done criteria:**
- composition 単位で `viewTransform` / `looks` を override 可能
- render 結果に view transform が反映
- `RENDER_FORMAT_CONTRACT_2026-05-16.md` 整合

### Phase 3: UI 露出 (P0, 1〜2 セッション)

- `ArtifactColorSciencePanel` に OCIO タブ
- Config / Display / View / Looks の選択 UI
- Project View の footer に working space 名
- 設定変更で `OCIOManager` 通知

**Done criteria:**
- panel から config 切替
- Display / View / Looks 選択
- working space が footer に表示
- 設定変更が `M-CE-CRIT-1` の smoke に反映

### Phase 4: LUT Write (P0, 1 セッション)

- `ArtifactCore/include/Color/LUTWriter.ixx` 新規
- `Artifact/src/Color/LUTWriter.cppm` 実装
- panel の `Export LUT` ボタン
- `.cube` / `.3dl` 書き出し

**Done criteria:**
- `Export LUT` で `.cube` が書き出し可能
- OCIO processor 経由の `.3dl` 書き出し
- 別ツール (Resolve / Nuke) で読み込み可能

### Phase 5: IDT / ODT / LMT プリセット (P0, 1 セッション)

- built-in preset を `OCIOManager` に追加
- `Rec.1886 / Rec.709` / `Rec.1886 / Rec.2020` / `ACES / P3-D60` / `ACES / sRGB`
- 適用 UI

**Done criteria:**
- 4 種の built-in preset を選択可能
- 適用後 render 結果が変わる

### Phase 6: Diagnostics + 永続化 (P1, 1 セッション)

- Problem View への `ocio.*` 健全性 contribution
- project JSON に `ocio` セクション追加
- 旧プロジェクトは OCIO 欠落を許容

**Done criteria:**
- `ocio.config.missing` が Problem View に表示
- project 保存 → 再読込で OCIO 設定復元
- 旧プロジェクトが開ける

### Phase 7: 3D LUT キャッシュ (P2, 別 milestone 推奨)

- `M-LUT-1 LUT Browser` との統合
- 3D LUT の cache / preview 拡張
- 別 milestone 推奨

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_OCIO_LUT_CACHE_2026-XX-XX.md` のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_ADVANCED_COLOR_SCIENCE_PIPELINE_2026-03-29.md` | 下位 layer。本 milestone は OCIO 統合で補完。 |
| `MILESTONE_LUT_BROWSER_2026-06-16.md` | 並走。library と config を並走。 |
| `MILESTONE_APP_COLOR_CORRECTION_RACK_2026-05-18.md` | 上位 UI。rack は OCIO 設定を参照。 |
| `MILESTONE_COLOR_GRADING_WORKSPACE_2026-03-30.md` | workspace。OCIO 設定が乗る。 |
| `MILESTONE_RENDER_FORMAT_CONTRACT_2026-05-16.md` | linear canonical と整合。 |
| `MILESTONE_BLEND_MODE_DESIGN_2026-06-16.md` | 別 topic。 |

---

## 6. リスクと未解決論点

### 6.1 実装上のリスク

1. **OCIO 外部依存**。`find_package(OpenColorIO)` の有無。CMake の `optional` 設定。`third_party/OpenColorIO/` を submodule 追加する選択肢は Phase 1 完了後に再評価
2. **既存 ColorACES との接続**。`ColorACES.ixx` を OCIO に置換するか並走するか。並走が安全
3. **Render 経路の view transform 適用**。`RENDER_FORMAT_CONTRACT_2026-05-16.md` との整合確認
4. **`builtinACES()` mock**。ライブラリ不在時の挙動。Phase 1 で十分検証
5. **LUT write 性能**。3D LUT 17^3 / 33^3 / 65^3 サンプル。Phase 4 で実測

### 6.2 契約上の未解決

- **HDR / Dolby Vision mastering**。`OCIO` 経由で可能か。Phase 7 以降で別途
- **Multi-display workflow**。複数 display への同時出力。Phase 7 以降
- **OCIO processor の thread safety**。Phase 1 で `QMutex` 保護
- **LUT の v2 / v3 互換**。`.cube` の `LUT_3D_SIZE` 範囲。Phase 4 で決定
- **Project 保存 schema**。OCIO path だけ保存するか、role / view / looks まで保存するか。Phase 6 で決定

### 6.3 サブモジュール境界

- `ArtifactCore/include/Color/OCIOConfig.ixx` 等を新規追加
- 実 OCIO ライブラリは **外部依存** として CMake `find_package` 経由
- `third_party/` 配下を直接触らない
- `ArtifactWidgets` は触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- `OCIOManager::setActiveConfig` で config 切替
- 4 種の built-in preset を選択可能
- composition 単位で `viewTransform` / `looks` を override
- render 結果に view transform が反映
- `Export LUT` で `.cube` / `.3dl` 書き出し可能
- Problem View に `ocio.*` 健全性表示
- project 保存 → 再読込で OCIO 設定復元
- 旧プロジェクトは OCIO 欠落を許容
- 実 OCIO ライブラリなしでも mock で全機能が動作
- 既存 `ColorACES.ixx` / `ColorGamutConversion.ixx` が温存
- 新規 `QImage` / `setStyleSheet` / 新規 signal-slot が増えていない
- `ArtifactWidgets` を触っていない
- `RENDER_FORMAT_CONTRACT_2026-05-16.md` 整合

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.8 / §4 を正式 milestone に起こした。DaVinci / Baselight / Nuke / Houdini 系の foundation。
