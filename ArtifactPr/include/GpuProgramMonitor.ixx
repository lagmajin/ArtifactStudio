module;

#include <windows.h>

#include <DeviceContext.h>
#include <RefCntAutoPtr.hpp>
#include <RenderDevice.h>
#include <Sampler.h>
#include <Shader.h>
#include <SwapChain.h>
#include <Texture.h>
#include <PipelineState.h>

#include <QString>
#include <QWidget>

#include <QtGlobal>

export module ArtifactPr.GpuProgramMonitor;

import Image.ImageF32x4_RGBA;
import Image.ImageF32x4RGBAWithCache;

export namespace ArtifactPr {

/// Program Monitor の preview-only GPU present 経路。
///
/// CPU の `SequenceCompositor` は合成結果の正規経路および GPU 初期化失敗時の
/// 明示 fallback として維持する。このクラスは合成済みの
/// `ImageF32x4_RGBA` を Diligent texture として swap chain に present する
/// 責務だけを持ち、NLE モデルや合成ロジックには触れない。
///
/// 動画 decode の GPU 化や export 経路の置換は本クラスの対象外。
class GpuProgramMonitorRenderer {
public:
    GpuProgramMonitorRenderer() = default;
    ~GpuProgramMonitorRenderer();

    GpuProgramMonitorRenderer(const GpuProgramMonitorRenderer&) = delete;
    GpuProgramMonitorRenderer& operator=(const GpuProgramMonitorRenderer&) = delete;

    /// 選択された backend 名 (D3D12 / Vulkan)。
    QString backendName() const;

    /// 最後の失敗理由。空なら直近の失敗はない。
    QString lastFailureReason() const;

    /// device / swap chain / PSO を初期化する。二度目以降は no-op。
    /// 成功時 true、初期化に失敗して CPU fallback が正となる場合 false。
    bool ensureInitialized(QWidget* host);

    /// GPU 諸資源と子 HWND を解放する。CPU 表示へ戻すために使う。
    void reset();

    /// present 対象の合成済みフレームを設定する。hot path ではない。
    ///
    /// generation を渡すと、accept 時より新しい generation が要求されて
    /// いる場合、この frame は破棄される (stale upload の巻き戻り防止)。
    void setFrame(const ArtifactCore::ImageF32x4_RGBA& frame, quint64 generation = 0);

    /// Fit / zoom / pan の配置を GPU 側へ伝える。scale は NDC スケール、
    /// offset は NDC オフセット。CPU ウィジェットと同じ表示になる。
    void setViewTransform(float scaleX, float scaleY, float offsetX, float offsetY);

    /// swap chain へ present する。resize 後の表示回復に使う。
    void renderAndPresent();

    /// widget の resize に合わせて swap chain を更新する。
    bool recreateSwapChain(QWidget* host);

private:
    class Impl;
    Impl* impl_ = nullptr;
};

} // namespace ArtifactPr
