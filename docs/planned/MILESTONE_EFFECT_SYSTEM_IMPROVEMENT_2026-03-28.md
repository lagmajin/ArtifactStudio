# エフェクトシステム改善 Milestone

**作成日:** 2026-03-28  
**ステータス:** 計画中  
**関連コンポーネント:** ArtifactEffect, EffectManager, GPU Pipeline, PropertyEditor

---

## 概要

エフェクトシステムを改善し、GPU 処理の拡充と CPU/GPU パイプラインの統一を実現する。

---

## 発見された問題点

### ★★★ 問題 1: GPU エフェクトの不足

**場所:** `ArtifactCore/include/Graphics/ComputePipeline.ixx`

**問題:**
- CPU エフェクトのみ実装
- GPU エフェクトが未実装
- 処理が重い

**影響:**
- リアルタイムプレビューが遅い
- 複雑なエフェクトが適用できない

**工数:** 16-20 時間

---

### ★★★ 問題 2: エフェクトスタックの管理不足

**場所:** `Artifact/src/Widgets/ArtifactInspectorWidget.cppm`

**問題:**
- エフェクトの順序変更が困難
- 有効/無効の切り替えがない
- プリセット管理がない

**工数:** 10-14 時間

---

### ★★ 問題 3: エフェクトパラメータの同期不足

**場所:** 全体

**問題:**
- プロパティパネルとの同期が不完全
- キーフレームとの連携がない
- リアルタイム更新が遅い

**工数:** 8-10 時間

---

### ★★ 問題 4: エフェクトの相互運用性

**場所:** 全体

**問題:**
- CPU/GPU エフェクトの混在
- 変換処理のオーバーヘッド
- 品質のばらつき

**工数:** 12-16 時間

---

### ★ 問題 5: エフェクトライブラリの不足

**場所:** 全体

**問題:**
- 基本エフェクトのみ
- カスタムエフェクトの作成が困難
- サードパーティ製エフェクトの読み込みがない

**工数:** 14-18 時間

---

## 優先度別実装計画

### P0（必須）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **GPU エフェクト実装** | 16-20h | 🔴 高 |
| **エフェクトスタック管理** | 10-14h | 🔴 高 |

### P1（重要）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **パラメータ同期** | 8-10h | 🟡 中 |
| **CPU/GPU 統一** | 12-16h | 🟡 中 |

### P2（推奨）

| 項目 | 工数 | 優先度 |
|------|------|--------|
| **エフェクトライブラリ** | 14-18h | 🟢 低 |

**合計工数:** 60-78 時間

---

## Phase 構成

### Phase 1: GPU エフェクト実装

- 目的:
  - リアルタイム処理を可能に

- 作業項目:
  - Compute Shader ベースのエフェクト
  - GPU パイプラインの整備
  - メモリ転送の最適化

- 完了条件:
  - Blur, Glow, Color Correction を GPU で処理
  - 60fps でプレビュー

- 実装案:
  ```cpp
  class GPUEffectBlur : public IGPUEffect {
  public:
      GPUEffectBlur() {
          // Compute Shader の作成
          shader_ = createComputeShader(R"(
              RWTexture2D<float4> output : register(u0);
              Texture2D<float4> input : register(t0);
              cbuffer Params : register(b0) {
                  float radius;
                  float2 direction;
              };
              
              [numthreads(16, 16, 1)]
              void main(uint3 dispatchThreadID : SV_DispatchThreadID) {
                  float4 color = float4(0, 0, 0, 0);
                  float totalWeight = 0;
                  
                  for (int i = -radius; i <= radius; i++) {
                      float2 offset = direction * i;
                      float weight = gaussianWeight(i, radius);
                      color += input.Load(int3(dispatchThreadID.xy + offset, 0)) * weight;
                      totalWeight += weight;
                  }
                  
                  output[dispatchThreadID.xy] = color / totalWeight;
              }
          )");
      }
      
      void process(IDeviceContext* ctx, 
                   ITextureView* input, 
                   ITextureView* output) override {
          // パラメータ設定
          shaderResources_->GetVariableByName("radius")->Set(radius_);
          shaderResources_->GetVariableByName("direction")->Set(direction_);
          
          // 実行
          ctx->SetPipelineState(pipelineState_);
          ctx->CommitShaderResources(shaderResources_);
          ctx->Dispatch(computeDimension(width_, height_, 16));
      }
      
  private:
      float gaussianWeight(int x, float radius) {
          float sigma = radius / 3.0f;
          return exp(-(x * x) / (2 * sigma * sigma));
      }
      
      IComputePipelineState* pipelineState_;
      IShaderResourceBinding* shaderResources_;
      float radius_ = 5.0f;
      float2 direction_ = float2(1, 0);
  };
  ```

