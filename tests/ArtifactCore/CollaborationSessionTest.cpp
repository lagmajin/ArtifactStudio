#include <gtest/gtest.h>
#include <QJsonObject>
#include <QString>

import Collaborate.Session;
import Collaborate.Review;
import Collaborate.Operations;
import Collaborate.SessionAdapter;
import Network.CollaborationWebSocket;

using namespace ArtifactCore;

namespace {
constexpr qint64 kT0 = 1'000'000;
}

TEST(CollaborationSessionTest, RosterTracksJoinLeaveAndPresence)
{
    CollaborationSession session;
    session.setLocalIdentity(QStringLiteral("local-1"), QStringLiteral("u0"),
                             QStringLiteral("Local"), QStringLiteral("#ffffff"));

    session.processUserJoined(QStringLiteral("c1"), QStringLiteral("u1"),
                              QStringLiteral("Alice"), QStringLiteral("#ff0000"),
                              kT0);
    ASSERT_TRUE(session.hasParticipant(QStringLiteral("c1")));
    EXPECT_EQ(session.participant(QStringLiteral("c1")).userName.toStdString(),
              "Alice");

    // Local echo of join is ignored.
    session.processUserJoined(QStringLiteral("local-1"), QStringLiteral("u0"),
                              QStringLiteral("Local"), QStringLiteral("#ffffff"),
                              kT0);
    EXPECT_EQ(session.participants().size(), 1);

    QJsonObject presence;
    presence[QStringLiteral("layer")] = QStringLiteral("layer-7");
    session.processPresence(QStringLiteral("c1"), presence, kT0 + 5);
    EXPECT_EQ(session.participant(QStringLiteral("c1")).lastPresenceMs,
              kT0 + 5);
    EXPECT_EQ(session.participant(QStringLiteral("c1"))
                  .presence.value(QStringLiteral("layer"))
                  .toString()
                  .toStdString(),
              "layer-7");

    // Leaving releases the departed peer's locks.
    session.processLockGranted(QStringLiteral("layer-1"), QStringLiteral("c1"),
                               QStringLiteral("u1"), QStringLiteral("Alice"),
                               kT0);
    EXPECT_TRUE(session.isLayerLocked(QStringLiteral("layer-1")));
    session.processUserLeft(QStringLiteral("c1"));
    EXPECT_FALSE(session.hasParticipant(QStringLiteral("c1")));
    EXPECT_FALSE(session.isLayerLocked(QStringLiteral("layer-1")));
}

TEST(CollaborationSessionTest, OperationEchoDeduplicationAndVersions)
{
    CollaborationSession session;
    session.setLocalIdentity(QStringLiteral("local-1"), QStringLiteral("u0"),
                             QStringLiteral("Local"), QStringLiteral("#ffffff"));

    auto local = session.createLocalOperation(
        QStringLiteral("transform.move"), QStringLiteral("layer-1"),
        QJsonObject{{QStringLiteral("x"), 12.0}}, kT0);
    EXPECT_EQ(local.version, -1); // pending server echo
    EXPECT_EQ(session.pendingLocalOperationCount(), 1);

    // Remote peer operation arrives first.
    CollabOperationData remote;
    remote.type = QStringLiteral("transform.move");
    remote.layerId = QStringLiteral("layer-2");
    remote.clientId = QStringLiteral("c1");
    remote.timestampMs = kT0 + 1;
    remote.version = 0;
    session.processRemoteOperation(remote);
    EXPECT_EQ(session.latestVersion(), 0);

    // Server echo assigns the version to the pending local op.
    CollabOperationData echo = local;
    echo.version = 1;
    session.processRemoteOperation(echo);
    EXPECT_EQ(session.pendingLocalOperationCount(), 0);
    ASSERT_EQ(session.operationLog().size(), 2);
    EXPECT_EQ(session.operationLog()[0].version, 1);

    // The same echo again must not duplicate the log entry.
    session.processRemoteOperation(echo);
    EXPECT_EQ(session.operationLog().size(), 2);

    // Two local operations in the SAME millisecond must not collide.
    const auto first = session.createLocalOperation(
        QStringLiteral("property.set"), QStringLiteral("layer-1"), {}, kT0);
    const auto second = session.createLocalOperation(
        QStringLiteral("property.set"), QStringLiteral("layer-1"), {}, kT0);
    EXPECT_NE(first.dedupeKey(), second.dedupeKey());
    EXPECT_NE(first.sequence, second.sequence);

    // Each echo resolves its own pending entry.
    CollabOperationData firstEcho = first;
    firstEcho.version = 2;
    session.processRemoteOperation(firstEcho);
    CollabOperationData secondEcho = second;
    secondEcho.version = 3;
    session.processRemoteOperation(secondEcho);
    EXPECT_EQ(session.pendingLocalOperationCount(), 0);
}

