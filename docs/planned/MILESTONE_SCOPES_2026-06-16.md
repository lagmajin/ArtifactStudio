# M-SCOPES-1 Scopes Milestone (Vector / Waveform / Parade)

作成日: 2026-06-16
ステータス: Draft
対象: `Artifact/src/Widgets/Viewer/ArtifactContentsViewer.cpp`,
      `Artifact/src/Widgets/Color/ArtifactColorSciencePanel.cppm`,
      `Artifact/src/Widgets/Color/ArtifactColorSwatchWidget.cppm`,
      `Artifact/src/Widgets/ArtifactMainWindow.cppm`,
      `ArtifactCore/src/Color/ColorLuminance.cppm`,
      `ArtifactCore/src/Color/ColorSpace.cppm`,
      `Artifact/src/Project/ArtifactProjectManager.cppm`
位置づけ: DaVinci 的な **3 種の scope** を、現 composition frame に **ライブ表示** する panel として追加。`MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_2026-06-02.md` を 1 段具体化。
参照:
- `docs/analysis/REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.8
- `docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md` (P2 Color Management)
- `docs/planned/MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_2026-06-02.md` (parent)
- `docs/planned/MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_PHASE1_EXECUTION_2026-06-02.md`
- `docs/planned/MILESTONE_VISUAL_DENSITY_MONITOR_2026-06-03.md`
- `docs/planned/MILESTONE_OCIO_INTEGRATION_2026-06-16.md` (display role 整合)
- `docs/technical/RENDER_FORMAT_CONTRACT_2026-05-16.md`

---

## 1. 目的

`REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.8:

> - Vector scope (full): 0 hit
> - Waveform scope (full): 0 hit
> - Parade scope: 0 hit

DaVinci Resolve / Premiere Pro の **Scopes** は露出 / 色相 / 彩度の基準確認に必須:

- **Vector scope (vectorscope)**: 色相 / 彩度を 2 次元表示
- **Waveform (luminance scope)**: 輝度を水平軸=列、垂直軸=値 で表示
- **Parade**: R / G / B 個別に並列表示

`ArtifactContentsViewer.cpp:968` に `ParadeScope` の言及はあるが独立 widget ではなく、3 種を統一 UI で扱う **Scope Panel** は未実装。

> 重要: `ArtifactWidgets` を触らず、parent repo `Artifact/` 配下のみで成立させる。

---

## 2. 現状整理 (2026-06-16 基準)

### 2.1 既存資産

- `ArtifactContentsViewer.cpp:968` — `ParadeScope` 関連コード（部分実装の可能性）
- `Artifact/src/Render/ArtifactHDRMonitor.cppm:153-190` — `Vectorscope` 関連
- `ArtifactCore/src/Color/ColorLuminance.cppm` — luminance 計算
- `MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_2026-06-02.md` — 親構想
- `MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_PHASE1_EXECUTION_2026-06-02.md` — Phase 1 実行メモ

### 2.2 不足

| 軸 | 状況 | 影響 |
|---|---|---|
| 3 種 scope 統一 panel | なし | 露出 / 彩度の確認導線が弱い |
| Live frame ソース | `frame_` 取り出し経路が散在 | 統一 source が無い |
| 設定 (intensity / scale) | なし | ユーザーがスケール調整できない |
| Display role (OCIO 整合) | なし | 最終 display と異なる色空間で表示 |
| Target placement (highlight / shadow) | なし | 露出警告 |
| Project 保存 | なし | 設定が消える |
| Diagnostics | なし | 不正 scope signal 検出なし |

### 2.3 既存 milestone との関係

- `MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_2026-06-02.md` — 親。本 milestone は **Scope 部分を抜き出した sub-milestone**
- `MILESTONE_OCIO_INTEGRATION_2026-06-16.md` — display role。本 milestone は OCIO の `display` を scope の reference にする
- `MILESTONE_VISUAL_DENSITY_MONITOR_2026-06-03.md` — 別系統。本 milestone は Scopes に特化

---

## 3. 設計の柱

### 3.1 ScopeFrame データ

`ArtifactCore/include/Color/ScopeFrame.ixx` を新規追加:

```cpp
namespace ArtifactCore {

struct ScopeFrame {
    // 1 frame の analysis
    QSize size;
    int  sampleStep;            // 間引き step
    std::vector<FloatRGBA> samples;   // (size.width / sampleStep) * height

    // 統計
    float minLuma;
    float maxLuma;
    float avgLuma;
    float lumaClippedLow;       // < 0.0
    float lumaClippedHigh;      // > 1.0 (OCIO display role での 0..1 が canonical)
    int   totalPixels;

    // 検出
    QList<QPointF> outOfGamut;  // gamut 外 pixel (HQ warn)
};

class ScopeFrameBuilder {
public:
    static ScopeFrame build(const ImageF32x4RGBAWithCache& img, int sampleStep = 4);
    static ScopeFrame build(const ImageF32x4_RGBA& img, int sampleStep = 4);
};

} // namespace ArtifactCore
```

- `QImage` を **使わず**、`ImageF32x4RGBAWithCache` / `ImageF32x4_RGBA` 経由で生成
- OCIO display role で正規化された 0..1 を前提

### 3.2 3 種の scope widget

`Artifact/src/Widgets/Scopes/ScopePanel.cppm` を新規追加:

```cpp
class ScopePanel : public QWidget {
public:
    explicit ScopePanel(QWidget* parent = nullptr);

    // 1 frame 更新
    void setFrame(const ScopeFrame& frame);

    // 設定
    void setScopeKind(ScopeKind kind);   // Vector / Waveform / Parade
    void setIntensity(float scale);     // 0.5 / 1.0 / 2.0
    void setShowHighlightShadow(bool on);

private:
    // 子: VectorScope / WaveformScope / ParadeScope
};

enum class ScopeKind { Vector, Waveform, Parade };
```

3 種の scope を `QStackedWidget` で切替。

#### 3.2.1 VectorScope

