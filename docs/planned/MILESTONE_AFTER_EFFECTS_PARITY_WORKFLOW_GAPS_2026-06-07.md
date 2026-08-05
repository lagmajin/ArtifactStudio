> **SUPERSEDED** — 2026-08-04: 統合先 [MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md](MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-05-28.md)

# MILESTONE: After Effects Parity Workflow Gaps - 2026-06-07

作成日: 2026-06-07  
対象: AE parity の制作導線ギャップ  
優先度: 🟠 高

---

## 目的

After Effects 風の制作ワークフローで、現状の導線がまだ弱い 3 領域を
ひとつの実行単位としてまとめる。

このマイルストーンは「機能があるか」ではなく、
**制作中に迷わず使えるか / やり直しが少ないか / 直感的か** を基準にする。

---

## 対象ギャップ

### 1. Text Animator: 文字ごとの色変更が直感的でない

不満:
- 一文字ずつ色を変えるには、レイヤー複製 + マスク切り抜き、または expression 頼みになりやすい
- CSS のようにテキスト上で直接範囲を選んで色を変える導線がない

改善:
- テキスト内の範囲指定に color property を設定できるようにする
- `Animator > Fill` のような既存の仕組みを、テキストエディタ上の範囲選択 + color picker へ近づける
- range / selector の見せ方を、編集対象に直接触っている感覚へ寄せる

完了条件:
- テキスト上で範囲選択した部分に色を割り当てられる
- 既存の text animator 構造と矛盾しない
- 複製やマスクに逃げなくても基本的な文字色アニメーションが成立する

実装の読み替え:
- range selector の「どの glyph に効くか」と color property の「何を変えるか」を分けて扱う
- まずは text animator の既存 property 系に寄せ、テキストエディタの selection を入口にする
- selection は単一 glyph, grapheme cluster, word, span のどれを対象にしているかを明示する

Phase 1:
- テキストエディタ上で selection range を取得できるようにする
- 選択範囲に対して color property を適用する最小経路を作る
- 既存 animator / effect stack の property 表示と同じ文法で表す

Phase 2:
- 範囲選択から color picker を直接開けるようにする
- 直前に使った color を履歴として再利用できるようにする
- selection を複数範囲に拡張し、部分的な色分けを自然にする

Phase 3:
- animator の fill / color と text editor selection を双方向同期する
- timeline 上で color-animator の存在が読めるようにする
- preset / reuse flow に接続する

---

### 2. Footage Interpret: frame rate 変更で time remap が壊れやすい

不満:
- 素材の frame rate を変更したとき、既に打っていた keyframe や time remap がずれる
- 後から interpret footage を触ると、やり直しになりやすい

改善:
- frame rate 変更時に `keyframes を維持 / time を維持 / pixel 補間` のような選択肢を出す
- 既存の time remap や keyframe が維持できない場合は警告を出す
- 変更後の影響を preview で確認できるようにする

完了条件:
- frame rate 変更時の影響が明示される
- 既存の時間情報を壊す場合、事前に分かる
- interpret の変更がワークフロー破壊ではなく、調整操作として扱える

実装の読み替え:
- interpret は「ソースメタデータの変更」だが、実際には layer 時間軸へ影響する
- そのため、変更前にどの契約が壊れるかを先に見せる
- time remap / keyframe / source duration の 3 つを同じ安全確認の文脈で扱う

Phase 1:
- frame rate 変更時に影響対象の事前検査を行う
- `keyframes を維持 / time を維持 / pixel 補間` の選択肢を出す
- 変更で崩れる対象がある場合は警告を出す

Phase 2:
- 変更後の frame-to-time 対応を preview で確認できるようにする
- time remap が存在する場合は、再サンプル方針を明示する
- undo で変更前に戻せるようにする

Phase 3:
- interpret の変更を project 層の metadata action として分離する
- existing keyframe / remap state の保存と再適用を安全化する
- 変更不能なケースでは理由を明示して操作を止める

---

### 3. Proxy Workflow: 生成と切り替えが面倒

不満:
- 重い素材に proxy を使う導線が分散している
- 生成、解像度指定、全体の on/off が別々で分かりにくい

改善:
- 右クリックや近い導線から proxy をワンクリック生成できるようにする
- proxy の解像度を簡単に指定できるようにする
- 全素材の proxy を一括で on/off する global switch を用意する

完了条件:
- 生成導線が 1 アクションに近い
- proxy の quality が理解しやすい
- project 全体で proxy をまとめて扱える

実装の読み替え:
- proxy は layer 単位の補助資産で、project 全体では policy と状態の両方を持つ
- 生成、参照、切替、無効化を別 UI に散らさない
- 既存の preview quality と混ぜず、proxy は source replacement の文脈で扱う

Phase 1:
- project / asset browser のコンテキストメニューから proxy generate を起動する
- proxy 解像度を簡単に選べる UI を置く
- 生成状態を asset metadata に保存する

