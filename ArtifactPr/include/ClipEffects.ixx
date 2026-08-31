module;

#include <QMap>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <cstdint>

export module ArtifactPr.ClipEffects;

import Image.ImageF32x4_RGBA;

export namespace ArtifactPr {

using FramePosition = int64_t;

// =====================================================================
// トランジション opacity 変調 (プレビュー/エクスポート共通)
// =====================================================================

enum class TransitionFadeCurve {
    Linear,
    DipToBlack,
};

/// 1 トランジション区間の 1 クリップ分のフェード指定。
/// isLeft は legacy Transition の leftClipId 側かどうか。
/// プレビュー実装 (旧 transitionOpacityAt) に合わせ、
/// Linear は left 側 = t / right 側 = 1-t、
/// DipToBlack は left 側 = 1-2t / right 側 = 2t-1 (中央で 0)。
struct TransitionFadeSpan {
    FramePosition startFrame = 0;
    FramePosition duration = 0;
    bool isLeft = true;
    TransitionFadeCurve curve = TransitionFadeCurve::Linear;
};

/// スパン集合に対する frame 時点の opacity 変調係数。
/// 範囲外は 1.0。複数スパンが重なる場合は最も強い減衰 (min) を採用。
/// 区間は半開区間 [startFrame, startFrame+duration)。
double transitionOpacityFactor(const QVector<TransitionFadeSpan>& spans,
                               FramePosition frame);

// =====================================================================
// エフェクトパラメータ読み取り (fx.<effectId>.<param> 規約)
// =====================================================================

/// fx.<effectId>.enabled が "1" / "true" か。
bool effectEnabled(const QMap<QString, QVariant>& effects, const QString& effectId);

/// fx.<effectId>.<param> を double として読む (文字列値前提)。
/// 読めない / 未設定の場合は fallback。
double effectParam(const QMap<QString, QVariant>& effects, const QString& effectId,
                   const QString& param, double fallback);

/// 映像系エフェクトが 1 つでも有効か (プレビューの fast path 判定用)。
bool hasEnabledImageEffects(const QMap<QString, QVariant>& effects);

/// fx.audioGain のリニア係数 (10^(gainDb/20))。無効時は 1.0。
double audioGainFactor(const QMap<QString, QVariant>& effects);

/// fx.audioEqualizer の 3 帯 dB を読む。無効時は 0。
void audioEqualizerDb(const QMap<QString, QVariant>& effects,
                      double& lowDb, double& midDb, double& highDb);

bool audioEqualizerActive(double lowDb, double midDb, double highDb);

// =====================================================================
// 映像エフェクト評価
// =====================================================================

/// effects を固定順 (色調補正 → geometry → blur 系) で評価して image を書き換える。
/// 有効エフェクトが 1 つも無い場合は false を返し image は無変更。
/// 途中の正規化は ImageF32x4_RGBA::toCanonicalRGBA32FC4 経由で行い
/// (BGRA backing 考慮)、QImage / QPainter は使わない (AGENTS.md 整合)。
/// 対応: brightnessContrast / hueSaturation / colorWheels / gaussianBlur /
///       boxBlur / unsharpMask / glow / posterize / scale / rotate
bool applyClipEffects(ArtifactCore::ImageF32x4_RGBA& image,
                      const QMap<QString, QVariant>& effects);

// =====================================================================
// 音声エフェクト (3 帯 EQ)
// =====================================================================

/// 3 帯 biquad EQ (low shelf 200Hz / peaking 1kHz Q=1 / high shelf 4kHz)。
/// 48kHz 前提のデコーダ出力を想定。チャンネル毎に 1 インスタンスを持ち、
/// セグメントを跨いでインスタンスを使い回すことでフィルタ状態を連続させる。
struct ClipEqualizer {
    bool active = false;
    double lowDb = 0.0;
    double midDb = 0.0;
    double highDb = 0.0;
    double sampleRate = 48000.0;

    /// 係数を計算して状態をリセットする (lowDb/midDb/highDb 設定後に呼ぶ)。
    void configure();
    void reset();
    /// in-place 処理 (frameCount はサンプル数)。
    void process(float* samples, int frameCount);

private:
    double b0_[3] = {1.0, 1.0, 1.0};
    double b1_[3] = {0.0, 0.0, 0.0};
    double b2_[3] = {0.0, 0.0, 0.0};
    double a1_[3] = {0.0, 0.0, 0.0};
    double a2_[3] = {0.0, 0.0, 0.0};
    double x1_[3] = {0.0, 0.0, 0.0};
    double x2_[3] = {0.0, 0.0, 0.0};
    double y1_[3] = {0.0, 0.0, 0.0};
    double y2_[3] = {0.0, 0.0, 0.0};
};

} // namespace ArtifactPr