TEST(CollaborationSessionTest, TwoClientsConcurrentPropertyEditsUseServerOrder)
{
    CollaborationSession clientA;
    CollaborationSession clientB;
    clientA.setLocalIdentity(QStringLiteral("client-a"), QStringLiteral("user-a"),
                             QStringLiteral("Alice"), QStringLiteral("#ff0000"));
    clientB.setLocalIdentity(QStringLiteral("client-b"), QStringLiteral("user-b"),
                             QStringLiteral("Bob"), QStringLiteral("#0000ff"));

    // Both clients begin from the same project value and submit an edit before
    // either has received the other's operation.
    const QJsonObject alicePayload{
        {QStringLiteral("propertyPath"), QStringLiteral("transform.opacity")},
        {QStringLiteral("expectedValue"), 1.0},
        {QStringLiteral("value"), 0.5},
    };
    const QJsonObject bobPayload{
        {QStringLiteral("propertyPath"), QStringLiteral("transform.opacity")},
        {QStringLiteral("expectedValue"), 1.0},
        {QStringLiteral("value"), 0.25},
    };
    const auto aliceEdit = clientA.createLocalOperation(
        QStringLiteral("property.set"), QStringLiteral("layer-title"),
        alicePayload, kT0);
    const auto bobEdit = clientB.createLocalOperation(
        QStringLiteral("property.set"), QStringLiteral("layer-title"),
        bobPayload, kT0);

    // Model serialized relay delivery and the receiving client's
    // expected-value check. Alice's compare-and-set wins; Bob's is stale.
    double serverOpacity = 1.0;
    qint64 serverVersion = 0;
    const auto serverSubmit = [&serverOpacity, &serverVersion](
                                 const CollabOperationData& operation,
                                 CollabOperationData& echo) {
        const auto payload = operation.payload;
        if (payload.value(QStringLiteral("expectedValue")).toDouble(-1.0) !=
            serverOpacity) {
            return false;
        }
        serverOpacity = payload.value(QStringLiteral("value")).toDouble();
        echo = operation;
        echo.version = serverVersion++;
        return true;
    };
    CollabOperationData accepted;
    ASSERT_TRUE(serverSubmit(aliceEdit, accepted));
    EXPECT_FALSE(clientA.processRemoteOperation(accepted)); // own echo
    EXPECT_TRUE(clientB.processRemoteOperation(accepted));
    CollabOperationData rejectedEcho;
    EXPECT_FALSE(serverSubmit(bobEdit, rejectedEcho));
    EXPECT_DOUBLE_EQ(serverOpacity, 0.5);
    EXPECT_EQ(clientA.pendingLocalOperationCount(), 0);
    EXPECT_EQ(clientB.latestVersion(), 0);
    EXPECT_EQ(clientB.operationLog().size(), 2); // pending local + remote winner

    // Rejection compensates the optimistic edit and clears its pending entry.
    EXPECT_TRUE(clientB.discardPendingLocalOperation(bobEdit));
    EXPECT_EQ(clientB.pendingLocalOperationCount(), 0);
    EXPECT_EQ(clientB.latestVersion(), 0);
    EXPECT_FALSE(clientB.processRemoteOperation(accepted)); // duplicate delivery
    EXPECT_EQ(clientB.operationLog().size(), 1);
}

