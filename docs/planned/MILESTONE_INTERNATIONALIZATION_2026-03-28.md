# インターナショナリゼーション (i18n) Milestone

**作成日:** 2026-03-28  
**ステータス:** 計画中  
**関連コンポーネント:** 全 UI コンポーネント，メッセージシステム

---

## 概要

多言語対応と地域別の設定を可能にし、グローバルなユーザーベースに対応する。

---

## 機能要件

### ★★★ 必須機能

#### 1. 文字列の外部化

- UI 文字列のリソースファイル化
- 翻訳キーの管理
- フォールバック機構

**工数:** 12-16 時間

#### 2. 多言語対応

- 日本語/英語/中国語対応
- 言語切り替え UI
- 実行時言語変更

**工数:** 10-14 時間

#### 3. 日付/数値フォーマット

- 地域別の形式
- 通貨表示
- 単位変換

**工数:** 8-10 時間

### ★★ 重要機能

#### 4. 右から左 (RTL) 対応

- アラビア語/ヘブライ語対応
- UI の反転
- テキスト方向の制御

**工数:** 12-16 時間

#### 5. フォントの多様性

- 多言語フォントのサポート
- フォールバックフォント
- 文字レンダリング

**工数:** 8-10 時間

### ★ 推奨機能

#### 6. 翻訳ワークフロー

- 翻訳プラットフォーム連携
- 自動翻訳の統合
- 翻訳者の管理

**工数:** 10-12 時間

---

## 優先度別実装計画

### P0（必須）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **文字列の外部化** | 12-16h | 🔴 高 |
| **多言語対応** | 10-14h | 🔴 高 |

### P1（重要）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **日付/数値フォーマット** | 8-10h | 🟡 中 |
| **フォント多様性** | 8-10h | 🟡 中 |

### P2（推奨）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **RTL 対応** | 12-16h | 🟢 低 |
| **翻訳ワークフロー** | 10-12h | 🟢 低 |

**合計工数:** 60-78 時間

---

## Phase 構成

### Phase 1: 文字列の外部化

- 目的:
  - コードから文字列を分離

- 作業項目:
  - リソースファイルの作成
  - 翻訳キーの命名規則
  - ビルドシステムの統合

- 完了条件:
  - 全 UI 文字列が外部化
  - 新規文字列は自動的にリソースへ

- 実装案:
  ```cpp
  // 修正前
  button->setText("Save Project");
  
  // 修正後
  button->setText(tr("project.save", "Save Project"));
  
  // リソースファイル (resources/i18n/en.json)
  {
      "project.save": "Save Project",
      "project.load": "Load Project",
      "layer.add": "Add Layer",
      "effect.blur": "Blur"
  }
  
  // リソースファイル (resources/i18n/ja.json)
  {
      "project.save": "プロジェクトを保存",
      "project.load": "プロジェクトを読み込み",
      "layer.add": "レイヤーを追加",
      "effect.blur": "ぼかし"
  }
  ```

### Phase 2: 多言語対応

- 目的:
  - 複数言語での表示

- 作業項目:
  - 言語設定の管理
  - 言語切り替え UI
  - 実行時変更

- 完了条件:
  - 3 言語（日/英/中）対応
  - 再起動なしで言語変更

- 実装案:
  ```cpp
  class LanguageManager {
      Q_OBJECT
      
  public:
      static LanguageManager* instance();
      
      void setLanguage(const QString& langCode) {
          currentLanguage_ = langCode;
          
          // リソース読み込み
          loadTranslations(langCode);
          
          // UI 更新
          emit languageChanged();
      }
      
      QString tr(const QString& key, const QString& fallback) {
          if (translations_.contains(key)) {
              return translations_[key];
          }
          return fallback;
      }
      
  signals:
      void languageChanged();
      
  private:
      QString currentLanguage_;
      QMap<QString, QString> translations_;
  };
  
  // 使用例
  connect(LanguageManager::instance(), &LanguageManager::languageChanged,
          this, [this]() {
              updateAllTexts();  // 全 UI テキストを更新
          });
  ```

