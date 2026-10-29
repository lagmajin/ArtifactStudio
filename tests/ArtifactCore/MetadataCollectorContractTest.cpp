#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVariant>

#include <array>

import Project.MetadataCollector;

using namespace ArtifactCore;

namespace {
class RecordingCollector final : public MetadataCollector {
public:
    void reset() override
    {
        ++resetCount;
        seenKinds.fill(0);
    }

    void onProject(const MetadataNode&) override { ++seenKinds[0]; }
    void onComposition(const MetadataNode&) override { ++seenKinds[1]; }
    void onLayer(const MetadataNode&) override { ++seenKinds[2]; }
    void onEffect(const MetadataNode&) override { ++seenKinds[3]; }
    void onProperty(const MetadataNode&) override { ++seenKinds[4]; }

    MetadataReport report() const override
    {
        MetadataReport result;
        for (std::size_t index = 0; index < seenKinds.size(); ++index) {
            result.addCount(QString::number(static_cast<qulonglong>(index)), seenKinds[index]);
        }
        return result;
    }

    int resetCount = 0;
    std::array<qint64, 5> seenKinds{};
};

QVector<MetadataNode> allNodeKinds()
{
    return {
        {MetadataNodeKind::Project, QStringLiteral("p"), {}, {}, 0, {}},
        {MetadataNodeKind::Composition, QStringLiteral("c"), {}, {}, 1, {}},
        {MetadataNodeKind::Layer, QStringLiteral("l"), {}, {}, 2, {}},
        {MetadataNodeKind::Effect, QStringLiteral("e"), {}, {}, 3, {}},
        {MetadataNodeKind::Property, QStringLiteral("v"), {}, {}, 4, QVariant(12)},
    };
}
}

TEST(MetadataCollectorContractTest, DriverDispatchesEveryNodeKindToEachCollector)
{
    RecordingCollector first;
    RecordingCollector second;
    const auto report = MetadataCollectorDriver::collectReport(
        allNodeKinds(), {&first, nullptr, &second});

    EXPECT_EQ(first.resetCount, 1);
    EXPECT_EQ(second.resetCount, 1);
    for (std::size_t index = 0; index < first.seenKinds.size(); ++index) {
        EXPECT_EQ(first.seenKinds[index], 1);
        EXPECT_EQ(second.seenKinds[index], 1);
        EXPECT_EQ(report.count(QString::number(static_cast<qulonglong>(index))), 2);
    }
}

TEST(MetadataCollectorContractTest, EveryVisitResetsCollectorsBeforeDispatch)
{
    RecordingCollector collector;
    const QVector<MetadataNode> nodes = {
        {MetadataNodeKind::Layer, QStringLiteral("one"), {}, {}, 1, {}},
    };

    MetadataCollectorDriver::visit(nodes, {&collector});
    MetadataCollectorDriver::visit(nodes, {&collector});

    EXPECT_EQ(collector.resetCount, 2);
    EXPECT_EQ(collector.seenKinds[2], 1);
}

TEST(MetadataCollectorContractTest, NullCollectorsAreIgnoredForEmptyAndNonEmptyVisits)
{
    const QVector<MetadataCollector*> collectors = {nullptr, nullptr};
    EXPECT_NO_THROW(MetadataCollectorDriver::visit(allNodeKinds(), collectors));
    const auto report = MetadataCollectorDriver::collectReport({}, collectors);
    EXPECT_EQ(report.count(QStringLiteral("anything")), 0);
    EXPECT_TRUE(report.values(QStringLiteral("anything")).isEmpty());
}

TEST(MetadataCollectorContractTest, ReportMergeAddsCountsAndDeduplicatesValues)
{
    MetadataReport first;
    first.addCount(QStringLiteral("layers"), 2);
    first.add(QStringLiteral("tags"), QStringLiteral("night"));
    first.add(QStringLiteral("tags"), QStringLiteral("night"));

    MetadataReport second;
    second.addCount(QStringLiteral("layers"), 3);
    second.add(QStringLiteral("tags"), QStringLiteral("day"));
    second.add(QStringLiteral("tags"), QStringLiteral("night"));

    first.merge(second);

    EXPECT_EQ(first.count(QStringLiteral("layers")), 5);
    ASSERT_EQ(first.values(QStringLiteral("tags")).size(), 2);
    EXPECT_EQ(first.values(QStringLiteral("tags")).at(0).toString(), QStringLiteral("night"));
    EXPECT_EQ(first.values(QStringLiteral("tags")).at(1).toString(), QStringLiteral("day"));
}

TEST(MetadataCollectorContractTest, JsonSerializationSeparatesCountsAndValueArrays)
{
    MetadataReport report;
    report.addCount(QStringLiteral("effects"), 4);
    report.add(QStringLiteral("name"), QStringLiteral("Grade"));
    report.add(QStringLiteral("name"), 12);

    const auto json = report.toJson();
    ASSERT_TRUE(json.value(QStringLiteral("counts")).isObject());
    ASSERT_TRUE(json.value(QStringLiteral("values")).isObject());
    EXPECT_EQ(json.value(QStringLiteral("counts")).toObject()
                  .value(QStringLiteral("effects")).toInteger(), 4);
    const auto values = json.value(QStringLiteral("values")).toObject()
                            .value(QStringLiteral("name")).toArray();
    ASSERT_EQ(values.size(), 2);
    EXPECT_EQ(values[0].toString(), QStringLiteral("Grade"));
    EXPECT_EQ(values[1].toInt(), 12);
}

TEST(MetadataCollectorContractTest, CsvEscapesQuotesCommasAndLineBreaks)
{
    MetadataReport report;
    report.addCount(QStringLiteral("a,b"), 2);
    report.add(QStringLiteral("a,b"), QStringLiteral("say \"hi\",\nnext"));

    const QString csv = report.toCsv();
    EXPECT_TRUE(csv.startsWith(QStringLiteral("kind,key,value\n")));
    EXPECT_TRUE(csv.contains(QStringLiteral("count,\"a,b\",2")));
    EXPECT_TRUE(csv.contains(QStringLiteral("value,\"a,b\",\"say \"\"hi\"\",\nnext\"")));
}

TEST(MetadataCollectorContractTest, MissingKeysReturnEmptyValuesAndZeroCounts)
{
    const MetadataReport report;
    EXPECT_EQ(report.count(QStringLiteral("missing")), 0);
    EXPECT_TRUE(report.values(QStringLiteral("missing")).isEmpty());
}
