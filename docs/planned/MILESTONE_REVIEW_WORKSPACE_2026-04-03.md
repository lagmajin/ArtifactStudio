# Review Workspace: Frame-Accurate Compare & Annotation Milestone (2026-04-03)

## Goal

制作中のコンポジション・レンダリング結果を、**「見えてるもの＝最終結果に近い」**状態でレビューできる,
独立した Workspace を提供する。

5つのコア要件:
1. **正確な再生** - フレームドロップなし、タイムコード固定、LUT/OCIO 適用
2. **A/B比較** - ワンタッチ切替、wipe（左右スライド）、overlay 差分
3. **ショット管理** - v001/v002/final などのバージョン管理、複数ショット並列レビュー
4. **注釈 (Annotation)** - フレームに直接描画、コメント付与、チーム共有
5. **ナビゲーション特化** - JKL シャトル再生、フレーム単位移動、範囲ループ

これにより「制作UI」と「レビューUI」を分離し、確認作業の精度と速度を大幅に向上させる。

## Scope

- `Artifact/src/Widgets/Render/ArtifactCompositionEditor.cppm` (review mode 追加)
- `Artifact/src/Widgets/Render/ArtifactRenderController.cppm` (frame-accurate timing)
- `Artifact/src/Render/ArtifactIRenderer.cppm` (LUT pipeline, wipe overlay)
- `ArtifactCore/src/Color` (OCIO integration, LUT application)
- `Artifact/src/Service/ArtifactAnnotationService.cpp` (新規)
- `Artifact/src/Widgets/Review/` (新規: ReviewWorkspace, ShotManager, CompareWidget)
- `Artifact/src/Preview` (frame cache, accurate seek)
- `ArtifactCore/src/Media/VideoDecode.cppm` (hwaccel aware frame-accurate decode)

## Non-Goals

- 実レンダリング（エンコード）自体の実装
- クラウド共有・リアルタイム共同編集（ローカルレビューに限定）
- 既存タイムラインの廃止（独立 workspace として追加）
- audio sync の全面再設計（M-AU が担当）

## Milestones

### M-FE-7-1 Review Mode Architecture

**目的:** 制作モードとレビューモードの明確な分離と切り替え基盤を作る。

- Review Workspace の Manager ウィジェットを作る (`ArtifactReviewWorkspace`)
- Composition Editor に `ReviewMode` フラグを追加（ overlay, wipe, annotation レイヤー状態）
- PlaybackController を拡張して review-specific timing policy を適用
- UI レイアウト: full-screen preview + minimal HUD (shot list, playback controls, annotation tools)

### M-FE-7-2 Frame-Accurate Playback & LUT Pipeline

**目的:** ビデオレイヤー・コンポジションの再生に frame-accurate 性とカラーマネージメントを保証する。

- `PlaybackEngine` に `frame-accurate seek` モード追加（current frame 追跡、decode cache 利用）
- `FFmpegBackend` を拡張して `avcodec_flush_buffers` を最小化し、シーク後のドロップを防ぐ
- `ArtifactIRenderer` に `setLUT` / `setOCIOConfig` API を追加
- LUT は composer の final output 段階で適用（overlay は適用後、annotation は適用前）
- タイムコード表示（HH:MM:SS:FF）と accurate frame count を HUD に常時表示

### M-FE-7-3 A/B Comparison Engine

**目的:** 前バージョン・レンダリング結果と現在の状態をワンタッチで比較。

- `CompareSession` クラス: `Base` vs `Review` コンポジション・レイヤーセット管理
- 比較モード: `Swap` (A/B), `Wipe` (左右スライド), `Diff` (overlay 差分)
- Wipe は垂直 line を mouse drag で動かす（Nuke 風）
- Diff モードは `abs(A-B)`, `Blend` (50%/50%), `EdgeDetect` を選べる
- ショートカット: `Tab` で swap, `W` で wipe 切替, `D` で diff

### M-FE-7-4 Shot Management System

**目的:** プロジェクト内の複数バージョン・複数シーンを切り替えてレビュー。

- `Shot` entity: 複数の `CompositionVersion` 关联付け (v001, v002, final, etc.)
- `ShotManager` service: 現在アクティブな shot を Workspace に提供
- UI: ショットリスト（名前, 日付, コメント）とバージョン切替プルダウン
- ショット切り替え時に、関連 composition を一括で読み込み、compare base に設定
- ショットカバーサムネイル生成（初回は非同期）

### M-FE-7-5 Annotation System

**目的:** レビュー中にフレームに注釈を描き、フィードバックを記録する。

- `AnnotationLayer` モデル: ベクトル図形（line, arrow, rect, text, freehand）
- ツール: Pen, Arrow, Rect, Text, Eraser
- 描画は composition 座標系で保存（解像度非依存）
- 描画は `ArtifactIRenderer` overlay レイヤーでリアルタイム表示
- コメント付きのアノテーション: クリックでテキストノート付与
- エクスポート: `JSON` (レイヤー付き) と `flattened PNG` (焼き込み)

### M-FE-7-6 Navigation & Playback Polish

