#include <gtest/gtest.h>

#include <QAbstractItemModel>
#include <QString>

#include <limits>

import Render.JobModel;
import Utils.Id;

using namespace ArtifactCore;

TEST(RenderJobModelContractTest, EmptyModelHasFlatFourColumnRoot)
{
    RenderJobModel model;

    EXPECT_EQ(model.rowCount(), 0);
    EXPECT_EQ(model.columnCount(), 4);
    EXPECT_EQ(model.rowCount(model.index(0, 0)), 0);
    EXPECT_EQ(model.columnCount(model.index(0, 0)), 0);
    EXPECT_FALSE(model.index(0, 0).isValid());
    EXPECT_FALSE(model.index(0, 4).isValid());
}

TEST(RenderJobModelContractTest, HorizontalHeadersExposeStableColumnNames)
{
    RenderJobModel model;
    const QString expected[] = {
        QStringLiteral("Composition"), QStringLiteral("Status"),
        QStringLiteral("Progress"), QStringLiteral("Output Path")
    };

    for (int column = 0; column < 4; ++column) {
        EXPECT_EQ(model.headerData(column, Qt::Horizontal).toString(),
                  expected[column]);
    }
    EXPECT_FALSE(model.headerData(0, Qt::Horizontal, Qt::ToolTipRole).isValid());
    EXPECT_FALSE(model.headerData(0, Qt::Vertical).isValid());
}

TEST(RenderJobModelContractTest, IndexesRemainFlatAndRejectInvalidCoordinates)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("first"));
    model.addJob(Id::Nil(), QStringLiteral("second"));

    const QModelIndex firstCell = model.index(0, 0);
    ASSERT_TRUE(firstCell.isValid());
    EXPECT_EQ(firstCell.row(), 0);
    EXPECT_EQ(firstCell.column(), 0);
    EXPECT_FALSE(model.parent(firstCell).isValid());
    EXPECT_FALSE(model.index(0, 0, firstCell).isValid());
    EXPECT_FALSE(model.index(-1, 0).isValid());
    EXPECT_FALSE(model.index(2, 0).isValid());
    EXPECT_FALSE(model.index(0, -1).isValid());
    EXPECT_FALSE(model.index(0, 4).isValid());
}

TEST(RenderJobModelContractTest, InsertedJobsExposeNameStatusProgressAndOutput)
{
    RenderJobModel model;
    model.addJob(QStringLiteral("Main Comp"), QStringLiteral("rendering"),
                 37, QStringLiteral("D:/renders/main.exr"));

    ASSERT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.data(model.index(0, 0)).toString(), QStringLiteral("Main Comp"));
    EXPECT_EQ(model.data(model.index(0, 1)).toString(), QStringLiteral("Rendering"));
    EXPECT_EQ(model.data(model.index(0, 2)).toString(), QStringLiteral("37%"));
    EXPECT_EQ(model.data(model.index(0, 3)).toString(), QStringLiteral("D:/renders/main.exr"));
    EXPECT_EQ(model.jobAt(0)->progress, 0.37f);
    EXPECT_EQ(model.jobAt(0)->compositionId, Id::Nil());
}

TEST(RenderJobModelContractTest, BasicInsertionUsesStableRenderJobDefaults)
{
    RenderJobModel model;
    const Id compositionId;
    model.addJob(compositionId, QStringLiteral("Main Comp"));

    ASSERT_NE(model.jobAt(0), nullptr);
    const RenderJob& job = *model.jobAt(0);
    EXPECT_EQ(job.compositionId, compositionId);
    EXPECT_EQ(job.compositionName, QStringLiteral("Main Comp"));
    EXPECT_EQ(job.status, RenderJobStatus::Queued);
    EXPECT_FLOAT_EQ(job.progress, 0.0f);
    EXPECT_TRUE(job.outputPath.isEmpty());
    EXPECT_TRUE(job.statusMessage.isEmpty());
    EXPECT_EQ(job.startFrame, 0);
    EXPECT_EQ(job.endFrame, 299);
    EXPECT_EQ(job.frameStep, 1);
    EXPECT_TRUE(job.multiFrameEnabled);
    EXPECT_EQ(job.mfrConcurrentFrames, 0);
    EXPECT_EQ(job.mfrMemoryLimitMB, 8192u);
    EXPECT_EQ(job.mfrRetryBackoffMs, 0);
}

TEST(RenderJobModelContractTest, AddedProgressIsClampedAtBothPercentBoundaries)
{
    RenderJobModel model;
    model.addJob(QStringLiteral("below"), QStringLiteral("queued"), -20,
                 QString());
    model.addJob(QStringLiteral("above"), QStringLiteral("queued"), 140,
                 QString());

    ASSERT_EQ(model.rowCount(), 2);
    EXPECT_FLOAT_EQ(model.jobAt(0)->progress, 0.0f);
    EXPECT_FLOAT_EQ(model.jobAt(1)->progress, 1.0f);
    EXPECT_EQ(model.data(model.index(0, 2)).toString(), QStringLiteral("0%"));
    EXPECT_EQ(model.data(model.index(1, 2)).toString(), QStringLiteral("100%"));
}

