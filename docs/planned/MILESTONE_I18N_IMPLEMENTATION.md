# MILESTONE: Internationalization (i18n) Implementation

**日付**: 2026-08-04
**最終更新:** 2026-08-15

## 現行コード監査 (2026-08-15)

Core の `LocalizationManager` が locale enum／コード設定／自動判定／翻訳ロードを持ち、App の `TranslationManager` は Core へ委譲しています。App 起動時には translations directory を両マネージャーへロードし、`--lang` と system locale による言語選択、ja／en／zh／ko 等の locale 認識が確認できます。翻訳監査ツールと主要メニュー・Property Editor・Command Palette 等の翻訳キー移行も反映済みです。

ただし文書内の「2系統の統合未完了」という旧記述は現状とずれています。残課題は UI 全域のハードコード文字列、Core／App の API 完全統一、動的言語切替時の既存 widget 更新、数値・日付の全画面 locale formatting、全 locale の実表示確認です。
**監査結果:** `tools/i18n/audit_translations.py` による ja/en 対照でカバレッジ100%、未翻訳4件（技術表記）を確認。
**今回の実装:** Core の LocalizationManager を翻訳カタログの単一実体とし、App の TranslationManager は既存 API を維持する薄い委譲層へ統合。loadedKeys API を追加。
Property Editor のキーフレーム、リセット、エクスプレッション、お気に入り操作ラベルも翻訳キー経由へ移行。
App 側の `clear()` も Core の共有カタログ消去へ委譲。
**実装状況:** 翻訳ローダーと複数言語のロケールJSONは既存。監査スクリプト、80%閾値と未翻訳値4件上限のCI監査、App/Core双方の不足・未翻訳キーAPI、Core側のネストキー修正、追加言語のロケール認識・一覧API・コード設定API、`LocaleFormatting` を追加。File/Edit/Help/Script Menu、Command Palette、メモ、クリップバッファ等の主要UI文字列を翻訳経由へ移行し、jaロケール監査はMissing keys 0件・カバレッジ100%を確認済み。残る4件はアプリ名・FPS・メモリなど技術表記。UI全域の外部化とCore/App統合は未完了。
**現状**: 2つの翻訳マネージャー（`Artifact::TranslationManager` + `ArtifactCore::LocalizationManager`）と `Artifact/translations/` のロケールJSONは存在するが、2系統の統合、UI文字列の全面外部化、数値・日付フォーマットは未完了。
**目標**: 日本語UIの最小実装、2系統の翻訳エンジンを統合、段階的にUI文字列を外部化、数値・日付のロケール対応。

## 現状の問題

| 問題 | 影響 |
|------|------|
| ロケールファイル不在 | `TranslationManager::loadFromDirectory()` が何もロードしない |
| 2系統の並行エンジン | Core と App で別々のマネージャー、別々のキー空間 |
| UI文字列のハードコード | `McpBridge.ixx` の数百行の日本語文字列が直接C++に埋め込まれている |
| 数値/日付フォーマット不在 | `QLocale` は言語検出のみ、フォーマットには使われていない |

---

## Phase 1: 翻訳エンジン統合

### 1.1 単一エンジンへの統合

`ArtifactCore::LocalizationManager` と `Artifact::TranslationManager` を統合。Core 層に一つのエンジンを置き、App層の `TranslationManager` はそれをラップする thin wrapper にする。

```cpp
// ArtifactCore/include/Localization/LocalizationEngine.ixx
class LocalizationEngine {
public:
    static LocalizationEngine& instance();
    
    // ロケール管理
    void setLocale(const QString& locale);    // "ja", "en", "zh-CN", "ko"...
    QString currentLocale() const;
    QStringList availableLocales() const;
    
    // 翻訳（フォールバック付き）
    QString translate(std::string_view key) const;
    QString translate(std::string_view key, 
                      const std::vector<QString>& args) const;  // {0}, {1} 置換
    
    // 未翻訳キーのフォールバック（英語キーそのものを返す）
    QString translateOr(std::string_view key, 
                        std::string_view fallback) const;
    
    // ロケールファイルのロード
    bool loadFromDirectory(const QString& dirPath);
    bool loadLocaleFile(const QString& filePath);
    
    // 開発支援
    QStringList missingKeys() const;      // 現在のロケールにないキー
    QStringList untranslatedKeys() const; // 値がフォールバックと同じキー
    void reload();                         // ホットリロード（開発用）
    
    W_SIGNAL(localeChanged, const QString& newLocale)

private:
    // locale → {key → value}
    std::map<QString, QHash<QString, QString>> translations_;
    QString currentLocale_;
    QString fallbackLocale_ = "en";
};
```