**目的:** レビュー専用の操作 Gideon とショートカットを整備。

- JKL シャトル: `J` 逆再生, `K` 停止, `L` 正再生（速度増加）
- JKL はプロ編集者の基本操作として扱い、押しっぱなしで速度段階を上げる挙動を前提にする
- フレーム単位移動: `Left/Right` (1f), `Shift+Left/Right` (10f)
- 範囲ループ再生: `I/O` キーで in/out 設定, `L` 押し続けで loop
- レビューモード時、通常タイムラインのショートカットと競合しないよう隔離
- HUD に speed と loop range を表示

## Recommended Order

1. `M-FE-7-1 Review Mode Architecture` (workspace framework)
2. `M-FE-7-2 Frame-Accurate Playback & LUT Pipeline` (必須基盤)
3. `M-FE-7-3 A/B Comparison Engine` (比較機能)
4. `M-FE-7-4 Shot Management System` (ショット管理)
5. `M-FE-7-5 Annotation System` (注釈)
6. `M-FE-7-6 Navigation & Playback Polish` (操作改善)

## Dependencies

- `M-AU-2 Playback Sync` (timer accuracy)
- `M-RD-1 Software Render Pipeline` (preview fidelity)
- `M-UI-14 QSS Reduction` (HUD スタイル)
- `ArtifactCore/docs/MILESTONE_TEXT_SYSTEM_2026-03-12.md` (annotation text rendering)

## Validation Checklist

- [ ] review mode 切り替えで通常編集と隔離される
- [ ] LUT 適用後の見た目が静止画でも動画でも一貫する
- [ ] A/B swap がショートカットで瞬時に行える
- [ ] Wipe line が滑らかに追従し、像素ずれしない
- [ ] Shot 切替で前バージョンが自動で base になる
- [ ] Annotation 描画が解像度変えても座標 unreserved される
- [ ] JKL 速度が段階的で、停止と逆再生が可能
- [ ] 全モードでタイムコード表示が常に正確

## Notes

- 既存 Composition Editor と機能を共有するが、Review Workspace は independent widget として実装する
- Annotation data はプロジェクトに保存可能にしておく（M-AS の Asset System と連携）
- LUT/OCIO は Review Workspace 全体で設定し、各 widget に broadcast する設計とする

---

## Next Execution Slice

M-FE-7-1 は、Review Workspace を独立 widget として立てる前に、編集モードとの境界を先に固定する。

### M-FE-7-1A の着手点

1. `ReviewMode` を overlay / wipe / annotation の状態集合として定義する
2. `ArtifactCompositionEditor` の通常編集と review mode を分離する
3. `ArtifactReviewWorkspace` は full-screen preview と minimal HUD を持つ器にする
4. review-specific timing policy は playback/controller 側に閉じる

### M-FE-7-1 完了条件

- review mode 切り替えで通常編集と隔離される
- workspace が独立 widget として読める
- overlay / wipe / annotation の状態が review mode にまとまる

### M-FE-7-2A の着手点

1. `frame-accurate seek` を review-specific timing policy に接続する
2. current frame 追跡と decode cache の接続点を先に固定する
3. `setLUT` / `setOCIOConfig` の適用位置を final output 段で決める
4. HUD に timecode と accurate frame count を常時出す前提を作る

### M-FE-7-2 完了条件

- LUT 適用後の見た目が静止画でも動画でも一貫する
- タイムコード表示が常に正確
- review timing が frame-accurate として追える

### M-FE-7-3 への前提

- A/B 比較は review mode と frame-accurate playback が固まってから入れる
- shot management は比較セッションの base/review 契約が固まってから詰める

### M-FE-7-4A の着手点

1. `Shot` entity を `CompositionVersion` の束として定義する
2. `ShotManager` が current active shot を workspace に提供する責務を持つ
3. ショットリストとバージョン切替プルダウンの最小 UI を用意する
4. ショット切り替え時に compare base を自動で切り替える前提を決める

### M-FE-7-4 完了条件

- shot 切替で前バージョンが自動で base になる
- ショット一覧とバージョン切替が 1 本の導線になる
- compare と shot management の責務が混ざらない

### M-FE-7-5A の着手点

1. annotation は composition 座標系で保存する前提を固定する
2. Pen / Arrow / Rect / Text / Eraser を review workspace 上で扱う
3. クリックでテキストノートを付ける最小導線を決める
4. annotation data を project に保存できる前提を置く

### M-FE-7-5 完了条件

- review 中にフレームへ注釈を残せる
- annotation が compare / shot management と衝突しない
- 保存先と表示先の責務が分かる

### M-FE-7-6A の着手点

1. `J / K / L` のシャトルと `Left / Right` の 1f 移動を分ける
2. `I / O` の範囲ループと speed 表示を HUD に出す
3. review mode 時は通常 timeline shortcut と競合しないよう隔離する
4. `Tab` の compare 切替との役割を混同しない

### M-FE-7-6 完了条件

- JKL 速度が段階的で、停止と逆再生が可能
- 範囲ループ再生の状態が読める
- review navigation が他モードと衝突しにくい
