#include <gtest/gtest.h>
#include <QJsonObject>
#include <QString>

import Collaborate.Session;
import Collaborate.Review;

using namespace ArtifactCore;

namespace {
constexpr qint64 kT0 = 5'000'000;

CollabOperationData makeOperation(const QString& layerId, const int value) {
    CollabOperationData op;
    op.type = QStringLiteral("property.set");
    op.layerId = layerId;
    op.payload = QJsonObject{{QStringLiteral("opacity"), value}};
    op.clientId = QStringLiteral("author-1");
    op.timestampMs = kT0 + value;
    return op;
}
} // namespace

TEST(CollabReviewTest, CommentsAnchorResolveAndThreads)
{
    CollaborationReview review;
    const QString root = review.addComment(
        QStringLiteral("c1"), QStringLiteral("u1"), QStringLiteral("Alice"),
        QStringLiteral("comp-1"), QStringLiteral("layer-2"), 48,
        QStringLiteral("This keyframe feels late"), kT0);
    ASSERT_FALSE(root.isEmpty());

    EXPECT_EQ(review.unresolvedCount(), 1);

    const QString reply =
        review.addReply(root, QStringLiteral("c2"), QStringLiteral("u2"),
                        QStringLiteral("Bob"), QStringLiteral("Agreed"), kT0 + 1);
    ASSERT_FALSE(reply.isEmpty());

    // Replies cannot nest.
    const QString nested = review.addReply(
        reply, QStringLiteral("c3"), QStringLiteral("u3"), QStringLiteral("Cara"),
        QStringLiteral("nope"), kT0 + 2);
    EXPECT_TRUE(nested.isEmpty());

    // Only thread roots resolve.
    EXPECT_FALSE(review.resolveComment(reply));
    EXPECT_TRUE(review.resolveComment(root));

    // Resolved comments are hidden by default, visible on request.
    EXPECT_EQ(review.unresolvedCount(), 0);
    EXPECT_EQ(review.commentsFor(QStringLiteral("comp-1")).size(), 0);
    EXPECT_EQ(review.commentsFor(QStringLiteral("comp-1"), QString{}, true).size(),
              2);

    // Layer filtering works.
    EXPECT_EQ(review.commentsFor(QStringLiteral("comp-1"),
                                QStringLiteral("layer-other"), true)
                  .size(),
              0);
}

TEST(CollabReviewTest, EditAndRemoveEnforceOwnership)
{
    CollaborationReview review;
    const QString commentId = review.addComment(
        QStringLiteral("c1"), QStringLiteral("u1"), QStringLiteral("Alice"),
        QStringLiteral("comp-1"), QString{}, -1, QStringLiteral("draft"), kT0);

    // Non-authors cannot edit.
    EXPECT_FALSE(review.editComment(commentId, QStringLiteral("hacked"),
                                    QStringLiteral("c2")));
    EXPECT_TRUE(review.editComment(commentId, QStringLiteral("final text"),
                                   QStringLiteral("c1")));
    EXPECT_EQ(review.commentsFor(QStringLiteral("comp-1"), QString{}, true)
                  .front()
                  .text.toStdString(),
              "final text");

    EXPECT_FALSE(review.removeComment(commentId, QStringLiteral("c2")));
    EXPECT_TRUE(review.removeComment(commentId, QStringLiteral("c1")));
    EXPECT_EQ(review.commentsFor(QStringLiteral("comp-1"), QString{}, true)
                  .size(),
              0);
}

TEST(ArtifactCollabProposalTest, ProposalLifecycleEnforcesTransitions)
{
    CollaborationReview review;
    Array<CollabOperationData> operations;
    operations.append(makeOperation(QStringLiteral("layer-1"), 30));
    operations.append(makeOperation(QStringLiteral("layer-1"), 60));

    const QString proposalId = review.createProposal(
        QStringLiteral("Opacity ramp"), QStringLiteral("c1"),
        QStringLiteral("Alice"), operations, kT0);
    ASSERT_FALSE(proposalId.isEmpty());
    EXPECT_EQ(review.pendingProposals().size(), 1);

    // Empty proposals are rejected at creation time.
    EXPECT_TRUE(review
                    .createProposal(QStringLiteral("empty"),
                                    QStringLiteral("c1"),
                                    QStringLiteral("Alice"), {}, kT0)
                    .isEmpty());

    // Non-authors cannot withdraw a pending proposal.
    EXPECT_FALSE(review.withdrawProposal(proposalId, QStringLiteral("c2"), kT0 + 1));
    EXPECT_EQ(review.pendingProposals().size(), 1);

    // The author can withdraw while it is still pending.
    EXPECT_TRUE(review.withdrawProposal(proposalId, QStringLiteral("c1"), kT0 + 2));
    EXPECT_EQ(review.pendingProposals().size(), 0);

    // Withdrawn proposals are terminal.
    EXPECT_FALSE(review.acceptProposal(proposalId, QStringLiteral("x"), kT0 + 3).size() > 0);
    EXPECT_FALSE(review.rejectProposal(proposalId, QStringLiteral("x"), kT0 + 4));
    EXPECT_EQ(review.proposal(proposalId).status,
              CollabProposalStatus::Withdrawn);
}

// Withdraw-after-accept must fail (status transition enforcement).
TEST(ArtifactCollabProposalTest, AcceptedProposalsCannotBeWithdrawn)
{
    CollaborationReview review;
    Array<CollabOperationData> operations;
    operations.append(makeOperation(QStringLiteral("layer-1"), 10));
    const QString proposalId = review.createProposal(
        QStringLiteral("Ramp"), QStringLiteral("c1"), QStringLiteral("Alice"),
        operations, kT0);

    const auto acceptedOps =
        review.acceptProposal(proposalId, QStringLiteral("reviewer"), kT0 + 1,
                              QStringLiteral("looks good"));
    ASSERT_EQ(acceptedOps.size(), 2);
    EXPECT_EQ(acceptedOps[1].payload.value(QStringLiteral("opacity")).toInt(), 60);

    EXPECT_FALSE(review.withdrawProposal(proposalId, QStringLiteral("c1"), kT0 + 2));
    EXPECT_FALSE(review.acceptProposal(proposalId, QStringLiteral("x"), kT0 + 3).size() > 0);
    EXPECT_FALSE(review.rejectProposal(proposalId, QStringLiteral("x"), kT0 + 4));

    const auto stored = review.proposal(proposalId);
    EXPECT_EQ(stored.status, CollabProposalStatus::Accepted);
    EXPECT_EQ(stored.decidedByClientId.toStdString(), "reviewer");
}