TEST(CollaborationSessionTest,
     TwoClientsInterleaveEditsAndReplayReviewHistoryAfterReconnect)
{
    CollaborationSession clientA;
    CollaborationSession clientB;
    clientA.setLocalIdentity(QStringLiteral("client-a"), QStringLiteral("user-a"),
                             QStringLiteral("Alice"), QStringLiteral("#ff0000"));
    clientB.setLocalIdentity(QStringLiteral("client-b"), QStringLiteral("user-b"),
                             QStringLiteral("Bob"), QStringLiteral("#0000ff"));
    clientA.processUserJoined(QStringLiteral("client-b"), QStringLiteral("user-b"),
                              QStringLiteral("Bob"), QStringLiteral("#0000ff"),
                              kT0);
    clientB.processUserJoined(QStringLiteral("client-a"), QStringLiteral("user-a"),
                              QStringLiteral("Alice"), QStringLiteral("#ff0000"),
                              kT0);

    // Presence is peer-scoped, and a lock grant from the relay is visible to
    // the other client while remaining non-blocking for its owner.
    CollabPresenceState presence;
    presence.hasSelection = true;
    presence.selectedLayerId = QStringLiteral("layer-accent");
    presence.hasPlayback = true;
    presence.playbackFrame = 36;
    clientA.processPresence(QStringLiteral("client-b"), presence, kT0 + 1);
    EXPECT_EQ(clientA.participantPresence(QStringLiteral("client-b")).playbackFrame,
              36);
    clientA.processLockGranted(QStringLiteral("layer-title"),
                               QStringLiteral("client-a"),
                               QStringLiteral("user-a"), QStringLiteral("Alice"),
                               kT0 + 2);
    clientB.processLockGranted(QStringLiteral("layer-title"),
                               QStringLiteral("client-a"),
                               QStringLiteral("user-a"), QStringLiteral("Alice"),
                               kT0 + 2);
    EXPECT_FALSE(clientA.isLayerLockedByOther(QStringLiteral("layer-title")));
    EXPECT_TRUE(clientB.isLayerLockedByOther(QStringLiteral("layer-title")));

    CollaborationReview reviewA;
    CollaborationReview reviewB;
    Array<CollabOperationData> durableHistory;
    qint64 serverVersion = 0;
    const auto relay = [&](CollaborationSession& sender,
                           CollaborationSession& receiver,
                           CollaborationReview& senderReview,
                           CollaborationReview& receiverReview,
                           const CollabOperationData& request) {
        EXPECT_TRUE(validateCollabOperation(request).isEmpty());
        auto committed = request;
        committed.version = serverVersion++;
        durableHistory.append(committed);
        const bool senderWasPending = sender.isPendingLocalOperation(committed);
        const bool senderAccepted = sender.processRemoteOperation(committed);
        EXPECT_FALSE(senderAccepted); // local echo is deduplicated
        EXPECT_TRUE(senderWasPending);
        EXPECT_TRUE(receiver.processRemoteOperation(committed));
        if (committed.type.startsWith(QStringLiteral("review.comment."))) {
            EXPECT_TRUE(validateCollabReviewOperation(senderReview, committed).isEmpty());
            EXPECT_TRUE(validateCollabReviewOperation(receiverReview, committed).isEmpty());
            EXPECT_TRUE(applyReviewOperation(senderReview, committed));
            EXPECT_TRUE(applyReviewOperation(receiverReview, committed));
        }
    };

    // Edits on separate layers interleave in one ordered operation stream.
    auto titleEdit = clientA.createLocalOperation(
        makePropertySetOperation(QStringLiteral("client-a"),
                                 QStringLiteral("layer-title"),
                                 QStringLiteral("transform.position.x"), 24.0,
                                 kT0 + 3),
        kT0 + 3);
    relay(clientA, clientB, reviewA, reviewB, titleEdit);
    auto accentEdit = clientB.createLocalOperation(
        makePropertySetOperation(QStringLiteral("client-b"),
                                 QStringLiteral("layer-accent"),
                                 QStringLiteral("transform.position.y"), -12.0,
                                 kT0 + 4),
        kT0 + 4);
    relay(clientB, clientA, reviewB, reviewA, accentEdit);
    EXPECT_EQ(clientA.latestVersion(), 1);
    EXPECT_EQ(clientB.latestVersion(), 1);
    EXPECT_EQ(clientA.pendingLocalOperationCount(), 0);
    EXPECT_EQ(clientB.pendingLocalOperationCount(), 0);

    // Bob disconnects. Alice creates an anchored root comment; it remains in
    // durable server history and is not applied to the disconnected review.
    clientA.processUserLeft(QStringLiteral("client-b"));
    EXPECT_FALSE(clientA.hasParticipant(QStringLiteral("client-b")));
    CollabComment root;
    root.commentId = QStringLiteral("review-root-1");
    root.authorClientId = QStringLiteral("client-a");
    root.authorUserId = QStringLiteral("user-a");
    root.authorName = QStringLiteral("Alice");
    root.compositionId = QStringLiteral("composition-main");
    root.layerId = QStringLiteral("layer-title");
    root.frame = 36;
    root.text = QStringLiteral("Check title timing at this frame");
    root.createdAtMs = kT0 + 5;
    auto rootOp = clientA.createLocalOperation(
        makeReviewCommentAddOperation(QStringLiteral("client-a"), root,
                                      kT0 + 5),
        kT0 + 5);
    auto rootCommit = rootOp;
    rootCommit.version = serverVersion++;
    durableHistory.append(rootCommit);
    EXPECT_FALSE(clientA.processRemoteOperation(rootCommit));
    ASSERT_TRUE(validateCollabReviewOperation(reviewA, rootCommit).isEmpty());
    ASSERT_TRUE(applyReviewOperation(reviewA, rootCommit));
    EXPECT_EQ(reviewB.unresolvedCount(), 0);

    // Reconnect history replay catches Bob up before his reply is accepted.
    clientA.processUserJoined(QStringLiteral("client-b"), QStringLiteral("user-b"),
                              QStringLiteral("Bob"), QStringLiteral("#0000ff"),
                              kT0 + 6);
    clientB.processHistory(durableHistory);
    for (const auto& operation : durableHistory) {
        if (operation.type.startsWith(QStringLiteral("review.comment."))) {
            ASSERT_TRUE(validateCollabReviewOperation(reviewB, operation).isEmpty());
            ASSERT_TRUE(applyReviewOperation(reviewB, operation));
        }
    }
    ASSERT_EQ(reviewB.unresolvedCount(), 1);
    const auto caughtUp = reviewB.commentsFor(QStringLiteral("composition-main"));
    ASSERT_EQ(caughtUp.size(), 1);
    ASSERT_TRUE(caughtUp.first().has_value());
    EXPECT_EQ(caughtUp.first()->layerId.toStdString(), "layer-title");
    EXPECT_EQ(caughtUp.first()->frame, 36);

    // Bob replies, then both clients observe the same anchored thread.
    CollabComment reply;
    reply.commentId = QStringLiteral("review-reply-1");
    reply.parentCommentId = root.commentId;
    reply.authorClientId = QStringLiteral("client-b");
    reply.authorUserId = QStringLiteral("user-b");
    reply.authorName = QStringLiteral("Bob");
    reply.compositionId = root.compositionId;
    reply.layerId = root.layerId;
    reply.frame = root.frame;
    reply.text = QStringLiteral("I will adjust the entrance");
    reply.createdAtMs = kT0 + 7;
    auto replyOp = clientB.createLocalOperation(
        makeReviewCommentAddOperation(QStringLiteral("client-b"), reply,
                                      kT0 + 7),
        kT0 + 7);
    relay(clientB, clientA, reviewB, reviewA, replyOp);

    EXPECT_EQ(reviewA.unresolvedCount(), 1);
    EXPECT_EQ(reviewB.unresolvedCount(), 1);
    const auto threadA = reviewA.commentsFor(QStringLiteral("composition-main"));
    const auto threadB = reviewB.commentsFor(QStringLiteral("composition-main"));
    ASSERT_EQ(threadA.size(), 2);
    ASSERT_EQ(threadB.size(), 2);
    EXPECT_EQ(threadA[0].commentId, threadB[0].commentId);
    EXPECT_EQ(threadA[1].parentCommentId, root.commentId);
    EXPECT_EQ(threadB[1].text.toStdString(), "I will adjust the entrance");
    EXPECT_EQ(clientA.latestVersion(), 3);
    EXPECT_EQ(clientB.latestVersion(), 3);
}

