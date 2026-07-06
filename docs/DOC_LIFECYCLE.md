# ドキュメントライフサイクルルール

## 文書の種類と配置

| ディレクトリ | 内容 | ライフサイクル |
|-------------|------|--------------|
| `docs/planned/` | 計画中のマイルストーン | Draft → In Progress → Complete 後は `done/` へ |
| `docs/done/` | 完了したマイルストーン | 6ヶ月以上更新がない場合 `archived/` へ |
| `docs/analysis/` | 分析レポート | 更新が必要になったら上書き、旧版は削除 |
| `docs/technical/` | 技術詳細・設計仕様 | コード変更に合わせて更新 |
| `docs/bugs/` | バグレポート・分析 | 修正確認後も参照用に残す |
| `docs/shared/` | AI 共有メモ | 新しい AI セッションのための引継ぎ |
| `docs/worklog/` | 作業ログ | 作業完了後も履歴として残す |

## ステータス行のルール

`docs/planned/` の全マイルストーン文書は、冒頭に以下の形式でステータスを持つこと：

```
**ステータス:** Not Started / In Progress / Blocked / Complete
```

完了時はファイルを `docs/planned/` から `docs/done/` へ移動し、冒頭に追加する。

**ステータス:** ✅ Complete

## 自動チェック

`tools/generate_doc_inventory.py` は、`docs/INDEX_GENERATED.md` の生成に加えて次を警告する。

- `docs/planned/` に残っている `Complete` 文書
- Markdown 内の絶対パスリンク
- 壊れている相対リンク

運用時は次のように使う。

```powershell
python tools/generate_doc_inventory.py
```