TEST(RenderJobModelContractTest, StatusAliasesMapToStableDisplayLabels)
{
    const struct Case {
        const char* input;
        const char* expected;
    } cases[] = {
        {" queued ", "Queued"}, {"running", "Rendering"},
        {"completed", "Done"}, {"failed", "Error"},
        {"cancelled", "Canceled"}, {"Paused", "Paused"},
        {"unknown-status", "Queued"},
    };

    for (const auto& testCase : cases) {
        RenderJobModel model;
        model.addJob(QStringLiteral("job"), QString::fromLatin1(testCase.input),
                     0, QString());
        ASSERT_EQ(model.rowCount(), 1);
        EXPECT_EQ(model.data(model.index(0, 1)).toString(),
                  QString::fromLatin1(testCase.expected)) << testCase.input;
    }
}

TEST(RenderJobModelContractTest, StatusSetterUpdatesEachStableDisplayLabel)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("job"));
    const struct Case {
        RenderJobStatus status;
        const char* expected;
    } cases[] = {
        {RenderJobStatus::Queued, "Queued"},
        {RenderJobStatus::Rendering, "Rendering"},
        {RenderJobStatus::Done, "Done"},
        {RenderJobStatus::Error, "Error"},
        {RenderJobStatus::Canceled, "Canceled"},
        {RenderJobStatus::Paused, "Paused"},
    };

    for (const auto& testCase : cases) {
        model.setJobStatus(0, testCase.status);
        EXPECT_EQ(model.data(model.index(0, 1)).toString(),
                  QString::fromLatin1(testCase.expected));
    }
}

TEST(RenderJobModelContractTest, UnknownStatusValueHasDefensiveDisplayLabel)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("job"));

    model.setJobStatus(0, static_cast<RenderJobStatus>(999));

    EXPECT_EQ(model.data(model.index(0, 1)).toString(), QStringLiteral("Unknown"));
}

TEST(RenderJobModelContractTest, InvalidStatusAndProgressRowsDoNotChangeJobs)
{
    RenderJobModel model;
    model.addJob(QStringLiteral("job"), QStringLiteral("running"), 42,
                 QStringLiteral("output.png"));
    const RenderJobStatus originalStatus = model.jobAt(0)->status;
    const float originalProgress = model.jobAt(0)->progress;

    model.setJobStatus(-1, RenderJobStatus::Error);
    model.setJobStatus(1, RenderJobStatus::Error);
    model.setJobProgress(-1, 0.9f);
    model.setJobProgress(1, 0.9f);

    EXPECT_EQ(model.jobAt(0)->status, originalStatus);
    EXPECT_FLOAT_EQ(model.jobAt(0)->progress, originalProgress);
}

TEST(RenderJobModelContractTest, ProgressSetterClampsAndSanitizesNonFiniteInput)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("job"));

    model.setJobProgress(0, 1.5f);
    EXPECT_FLOAT_EQ(model.jobAt(0)->progress, 1.0f);
    EXPECT_EQ(model.data(model.index(0, 2)).toString(), QStringLiteral("100%"));

    model.setJobProgress(0, -0.5f);
    EXPECT_FLOAT_EQ(model.jobAt(0)->progress, 0.0f);
    EXPECT_EQ(model.data(model.index(0, 2)).toString(), QStringLiteral("0%"));

    model.setJobProgress(0, std::numeric_limits<float>::quiet_NaN());
    EXPECT_FLOAT_EQ(model.jobAt(0)->progress, 0.0f);
    model.setJobProgress(0, std::numeric_limits<float>::infinity());
    EXPECT_FLOAT_EQ(model.jobAt(0)->progress, 0.0f);

    // The display layer also defends against state modified outside the setter.
    model.jobAt(0)->progress = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(model.data(model.index(0, 2)).toString(), QStringLiteral("0%"));
    model.jobAt(0)->progress = std::numeric_limits<float>::infinity();
    EXPECT_EQ(model.data(model.index(0, 2)).toString(), QStringLiteral("0%"));
}