TEST(CollaborationSessionTest,
     DelimiterCharactersInOperationFieldsDoNotCollideInDeduplication)
{
    CollaborationSession session;
    session.setLocalIdentity(QStringLiteral("local-1"), QStringLiteral("u0"),
                             QStringLiteral("Local"), QStringLiteral("#ffffff"));

    CollabOperationData first;
    first.clientId = QStringLiteral("peer|x");
    first.type = QStringLiteral("property.set");
    first.layerId = QStringLiteral("layer-1");
    first.timestampMs = kT0;
    first.version = 4;

    CollabOperationData second;
    second.clientId = QStringLiteral("peer");
    second.type = QStringLiteral("x|property.set");
    second.layerId = QStringLiteral("layer-1");
    second.timestampMs = kT0;
    second.version = 5;

    EXPECT_NE(first.dedupeKey(), second.dedupeKey());
    EXPECT_TRUE(session.processRemoteOperation(first));
    EXPECT_TRUE(session.processRemoteOperation(second));
    ASSERT_EQ(session.operationLog().size(), 2);
    EXPECT_EQ(session.operationLog()[0].clientId, first.clientId);
    EXPECT_EQ(session.operationLog()[1].clientId, second.clientId);
    EXPECT_EQ(session.latestVersion(), 5);
}