### Phase 2: エフェクトスタック管理

- 目的:
  - 効率的なエフェクト管理

- 作業項目:
  - ドラッグ＆ドロップで順序変更
  - 有効/無効の切り替え
  - プリセット保存/読み込み

- 完了条件:
  - エフェクトの順序を自由に変更
  - プリセットをワンクリックで適用

- 実装案:
  ```cpp
  class EffectStackWidget : public QWidget {
      Q_OBJECT
      
  public:
      EffectStackWidget(QWidget* parent = nullptr)
          : QWidget(parent) {
          
          auto layout = new QVBoxLayout(this);
          
          // エフェクトリスト
          effectList_ = new QListWidget();
          effectList_->setDragDropMode(QAbstractItemView::InternalMove);
          effectList_->setSelectionMode(
              QAbstractItemView::ExtendedSelection);
          layout->addWidget(effectList_);
          
          // コントロール
          auto controlLayout = new QHBoxLayout();
          
          addBtn_ = new QPushButton("Add Effect");
          controlLayout->addWidget(addBtn_);
          
          removeBtn_ = new QPushButton("Remove");
          controlLayout->addWidget(removeBtn_);
          
          upBtn_ = new QPushButton("↑");
          controlLayout->addWidget(upBtn_);
          
          downBtn_ = new QPushButton("↓");
          controlLayout->addWidget(downBtn_);
          
          enableBtn_ = new QPushButton("Enable");
          enableBtn_->setCheckable(true);
          controlLayout->addWidget(enableBtn_);
          
          layout->addLayout(controlLayout);
          
          // プリセット
          presetCombo_ = new QComboBox();
          layout->addWidget(presetCombo_);
          
          savePresetBtn_ = new QPushButton("Save Preset");
          layout->addWidget(savePresetBtn_);
          
          // シグナル接続
          connect(addBtn_, &QPushButton::clicked,
                  this, &EffectStackWidget::addEffect);
          connect(removeBtn_, &QPushButton::clicked,
                  this, &EffectStackWidget::removeEffect);
          connect(upBtn_, &QPushButton::clicked,
                  this, &EffectStackWidget::moveEffectUp);
          connect(downBtn_, &QPushButton::clicked,
                  this, &EffectStackWidget::moveEffectDown);
          connect(enableBtn_, &QPushButton::toggled,
                  this, &EffectStackWidget::toggleEffect);
          connect(effectList_->model(), 
                  &QAbstractItemModel::rowsMoved,
                  this, &EffectStackWidget::onOrderChanged);
      }
      
      void setLayer(ArtifactLayer* layer) {
          currentLayer_ = layer;
          refreshList();
      }
      
  signals:
      void effectOrderChanged();
      void effectToggled(int index, bool enabled);
      
  private slots:
      void addEffect() {
          // エフェクト選択ダイアログ
          EffectSelectionDialog dialog(this);
          if (dialog.exec() == QDialog::Accepted) {
              auto effect = dialog.selectedEffect();
              currentLayer_->addEffect(effect);
              refreshList();
          }
      }
      
      void removeEffect() {
          auto items = effectList_->selectedItems();
          for (auto item : items) {
              int index = effectList_->row(item);
              currentLayer_->removeEffect(index);
          }
          refreshList();
      }
      
      void moveEffectUp() {
          int index = effectList_->currentRow();
          if (index > 0) {
              currentLayer_->moveEffect(index, index - 1);
              refreshList();
          }
      }
      
      void moveEffectDown() {
          int index = effectList_->currentRow();
          if (index >= 0 && index < effectList_->count() - 1) {
              currentLayer_->moveEffect(index, index + 1);
              refreshList();
          }
      }
      
      void refreshList() {
          effectList_->clear();
          
          if (!currentLayer_) return;
          
          for (const auto& effect : currentLayer_->effects()) {
              auto item = new QListWidgetItem(effect->name());
              item->setFlags(item->flags() | Qt::ItemIsEditable);
              item->setCheckState(
                  effect->isEnabled() ? 
                  Qt::Checked : Qt::Unchecked);
              effectList_->addItem(item);
          }
      }
      
  private:
      QListWidget* effectList_;
      QPushButton *addBtn_, *removeBtn_, *upBtn_, *downBtn_, *enableBtn_;
      QComboBox* presetCombo_;
      QPushButton* savePresetBtn_;
      ArtifactLayer* currentLayer_ = nullptr;
  };
  ```

