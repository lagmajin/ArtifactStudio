import re

with open(r'X:\dev\artifactstudio\Artifact\src\Widgets\AudioPreviewWidget.cppm', 'r', encoding='utf-8') as f:
    content = f.read()

# Add AudioRenderer import
old_import = 'import Audio.Segment;'
new_import = 'import Audio.Segment;\nimport AudioRenderer;'
content = content.replace(old_import, new_import)

replacement = r'''class AudioPlaybackEngine : public QObject {
    W_OBJECT(AudioPlaybackEngine)
public:
    explicit AudioPlaybackEngine(QObject* parent = nullptr)
        : QObject(parent)
    {
        decoder_ = std::make_unique<ArtifactCore::FFmpegAudioDecoder>();
        renderer_ = std::make_unique<ArtifactCore::AudioRenderer>();

        renderer_->setLevelCallback([this](const ArtifactCore::AudioLevelData& data) {
            levelTickCounter_++;
            if (levelTickCounter_ % 4 == 0) {
                QMetaObject::invokeMethod(this, [this, data]() {
                    emit levelUpdated(data.leftPeak, data.leftRms, data.rightPeak, data.rightRms);
                }, Qt::QueuedConnection);
            }
        });

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
        eosReached_ = false;

        levelTickCounter_ = 0;
        emit levelUpdated(-96.0f, -96.0f, -96.0f, -96.0f);

        generateWaveformSamples();
        return true;
    }

    void play() {
        if (!decoder_ || preloadedSegments_.empty()) return;

        if (!renderer_->isDeviceOpen()) {
            renderer_->openDevice(ArtifactCore::AudioBackendType::WASAPI);
            renderer_->start();
        }
        renderer_->setMasterVolume(volumeToDb(volume_));
        eosReached_ = false;
        isPlaying_ = true;
        timer_->start();
        emit playbackStarted();
    }

    void pause() {
        if (!decoder_) return;
        isPlaying_ = false;
        timer_->stop();
        renderer_->clearBuffer();
        renderer_->stop();
        emit levelUpdated(-96.0f, -96.0f, -96.0f, -96.0f);
    }

    void stop() {
        if (!decoder_) return;
        isPlaying_ = false;
        timer_->stop();
        renderer_->stop();
        renderer_->closeDevice();
        decoder_->closeFile();
        currentSegmentIndex_ = 0;
        currentSegmentOffset_ = 0;
        currentSample_ = 0;
        eosReached_ = false;
        emit positionChanged(0);
        emit levelUpdated(-96.0f, -96.0f, -96.0f, -96.0f);
    }

    ~AudioPlaybackEngine() {
        stop();
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
        if (renderer_ && renderer_->isDeviceOpen()) {
            renderer_->setMasterVolume(volumeToDb(volume_));
        }
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

        // Feed segments to AudioRenderer at half the buffer rate (~10ms chunks)
        static int feedCounter = 0;
        feedCounter++;
        if (feedCounter % 4 == 0 || renderer_->bufferedFrames() < 1024) {
            const auto& seg = preloadedSegments_[currentSegmentIndex_];
            if (currentSegmentOffset_ < seg.frameCount()) {
                ArtifactCore::AudioSegment chunk;
                chunk.sampleRate = seg.sampleRate;
                chunk.layout = seg.layout;
                chunk.startFrame = seg.startFrame + currentSegmentOffset_;
                int available = seg.frameCount() - currentSegmentOffset_;
                int chunkSize = std::min(available, 512);
                chunk.channelData.resize(seg.channelData.size());
                for (int ch = 0; ch < static_cast<int>(seg.channelData.size()); ++ch) {
                    chunk.channelData[ch] = seg.channelData[ch].mid(currentSegmentOffset_, chunkSize);
                }
                renderer_->enqueue(chunk);
            }
        }

        // Advance position based on actual hardware playback
        size_t bufferedBefore = renderer_->bufferedFrames();
        if (bufferedBefore < 4096) {
            const auto& seg = preloadedSegments_[currentSegmentIndex_];
            int remaining = seg.frameCount() - currentSegmentOffset_;
            int advance = std::min(remaining, 256);
            currentSegmentOffset_ += advance;
            currentSample_ += advance;
            if (currentSegmentOffset_ >= seg.frameCount()) {
                currentSegmentOffset_ = 0;
                ++currentSegmentIndex_;
            }
        }

        emit positionChanged(currentSample_);

        if (currentSample_ >= totalSamples_) {
            eosReached_ = true;
            stop();
            emit playbackStopped();
        }
    }

private:
    static float volumeToDb(float linear) {
        if (linear < 1e-6f) return -144.0f;
        return 20.0f * std::log10(linear);
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
    std::unique_ptr<ArtifactCore::AudioRenderer> renderer_;
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
    int levelTickCounter_ = 0;
};

W_OBJECT_IMPL(AudioPlaybackEngine)'''

pattern = r'class AudioPlaybackEngine.*?W_OBJECT_IMPL\(AudioPlaybackEngine\)'
content = re.sub(pattern, replacement, content, flags=re.DOTALL)

# Also add QMetaObject include if not present
if '#include <QMetaObject>' not in content:
    content = content.replace('#include <QFileInfo>', '#include <QFileInfo>\n#include <QMetaObject>')

with open(r'X:\dev\artifactstudio\Artifact\src\Widgets\AudioPreviewWidget.cppm', 'w', encoding='utf-8') as f:
    f.write(content)
print('DONE')