TEST(CollaborationSessionTest, HistoryIngestionFillsVersionGaps)
{
    CollaborationSession session;
    session.setLocalIdentity(QStringLiteral("local-1"), QStringLiteral("u0"),
                             QStringLiteral("Local"), QStringLiteral("#ffffff"));

    Array<CollabOperationData> history;
    for (int i = 0; i < 3; ++i) {
        CollabOperationData op;
        op.type = QStringLiteral("layer.add");
        op.layerId = QString(QStringLiteral("layer-%1")).arg(i);
        op.clientId = QStringLiteral("c1");
        op.timestampMs = kT0 + i;
        op.version = i;
        history.append(op);
    }
    session.processHistory(history);

    EXPECT_EQ(session.operationLog().size(), 3);
    EXPECT_EQ(session.latestVersion(), 2);
}

TEST(CollaborationSessionTest, LockLedgerAndDenials)
{
    CollaborationSession session;
    session.setLocalIdentity(QStringLiteral("local-1"), QStringLiteral("u0"),
                             QStringLiteral("Local"), QStringLiteral("#ffffff"));
    session.processUserJoined(QStringLiteral("c1"), QStringLiteral("u1"),
                              QStringLiteral("Alice"), QStringLiteral("#ff0000"),
                              kT0);

    // Local intent alone does not lock.
    session.requestLocalLock(QStringLiteral("layer-9"));
    EXPECT_TRUE(session.hasPendingLockRequest(QStringLiteral("layer-9")));
    EXPECT_FALSE(session.isLayerLocked(QStringLiteral("layer-9")));

    // Denial clears the pending request and records the reason.
    session.processLockDenied(QStringLiteral("layer-9"),
                              QStringLiteral("Locked by Alice"));
    EXPECT_FALSE(session.hasPendingLockRequest(QStringLiteral("layer-9")));
    EXPECT_EQ(session.lockDenialReason(QStringLiteral("layer-9")).toStdString(),
              "Locked by Alice");

    // A grant to a peer locks it for everyone else.
    session.processLockGranted(QStringLiteral("layer-9"), QStringLiteral("c1"),
                               QStringLiteral("u1"), QStringLiteral("Alice"),
                               kT0 + 1);
    EXPECT_TRUE(session.isLayerLockedByOther(QStringLiteral("layer-9")));

    // Local grant does not block the local user.
    session.processLockGranted(QStringLiteral("layer-8"),
                               QStringLiteral("local-1"), QStringLiteral("u0"),
                               QStringLiteral("Local"), kT0 + 1);
    EXPECT_FALSE(session.isLayerLockedByOther(QStringLiteral("layer-8")));
    session.releaseLocalLock(QStringLiteral("layer-8"));
    EXPECT_FALSE(session.isLayerLocked(QStringLiteral("layer-8")));

    const auto locks = session.activeLocks();
    ASSERT_EQ(locks.size(), 1);
    const auto firstLock = locks.first();
    ASSERT_TRUE(firstLock.has_value());
    EXPECT_EQ(firstLock->layerId.toStdString(), "layer-9");
}