TEST(RenderJobModelContractTest, InvalidRowsAndUnsupportedRolesReturnSafeResults)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("job"));

    EXPECT_EQ(model.jobAt(-1), nullptr);
    EXPECT_EQ(model.jobAt(1), nullptr);
    EXPECT_FALSE(model.data(QModelIndex()).isValid());
    EXPECT_FALSE(model.data(model.index(-1, 0)).isValid());
    EXPECT_FALSE(model.data(model.index(1, 0)).isValid());
    EXPECT_FALSE(model.data(model.index(0, 0), Qt::ToolTipRole).isValid());
    EXPECT_FALSE(model.headerData(4, Qt::Horizontal).isValid());
    EXPECT_FALSE(model.headerData(0, Qt::Vertical).isValid());
}

TEST(RenderJobModelContractTest, FrameRangeRejectsInvalidUpdatesWithoutChangingPreviousRange)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("job"), 10, 30, 2);
    RenderJob* job = model.jobAt(0);
    ASSERT_NE(job, nullptr);

    EXPECT_EQ(job->startFrame, 10);
    EXPECT_EQ(job->endFrame, 30);
    EXPECT_EQ(job->frameStep, 2);
    EXPECT_FALSE(model.setJobFrameRange(0, 31, 30, 1));
    EXPECT_FALSE(model.setJobFrameRange(0, 0, 10, 0));
    EXPECT_FALSE(model.setJobFrameRange(-1, 0, 10, 1));
    EXPECT_EQ(job->startFrame, 10);
    EXPECT_EQ(job->endFrame, 30);
    EXPECT_EQ(job->frameStep, 2);

    EXPECT_TRUE(model.setJobFrameRange(0, -5, 0, 3));
    EXPECT_EQ(job->startFrame, -5);
    EXPECT_EQ(job->endFrame, 0);
    EXPECT_EQ(job->frameStep, 3);
}

TEST(RenderJobModelContractTest, InvalidInitialFrameRangeDoesNotInsertJob)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("reversed"), 5, 4, 1);
    model.addJob(Id::Nil(), QStringLiteral("zero step"), 0, 1, 0);
    EXPECT_EQ(model.rowCount(), 0);

    model.addJob(Id::Nil(), QStringLiteral("single frame"), 5, 5, 1);
    ASSERT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.jobAt(0)->startFrame, 5);
    EXPECT_EQ(model.jobAt(0)->endFrame, 5);
}

TEST(RenderJobModelContractTest, MfrSettingsRejectNegativeConcurrencyAndBackoffAtomically)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("job"));
    RenderJob* job = model.jobAt(0);
    ASSERT_NE(job, nullptr);

    EXPECT_TRUE(model.setJobMfrSettings(0, true, 4, 2048u, 75));
    EXPECT_TRUE(job->multiFrameEnabled);
    EXPECT_EQ(job->mfrConcurrentFrames, 4);
    EXPECT_EQ(job->mfrMemoryLimitMB, 2048u);
    EXPECT_EQ(job->mfrRetryBackoffMs, 75);

    EXPECT_FALSE(model.setJobMfrSettings(0, false, -1, 0u, 0));
    EXPECT_FALSE(model.setJobMfrSettings(0, false, 1, 0u, -1));
    EXPECT_FALSE(model.setJobMfrSettings(4, false, 1, 0u, 0));
    EXPECT_TRUE(job->multiFrameEnabled);
    EXPECT_EQ(job->mfrConcurrentFrames, 4);
    EXPECT_EQ(job->mfrMemoryLimitMB, 2048u);
    EXPECT_EQ(job->mfrRetryBackoffMs, 75);
}

TEST(RenderJobModelContractTest, MfrSettingsAcceptZeroLimitsAndCanBeDisabled)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("job"));
    RenderJob* job = model.jobAt(0);
    ASSERT_NE(job, nullptr);

    EXPECT_TRUE(model.setJobMfrSettings(0, false, 0, 0u, 0));
    EXPECT_FALSE(job->multiFrameEnabled);
    EXPECT_EQ(job->mfrConcurrentFrames, 0);
    EXPECT_EQ(job->mfrMemoryLimitMB, 0u);
    EXPECT_EQ(job->mfrRetryBackoffMs, 0);
}

TEST(RenderJobModelContractTest, RemoveAndClearMaintainRowCountsAndBounds)
{
    RenderJobModel model;
    model.addJob(Id::Nil(), QStringLiteral("first"));
    model.addJob(Id::Nil(), QStringLiteral("second"));
    model.addJob(Id::Nil(), QStringLiteral("third"));

    model.removeJob(-1);
    model.removeJob(3);
    ASSERT_EQ(model.rowCount(), 3);
    EXPECT_EQ(model.data(model.index(1, 0)).toString(), QStringLiteral("second"));

    model.removeJob(1);
    ASSERT_EQ(model.rowCount(), 2);
    EXPECT_EQ(model.data(model.index(1, 0)).toString(), QStringLiteral("third"));
    model.clearJobs();
    EXPECT_EQ(model.rowCount(), 0);
    EXPECT_EQ(model.jobAt(0), nullptr);
}