### 1.2 ロケールファイル形式

```json
// Artifact/translations/ja.json
{
  "_meta": {
    "language": "Japanese",
    "locale": "ja",
    "version": 1,
    "lastUpdated": "2026-08-04",
    "translators": ["Artifact Team"]
  },
  "strings": {
    "app.name": "Artifact Studio",
    "menu.file": "ファイル",
    "menu.file.new": "新規プロジェクト",
    "menu.file.open": "プロジェクトを開く...",
    "menu.file.save": "保存",
    "menu.file.saveAs": "名前を付けて保存...",
    "menu.file.export": "書き出し",
    "menu.file.exit": "終了",
    
    "menu.edit": "編集",
    "menu.edit.undo": "元に戻す",
    "menu.edit.redo": "やり直し",
    "menu.edit.cut": "切り取り",
    "menu.edit.copy": "コピー",
    "menu.edit.paste": "貼り付け",
    
    "menu.composition": "コンポジション",
    "menu.composition.new": "新規コンポジション...",
    "menu.composition.settings": "コンポジション設定...",
    
    "menu.layer": "レイヤー",
    "menu.layer.new": "新規",
    "menu.layer.new.solid": "平面...",
    "menu.layer.new.text": "テキスト",
    "menu.layer.new.shape": "シェイプ",
    "menu.layer.new.camera": "カメラ",
    "menu.layer.new.light": "ライト",
    "menu.layer.new.null": "ヌル",
    "menu.layer.new.adjustment": "調整レイヤー",
    
    "menu.view": "表示",
    "menu.view.zoomIn": "拡大",
    "menu.view.zoomOut": "縮小",
    "menu.view.fitToScreen": "画面に合わせる",
    
    "menu.window": "ウィンドウ",
    "menu.window.workspace": "ワークスペース",
    
    "menu.help": "ヘルプ",
    "menu.help.about": "Artifact Studio について",
    
    "dialog.save.title": "保存",
    "dialog.save.changes": "変更を保存しますか？",
    "dialog.save.save": "保存",
    "dialog.save.discard": "破棄",
    "dialog.save.cancel": "キャンセル",
    
    "timeline.play": "再生",
    "timeline.pause": "一時停止",
    "timeline.stop": "停止",
    "timeline.previousFrame": "前のフレーム",
    "timeline.nextFrame": "次のフレーム",
    "timeline.goToStart": "先頭に移動",
    "timeline.goToEnd": "末尾に移動",
    
    "property.name": "名前",
    "property.position": "位置",
    "property.scale": "スケール",
    "property.rotation": "回転",
    "property.opacity": "不透明度",
    "property.anchorPoint": "アンカーポイント",
    
    "inspector.transform": "トランスフォーム",
    "inspector.effects": "エフェクト",
    "inspector.noSelection": "選択されていません",
    
    "composition.new.title": "新規コンポジション",
    "composition.width": "幅",
    "composition.height": "高さ",
    "composition.frameRate": "フレームレート",
    "composition.duration": "長さ",
    "composition.backgroundColor": "背景色",
    
    "render.queue": "レンダーキュー",
    "render.addToQueue": "キューに追加",
    "render.start": "レンダリング開始",
    "render.pause": "一時停止",
    "render.stop": "停止",
    "render.status.queued": "待機中",
    "render.status.rendering": "レンダリング中",
    "render.status.completed": "完了",
    "render.status.failed": "失敗",
    
    "asset.browser.title": "アセットブラウザ",
    "asset.import": "読み込み...",
    "asset.relink": "再リンク...",
    "asset.delete": "削除",
    "asset.rename": "名前変更",
    
    "dialog.confirmDelete.title": "削除の確認",
    "dialog.confirmDelete.message": "{0} を削除しますか？この操作は元に戻せません。",
    
    "error.saveFailed": "保存に失敗しました: {0}",
    "error.fileNotFound": "ファイルが見つかりません: {0}",
    "error.unsupportedFormat": "未対応の形式です: {0}"
  }
}
```

### 1.3 完了条件

