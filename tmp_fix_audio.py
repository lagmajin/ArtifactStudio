import re

with open(r'X:\dev\artifactstudio\Artifact\src\Widgets\AudioPreviewWidget.cppm', 'r', encoding='utf-8') as f:
    content = f.read()

old_import = 'import Artifact.Audio.Waveform;'
new_import = 'import Artifact.Audio.Waveform;\nimport Media.Encoder.FFmpegAudioDecoder;\nimport Audio.Segment;'
content = content.replace(old_import, new_import)

replacement = r'''class AudioPlaybackEngine : public QObject {
    W_OBJECT(AudioPlaybackEngine)
public:
    explicit AudioPlaybackEngine(QObject* parent = nullptr)
        : QObject(parent)
    {
        decoder_ = std::make_unique<ArtifactCore::FFmpegAudioDecoder>();
        timer_ = new QTimer(this);
        timer_->setInterval(50);
        connect(timer_, &QTimer::timeout, this, &AudioPlaybackEngine::onTimerTick);
    }

    bool loadFile(const QString& filePath) {
        stop();
        if (!decoder_->openFile(filePath)) {
            return false;
        }
        sampleRate_ = decoder_->sampleRate();
        numChannels_ = decoder_->channelCount();
        totalSamples_ = 0;
        preloadedSegments_.clear();

        ArtifactCore::AudioSegment seg;
        while (decoder_->decodeNextSegment(seg)) {
            totalSamples_ += seg.frameCount();
            preloadedSegments_.push_back(seg);
        }

        if (preloadedSegments_.empty()) {
            decoder_->closeFile();
            return false;
        }

        currentSample_ = 0;
        currentSegmentIndex_ = 0;
        currentSegmentOffset_ = 0;
        isPlaying_ = false;
        eosReached_ = false;

        leftPeak_ = -96.0f;
        leftRms_ = -96.0f;
        rightPeak_ = -96.0f;
        rightRms_ = -96.0f;
        levelTickCounter_ = 0;

        generateWaveformSamples();
        return true;
    }

    void play() {
        if (!decoder_ || preloadedSegments_.empty()) return;
        isPlaying_ = true;
        eosReached_ = false;
        timer_->start();
        emit playbackStarted();
    }

    void pause() {
        if (!decoder_) return;
        isPlaying_ = false;
        timer_->stop();
        leftPeak_ = -96.0f; leftRms_ = -96.0f;
        rightPeak_ = -96.0f; rightRms_ = -96.0f;
        emit levelUpdated(leftPeak_, leftRms_, rightPeak_, rightRms_);
    }

    void stop() {
        if (!decoder_) return;
        isPlaying_ = false;
        timer_->stop();
        decoder_->closeFile();
        currentSegmentIndex_ = 0;
        currentSegmentOffset_ = 0;
        currentSample_ = 0;
        eosReached_ = false;
        leftPeak_ = -96.0f; leftRms_ = -96.0f;
        rightPeak_ = -96.0f; rightRms_ = -96.0f;
        emit positionChanged(0);
        emit levelUpdated(leftPeak_, leftRms_, rightPeak_, rightRms_);
    }

    bool isPlaying() const { return isPlaying_; }
    int currentPosition() const { return currentSample_; }
    int totalSamples() const { return totalSamples_; }
    int sampleRate() const { return sampleRate_; }

    void setPosition(int sampleIndex) {
        currentSample_ = std::clamp(sampleIndex, 0, totalSamples_ - 1);
        int pos = 0;
        for (size_t i = 0; i < preloadedSegments_.size(); ++i) {
            int frames = preloadedSegments_[i].frameCount();
            if (pos + frames > currentSample_) {
                currentSegmentIndex_ = i;
                currentSegmentOffset_ = currentSample_ - pos;
                emit positionChanged(currentSample_);
                return;
            }
            pos += frames;
        }
        emit positionChanged(currentSample_);
    }

    void setVolume(float volume) {
        volume_ = std::clamp(volume, 0.0f, 1.0f);
    }

    float volume() const { return volume_; }

    const QVector<float>& waveformSamples() const { return waveformSamples_; }

signals:
    void playbackStarted() W_SIGNAL(playbackStarted);
    void playbackStopped() W_SIGNAL(playbackStopped);
    void positionChanged(int sampleIndex) W_SIGNAL(positionChanged, sampleIndex);
    void levelUpdated(float leftPeak, float leftRms, float rightPeak, float rightRms)
        W_SIGNAL(levelUpdated, leftPeak, leftRms, rightPeak, rightRms);

private slots:
    void onTimerTick() {
        if (!isPlaying_ || preloadedSegments_.empty()) return;
        if (eosReached_) {
            stop();
            emit playbackStopped();
            return;
        }

        if (currentSegmentIndex_ >= preloadedSegments_.size()) {
            eosReached_ = true;
            stop();
            emit playbackStopped();
            return;
        }

        const auto& seg = preloadedSegments_[currentSegmentIndex_];
        int remaining = seg.frameCount() - currentSegmentOffset_;
        const int samplesToAdvance = std::min(remaining, 1024);

        computeLevelsFromSegment(seg, currentSegmentOffset_, samplesToAdvance);

        currentSegmentOffset_ += samplesToAdvance;
        currentSample_ += samplesToAdvance;

        if (currentSegmentOffset_ >= seg.frameCount()) {
            currentSegmentOffset_ = 0;
            ++currentSegmentIndex_;
        }

        emit positionChanged(currentSample_);

        ++levelTickCounter_;
        if (levelTickCounter_ % 2 == 0) {
            emit levelUpdated(leftPeak_, leftRms_, rightPeak_, rightRms_);
        }

        if (currentSample_ >= totalSamples_) {
            eosReached_ = true;
            stop();
            emit playbackStopped();
        }
    }

private:
    static float linearToDb(float linear) {
        if (linear < 1e-10f) return -96.0f;
        return 20.0f * std::log10(linear);
    }

    void computeLevelsFromSegment(const ArtifactCore::AudioSegment& seg, int offset, int count) {
        if (seg.channelData.isEmpty()) return;
        const auto& ch0 = seg.channelData[0];
        const bool stereo = seg.channelData.size() >= 2;
        const auto& ch1 = stereo ? seg.channelData[1] : ch0;

        float leftSumSq = 0.0f;
        float rightSumSq = 0.0f;
        float leftAbsMax = 0.0f;
        float rightAbsMax = 0.0f;

        int end = std::min(offset + count, static_cast<int>(ch0.size()));
        for (int i = offset; i < end; ++i) {
            float l = std::abs(ch0[i]);
            float r = stereo ? std::abs(ch1[i]) : l;
            leftSumSq += l * l;
            rightSumSq += r * r;
            if (l > leftAbsMax) leftAbsMax = l;
            if (r > rightAbsMax) rightAbsMax = r;
        }

        int n = end - offset;
        if (n <= 0) return;
        leftPeak_ = linearToDb(leftAbsMax);
        leftRms_ = linearToDb(std::sqrt(leftSumSq / n));
        rightPeak_ = linearToDb(rightAbsMax);
        rightRms_ = linearToDb(std::sqrt(rightSumSq / n));
    }

    void generateWaveformSamples() {
        if (preloadedSegments_.empty()) return;
        const int targetSamples = 4000;
        const int samplesPerPixel = std::max(1, totalSamples_ / targetSamples);
        waveformSamples_.resize(targetSamples);

        int segIdx = 0;
        int segOffset = 0;
        for (int i = 0; i < targetSamples; ++i) {
            float maxVal = 0.0f;
            for (int s = 0; s < samplesPerPixel; ++s) {
                if (segIdx >= static_cast<int>(preloadedSegments_.size())) break;
                const auto& seg = preloadedSegments_[segIdx];
                if (seg.channelData.isEmpty()) break;
                const auto& ch0 = seg.channelData[0];
                if (segOffset < static_cast<int>(ch0.size())) {
                    float v = std::abs(ch0[segOffset]);
                    if (v > maxVal) maxVal = v;
                }
                ++segOffset;
                if (segOffset >= seg.frameCount()) {
                    segOffset = 0;
                    ++segIdx;
                }
            }
            waveformSamples_[i] = maxVal;
        }
    }

    std::unique_ptr<ArtifactCore::FFmpegAudioDecoder> decoder_;
    QTimer* timer_ = nullptr;
    std::vector<ArtifactCore::AudioSegment> preloadedSegments_;
    QVector<float> waveformSamples_;
    int sampleRate_ = 44100;
    int numChannels_ = 2;
    int totalSamples_ = 0;
    int currentSample_ = 0;
    size_t currentSegmentIndex_ = 0;
    int currentSegmentOffset_ = 0;
    bool isPlaying_ = false;
    bool eosReached_ = false;
    float volume_ = 1.0f;

    float leftPeak_ = -96.0f;
    float leftRms_ = -96.0f;
    float rightPeak_ = -96.0f;
    float rightRms_ = -96.0f;
    int levelTickCounter_ = 0;
};

W_OBJECT_IMPL(AudioPlaybackEngine)'''

pattern = r'class AudioPlaybackEngine.*?W_OBJECT_IMPL\(AudioPlaybackEngine\)'
content = re.sub(pattern, replacement, content, flags=re.DOTALL)

content = content.replace(
    'QString("Failed to load audio file:\\n%1\\n\\nOnly WAV files are supported.").arg(filePath)',
    'QString("Failed to load audio file:\\n%1").arg(filePath)'
)

with open(r'X:\dev\artifactstudio\Artifact\src\Widgets\AudioPreviewWidget.cppm', 'w', encoding='utf-8') as f:
    f.write(content)
print('DONE')
