# マイルストーン: Audio Waveform Thumbnail Preview

> 2026-03-31 作成

## 目的

アセットブラウザや関連ビューで、音声ファイルのサムネイルとして波形プレビューを表示する。

単なるアイコンではなく、音の密度や区切りが一目で分かるようにする。

---

## 背景

音声アセットは現在のところ、基本的にアイコン表示に近い。

波形が見えると:

- 無音やピークの有無が分かる
- 近い内容のファイルを見分けやすい
- 編集対象の選定が速くなる

---

## 方針

### 原則

1. wave preview は thumbnail の拡張として扱う
2. 重い解析は非同期化する
3. 表示用の簡易波形と、再生/編集用の高精度波形を分ける
4. まずは mono / stereo の基本波形で十分
5. 未解析時は通常アイコンへ fallback する

### 想定する表示

- 全体の振幅 envelope
- 中央線を基準にした上下波形
- クリップ位置や duration の視覚化

---

## Phase 1: Waveform Extraction

### 目的

音声ファイルから thumbnail 用の波形データを生成する。

### 作業項目

- FFmpeg もしくは既存 audio decode 経路から PCM を取得する
- sample を縮約して preview 用 envelope に落とす
- cache key を file path + size + sample rate で管理する

### 完了条件

- 音声 1 ファイルから軽量波形データを作れる
- 生成が UI を止めない

---

## Phase 2: Thumbnail Rendering

### 目的

waveform を `QPainter` で描画し、サムネイルとして表示する。

### 作業項目

- normalized amplitude を線/面で描く
- background / accent / peak 色を theme と合わせる
- thumbnail cache に波形 pixmap を保存する

### 完了条件

- Asset Browser で音声の見た目が判別しやすい
- 小さな表示でも波形と分かる

---

## Phase 3: UX Integration

### 目的

waveform preview をワークフローに乗せる。

### 作業項目

- hover / detail panel で波形プレビューを出す
- render queue / source inspector と整合する
- long audio の warmup / cache eviction を整える

### 完了条件

- 音声素材がプロっぽく見える
- 検索 / sort / status 表示と共存できる