- [ ] `LocalizationEngine` が `ja.json` をロードし、`translate("menu.file")` が「ファイル」を返す
- [ ] `translate("menu.file.new", {})` が「新規プロジェクト」を返す
- [ ] 引数置換: `translate("error.saveFailed", {"disk full"})` →「保存に失敗しました: disk full」
- [ ] 未定義キーは英語キーをそのまま返す（`translate("unknown.key")` → `"unknown.key"`）
- [ ] `missingKeys()` が現在のロケールに足りないキーを列挙する

---

## Phase 2: 主要 UI の日本語化

### 2.1 移行パターン

現状のハードコード:
```cpp
// 現状
QAction* saveAction = fileMenu->addAction("Save");
saveAction->setShortcut(QKeySequence::Save);
```

変更後:
```cpp
QAction* saveAction = fileMenu->addAction(tr("menu.file.save"));
saveAction->setShortcut(QKeySequence::Save);
```

`tr()` はクラス内で以下のように定義する（またはマクロで共通化）:
```cpp
inline QString tr(std::string_view key) {
    return ArtifactCore::LocalizationEngine::instance().translateOr(key, key);
}
```

### 2.2 移行優先度（ユーザーに見える頻度順）

| 優先度 | UI 領域 | 概算文字列数 | ファイル |
|--------|--------|------------|---------|
| P0 | メニューバー（File/Edit/Composition/Layer/View/Window/Help） | ~80 | `ArtifactMainWindow.cppm`, 各メニューファイル |
| P0 | タイムライン操作用語（Play/Pause/Stop等） | ~20 | `ArtifactTimelineWidget.cppm` |
| P0 | プロパティ名（Transform/Position/Scale等） | ~30 | `ArtifactPropertyWidget.cppm`, PropertyEditor |
| P0 | ダイアログ（保存確認/エラー/警告） | ~30 | 各ダイアログファイル |
| P1 | Inspector ラベル | ~20 | `ArtifactInspectorWidget.cppm` |
| P1 | レンダーキュー | ~20 | `ArtifactRenderQueueManagerWidget.cpp` |
| P1 | アセットブラウザ | ~20 | `ArtifactAssetBrowser.cppm` |
| P1 | コンポジション設定ダイアログ | ~15 | `CompositionSettingsDialog` |
| P2 | エフェクト名 | ~140 | 各エフェクト登録ファイル |
| P2 | ツールチップ | ~50 | 各所 |
| P2 | ステータスバーメッセージ | ~20 | `ArtifactMainWindow.cppm` |

### 2.3 完了条件

- [ ] P0 の全領域（メニュー・タイムライン・プロパティ・ダイアログ）が日本語表示される
- [ ] 英語ロケールに切り替えても全ての文字列が正しく表示される
- [ ] `ja.json` に 100 以上の文字列エントリが存在する
- [ ] 欠落している翻訳キーが `missingKeys()` で検出可能

---

## Phase 3: 数値・日付のロケール対応

### 3.1 数値フォーマット

```cpp
// ArtifactCore/include/Localization/LocaleFormatting.ixx
class LocaleFormatting {
public:
    // 現在のロケールに応じた数値フォーマット
    static QString formatNumber(double value, int decimals = 2);
    static QString formatPercentage(double value, int decimals = 1);
    static QString formatFileSize(int64_t bytes);
    
    // フレーム番号・タイムコード（ロケール非依存）
    static QString formatFrame(int64_t frame);
    static QString formatTimecode(double seconds, double fps);
    
    // 日付・時刻（ロケール依存）
    static QString formatDate(const QDate& date);
    static QString formatDateTime(const QDateTime& dt);
    static QString formatDuration(int64_t milliseconds);
};
```

使用例:
```cpp
// 日本ロケール: "1,234.56 px"
// 英語ロケール: "1,234.56 px"
// ドイツロケール: "1.234,56 px"
statusBar->showMessage(
    LocaleFormatting::formatNumber(position.x, 2) + " px"
);

// プロジェクトの最終更新日
// 日本: "2026年8月4日 14:30"
// English: "August 4, 2026 2:30 PM"
infoPanel->setLastModified(
    LocaleFormatting::formatDateTime(project.lastModified())
);
```

### 3.2 完了条件

- [ ] 数値表示がロケールに応じて正しくフォーマットされる（小数点・桁区切り）
- [ ] 日付表示が `QLocale` のロケール設定に従う
- [ ] タイムコード・フレーム番号はロケール非依存（SMPTE フォーマット）