Phase 2:
- video layer / footage layer が proxy path を優先参照できるようにする
- proxy の有効 / 無効を item 単位で切り替える
- 既存の source と proxy のどちらを読んでいるかを UI で見える化する

Phase 3:
- project 全体の global switch を追加する
- proxy の一括再生成 / 一括無効化を整理する
- stale proxy の検出と再生成導線を作る

---

## 実装の読み替え

この 3 つは別機能に見えるが、共通点はかなり大きい。

- Text Animator は「直接編集できる text workflow」
- Interpret Footage は「ソース解釈を後から安全に変える workflow」
- Proxy は「重い素材を軽く扱う workflow」

どれも、単体の機能追加よりも **編集導線の再設計** が主題になる。

---

## 詳細実装スライス

### A. Text Animator Range Color Editing

#### 入口
- Text editor selection
- Inspector の text animator section
- Timeline 上の text animator track

#### 触るもの
- `ArtifactTextLayer`
- `TextAnimatorEngine`
- `ArtifactPropertyEditor`
- `ArtifactTimelineWidget`
- `ArtifactTimelineTrackPainterView`
- text selection model
- color picker entry
- text animator preset / property row
- selection to range resolver

#### データ契約
- selection range
- glyph / cluster / span 単位
- color property の target range
- animator preset との互換性
- selection anchor / focus
- color application mode
- range merge / split policy
- active text style state

#### 実装順
1. selection から range を取る
2. selection に色を当てる
3. 既存 animator の color property と接続する
4. timeline 表示と preset 再利用を足す
5. multiple selection の扱いを整理する
6. text editor と inspector の state を同期する
7. preview / undo / redo の契約を固める

#### 失敗時の扱い
- grapheme cluster を壊す selection は無効化する
- selection が空なら color action を無効化する
- editor と animator の対象がずれたら警告を出す
- selection が text range として解釈できない場合は action を止める
- partially applied color は silent に残さず、失敗箇所を明示する

#### 詳細フェーズ

##### Phase 1: selection to range
- [ ] text editor から selection range を取得する
- [ ] glyph / cluster / span のどれに当たるかを判定する
- [ ] 空 selection 時は color action を disabled にする

##### Phase 2: direct color application
- [ ] selection に対して color property を適用する最小経路を作る
- [ ] 直前の color を再利用できるようにする
- [ ] 既存の text style と競合する場合は優先順位を明示する

##### Phase 3: animator integration
- [ ] text animator の fill / color property に接続する
- [ ] property row の見え方を inspector と揃える
- [ ] timeline 上で color animator の存在が読めるようにする

##### Phase 4: multi-range and reuse
- [ ] 複数範囲への color assignment を扱う
- [ ] preset / reuse flow に接続する
- [ ] selection ごとの色を復元できるようにする

##### Phase 5: undo / preview safety
- [ ] color change を undo / redo できるようにする
- [ ] preview で色変化が見えてから確定できるようにする
- [ ] editor 側の state と timeline 側の state を一致させる

#### 具体的 UI 案
- テキスト上で範囲選択すると、近くに color chip を出す
- chip を押すと既存の color picker を開く
- inspector の text animator section に `Apply to Selection` を置く
- timeline では color animator の存在を badge で示す
- 選択範囲が複数ある場合は summary chip に数を表示する

#### 競合ルール
- selection に既存 color animation がある場合は上書き前に示す
- span と glyph が混ざる操作は明示的に分ける
- selection が cluster をまたぐ場合は cluster 単位で丸める
- text style の静的 color と animator color が競合する場合は、どちらが最終値かを一貫させる

### B. Footage Interpret Safety

#### 入口
- Project View の footage action
- Inspector の footage metadata editor
- File import / relink 周辺

#### 触るもの
- footage metadata / source state
- time remap state
- keyframe storage
- preview cache invalidation
- interpret dialog / confirmation surface
- source duration / frame rate presentation

#### データ契約
- source frame rate
- comp frame rate
- layer time mapping
- keyframe alignment policy
- source duration
- remap sampling policy
- preserve mode

#### 実装順
1. 変更前診断を入れる
2. 維持方針を選ばせる
3. 変更後 preview を出す
4. undo / restore を安定化する
5. 変更不能ケースの警告文を整える
6. project / layer 側の再評価を同期する

#### 失敗時の扱い
- 互換維持できない場合は差分を警告する
- time remap が破綻する場合は適用を止める
- 再計算不能な場合は明示的に fallback を返す
- 変更後の表示が元と異なる場合は、適用前に明示する
- 自動補正で意味が変わる場合は silent fix しない

#### 詳細フェーズ

##### Phase 1: preflight と選択肢提示
- [ ] frame rate 変更前に影響対象を列挙する
- [ ] `keyframes を維持 / time を維持 / pixel 補間` の 3 択を出す
- [ ] time remap がある layer は警告を強める

