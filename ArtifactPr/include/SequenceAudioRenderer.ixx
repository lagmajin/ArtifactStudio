module;

#include <QString>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

export module ArtifactPr.SequenceAudioRenderer;

import ArtifactPr.EditorEngine;
import ArtifactPr.ClipEffects;
import Audio.Segment;
import Media.Encoder.FFmpegAudioDecoder;
import Audio.Render.Writer;

export namespace ArtifactPr {

struct AudioRenderResult {
    bool success = false;
    QString error;
};

/// RenderPlan の音声クリップ (mute/solo/enabled フィルタ済み) をオフラインで
/// ミックスし、48kHz stereo の WAV へ書き出す。
///
/// クリップ毎に FFmpegAudioDecoder で全 PCM をデコード (48kHz stereo float
/// 正規化済み) → EQ (ClipEqualizer) をセグメント連続状態で適用 →
/// timeline 上の配置 (startFrame / durationFrames / sourceIn / fps) に従い
/// durationFrames で打ち切りながら 1 秒ブロック加算ミックス → AudioWriter へ
/// ストリーミング書き出し。プレビュー (AudioPreviewMixer) と異なり
/// durationFrames によるトリムを行う。
///
/// cancel はデコード中 / ブロック境界で検査。onProgress は 0-100
/// (デコード 0-60 / ミックス 60-100)。
AudioRenderResult renderSequenceAudio(const RenderPlan& plan,
                                      double fps,
                                      const QString& wavOutputPath,
                                      int bitDepth,
                                      const std::atomic_bool& cancel,
                                      const std::function<void(int percent)>& onProgress);

} // namespace ArtifactPr
