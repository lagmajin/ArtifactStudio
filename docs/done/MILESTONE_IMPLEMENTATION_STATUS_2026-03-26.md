# Milestone Implementation Status (2026-03-26)

この文書は、現在の docs 上で「実装済み」と整理しやすいマイルストーンをまとめたもの。
未完了のものを無理に完了扱いにせず、確度の高いものだけを記録する。

## Confirmed Complete

### M-TL-1 Layer Basic Operations

- 追加
- 削除
- 複製
- rename
- 親子
- 並び替え

詳細は [docs/done/M_TL_COMPLETION_REPORT.md](/c:/Users/kukul/OneDrive/デスクトップ/Programming/ArtifactStudio/docs/done/M_TL_COMPLETION_REPORT.md) を参照。

### M-TL-2 Layer View Sync

- 左ツリーと右トラック行の同期
- 1 レイヤー 1 クリップの維持
- 展開 / 折りたたみ

詳細は [docs/done/M_TL_COMPLETION_REPORT.md](/c:/Users/kukul/OneDrive/デスクトップ/Programming/ArtifactStudio/docs/done/M_TL_COMPLETION_REPORT.md) を参照。

### M-TL-3 Work Area / Range Unification

- in / out
- work area
- seek
- render 範囲の一本化

詳細は [docs/done/M_TL_COMPLETION_REPORT.md](/c:/Users/kukul/OneDrive/デスクトップ/Programming/ArtifactStudio/docs/done/M_TL_COMPLETION_REPORT.md) を参照。

## Recently Implemented but Not Yet Promoted

### Dock / Tab Polish

- Dock タブの inline style と stylesheet 競合を抑える修正
- 変更メモは [Artifact/docs/done/SESSION_WORK_2026-03-26.md](/c:/Users/kukul/OneDrive/デスクトップ/Programming/ArtifactStudio/Artifact/docs/done/SESSION_WORK_2026-03-26.md) に記録

### Light Layer Wiring

- `LayerType::Light` の生成導線
- project 側の型推定
- layer menu からの追加導線

## In Progress or Partial

- `M-CE Composition Editor & Layer View`
- `M-AS-3 Save / Load Integrity`
- `M-QA-1 Software Test Windows`
- `M-AR-3 Serialization Cleanup`
- `M-LV-1 Layer Solo View (Diligent)`

## Notes

- 今後は、完了が明確になったら backlog 側の該当項目にも `✅ 完了` を付ける
- ただし、`Layer Solo View` や保存系は scope が広いので、docs と実装の両方が揃った段階で更新する

