#include <gtest/gtest.h>

#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QtGlobal>

#include <cmath>
#include <utility>

import Audio.Mixer;
import Audio.Bus;
import Audio.Segment;
import Audio.Effect;
import Audio.Effect.Tremolo;
import Audio.Effect.PingPongDelay;
import Core.ArtifactString;
import Memory.SharedPtr;

using namespace ArtifactCore;
using namespace ArtifactCore::Memory;

namespace {

// deserialize() がバスラックを再構築するための最小 factory。
// 未知の type は null を返し、呼び出し側が skip する。
SharedPtr<AudioEffect> testEffectFactory(const QString& type, const QJsonObject& payload)
{
    SharedPtr<AudioEffect> created;
    if (type == QStringLiteral("tremolo")) {
        auto effect = makeShared<AudioTremolo>();
        effect->fromJson(payload);
        created = SharedPtr<AudioEffect>(effect);
    } else if (type == QStringLiteral("ping_pong_delay")) {
        auto effect = makeShared<AudioPingPongDelay>();
        effect->fromJson(payload);
        created = SharedPtr<AudioEffect>(effect);
    }
    return created;
}

void addTremolo(SharedPtr<AudioBus> bus)
{
    auto effect = makeShared<AudioTremolo>();
    effect->setRate(8.0f);
    effect->setDepth(0.5f);
    effect->setMix(1.0f);
    bus->addEffect(SharedPtr<AudioEffect>(effect));
}

void addPingPong(SharedPtr<AudioBus> bus)
{
    auto effect = makeShared<AudioPingPongDelay>();
    effect->setTimeMs(250.0f);
    effect->setFeedback(0.6f);
    effect->setMix(1.0f);
    bus->addEffect(SharedPtr<AudioEffect>(effect));
}

AudioSegment makeStereoSine(int sampleRate, int frames)
{
    AudioSegment segment;
    segment.sampleRate = sampleRate;
    segment.layout = AudioChannelLayout::Stereo;
    segment.startFrame = 0;
    segment.channelData.resize(2);
    segment.setFrameCount(frames);
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < frames; ++i) {
            segment.channelData[ch][i] =
                0.5f * std::sin(2.0f * 3.14159265f * 440.0f * i / sampleRate);
        }
    }
    return segment;
}

QJsonObject buildDocumentWithRacks()
{
    AudioMixer mixer;
    auto keys = mixer.createBus(String("Keys"), AudioBusKind::Group);
    auto vox = mixer.createBus(String("Vox"), AudioBusKind::Group);
    // GTest の ASSERT_* は非 void 関数で使えないため手動で空文書を返す。
    if (!keys || !vox) return {};
    addTremolo(keys);
    addPingPong(vox);
    addPingPong(mixer.getMasterBus());
    return mixer.serialize();
}

} // namespace

TEST(AudioMixerFxPersistenceStandaloneTest, RoundTripRestoresRacksWithParameters)
{
    const QJsonObject document = buildDocumentWithRacks();

    AudioMixer restored;
    restored.setEffectFactoryHook(&testEffectFactory);
    ASSERT_TRUE(restored.deserialize(document));

    AudioMixer reference;
    EXPECT_EQ(restored.busCount(), reference.busCount() + 2);

    const auto keys = restored.findBusByName(String("Keys"));
    ASSERT_TRUE(keys);
    ASSERT_EQ(keys->getEffectCount(), 1);
    EXPECT_EQ(toStdString(keys->getEffect(0)->effectType()), "tremolo");
    EXPECT_FLOAT_EQ(keys->getEffect(0)->getParameterValue(String("rate")), 8.0f);
    EXPECT_FLOAT_EQ(keys->getEffect(0)->getParameterValue(String("depth")), 0.5f);

    const auto vox = restored.findBusByName(String("Vox"));
    ASSERT_TRUE(vox);
    ASSERT_EQ(vox->getEffectCount(), 1);
    EXPECT_EQ(toStdString(vox->getEffect(0)->effectType()), "ping_pong_delay");
    EXPECT_FLOAT_EQ(vox->getEffect(0)->getParameterValue(String("time_ms")), 250.0f);

    const auto master = restored.getMasterBus();
    ASSERT_TRUE(master);
    ASSERT_EQ(master->getEffectCount(), 1);
    EXPECT_EQ(toStdString(master->getEffect(0)->effectType()), "ping_pong_delay");
}

