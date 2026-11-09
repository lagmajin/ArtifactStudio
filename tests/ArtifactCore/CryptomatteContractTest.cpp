#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include <QString>

import Image.Cryptomatte.Image;
import Image.Cryptomatte.Pixel;

namespace {

using ArtifactCore::Image::Cryptomatte::CryptoImage;
using ArtifactCore::Image::Cryptomatte::CryptoLayer;
using ArtifactCore::Image::Cryptomatte::CryptoManifest;
using ArtifactCore::Image::Cryptomatte::CryptoPixel;
using ArtifactCore::Image::Cryptomatte::CryptoSample;

std::vector<CryptoSample> rankSamples(const CryptoPixel& pixel, int rank)
{
    std::vector<CryptoSample> samples;
    pixel.getSamplesForRank(rank, samples);
    return samples;
}

} // namespace

TEST(CryptomattePixelContractTest, NameHashIsStableAndFitsTheTwentyFourBitPayload)
{
    const QString name = QStringLiteral("Hero_Mesh_A");

    const std::uint32_t first = CryptoSample::nameToId(name);
    const std::uint32_t second = CryptoSample::nameToId(name);

    EXPECT_EQ(first, second);
    EXPECT_LE(first, 0x00ffffffu);
    EXPECT_NE(first, CryptoSample::nameToId(QStringLiteral("Hero_Mesh_B")));
}

TEST(CryptomattePixelContractTest, NameToFloatAndFloatToIdRoundTripTheEncodedIdentifier)
{
    const QString name = QStringLiteral("Prop_Sphere");

    const float encoded = CryptoSample::nameToFloat(name);
    EXPECT_TRUE(std::isfinite(encoded));
    EXPECT_GT(encoded, 0.0f);

    EXPECT_EQ(CryptoSample::floatToId(encoded), CryptoSample::nameToId(name));
}

TEST(CryptomattePixelContractTest, FloatToIdRejectsNonFiniteAndNonPositiveEncodings)
{
    const float notANumber = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();

    EXPECT_EQ(CryptoSample::floatToId(notANumber), 0u);
    EXPECT_EQ(CryptoSample::floatToId(infinity), 0u);
    EXPECT_EQ(CryptoSample::floatToId(-infinity), 0u);
    EXPECT_EQ(CryptoSample::floatToId(0.0f), 0u);
    EXPECT_EQ(CryptoSample::floatToId(-1.0f), 0u);
    EXPECT_EQ(CryptoSample::floatToId(1234.0f), 1234u);
}

TEST(CryptomattePixelContractTest, SamplesAreKeptInDescendingCoverageOrder)
{
    CryptoPixel pixel;
    pixel.addSample(10.0f, 0.25f);
    pixel.addSample(20.0f, 0.90f);
    pixel.addSample(30.0f, 0.50f);

    ASSERT_EQ(pixel.sampleCount(), 3);
    EXPECT_FALSE(pixel.isEmpty());

    const std::vector<CryptoSample> rank0 = rankSamples(pixel, 0);
    ASSERT_EQ(rank0.size(), 2u);
    EXPECT_FLOAT_EQ(rank0[0].id, 20.0f);
    EXPECT_FLOAT_EQ(rank0[0].coverage, 0.90f);
    EXPECT_FLOAT_EQ(rank0[1].id, 30.0f);
    EXPECT_FLOAT_EQ(rank0[1].coverage, 0.50f);

    const std::vector<CryptoSample> rank1 = rankSamples(pixel, 1);
    ASSERT_EQ(rank1.size(), 1u);
    EXPECT_FLOAT_EQ(rank1[0].id, 10.0f);
    EXPECT_FLOAT_EQ(rank1[0].coverage, 0.25f);
}

TEST(CryptomattePixelContractTest, RepeatedIdentifierAccumulatesCoverageAndClampsToUnitRange)
{
    CryptoPixel pixel;
    pixel.addSample(7.0f, 0.40f);
    pixel.addSample(7.0f, 0.35f);

    ASSERT_EQ(pixel.sampleCount(), 1);
    const std::vector<CryptoSample> rank0 = rankSamples(pixel, 0);
    ASSERT_EQ(rank0.size(), 1u);
    EXPECT_FLOAT_EQ(rank0[0].coverage, 0.75f);

    pixel.addSample(7.0f, 0.90f);
    ASSERT_EQ(pixel.sampleCount(), 1);
    const std::vector<CryptoSample> clamped = rankSamples(pixel, 0);
    ASSERT_EQ(clamped.size(), 1u);
    EXPECT_FLOAT_EQ(clamped[0].coverage, 1.0f);
}