- 中心: 中性灰 (R=G=B=0.5)
- R-X / R-Y 軸で **R-G` / `B-Y`** をプロット
- skin tone ライン: 33° / 60° (I/Q) を薄い線で表示
- 彩度スケール: 中心からの距離を 0.0〜1.0+ でラベル

#### 3.2.2 WaveformScope

- 横軸: 画像 X 座標 (0..W-1)
- 縦軸: 輝度 (0.0..1.0、OCIO display role)
- 各列で R / G / B 個別ライン または 輝度 1 本
- 露出クリップ: 0.0 未満 / 1.0 超を赤帯で警告

#### 3.2.3 ParadeScope

- 横軸: 画像 X 座標 (0..W-1)
- 縦軸: 輝度
- 3 つの R / G / B ペインを **横並び** で表示
- 各ペインにスケール

### 3.3 共通設定

`ScopePanel` の context menu / 設定:

- `Intensity` (0.5 / 1.0 / 2.0)
- `Highlight/Shadow warning` (on/off)
- `Reference color space` (OCIO display role)
- `Sample step` (1 / 2 / 4 / 8)
- `Show skin tone line` (Vector のみ)

設定は `ArtifactColorSciencePanel` の `Scopes` タブに統合。

### 3.4 Live frame ソース

`ArtifactCompositionRenderController::renderOneFrame` 完了後、**cache された最終 image** を `ScopeFrameBuilder::build()` に渡す経路を追加:

```cpp
// render controller 側
auto* scopePanel = mainWindow->scopePanel();
if (scopePanel && cacheResult) {
    ScopeFrame frame = ScopeFrameBuilder::build(*cacheResult, sampleStep);
    scopePanel->setFrame(frame);
}
```

- render 1 サイクルで 1 setFrame
- 描画と統計は **非同期** (frame 評価 → UI 更新)
- playback 中は `playheadFrame` が変わるたび更新
- scrub 中は 60 fps 上限

### 3.5 OCIO 整合

`OCIOManager` から `display()` role を取得し、scope の reference にする:

```cpp
QString displayCS = OCIOManager::instance().display();
QMatrix4x4 displayMatrix = OCIOManager::instance().sceneToDisplay(displayCS, currentView, looks);
```

- OCIO 経由で正規化された値で描画
- `MILESTONE_OCIO_INTEGRATION_2026-06-16.md` 完了後は自動で OCIO 設定に従う

### 3.6 Project 保存

- `ArtifactProjectManager` の project JSON に `scope.intensity / scope.kind / scope.showHighlightShadow` 追加
- 旧プロジェクトは default (intensity=1.0, kind=Parade) で開く

### 3.7 Diagnostics

`MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12` の `goal/expected/actual` 枠に:

- `scope.highlight-clipped` (severity=info, 露出クリップ検出)
- `scope.shadow-clipped` (severity=info, シャドークリップ検出)
- `scope.gamut-out` (severity=warning, gamut 外 pixel 検出)

### 3.8 不変条件 (Guardrails)

- `QImage` を **新規 hot path に入れない**。`ImageF32x4RGBAWithCache` 経由
- 既存 `ParadeScope` (`ArtifactContentsViewer.cpp:968`) と `Vectorscope` (`ArtifactHDRMonitor.cppm:153`) は **温存**。本 milestone は独立 panel として追加
- `ArtifactWidgets` は触らない
- 新規 signal-slot 接続は `frameUpdated` 1 個に限定
- 60 fps 上限で update。重い frame では自動で `sampleStep` を増やす

---

## 4. フェーズ計画

### Phase 1: Core data + ScopeFrameBuilder (P0, 1〜2 セッション)

- `ArtifactCore/include/Color/ScopeFrame.ixx` 新規
- `ArtifactCore/src/Color/ScopeFrame.cppm` 実装
- `ScopeFrameBuilder::build(ImageF32x4RGBAWithCache&)` 実装
- 統計計算 (minLuma / maxLuma / clipped / outOfGamut)

**Done criteria:**
- `ScopeFrameBuilder::build()` が 1 frame から統計を返す
- `ImageF32x4RGBAWithCache` 経由で動作
- 既存 `QImage` 経由の解析 path と並走可能

### Phase 2: 3 種 scope widget (P0, 2〜3 セッション)

- `Artifact/src/Widgets/Scopes/ScopePanel.cppm` 新規
- `VectorScope` / `WaveformScope` / `ParadeScope` 3 種の内部 widget
- `ScopeKind` 切替

**Done criteria:**
- 3 種すべてが `setFrame(ScopeFrame)` で描画更新
- `Intensity` / `Sample step` 設定が反映
- skin tone ライン表示 (Vector のみ)

### Phase 3: Live frame 接続 (P0, 1 セッション)

- `ArtifactCompositionRenderController` の render 経路から `ScopeFrameBuilder` 呼出
- `ArtifactMainWindow` に `ScopePanel` を dock 化
- 60 fps 上限

**Done criteria:**
- playback 中に scope がライブ更新
- scrub 中も更新
- 重い frame で sampleStep が自動調整

### Phase 4: Highlight / shadow warning + Diagnostics (P0, 1 セッション)

- 露出クリップ検出
- gamut 外 pixel 検出
- Problem View への contribution

**Done criteria:**
- 露出クリップを赤帯で表示
- gamut 外 pixel を warning 表示
- Problem View に `scope.highlight-clipped` 等が出る

### Phase 5: OCIO 整合 (P0, 1 セッション)

- `OCIOManager::display()` 経由で reference を取得
- OCIO matrix 経由で正規化
- `MILESTONE_OCIO_INTEGRATION_2026-06-16.md` 完了後、自動連携

**Done criteria:**
- OCIO display role に追従
- view transform 変更が scope に反映
- OCIO 設定が scope 描画に一致

### Phase 6: Project 保存 + 設定 UI (P1, 1 セッション)

- `ArtifactColorSciencePanel` の `Scopes` タブ
- project JSON に `scope.*` 設定
- 旧プロジェクトは default で開く

**Done criteria:**
- panel から設定変更
- project 保存 → 再読込で復元
- 旧プロジェクトが開ける

### Phase 7: 比較 / diff 機能 (P2, 別 milestone 推奨)

- 2 frame 比較 (before / after)
- A/B scope 表示
- 別 milestone 推奨

**Done criteria (本 milestone 内):**
- 将来 `MILESTONE_SCOPE_COMPARE_2026-XX-XX.md` のエントリポイントを作る

---

## 5. 既存 milestone との関係

| 既存 | 関係 |
|---|---|
| `MILESTONE_CROSS_INDUSTRY_INSPECTION_TOOLS_2026-06-02.md` | 親構想。本 milestone は Scopes 部分だけ抜き出し。 |
| `MILESTONE_VISUAL_DENSITY_MONITOR_2026-06-03.md` | 別系統 (density / coverage)。並走。 |
| `MILESTONE_OCIO_INTEGRATION_2026-06-16.md` | display role。本 milestone は OCIO を参照。 |
| `MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-05-12.md` | diagnostics 文法。 |
| `MILESTONE_OCIO_INTEGRATION_2026-06-16.md` | OCIO 経由で整合。 |

---

## 6. リスクと未解決論点

### 6.1 実装上のリスク

1. **描画性能**。`ScopeFrame::samples` 50k samples 程度。`paintEvent` で毎フレーム再描画。Phase 3 で最適化
2. **OCIO 不在時**。`MILESTONE_OCIO_INTEGRATION_2026-06-16.md` が未完了でも、fallback で `sRGB` として動作
3. **`ImageF32x4RGBAWithCache` の hot path 流入**。既存 thumbnail cache を使い回すか、別 cache を作るか
4. **60 fps 更新**。重い frame でフレーム落ち。`sampleStep` 動的調整
5. **3 種 scope 切替**。`QStackedWidget` 切替時の再描画コスト

### 6.2 契約上の未解決

- **HDR scope**。`scope > 1.0` の表示。Phase 5 で OCIO と並走
- **Multi-display scope**。複数 display の同時表示。Phase 7 以降
- **`Parade` 既存実装との接続**。`ArtifactContentsViewer.cpp:968` の `ParadeScope` をどう統合するか。Phase 1 で確認
- **Scope data の永続化**。frame 単位の scope data は保存しない
- **Waveform 個別 R/G/B モード**。`RGB Combined` / `RGB Parade` 切替。Phase 2 で決定

### 6.3 サブモジュール境界

- `ArtifactCore/include/Color/ScopeFrame.ixx` を新規追加
- `Artifact/src/Widgets/Scopes/ScopePanel.cppm` を新規追加
- `ArtifactCore/CMakeLists.txt` に登録
- `ArtifactWidgets` は触らない
- bump 手順は `.github/GIT_WORKFLOW_PARENT_CHILD.md` 準拠

---

## 7. Done Criteria (全体)

- playback 中に Vector / Waveform / Parade がライブ更新
- skin tone ラインが Vector scope に表示
- 露出クリップが赤帯で警告
- OCIO display role に追従
- intensity / sample step 設定が反映
- project 保存 → 再読込で scope 設定復元
- 旧プロジェクトは default で開く
- Problem View に `scope.*` 健全性表示
- 60 fps 上限で更新
- 既存 `ParadeScope` (`ArtifactContentsViewer`) と並走可能
- 新規 `QImage` / `setStyleSheet` / 新規 signal-slot が増えていない
- `ArtifactWidgets` を触っていない

---

## 8. 更新履歴

- 2026-06-16: 初版作成。`REPORT_LATE_STAGE_AND_DCC_GAP_2026-06-16.md` §2.8 / §4 を正式 milestone に起こした。DaVinci 風 Scopes foundation。

## Static Audit (2026-07-25)

現行ソースでは `Color.Grading.ColorScopes` に waveform/vectorscope/parade の CPU renderer、`Graphics.Compute.ScopeComputer` と専用 HLSL に GPU bin 計算、`ArtifactContentsViewer` に既存 ParadeScope の入口がある。したがって scope の計算素材と一部表示基盤は存在する。

ただし、設計どおりの `ScopeFrame` / `ScopeFrameBuilder`、統一 `ScopePanel`、live composition frame の非同期更新、OCIO display-role 整合、intensity/sample-step 設定、highlight/shadow/gamut diagnostics、project 保存、Problem View 接続は確認できない。既存 CPU renderer は `QImage` 入力/出力であり、本 milestone の typed-buffer hot-path guardrail とも未整合である。skin-tone line、60fps上限、旧ParadeScopeとの統合受け入れも未検証。

判定: 計算・shader の基盤は partial、Phase 1〜7 の統一 panel／live／保存／diagnostics は未完了。
