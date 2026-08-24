module;

#include <QSize>
#include <QVector>

#include <algorithm>
#include <cstdint>

export module ArtifactPr.SequenceCompositor;

import Image.ImageF32x4_RGBA;
import FloatRGBA;

export namespace ArtifactPr {

/// 合成する 1 レイヤー分のフレーム。
/// frame は canonical RGBA (元解像度のまま)。
struct CompositeLayer {
    ArtifactCore::ImageF32x4_RGBA frame;
    double opacity = 1.0;
};

/// レイヤー配列（下→上の順）を canvas サイズへ fit 合成した 1 枚を返す。
///
/// 各レイヤーはアスペクト比維持で canvas にスケールされ、中央配置される。
/// ブレンドは ImageF32x4_RGBA::alphaBlend のみを使用し、
/// QPainter / Qt CompositionMode は使わない (AGENTS.md 整合)。
///
/// トランジション (Crossfade 等) の opacity 変調は将来ここに追加する
/// (CompositeLayer.opacity をトラック/トランジション範囲から変調させる)。
ArtifactCore::ImageF32x4_RGBA composeSequenceLayers(
    const QSize& canvasSize,
    const QVector<CompositeLayer>& layers,
    const ArtifactCore::FloatRGBA& background);

} // namespace ArtifactPr