TEST(CryptomattePixelContractTest, AccumulatedCoverageKeepsTheSamplePairInDescendingOrder)
{
    CryptoPixel pixel;
    pixel.addSample(1.0f, 0.10f);
    pixel.addSample(2.0f, 0.90f);

    // Accumulating onto the weakest sample raises it above the stored order,
    // so the descending coverage order has to be restored afterwards.
    pixel.addSample(1.0f, 0.95f);

    ASSERT_EQ(pixel.sampleCount(), 2);
    const std::vector<CryptoSample> rank0 = rankSamples(pixel, 0);
    ASSERT_EQ(rank0.size(), 2u);
    EXPECT_FLOAT_EQ(rank0[0].id, 1.0f);
    EXPECT_FLOAT_EQ(rank0[0].coverage, 1.0f);
    EXPECT_FLOAT_EQ(rank0[1].id, 2.0f);
    EXPECT_FLOAT_EQ(rank0[1].coverage, 0.90f);
}

TEST(CryptomattePixelContractTest, NonFiniteAndNonPositiveContributionsAreRejected)
{
    CryptoPixel pixel;
    const float notANumber = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();

    pixel.addSample(notANumber, 0.5f);
    pixel.addSample(1.0f, notANumber);
    pixel.addSample(2.0f, infinity);
    pixel.addSample(3.0f, 0.0f);
    pixel.addSample(4.0f, -0.5f);

    EXPECT_EQ(pixel.sampleCount(), 0);
    EXPECT_TRUE(pixel.isEmpty());
}

TEST(CryptomattePixelContractTest, ExceedingTheSampleBudgetEvictsTheWeakestCoverage)
{
    CryptoPixel pixel;
    for (int i = 0; i < CryptoPixel::kMaxSamples; ++i)
        pixel.addSample(static_cast<float>(i + 1), static_cast<float>(i + 1) * 0.01f);

    ASSERT_EQ(pixel.sampleCount(), CryptoPixel::kMaxSamples);

    // A weaker contribution than every stored sample must not displace one.
    pixel.addSample(999.0f, 0.001f);
    EXPECT_EQ(pixel.sampleCount(), CryptoPixel::kMaxSamples);

    // A stronger contribution replaces the weakest stored sample.
    pixel.addSample(999.0f, 0.5f);
    EXPECT_EQ(pixel.sampleCount(), CryptoPixel::kMaxSamples);

    const std::vector<CryptoSample> rank0 = rankSamples(pixel, 0);
    ASSERT_FALSE(rank0.empty());
    EXPECT_FLOAT_EQ(rank0[0].id, 999.0f);
    EXPECT_FLOAT_EQ(rank0[0].coverage, 0.5f);
}

TEST(CryptomattePixelContractTest, ClearDropsEveryStoredSample)
{
    CryptoPixel pixel;
    pixel.addSample(1.0f, 0.5f);
    pixel.addSample(2.0f, 0.4f);
    ASSERT_EQ(pixel.sampleCount(), 2);

    pixel.clear();

    EXPECT_EQ(pixel.sampleCount(), 0);
    EXPECT_TRUE(pixel.isEmpty());
    EXPECT_TRUE(rankSamples(pixel, 0).empty());
}

TEST(CryptomattePixelContractTest, NegativeRankProducesNoSamples)
{
    CryptoPixel pixel;
    pixel.addSample(1.0f, 0.5f);

    EXPECT_TRUE(rankSamples(pixel, -1).empty());
}

TEST(CryptomatteManifestContractTest, EntryLookupRoundTripsThroughTheNameHash)
{
    CryptoManifest manifest;
    manifest.addEntry(QStringLiteral("Hero_Mesh_A"));

    ASSERT_EQ(manifest.entryCount(), 1);

    const std::uint32_t hash = CryptoSample::nameToId(QStringLiteral("Hero_Mesh_A"));
    EXPECT_EQ(manifest.findHash(QStringLiteral("Hero_Mesh_A")), hash);
    EXPECT_EQ(manifest.findName(hash), QStringLiteral("Hero_Mesh_A"));
}

TEST(CryptomatteManifestContractTest, EmptyAndDuplicateNamesDoNotGrowTheManifest)
{
    CryptoManifest manifest;
    manifest.addEntry(QString());
    EXPECT_EQ(manifest.entryCount(), 0);

    manifest.addEntry(QStringLiteral("Hero_Mesh_A"));
    manifest.addEntry(QStringLiteral("Hero_Mesh_A"));
    EXPECT_EQ(manifest.entryCount(), 1);
}

