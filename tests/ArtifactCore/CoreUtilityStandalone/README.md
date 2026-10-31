# ArtifactCore Core Utility Standalone Tests

This project registers fifty-four Core utility, container, profiling, math, animation, simulation, Qt utility, threading, and diagnostic contract suites.
Their shared Core, Container, and SharedPtr modules use one foundation target.
Core, Qt Core, and thread modules use separate feature targets, compiled once
for the test executables that import them. HashValue's Qt implementation has a
single owner target shared by its test executable. CrashReportParser reuses the existing Snapshot, Result, and ArtifactString module targets. NameGenerator has a separate feature target so Qt-only tests do not depend on the full Core utility module graph.

Configure this directory for focused work, or use
`tests/ArtifactCore/StandaloneSuite` to share the foundation artifacts with the
Core value, Data, and Audio suites.