##### Phase 2: preview と再計算
- [ ] 変更後の frame-to-time 対応を preview で見せる
- [ ] 既存 keyframe の位置がどう動くかを示す
- [ ] remap 曲線がある場合は再サンプル結果を確認できるようにする

##### Phase 3: 永続化と復元
- [ ] interpret change を metadata action として記録する
- [ ] undo / redo で元の frame rate に戻せるようにする
- [ ] project 再読み込み時も preserve mode を再現する

##### Phase 4: 安全策の強化
- [ ] 変更不能な素材は理由を明示して操作を止める
- [ ] preview と実適用の差をなくす
- [ ] source / layer / timeline の表記を統一する

#### 具体的 UI 案
- 変更ダイアログに `Keep Keyframes`, `Keep Time`, `Re-sample` を並べる
- 右側に impact summary を出す
- time remap が含まれる場合は warning badge を出す
- 適用前後の frame mapping を small preview で表示する

### C. Proxy Workflow

#### 入口
- Project View の context menu
- Asset Browser の context menu
- Playback / Viewer の proxy toggle

#### 触るもの
- proxy generation job
- proxy quality / resolution
- source path / proxy path
- global proxy enable state
- proxy status badge
- proxy job queue
- stale / missing proxy detection

#### データ契約
- source asset identity
- proxy asset location
- quality preset
- stale 判定条件
- generation timestamp
- source checksum or revision
- per-item enable state

#### 実装順
1. context menu から proxy を生成する
2. item 単位で on/off する
3. global switch を追加する
4. stale proxy の再生成を足す
5. quality / resolution preset を整理する
6. batch generate / batch refresh を足す

#### 失敗時の扱い
- proxy が無い場合は source に戻す
- 生成失敗時は理由を表示する
- stale proxy は見つけた時点で再生成候補に上げる
- source との不一致が大きい場合は proxy を無効化する
- 生成中に source が変わったら job を invalid にする

#### 詳細フェーズ

##### Phase 1: context menu generation
- [ ] asset / footage の context menu から generate を起動する
- [ ] 解像度の quick preset を選ばせる
- [ ] 生成結果を asset metadata に保存する

##### Phase 2: per-item switching
- [ ] item 単位で proxy on/off を切り替える
- [ ] source と proxy のどちらを使っているかを badge で見せる
- [ ] viewer / playback 側で現在の参照先を明示する

##### Phase 3: global switch
- [ ] 全素材の proxy を一括で enable / disable する
- [ ] composition / project 単位の global state を持つ
- [ ] toggle 後に cache を整理する

##### Phase 4: stale management
- [ ] source revision と proxy revision を比較する
- [ ] stale を見つけたら再生成候補にする
- [ ] missing proxy は source fallback に倒す

#### 具体的 UI 案
- context menu の先頭に `Generate Proxy...`
- 生成ダイアログに `1/4`, `1/2`, `Custom` を並べる
- item badge に `Proxy`, `Stale`, `Off` を表示する
- viewer footer に global proxy switch を置く

---

## 推奨実行順

1. Text Animator の色範囲編集
2. Footage Interpret の frame rate 変更保護
3. Proxy workflow の統合

理由:
- Text Animator は UI の直感性に直結し、制作中の満足度改善が大きい
- Interpret Footage は事故コストが高いので、先に安全化したい
- Proxy は運用改善なので、最後にまとめて整理すると収まりがよい

---

## 検証条件

- Text Animator で selection した範囲にだけ色が乗る
- Interpret 変更時に既存 keyframe / time remap の扱いが明示される
- Proxy 生成と切り替えが project 内の 1 つの導線にまとまる
- どの機能も「何が壊れるか」が先に分かる
- どの機能も、既存の timeline / inspector / preview と文法がずれない

---

## 関連

- [`docs/planned/MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_TEXT_ANIMATOR_NEXT_GEN_2026-04-18.md)
- [`docs/done/MILESTONE_TEXT_EFFECT_WORKFLOW_BRIDGE_2026-05-25.md`](X:/Dev/ArtifactStudio/docs/done/MILESTONE_TEXT_EFFECT_WORKFLOW_BRIDGE_2026-05-25.md)
- [`docs/planned/MILESTONE_VIDEO_PROXY_IMPROVEMENT_2026-03-28.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_VIDEO_PROXY_IMPROVEMENT_2026-03-28.md)
- [`docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-04-08.md`](X:/Dev/ArtifactStudio/docs/planned/MILESTONE_AFTER_EFFECTS_PARITY_GAP_2026-04-08.md)
- [`docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md`](X:/Dev/ArtifactStudio/docs/analysis/AFTER_EFFECTS_MISSING_FEATURES_CURRENT_2026-05-28.md)

---

## 備考

- これは実装タスクの詳細設計ではなく、AE parity の不足を実行可能な単位に切り分けるためのミドルレイヤー文書。
- ビルドやテストは実施していない。