TEST(CollaborationSessionTest, TypedPresenceRoundTripsAndPreservesUnknownKeys)
{
    CollaborationSession session;
    session.setLocalIdentity(QStringLiteral("local-1"), QStringLiteral("u0"),
                             QStringLiteral("Local"), QStringLiteral("#ffffff"));
    session.processUserJoined(QStringLiteral("c1"), QStringLiteral("u1"),
                              QStringLiteral("Alice"), QStringLiteral("#ff0000"),
                              kT0);

    CollabPresenceState state;
    state.hasCursor = true;
    state.cursorX = 120.5;
    state.cursorY = 64.0;
    state.hasSelection = true;
    state.selectedLayerId = QStringLiteral("layer-3");
    state.hasComposition = true;
    state.compositionId = QStringLiteral("comp-1");
    state.hasPlayback = true;
    state.playbackFrame = 42;
    state.statusText = QStringLiteral("editing text");
    session.processPresence(QStringLiteral("c1"), state, kT0 + 9);

    // Unknown keys survive through raw.
    QJsonObject extended = state.toJson();
    extended.insert(QStringLiteral("customFlag"), true);
    session.processPresence(QStringLiteral("c1"),
                            CollabPresenceState::fromJson(extended), kT0 + 10);

    const auto presence = session.participantPresence(QStringLiteral("c1"));
    EXPECT_TRUE(presence.hasCursor);
    EXPECT_DOUBLE_EQ(presence.cursorX, 120.5);
    EXPECT_EQ(presence.selectedLayerId.toStdString(), "layer-3");
    EXPECT_EQ(presence.compositionId.toStdString(), "comp-1");
    EXPECT_EQ(presence.playbackFrame, 42);
    EXPECT_EQ(presence.statusText.toStdString(), "editing text");
    EXPECT_TRUE(presence.raw.value(QStringLiteral("customFlag")).toBool());
}

TEST(CollaborationSessionTest, StaleParticipantsDetectedByHeartbeatTimeout)
{
    CollaborationSession session;
    session.setLocalIdentity(QStringLiteral("local-1"), QStringLiteral("u0"),
                             QStringLiteral("Local"), QStringLiteral("#ffffff"));
    session.processUserJoined(QStringLiteral("fresh"), QStringLiteral("u1"),
                              QStringLiteral("Fresh"), QStringLiteral("#ff0000"),
                              kT0 + 100);
    session.processUserJoined(QStringLiteral("stale"), QStringLiteral("u2"),
                              QStringLiteral("Stale"), QStringLiteral("#00ff00"),
                              kT0);

    // fresh sent a presence recently; stale went silent.
    session.processPresence(QStringLiteral("fresh"), QJsonObject{}, kT0 + 190);

    const auto stale =
        session.staleParticipantClientIds(/*nowMs*/ kT0 + 200,
                                         /*timeoutMs*/ 150);
    ASSERT_EQ(stale.size(), 1);
    const auto firstStale = stale.first();
    ASSERT_TRUE(firstStale.has_value());
    EXPECT_EQ(firstStale->toStdString(), "stale");

    // A new presence refresh clears staleness.
    session.processPresence(QStringLiteral("stale"), QJsonObject{}, kT0 + 195);
    EXPECT_EQ(session.staleParticipantClientIds(kT0 + 200, 150).size(), 0);
}

