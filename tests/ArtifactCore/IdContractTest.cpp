#include <gtest/gtest.h>

#include <QHashFunctions>
#include <QString>
#include <QStringList>

#include <utility>

import Utils.Id;

using namespace ArtifactCore;

TEST(IdContractTest, DefaultConstructionGeneratesValidDistinctIds)
{
    const Id first;
    const Id second;
    EXPECT_TRUE(static_cast<bool>(first));
    EXPECT_TRUE(static_cast<bool>(second));
    EXPECT_NE(first, second);
    EXPECT_EQ(first.toString().size(), 36);
    EXPECT_EQ(second.toString().size(), 36);
}

TEST(IdContractTest, NilFactoryAndClearProduceNilUuid)
{
    Id nil = Id::Nil();
    EXPECT_TRUE(nil.isNil());
    EXPECT_FALSE(nil);
    EXPECT_TRUE(!nil);
    EXPECT_EQ(nil, Id::Nil());

    Id value;
    value.clear();
    EXPECT_TRUE(value.isNil());
    EXPECT_EQ(value, nil);
}

TEST(IdContractTest, ValidUuidStringRoundTripsInCanonicalForm)
{
    const Id value(QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"));
    EXPECT_TRUE(value.isValid());
    EXPECT_EQ(value.toString(), QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"));
}

TEST(IdContractTest, UuidValueConstructorPreservesTheUuid)
{
    const Id source(QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"));
    const Id copiedFromUuid(source.getUuid());

    EXPECT_EQ(copiedFromUuid, source);
    EXPECT_EQ(copiedFromUuid.toString(), source.toString());
}

TEST(IdContractTest, UppercaseUuidStringNormalizesToLowercase)
{
    const Id value(QStringLiteral("ABCDEF01-2345-6789-ABCD-EF0123456789"));
    EXPECT_EQ(value.toString(), QStringLiteral("abcdef01-2345-6789-abcd-ef0123456789"));
}

TEST(IdContractTest, InvalidUuidStringFallsBackToNil)
{
    const Id invalid(QStringLiteral("not-a-uuid"));
    EXPECT_TRUE(invalid.isNil());
    EXPECT_FALSE(invalid);
}

TEST(IdContractTest, NilStringUsesCanonicalUuidRepresentation)
{
    EXPECT_EQ(Id::Nil().toString(), QStringLiteral("00000000-0000-0000-0000-000000000000"));
}

TEST(IdContractTest, CopyConstructionAndAssignmentPreserveValueIndependently)
{
    const Id source(QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"));
    Id copied(source);
    Id assigned;
    assigned = source;
    EXPECT_EQ(copied, source);
    EXPECT_EQ(assigned, source);

    copied.clear();
    EXPECT_TRUE(copied.isNil());
    EXPECT_FALSE(source.isNil());
    EXPECT_EQ(assigned, source);
}

TEST(IdContractTest, MoveConstructionTransfersValueAndLeavesSourceNil)
{
    Id source(QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"));
    Id moved(std::move(source));
    EXPECT_EQ(moved.toString(), QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"));
    EXPECT_TRUE(source.isNil());
    EXPECT_TRUE(source.toString().isEmpty());
}

TEST(IdContractTest, MoveAssignmentTransfersValueAndLeavesSourceNil)
{
    Id source(QStringLiteral("fedcba98-7654-3210-fedc-ba9876543210"));
    Id destination;
    destination = std::move(source);
    EXPECT_EQ(destination.toString(), QStringLiteral("fedcba98-7654-3210-fedc-ba9876543210"));
    EXPECT_TRUE(source.isNil());
    EXPECT_TRUE(source.toString().isEmpty());
}

TEST(IdContractTest, CopyAndMoveSelfAssignmentKeepTheIdentifierValid)
{
    Id value(QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"));
    value = value;
    EXPECT_EQ(value.toString(), QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"));
    value = std::move(value);
    EXPECT_EQ(value.toString(), QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"));
}

TEST(IdContractTest, EqualityInequalityAndOrderingUseUuidValue)
{
    const Id first(QStringLiteral("00000000-0000-0000-0000-000000000001"));
    const Id second(QStringLiteral("00000000-0000-0000-0000-000000000002"));
    const Id firstCopy(first);
    EXPECT_EQ(first, firstCopy);
    EXPECT_NE(first, second);
    EXPECT_LT(first, second);
    EXPECT_FALSE(second < first);
}

TEST(IdContractTest, TypedIdWrappersCanBeConstructedFromBaseId)
{
    const Id base(QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"));
    const CompositionID composition(base);
    const LayerID layer(base);
    const AssetID asset(base);
    const ProjectID project(base);
    EXPECT_EQ(composition, base);
    EXPECT_EQ(layer, base);
    EXPECT_EQ(asset, base);
    EXPECT_EQ(project, base);
}

TEST(IdContractTest, EqualIdsProduceEqualQtHashes)
{
    const Id first(QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"));
    const Id copy(first);
    EXPECT_EQ(qHash(first), qHash(copy));
    EXPECT_EQ(qHash(first, 19), qHash(copy, 19));
}

TEST(IdContractTest, ClearIsIdempotentAndClearedIdCanBeReassigned)
{
    Id value(QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"));
    value.clear();
    value.clear();
    EXPECT_TRUE(value.isNil());
    value = Id(QStringLiteral("fedcba98-7654-3210-fedc-ba9876543210"));
    EXPECT_EQ(value.toString(), QStringLiteral("fedcba98-7654-3210-fedc-ba9876543210"));
}

TEST(IdContractTest, MovedFromIdCanBeReinitializedByClear)
{
    Id source(QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"));
    Id moved(std::move(source));
    source.clear();
    EXPECT_TRUE(source.isNil());
    EXPECT_EQ(source.toString(), Id::Nil().toString());
    EXPECT_FALSE(moved.isNil());
}

TEST(IdContractTest, MovedFromIdHasNilComparisonOrderingAndHashSemantics)
{
    Id source(QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"));
    Id moved(std::move(source));
    const Id nil = Id::Nil();

    EXPECT_EQ(source, nil);
    EXPECT_FALSE(source != nil);
    EXPECT_FALSE(source < nil);
    EXPECT_FALSE(nil < source);
    EXPECT_EQ(qHash(source), qHash(nil));
    EXPECT_EQ(qHash(source, 47), qHash(nil, 47));
    EXPECT_NE(moved, nil);
}

TEST(IdContractTest, CopyingMovedFromIdProducesUsableNilIdentifiers)
{
    Id source(QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"));
    Id moved(std::move(source));

    const Id copied(source);
    Id assigned(QStringLiteral("fedcba98-7654-3210-fedc-ba9876543210"));
    assigned = source;

    EXPECT_TRUE(copied.isNil());
    EXPECT_EQ(copied.toString(), Id::Nil().toString());
    EXPECT_TRUE(assigned.isNil());
    EXPECT_EQ(assigned.toString(), Id::Nil().toString());
    EXPECT_EQ(moved.toString(), QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"));
}

TEST(IdContractTest, TypedIdWrappersWithTheSameUuidCompareEqual)
{
    const Id base(QStringLiteral("01234567-89ab-cdef-0123-456789abcdef"));
    const CompositionID composition(base);
    const LayerID layer(base);
    EXPECT_EQ(composition, layer);
    EXPECT_FALSE(composition != layer);
}

TEST(IdContractTest, NilIdentifiersHaveEqualHashesAcrossCopies)
{
    const Id first = Id::Nil();
    const Id second(first);
    EXPECT_EQ(qHash(first), qHash(second));
    EXPECT_EQ(qHash(first, 31), qHash(second, 31));
}

TEST(IdContractTest, MalformedUuidShapesFallBackToNil)
{
    const QStringList malformed{
        QStringLiteral(""),
        QStringLiteral("01234567-89ab-cdef-0123-456789abcde"),
        QStringLiteral("01234567-89ab-cdef-0123-456789abcdeZ")};
    for (const QString& text : malformed) {
        EXPECT_TRUE(Id(text).isNil()) << text.toStdString();
    }
}

TEST(IdContractTest, EmbeddedNullSuffixDoesNotTruncateMalformedUuidInput)
{
    QString input = QStringLiteral("01234567-89ab-cdef-0123-456789abcdef");
    input.append(QChar(u'\0'));
    input.append(QStringLiteral("ignored"));

    const Id value(input);

    EXPECT_TRUE(value.isNil());
}