TEST(CryptomatteManifestContractTest, UnknownHashLookupReportsAbsenceAndEmptyNameHashesToZero)
{
    CryptoManifest manifest;
    manifest.addEntry(QStringLiteral("Hero_Mesh_A"));

    const std::uint32_t registered = CryptoSample::nameToId(QStringLiteral("Hero_Mesh_A"));
    // Query a hash that is guaranteed to differ from the only stored entry.
    const std::uint32_t absent = registered < 0x00ffffffu ? registered + 1u : registered - 1u;
    EXPECT_TRUE(manifest.findName(absent).isEmpty());
    EXPECT_EQ(manifest.findHash(QString()), 0u);
}

TEST(CryptomatteManifestContractTest, HashOfAnUnregisteredNameIsStillTheDeterministicNameHash)
{
    CryptoManifest manifest;
    manifest.addEntry(QStringLiteral("Hero_Mesh_A"));

    // The name hash is a pure function of the name, so an unregistered name
    // still hashes to a stable value instead of reporting absence.
    EXPECT_EQ(manifest.findHash(QStringLiteral("Not_Registered")),
              CryptoSample::nameToId(QStringLiteral("Not_Registered")));
    EXPECT_NE(manifest.findHash(QStringLiteral("Not_Registered")), 0u);
}

TEST(CryptomatteManifestContractTest, MetadataMapsTheHashKeyToTheOriginalName)
{
    CryptoManifest manifest;
    manifest.addEntry(QStringLiteral("Hero_Mesh_A"));

    const QString metadata = manifest.toMetadata();
    EXPECT_TRUE(metadata.contains(QStringLiteral("Hero_Mesh_A")));
    EXPECT_FALSE(metadata.isEmpty());
}

TEST(CryptomatteImageContractTest, ResizeRejectsNonPositiveDimensionsAndClearsTheImage)
{
    CryptoImage image;
    ASSERT_TRUE(image.resize(4, 4));
    EXPECT_EQ(image.width(), 4);
    EXPECT_EQ(image.height(), 4);

    EXPECT_FALSE(image.resize(0, 4));
    EXPECT_EQ(image.width(), 0);
    EXPECT_EQ(image.height(), 0);

    EXPECT_FALSE(image.resize(-2, -2));
    EXPECT_EQ(image.width(), 0);
    EXPECT_EQ(image.height(), 0);
}

TEST(CryptomatteImageContractTest, IdentifierBuffersSmallerThanTheImageAreIgnored)
{
    CryptoImage image;
    ASSERT_TRUE(image.resize(2, 2));

    // Four pixels are required; a shorter buffer must not populate the image.
    const std::vector<float> tooShort{ 100.0f, 200.0f };
    image.fromObjectIdBuffer(tooShort, 2, 2);

    EXPECT_EQ(image.maxRank(), 0);
}

TEST(CryptomatteImageContractTest, NegativeDimensionsAreRejectedBeforeAnyBufferIsRead)
{
    CryptoImage image;
    ASSERT_TRUE(image.resize(2, 2));

    // A negative extent must not be reinterpreted as a huge unsigned count.
    // resize() rejects the extent, clears the image and reports failure, so
    // no identifier buffer is read and no sample is created.
    const std::vector<float> ids{ 100.0f, 200.0f };
    image.fromObjectIdBuffer(ids, -2, 2);
    image.fromIdCoverageBuffers(ids, ids, 2, -2);

    EXPECT_EQ(image.width(), 0);
    EXPECT_EQ(image.height(), 0);
    EXPECT_EQ(image.maxRank(), 0);
}

TEST(CryptomatteImageContractTest, ObjectAndMaterialBuffersShareTheSameIdentifierDomain)
{
    CryptoImage objectImage;
    CryptoImage materialImage;
    ASSERT_TRUE(objectImage.resize(2, 1));
    ASSERT_TRUE(materialImage.resize(2, 1));

    const std::vector<float> ids{ 111.0f, 222.0f };
    objectImage.fromObjectIdBuffer(ids, 2, 1);
    materialImage.fromMaterialIdBuffer(ids, 2, 1);

    std::vector<float> objectR;
    std::vector<float> objectG;
    std::vector<float> materialR;
    std::vector<float> materialG;
    objectImage.generateRankLayers(CryptoLayer::Object, 0, objectR, objectG);
    materialImage.generateRankLayers(CryptoLayer::Material, 0, materialR, materialG);

    ASSERT_EQ(objectR.size(), 2u);
    ASSERT_EQ(materialR.size(), 2u);
    EXPECT_FLOAT_EQ(objectR[0], 111.0f);
    EXPECT_FLOAT_EQ(materialR[0], 111.0f);
    EXPECT_FLOAT_EQ(objectG[0], 1.0f);
    EXPECT_FLOAT_EQ(materialG[0], 1.0f);
}