TEST(CollaborateOperationsTest, TypedBuildersProduceValidatableOperations)
{
    CollaborationSession session;
    session.setLocalIdentity(QStringLiteral("local-1"), QStringLiteral("u0"),
                             QStringLiteral("Local"), QStringLiteral("#ffffff"));

    auto propertyOp = makePropertySetOperation(
        QStringLiteral("local-1"), QStringLiteral("layer-1"),
        QStringLiteral("transform.position.x"), 42.0, kT0);
    propertyOp = session.createLocalOperation(propertyOp, kT0);
    EXPECT_TRUE(validateCollabOperation(propertyOp).isEmpty());
    EXPECT_EQ(propertyOp.payload.value(QStringLiteral("propertyPath"))
                  .toString()
                  .toStdString(),
              "transform.position.x");
    EXPECT_GE(propertyOp.sequence, 0);

    auto transformOp = makeLayerTransformOperation(
        QStringLiteral("local-1"), QStringLiteral("layer-2"), 10.0, -5.0, 45.0,
        1.25f, 0.75f, kT0);
    transformOp = session.createLocalOperation(transformOp, kT0);
    EXPECT_TRUE(validateCollabOperation(transformOp).isEmpty());

    auto addOp = makeLayerAddOperation(
        QStringLiteral("local-1"), QStringLiteral("layer-3"),
        QStringLiteral("solid"), QStringLiteral("My Solid"), QJsonObject{}, kT0);
    addOp.payload.insert(QStringLiteral("compositionId"), QStringLiteral("comp-1"));
    addOp.payload.insert(QStringLiteral("index"), 0);
    addOp.payload.insert(QStringLiteral("leftNeighborId"), QString());
    addOp.payload.insert(QStringLiteral("rightNeighborId"), QString());
    addOp.payload.insert(QStringLiteral("anchorLayerId"), QString());
    addOp.payload.insert(QStringLiteral("layerJson"), QJsonObject{
        {QStringLiteral("id"), QStringLiteral("layer-3")},
        {QStringLiteral("layerType"), QStringLiteral("solid")}});
    EXPECT_TRUE(validateCollabOperation(addOp).isEmpty());

    auto removeOp = makeLayerRemoveOperation(
        QStringLiteral("local-1"), QStringLiteral("layer-3"), kT0);
    removeOp.payload = QJsonObject{
        {QStringLiteral("compositionId"), QStringLiteral("comp-1")},
        {QStringLiteral("index"), 0},
        {QStringLiteral("expectedLayerJson"), QJsonObject{
            {QStringLiteral("id"), QStringLiteral("layer-3")}}}};
    EXPECT_TRUE(validateCollabOperation(removeOp).isEmpty());

    // Malformed payloads are caught by the validator.
    CollabOperationData badProperty;
    badProperty.type = QStringLiteral("property.set");
    badProperty.layerId = QStringLiteral("layer-1");
    EXPECT_FALSE(validateCollabOperation(badProperty).isEmpty());

    CollabOperationData badTransform;
    badTransform.type = QStringLiteral("layer.transform");
    badTransform.layerId = QStringLiteral("layer-1");
    badTransform.payload = QJsonObject{{QStringLiteral("positionX"), 1.0}};
    EXPECT_FALSE(validateCollabOperation(badTransform).isEmpty());

    // Unknown types pass through for forward compatibility.
    CollabOperationData future;
    future.type = QStringLiteral("future.thing");
    EXPECT_TRUE(validateCollabOperation(future).isEmpty());
}