TEST(AudioMixerFxPersistenceStandaloneTest, RoundTripPreservesEffectChainOrder)
{
    AudioMixer source;
    const auto bus = source.createBus(String("Ordered effects"), AudioBusKind::Group);
    ASSERT_TRUE(bus);
    addTremolo(bus);
    addPingPong(bus);
    const QJsonObject document = source.serialize();

    AudioMixer restored;
    restored.setEffectFactoryHook(&testEffectFactory);
    ASSERT_TRUE(restored.deserialize(document));
    const auto restoredBus = restored.findBusByName(String("Ordered effects"));
    ASSERT_TRUE(restoredBus);
    ASSERT_EQ(restoredBus->getEffectCount(), 2);
    EXPECT_EQ(toStdString(restoredBus->getEffect(0)->effectType()), "tremolo");
    EXPECT_EQ(toStdString(restoredBus->getEffect(1)->effectType()), "ping_pong_delay");
    EXPECT_FLOAT_EQ(restoredBus->getEffect(0)->getParameterValue(String("rate")), 8.0f);
    EXPECT_FLOAT_EQ(restoredBus->getEffect(1)->getParameterValue(String("time_ms")), 250.0f);
}

TEST(AudioMixerFxPersistenceStandaloneTest, UnknownMiddleEntryDoesNotReorderKnownEffects)
{
    AudioMixer source;
    const auto bus = source.createBus(String("Mixed effects"), AudioBusKind::Group);
    ASSERT_TRUE(bus);
    addTremolo(bus);
    addPingPong(bus);
    QJsonObject document = source.serialize();
    QJsonArray buses = document.value(QStringLiteral("buses")).toArray();
    ASSERT_EQ(buses.size(), 1);
    QJsonObject serializedBus = buses[0].toObject();
    QJsonArray effects = serializedBus.value(QStringLiteral("effects")).toArray();
    ASSERT_EQ(effects.size(), 2);
    QJsonObject unknown;
    unknown[QStringLiteral("type")] = QStringLiteral("not-installed");
    effects.insert(1, unknown);
    serializedBus[QStringLiteral("effects")] = effects;
    buses[0] = serializedBus;
    document[QStringLiteral("buses")] = buses;

    AudioMixer restored;
    restored.setEffectFactoryHook(&testEffectFactory);
    ASSERT_TRUE(restored.deserialize(document));
    const auto restoredBus = restored.findBusByName(String("Mixed effects"));
    ASSERT_TRUE(restoredBus);
    ASSERT_EQ(restoredBus->getEffectCount(), 2);
    EXPECT_EQ(toStdString(restoredBus->getEffect(0)->effectType()), "tremolo");
    EXPECT_EQ(toStdString(restoredBus->getEffect(1)->effectType()), "ping_pong_delay");
}

TEST(AudioMixerFxPersistenceStandaloneTest, RestoredEffectsSoundIdentical)
{
    const QJsonObject document = buildDocumentWithRacks();

    AudioMixer restored;
    restored.setEffectFactoryHook(&testEffectFactory);
    ASSERT_TRUE(restored.deserialize(document));

    // 同一ドキュメントから作り直した 2 系統で DSP 出力が一致する。
    AudioMixer second;
    second.setEffectFactoryHook(&testEffectFactory);
    ASSERT_TRUE(second.deserialize(document));

    const auto first = restored.findBusByName(String("Keys"));
    const auto other = second.findBusByName(String("Keys"));
    ASSERT_TRUE(first);
    ASSERT_TRUE(other);
    ASSERT_EQ(first->getEffectCount(), 1);
    ASSERT_EQ(other->getEffectCount(), 1);

    auto left = makeStereoSine(48000, 4800);
    auto right = makeStereoSine(48000, 4800);
    first->getEffect(0)->process(left);
    other->getEffect(0)->process(right);
    EXPECT_EQ(left.channelData, right.channelData);
}

TEST(AudioMixerFxPersistenceStandaloneTest, DeserializeIsIdempotent)
{
    const QJsonObject document = buildDocumentWithRacks();

    AudioMixer restored;
    restored.setEffectFactoryHook(&testEffectFactory);
    ASSERT_TRUE(restored.deserialize(document));
    // 同一ドキュメントの再適用でラックが増殖しない。
    ASSERT_TRUE(restored.deserialize(document));

    const auto keys = restored.findBusByName(String("Keys"));
    ASSERT_TRUE(keys);
    EXPECT_EQ(keys->getEffectCount(), 1);
    EXPECT_EQ(restored.getMasterBus()->getEffectCount(), 1);
}

