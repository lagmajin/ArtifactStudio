# マイルストーン: Project Health Problem View Phase 2

**作成日:** 2026-05-25  
**ステータス:** 計画中  
**優先度:** 高  
**関連:** `docs/DIAGNOSTIC_SYSTEM_MILESTONE.md`, `docs/DIAGNOSTIC_SYSTEM_BACKLOG.md`, `Artifact\docs\MILESTONE_PROJECT_HEALTH_PROBLEM_VIEW_2026-04-14.md`

---

## 概要

診断系は、ログ出力やデバッグコンソールとは別に「問題を先に見つける」ための面が必要になる。
このフェーズでは、project health / problem view を実運用で役立つ状態に寄せ、壊れる前の気づきを増やす。

---

## Phase 1: Diagnostic Core Refresh

**目標:** 問題検出の基盤を、UI から使いやすい形へ整える。

- [ ] diagnostic item の種別 / 重要度 / 発生源 / 修復候補を整理する
- [ ] project scan の結果を安定した順序で返す
- [ ] selection change で更新できるようにする
- [ ] 失敗しても diagnostic UI 自体が壊れないようにする

## Phase 2: Problem View Surface

**目標:** 問題一覧を一目で把握できる surface を作る。

- [ ] Error / Warning / Info を見た目で即判別できるようにする
- [ ] source layer / composition / asset の参照先を辿れるようにする
- [ ] filter / search / grouping を最低限整える
- [ ] render / timeline / asset 系の問題を同じ一覧で扱う

## Phase 3: Validation Hooks

**目標:** 問題を見つけたあとに、自然な導線で修正へ移れるようにする。

- [ ] project open 時に軽い検証を走らせる
- [ ] render start 前に致命的な問題だけブロックできるようにする
- [ ] missing reference / stale link / circular dependency を扱う
- [ ] 修復アクションがあるものは UI から実行できるようにする

## Phase 4: Health Summary Integration

**目標:** 問題一覧を、プロジェクト全体の健康状態として見せる。

- [ ] project manager / dashboard から summary を見える化する
- [ ] warning 数や error 数を軽く表示する
- [ ] active project の状態を切り替えに追従させる
- [ ] diagnostics を debug だけの機能にしない

---

## 完了条件

1. 問題を一覧で見つけられる
2. 問題の出所をすぐ辿れる
3. 重大な問題は render 前に止められる
4. Project 健全性が UI 上で見える
5. デバッグログとは役割が重ならない

## 2026-07-25 実装監査

DiagnosticEngine／ProjectDiagnostic の severity・category・source・fix action、health report から diagnostic への変換、missing／matte／circular／performance の検査、Problem View／health dashboard の基盤は確認した。一方、Error／Warning／Info の一貫した視覚表示、source layer／composition／asset への追跡、render／timeline／asset の横断一覧、open／render前の validation hook、致命Errorの確実なblock、修復アクションの実行とsummary追従は、静的検索だけでは完了を断定できない。Phase 1〜4 は部分実装・統合／runtime未検証とする。