---

## Phase 4: 開発者ツールと CI

### 4.1 翻訳キー監査スクリプト

```python
# tools/i18n/audit_translations.py
"""全ソースから tr("...") 呼び出しを抽出し、ja.json と突合する"""

import re
import json
import sys
from pathlib import Path

def extract_keys(source_dir: Path) -> set[str]:
    """ソースコードから tr("...") / AT_TR("...") 呼び出しを抽出"""
    keys = set()
    for f in source_dir.rglob("*.{cpp,cppm,ixx,h,hpp}"):
        content = f.read_text(encoding="utf-8")
        # tr("key.name") パターン
        for m in re.finditer(r'(?:tr|AT_TR)\("([^"]+)"\)', content):
            keys.add(m.group(1))
    return keys

def check_coverage(source_dir: Path, locale_file: Path) -> dict:
    source_keys = extract_keys(source_dir)
    locale_data = json.loads(locale_file.read_text(encoding="utf-8"))
    translated = set(locale_data["strings"].keys())
    
    return {
        "total_source_keys": len(source_keys),
        "translated": len(source_keys & translated),
        "missing": sorted(source_keys - translated),
        "unused_in_locale": sorted(translated - source_keys),
        "coverage": len(source_keys & translated) / len(source_keys) * 100
    }

if __name__ == "__main__":
    result = check_coverage(
        Path(sys.argv[1]),  # Artifact/ + ArtifactCore/
        Path(sys.argv[2])    # Artifact/translations/ja.json
    )
    print(f"Coverage: {result['coverage']:.1f}% ({result['translated']}/{result['total_source_keys']})")
    if result["missing"]:
        print(f"\nMissing keys ({len(result['missing'])}):")
        for k in result["missing"][:20]:
            print(f"  - {k}")
```

### 4.2 CI 統合

```yaml
# .github/workflows/i18n-check.yml（既存のCIに追加）
i18n-coverage:
  runs-on: windows-latest
  steps:
    - run: python tools/i18n/audit_translations.py Artifact ArtifactCore --locale Artifact/translations/ja.json --baseline Artifact/translations/en.json
    - if: coverage < 80%
      run: exit 1  # 80% 未満でCI失敗
```

### 4.3 完了条件

- [ ] `audit_translations.py` が全ソースから翻訳キーを抽出できる
- [ ] CI で 80% 以上のカバレッジを強制
- [ ] `missingKeys()` API が開発中に不足キーをログ出力する

---

## ファイル一覧

| Phase | ファイル | 変更内容 |
|-------|---------|---------|
| P1 | `ArtifactCore/include/Localization/LocalizationEngine.ixx` | 新規: 統合翻訳エンジン |
| P1 | `ArtifactCore/src/Localization/LocalizationEngine.cppm` | 新規: JSON ロード・キー解決・引数置換 |
| P1 | `Artifact/translations/en.json` | 英語ベース（全キーの定義元） |
| P1 | `Artifact/translations/ja.json` | 日本語翻訳 |
| P2 | `Artifact/src/Widgets/Menu/*.cppm` | メニュー文字列の `tr()` 化 |
| P2 | `Artifact/src/Widgets/Timeline/*.cppm` | タイムライン文字列の `tr()` 化 |
| P2 | `Artifact/src/Widgets/PropertyEditor/*.cppm` | プロパティ名の `tr()` 化 |
| P2 | `Artifact/src/Widgets/Dialog/*.cppm` | ダイアログメッセージの `tr()` 化 |
| P3 | `ArtifactCore/include/Localization/LocaleFormatting.ixx` | 新規: 数値・日付フォーマット |
| P4 | `tools/i18n/audit_translations.py` | 新規: カバレッジ監査スクリプト |

## 優先度・工数

| Phase | 優先度 | 工数 | 理由 |
|-------|--------|------|------|
| P1: エンジン統合 | **P0** | 小 | 既存の2つをマージするだけ。大部分は既存コードを動かす |
| P2: UI日本語化 (P0) | **P0** | 中 | 文字列抽出と置換。メニュー系80件+ダイアログ30件 |
| P3: 数値ロケール | **P1** | 小 | QLocale のラッパー追加のみ |
| P4: 監査ツール | **P2** | 小 | Python スクリプト + CI |