TEST(AudioMixerFxPersistenceStandaloneTest, ExplicitEmptyMasterRackClearsPreviouslyRestoredEffects)
{
    QJsonObject document = buildDocumentWithRacks();
    AudioMixer restored;
    restored.setEffectFactoryHook(&testEffectFactory);
    ASSERT_TRUE(restored.deserialize(document));
    ASSERT_EQ(restored.getMasterBus()->getEffectCount(), 1);

    QJsonObject master = document.value(QStringLiteral("master")).toObject();
    master[QStringLiteral("effects")] = QJsonArray();
    document[QStringLiteral("master")] = master;

    ASSERT_TRUE(restored.deserialize(document));
    EXPECT_EQ(restored.getMasterBus()->getEffectCount(), 0);
}

TEST(AudioMixerFxPersistenceStandaloneTest, ExplicitEmptyBusRackClearsPreviouslyRestoredEffects)
{
    QJsonObject document = buildDocumentWithRacks();
    AudioMixer restored;
    restored.setEffectFactoryHook(&testEffectFactory);
    ASSERT_TRUE(restored.deserialize(document));
    const auto keys = restored.findBusByName(String("Keys"));
    ASSERT_TRUE(keys);
    ASSERT_EQ(keys->getEffectCount(), 1);

    QJsonArray buses = document.value(QStringLiteral("buses")).toArray();
    for (qsizetype index = 0; index < buses.size(); ++index) {
        QJsonObject bus = buses[index].toObject();
        if (bus.value(QStringLiteral("name")).toString() != QStringLiteral("Keys"))
            continue;
        bus[QStringLiteral("effects")] = QJsonArray();
        buses[index] = bus;
        break;
    }
    document[QStringLiteral("buses")] = buses;

    ASSERT_TRUE(restored.deserialize(document));
    const auto clearedKeys = restored.findBusByName(String("Keys"));
    ASSERT_TRUE(clearedKeys);
    EXPECT_EQ(clearedKeys->getEffectCount(), 0);
}

TEST(AudioMixerFxPersistenceStandaloneTest, UnknownEffectTypeIsSkipped)
{
    QJsonObject document = buildDocumentWithRacks();
    QJsonArray buses = document[QStringLiteral("buses")].toArray();
    ASSERT_FALSE(buses.isEmpty());
    QJsonObject first = buses[0].toObject();
    QJsonArray effects = first[QStringLiteral("effects")].toArray();
    const int before = effects.size();
    QJsonObject unknown;
    unknown[QStringLiteral("type")] = QStringLiteral("no_such_fx");
    effects.append(unknown);
    first[QStringLiteral("effects")] = effects;
    buses[0] = first;
    document[QStringLiteral("buses")] = buses;

    AudioMixer restored;
    restored.setEffectFactoryHook(&testEffectFactory);
    ASSERT_TRUE(restored.deserialize(document));

    const auto keys = restored.findBusByName(String("Keys"));
    ASSERT_TRUE(keys);
    EXPECT_EQ(keys->getEffectCount(), before);
}

TEST(AudioMixerFxPersistenceStandaloneTest, DocumentWithoutRacksKeepsEmptyMixer)
{
    QJsonObject document;
    document[QStringLiteral("buses")] = QJsonArray();

    AudioMixer restored;
    restored.setEffectFactoryHook(&testEffectFactory);
    ASSERT_TRUE(restored.deserialize(document));

    AudioMixer reference;
    EXPECT_EQ(restored.busCount(), reference.busCount());
    EXPECT_EQ(restored.getMasterBus()->getEffectCount(), 0);
}

TEST(AudioMixerFxPersistenceStandaloneTest, InvalidDocumentPreservesExistingMixerState)
{
    AudioMixer mixer;
    mixer.setEffectFactoryHook(&testEffectFactory);
    ASSERT_TRUE(mixer.deserialize(buildDocumentWithRacks()));
    const int originalBusCount = mixer.busCount();
    const auto keys = mixer.findBusByName(String("Keys"));
    ASSERT_TRUE(keys);
    ASSERT_EQ(keys->getEffectCount(), 1);
    ASSERT_EQ(mixer.getMasterBus()->getEffectCount(), 1);

    QJsonObject invalidDocument;
    invalidDocument[QStringLiteral("buses")] = QStringLiteral("not-an-array");
    EXPECT_FALSE(mixer.deserialize(invalidDocument));

    EXPECT_EQ(mixer.busCount(), originalBusCount);
    const auto preservedKeys = mixer.findBusByName(String("Keys"));
    ASSERT_TRUE(preservedKeys);
    ASSERT_EQ(preservedKeys->getEffectCount(), 1);
    EXPECT_EQ(toStdString(preservedKeys->getEffect(0)->effectType()), "tremolo");
    ASSERT_EQ(mixer.getMasterBus()->getEffectCount(), 1);
    EXPECT_EQ(toStdString(mixer.getMasterBus()->getEffect(0)->effectType()),
              "ping_pong_delay");
}