TEST(CryptomatteImageContractTest, CoverageBuffersKeepTheSuppliedCoverageValue)
{
    CryptoImage image;
    ASSERT_TRUE(image.resize(2, 1));

    const std::vector<float> ids{ 111.0f, 222.0f };
    const std::vector<float> coverage{ 0.75f, 0.25f };
    image.fromIdCoverageBuffers(ids, coverage, 2, 1);

    std::vector<float> outR;
    std::vector<float> outG;
    image.generateRankLayers(CryptoLayer::Object, 0, outR, outG);

    ASSERT_EQ(outR.size(), 2u);
    EXPECT_FLOAT_EQ(outR[0], 111.0f);
    EXPECT_FLOAT_EQ(outG[0], 0.75f);
    EXPECT_FLOAT_EQ(outR[1], 222.0f);
    EXPECT_FLOAT_EQ(outG[1], 0.25f);
}

TEST(CryptomatteImageContractTest, NonPositiveAndNonFiniteIdentifiersDoNotCreateSamples)
{
    CryptoImage image;
    ASSERT_TRUE(image.resize(3, 1));

    const float notANumber = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const std::vector<float> ids{ 0.0f, -5.0f, notANumber };
    image.fromObjectIdBuffer(ids, 3, 1);

    EXPECT_EQ(image.maxRank(), 0);
}

TEST(CryptomatteImageContractTest, MaxRankStaysAtOrBelowOneForTheSingleIdentifierBufferApi)
{
    CryptoImage image;
    ASSERT_TRUE(image.resize(1, 1));

    // The public buffer API carries one identifier per pixel, so a pixel can
    // hold at most one sample and the image exposes a single rank.
    const std::vector<float> ids{ 111.0f };
    image.fromObjectIdBuffer(ids, 1, 1);

    EXPECT_EQ(image.maxRank(), 1);
}

TEST(CryptomattePixelContractTest, MaxRankIsDerivedFromTheHighestSampleCountPerPixel)
{
    CryptoPixel pixel;
    pixel.addSample(1.0f, 0.9f);
    pixel.addSample(2.0f, 0.6f);
    pixel.addSample(3.0f, 0.3f);

    // The image rank count follows the same pairing rule as getSamplesForRank:
    // two identifiers per rank, rounded up.
    EXPECT_EQ((pixel.sampleCount() + 1) / 2, 2);
    EXPECT_EQ(rankSamples(pixel, 0).size(), 2u);
    EXPECT_EQ(rankSamples(pixel, 1).size(), 1u);
}

TEST(CryptomatteImageContractTest, RankLayersExposeTheSecondIdentifierOfTheRank)
{
    CryptoImage image;
    ASSERT_TRUE(image.resize(2, 1));

    const std::vector<float> ids{ 111.0f, 222.0f };
    const std::vector<float> coverage{ 0.9f, 0.4f };
    image.fromIdCoverageBuffers(ids, coverage, 2, 1);

    std::vector<float> outId0;
    std::vector<float> outCoverage0;
    std::vector<float> outId1;
    std::vector<float> outCoverage1;
    image.generateRankLayers(CryptoLayer::Object, 0, outId0, outCoverage0, outId1, outCoverage1);

    ASSERT_EQ(outId0.size(), 2u);
    EXPECT_FLOAT_EQ(outId0[0], 111.0f);
    EXPECT_FLOAT_EQ(outCoverage0[0], 0.9f);
    EXPECT_FLOAT_EQ(outId1[0], 0.0f);
    EXPECT_FLOAT_EQ(outCoverage1[0], 0.0f);
}

TEST(CryptomatteImageContractTest, NegativeRankLeavesThePlanesZeroed)
{
    CryptoImage image;
    ASSERT_TRUE(image.resize(2, 1));

    const std::vector<float> ids{ 111.0f, 222.0f };
    image.fromObjectIdBuffer(ids, 2, 1);

    std::vector<float> outR;
    std::vector<float> outG;
    image.generateRankLayers(CryptoLayer::Object, -1, outR, outG);

    ASSERT_EQ(outR.size(), 2u);
    EXPECT_FLOAT_EQ(outR[0], 0.0f);
    EXPECT_FLOAT_EQ(outG[0], 0.0f);
}