### Phase 3: 日付/数値フォーマット

- 目的:
  - 地域別の形式

- 作業項目:
  - QLocale の活用
  - カスタムフォーマッター
  - 単位変換

- 完了条件:
  - 日付/時刻/数値/通貨が地域別
  - フレームレート表示の統一

- 実装案:
  ```cpp
  class LocaleFormatter {
  public:
      static QString formatFrameRate(double fps, const QString& locale) {
          QLocale loc(locale);
          return loc.toString(fps, 'f', 2) + " fps";
      }
      
      static QString formatDuration(qint64 frames, double fps, const QString& locale) {
          QLocale loc(locale);
          double seconds = frames / fps;
          
          QTime time(0, 0, 0);
          time = time.addSecs((int)seconds);
          
          // 地域別の時刻形式
          return loc.toString(time, "HH:mm:ss:zzz");
      }
      
      static QString formatFileSize(qint64 bytes, const QString& locale) {
          QLocale loc(locale);
          
          if (bytes < 1024) {
              return loc.toString(bytes) + " B";
          } else if (bytes < 1024 * 1024) {
              return loc.toString(bytes / 1024.0, 'f', 1) + " KB";
          } else if (bytes < 1024 * 1024 * 1024) {
              return loc.toString(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
          } else {
              return loc.toString(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
          }
      }
  };
  ```

### Phase 4: フォント多様性

- 目的:
  - 多言語文字の表示

- 作業項目:
  - フォールバックフォント
  - 文字エンコーディング
  - レンダリング最適化

- 完了条件:
  - 全言語の文字が表示可能
  - 文字化けなし

### Phase 5: RTL 対応

- 目的:
  - 右から左言語への対応

- 作業項目:
  - UI の反転
  - テキスト方向の制御
  - レイアウト調整

- 完了条件:
  - アラビア語/ヘブライ語対応
  - 自然な表示

### Phase 6: 翻訳ワークフロー

- 目的:
  - 翻訳プロセスの効率化

- 作業項目:
  - 翻訳プラットフォーム連携
  - 自動翻訳の統合
  - バージョン管理

- 完了条件:
  - 翻訳者が Web で作業可能
  - 自動でビルドに統合

---

## 技術的課題

### 1. 文字列長の差異

**課題:**
- 翻訳により文字列長が変化

**解決案:**
- 柔軟なレイアウト
- テキストの折り返し
- フォントサイズの自動調整

### 2. 実行時変更

**課題:**
- 再起動なしでの言語変更

**解決案:**
- シグナル/スロットでの更新
- 状態の保持
- リソースの再読み込み

### 3. 文脈の保持

**課題:**
- 翻訳者に文脈を伝える

**解決案:**
- 翻訳キーへのコメント
- スクリーンショットの提供
- 使用例の記載

---

## 期待される効果

### グローバル対応

| 指標 | 現在 | 改善後 |
|------|------|--------|
| **対応言語** | 日本語のみ | 3+ 言語 |
| **翻訳時間** | 手動 | 自動化 |
| **市場** | 日本 | グローバル |

### ユーザー体験

- 母国語での操作
- 地域別の形式
- 文化的な配慮

---

## 関連ドキュメント

- `docs/planned/MILESTONE_UI_UX_UNIFICATION_2026-03-28.md` - UI/UX 統一
- `docs/planned/MILESTONES_BACKLOG.md` - 全体バックログ

---

## 実装順序の推奨

1. **Phase 1: 文字列外部化** - 基盤
2. **Phase 2: 多言語対応** - 主要機能
3. **Phase 3: フォーマット** - 品質向上
4. **Phase 4: フォント** - 表示品質
5. **Phase 5: RTL** - 追加言語
6. **Phase 6: ワークフロー** - 効率化

---

**文書終了**