### Phase 3: パラメータ同期

- 目的:
  - リアルタイムでのパラメータ調整

- 作業項目:
  - プロパティパネルとの連携
  - キーフレームとの同期
  - プレビューの即時更新

- 完了条件:
  - パラメータ変更が即座に反映
  - キーフレームと連動

### Phase 4: CPU/GPU 統一

- 目的:
  - 透過的な切り替え

- 作業項目:
  - 共通インターフェース
  - 自動切り替え
  - 品質の統一

- 完了条件:
  - CPU/GPU を意識せずに使用
  - 性能に応じて自動切り替え

### Phase 5: エフェクトライブラリ

- 目的:
  - 豊富なエフェクト

- 作業項目:
  - 基本エフェクトの拡充
  - カスタムエフェクト作成 API
  - サードパーティ製エフェクトの読み込み

- 完了条件:
  - 50+ のエフェクト
  - カスタムエフェクトを作成可能

---

## 技術的課題

### 1. GPU メモリ管理

**課題:**
- テクスチャの転送コスト

**解決案:**
- VRAM 上の処理
- 最小限の転送
- ピン留めメモリ

### 2. 品質の統一

**課題:**
- CPU/GPU で結果が異なる

**解決案:**
- 参照実装の定義
- 許容誤差の設定
- テストスイート

### 3. 互換性

**課題:**
- 旧プロジェクトとの互換性

**解決案:**
- 後方互換性の維持
- 変換ツール
- 段階的な移行

---

## 期待される効果

### 性能向上

| 指標 | 現在 | 改善後 | 向上率 |
|------|------|--------|--------|
| **Blur エフェクト** | 500ms | 16ms | -97% |
| **Glow エフェクト** | 800ms | 16ms | -98% |
| **Color Correction** | 300ms | 16ms | -95% |

### ユーザー体験

- リアルタイムプレビュー
- 複雑なエフェクトも軽快
- 直感的なエフェクト管理

---

## 関連ドキュメント

- `docs/planned/MILESTONE_GPU_EFFECT_PARITY_2026-03-27.md` - GPU エフェクト対等
- `docs/planned/MILESTONE_COLOR_CORRECTION_2026-03-27.md` - カラーコレクション
- `docs/planned/MILESTONE_PLUGIN_SYSTEM_2026-03-28.md` - プラグインシステム

---

## 実装順序の推奨

1. **Phase 1: GPU エフェクト** - 性能向上
2. **Phase 2: エフェクトスタック** - 使いやすさ
3. **Phase 3: パラメータ同期** - 統合
4. **Phase 4: CPU/GPU 統一** - 透過化
5. **Phase 5: エフェクトライブラリ** - 拡充

---

**文書終了**
