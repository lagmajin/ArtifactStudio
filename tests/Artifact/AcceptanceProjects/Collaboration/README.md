# Collaboration Acceptance Project

**最終更新:** 2026-11-19

## Purpose

Manual multi-client acceptance scenario for the existing collaboration session. Use one shared project baseline in two ArtifactStudio instances and the local collaboration server. The scenario covers joining, presence, lock ownership, review threads, and reconnect history replay.

## Prepare the project

Create and save `CollaborationAcceptance.artifact` with:

- One composition named `Collaboration Acceptance`, 1280×720 at 24 fps, frames 0–120.
- Three editable layers with distinct names: `Background`, `Title`, and `Accent`.
- A visible, easy-to-recognize difference between the layers (for example, a solid background, a text title, and a shape accent).
- Save the project, then make a byte-for-byte copy for the second client. Both clients must open the same baseline; do not independently recreate or save divergent copies before joining.

The server compares a canonical project fingerprint at join. It does not transfer project snapshots or external assets. Keep this project self-contained and do not use external media paths.

## Run the manual scenario

1. Start `tools/collaboration-server` using its README instructions.
2. Open the same project baseline in two ArtifactStudio instances. Join the same project ID with distinct display names.
3. Confirm both clients show the same participant roster. Change composition, selected layer, and playback frame on each client and confirm the other client's presence updates.
4. On client A, select `Title` and request its layer lock. Confirm client B sees the lock owner and cannot acquire the same lock. Release it, then confirm B can acquire it.
5. With the lock held by the editing client, change a supported layer property such as opacity. Confirm the other client receives the change once. Try an unsupported edit while collaboration is active and confirm it is rejected before changing the local project.
6. Add a review note anchored to `Title` at frame 24. Reply from the other client, resolve the thread, then reopen it. Confirm both clients show the same thread state and anchor navigation works.
7. Disconnect one client, add a review reply from the remaining client, and reconnect. Confirm history replay restores the missed review operation and the initial lock snapshot is reflected before editing resumes.
8. Save both clients and reopen the project files. Confirm the saved project state is readable and record any collaboration state that is intentionally server-history-only.

## Record results

For each step, record the client pair, server URL, project ID, pass/fail, visible result, and any rejection text. The server prototype does not authenticate individual identities, encrypt stored history, or synchronize arbitrary project edits. A passing manual run does not establish coverage for unsupported mutation paths.

## Related automated coverage

- `tests/ArtifactCore/CollaborationSessionTest.cpp` covers participant/presence tracking, locks, operation echo handling, and reconnect state.
- `CollaborationSessionTest.TwoClientsConcurrentPropertyEditsUseServerOrder` models two clients submitting edits against the same expected value, then verifies server ordering, echo deduplication, stale-edit rejection compensation, and convergence of session history.
- `CollaborationSessionTest.TwoClientsInterleaveEditsAndReplayReviewHistoryAfterReconnect` covers interleaved edits, peer presence, lock visibility, anchored review comments and replies, disconnect, and replay-based catch-up using the production operation validators and review-operation applier.
- `tests/ArtifactCore/CollabReviewTest.cpp` covers anchored comments, replies, ownership, history, and thread state.
- `tools/collaboration-server/README.md` documents server setup and protocol limitations.