TEST(CollaborateOperationsTest, ProposalValidationGatesAcceptance)
{
    CollaborationReview review;
    Array<CollabOperationData> operations;
    operations.append(makePropertySetOperation(
        QStringLiteral("c1"), QStringLiteral("layer-1"),
        QStringLiteral("transform.position.x"), 42.0, kT0));
    // Second bundled operation is malformed.
    CollabOperationData broken;
    broken.type = QStringLiteral("property.set");
    broken.layerId = QStringLiteral("layer-1");
    operations.append(broken);

    const QString proposalId = review.createProposal(
        QStringLiteral("Mixed bag"), QStringLiteral("c1"),
        QStringLiteral("Alice"), operations, kT0);
    ASSERT_FALSE(proposalId.isEmpty());

    const QString error = validateCollabProposal(review, proposalId);
    EXPECT_TRUE(error.contains(QStringLiteral("operation 1")));

    // Accept is gated: stays pending with no operations returned.
    const auto gated = acceptValidatedProposal(review, proposalId,
                                               QStringLiteral("rev"), kT0 + 1);
    EXPECT_FALSE(gated.first.isEmpty());
    EXPECT_EQ(gated.second.size(), 0u);
    EXPECT_EQ(review.pendingProposals().size(), 1);

    // After fixing the bundle (replace via new pending proposal), acceptance
    // succeeds and yields the operations.
    Array<CollabOperationData> fixed;
    fixed.append(makePropertySetOperation(
        QStringLiteral("c1"), QStringLiteral("layer-1"),
        QStringLiteral("transform.position.x"), 42.0, kT0 + 2));
    const QString fixedId = review.createProposal(
        QStringLiteral("Clean"), QStringLiteral("c1"), QStringLiteral("Alice"),
        fixed, kT0 + 2);
    const auto accepted =
        acceptValidatedProposal(review, fixedId, QStringLiteral("rev"), kT0 + 3);
    EXPECT_TRUE(accepted.first.isEmpty());
    ASSERT_EQ(accepted.second.size(), 1u);
}

TEST(CollaborationSessionAdapterTest, InboundSignalsRouteIntoSessionModel)
{
    CollaborationWebSocket ws;
    CollaborationSession session;
    session.setLocalIdentity(QStringLiteral("local-1"), QStringLiteral("u0"),
                             QStringLiteral("Local"), QStringLiteral("#ffffff"));

    CollaborationSessionAdapter adapter(ws, session, QStringLiteral("proj-1"));
    static_cast<void>(adapter);

    // Simulate inbound wire messages by emitting the socket signals directly
    // (signals are ordinary member functions under Verdigris).
    OperationMessage operation;
    operation.clientId = QStringLiteral("c1");
    operation.version = 4;
    operation.operation = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("property.set")},
        {QStringLiteral("layerId"), QStringLiteral("layer-1")},
        {QStringLiteral("payload"),
         QJsonObject{{QStringLiteral("propertyPath"), QStringLiteral("x")},
                     {QStringLiteral("value"), 5}}},
        {QStringLiteral("opSeq"), 3}};
    ws.remoteOperation(operation);

    ASSERT_EQ(session.operationLog().size(), 1);
    const auto firstOperation = session.operationLog().first();
    ASSERT_TRUE(firstOperation.has_value());
    EXPECT_EQ(firstOperation->clientId.toStdString(), "c1");
    EXPECT_EQ(firstOperation->version, 4);
    // opSeq survived the round trip through the adapter.
    EXPECT_EQ(firstOperation->sequence, 3);
    EXPECT_EQ(session.latestVersion(), 4);

    ws.userJoined(QStringLiteral("c2"), QStringLiteral("u2"),
                  QStringLiteral("Bob"), QStringLiteral("#ffffff"));
    EXPECT_TRUE(session.hasParticipant(QStringLiteral("c2")));

    ws.remoteLockGranted(QStringLiteral("layer-1"), QStringLiteral("c2"),
                         QStringLiteral("u2"));
    EXPECT_TRUE(session.isLayerLockedByOther(QStringLiteral("layer-1")));

    PresenceMessage presence;
    presence.clientId = QStringLiteral("c2");
    presence.presence =
        QJsonObject{{QStringLiteral("selectedLayerId"), QStringLiteral("layer-9")}};
    ws.remotePresence(presence);
    EXPECT_TRUE(session.participantPresence(QStringLiteral("c2")).hasSelection);

    ws.userLeft(QStringLiteral("c2"), QStringLiteral("u2"),
                QStringLiteral("Bob"));
    EXPECT_FALSE(session.hasParticipant(QStringLiteral("c2")));
    EXPECT_FALSE(session.isLayerLocked(QStringLiteral("layer-1")));
}
