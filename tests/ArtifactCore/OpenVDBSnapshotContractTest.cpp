#include <gtest/gtest.h>

#include <QString>
#include <QVector3D>

#include <vector>

import Core.Simulation.OpenVDBVolumeReference;
import Core.Simulation.Pyro;

using namespace ArtifactCore;

TEST(OpenVDBSnapshotContractTest, VolumeReferenceRequiresEnabledNonblankPath)
{
    OpenVDBVolumeReference reference;
    EXPECT_FALSE(reference.isValid());

    reference.filePath = QStringLiteral("  ");
    EXPECT_FALSE(reference.isValid());
    reference.filePath = QStringLiteral("cache/smoke.vdb");
    EXPECT_TRUE(reference.isValid());
    reference.enabled = false;
    EXPECT_FALSE(reference.isValid());
}

TEST(OpenVDBSnapshotContractTest, SnapshotEmptyReportsWhetherValuesExist)
{
    OpenVDBScalarSnapshot snapshot;
    EXPECT_TRUE(snapshot.empty());
    snapshot.values = {0.0f};
    EXPECT_FALSE(snapshot.empty());
}

TEST(OpenVDBSnapshotContractTest, RejectsInvalidSnapshotWithoutChangingDestination)
{
    PyroFieldSet fields;
    fields.resize({1, 1, 1});
    fields.densityStorage() = {42.0f};

    OpenVDBScalarSnapshot invalid;
    invalid.valid = true;
    invalid.width = 1;
    invalid.height = 1;
    invalid.depth = 1;
    invalid.values = {1.0f, 2.0f};
    EXPECT_FALSE(copyOpenVDBDensityToPyro(invalid, fields));
    EXPECT_EQ(fields.resolution().width, 1);
    EXPECT_EQ(fields.resolution().height, 1);
    EXPECT_EQ(fields.resolution().depth, 1);
    ASSERT_EQ(fields.densityStorage().size(), 1u);
    EXPECT_FLOAT_EQ(fields.densityStorage().front(), 42.0f);
}

TEST(OpenVDBSnapshotContractTest, RejectsInvalidFlagAndNonpositiveDimensions)
{
    PyroFieldSet fields;
    OpenVDBScalarSnapshot snapshot;
    snapshot.values = {1.0f};
    snapshot.width = snapshot.height = snapshot.depth = 1;
    EXPECT_FALSE(copyOpenVDBDensityToPyro(snapshot, fields));

    snapshot.valid = true;
    snapshot.width = 0;
    EXPECT_FALSE(copyOpenVDBDensityToPyro(snapshot, fields));
    snapshot.width = 1;
    snapshot.height = -1;
    EXPECT_FALSE(copyOpenVDBDensityToPyro(snapshot, fields));
    snapshot.height = 1;
    snapshot.depth = 0;
    EXPECT_FALSE(copyOpenVDBDensityToPyro(snapshot, fields));
    EXPECT_TRUE(fields.empty());
}

TEST(OpenVDBSnapshotContractTest, CopiesThreeDimensionalDensityInStorageOrder)
{
    OpenVDBScalarSnapshot snapshot;
    snapshot.valid = true;
    snapshot.width = 2;
    snapshot.height = 2;
    snapshot.depth = 2;
    snapshot.origin = QVector3D(-1.0f, 2.0f, 3.0f);
    snapshot.voxelSize = QVector3D(0.5f, 1.0f, 2.0f);
    snapshot.values = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f};

    PyroFieldSet fields;
    ASSERT_TRUE(copyOpenVDBDensityToPyro(snapshot, fields));
    EXPECT_EQ(fields.resolution().width, 2);
    EXPECT_EQ(fields.resolution().height, 2);
    EXPECT_EQ(fields.resolution().depth, 2);
    EXPECT_EQ(fields.cellCount(), 8u);
    EXPECT_EQ(fields.densityStorage(), snapshot.values);
}
