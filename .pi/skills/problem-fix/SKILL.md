---
name: problem-fix
description: Diagnose and minimally fix a reported defect. Use when investigating errors, regressions, crashes, or incorrect behavior that require reproduction, log/code analysis, root-cause confirmation, and a narrowly scoped correction.
---

# Problem Fix

Fix defects using evidence before edits. Preserve a short, auditable chain from report to reproduction, root cause, correction, and verification.

## Required workflow

1. **Establish the report**
   - Capture the expected behavior, actual behavior, environment, exact steps, and available error logs.
   - Read applicable repository instructions and relevant existing documentation before investigating.
   - Inspect `git status -s` and do not overwrite unrelated user changes.

2. **Reproduce before editing**
   - Do not modify production code, configuration, or tests until the reported failure has been reproduced or there is a documented blocker.
   - Use the smallest reliable reproduction: existing focused test, command, fixture, or manual steps.
   - Preserve the command/steps, relevant output, error text, stack trace, exit status, and environment details.
   - Obey repository execution rules. If a build, test, application launch, or other execution requires explicit user approval, request approval before running it. Never replace reproduction with speculation when approval is withheld.
   - If reproduction is blocked, report the blocker and gather static evidence only; do not claim the bug is fixed.

3. **Investigate the failure**
   - Read the full relevant error log, including the first causal error and surrounding context.
   - Trace the implicated code, callers, inputs, state transitions, and error handling. Search for analogous working and failing paths.
   - Separate verified facts from hypotheses. Prefer direct evidence over broad refactors or dependency changes.

4. **State the root-cause hypothesis before changing code**
   - Tell the user, before any edit:
     - reproduction result;
     - relevant evidence from logs and code;
     - the suspected root cause and why it explains the failure;
     - the minimal files and change proposed.
   - If evidence is insufficient, ask a focused question or request the missing log/reproduction access rather than guessing.

5. **Apply the smallest correction**
   - Change only files required to correct the confirmed cause. Avoid opportunistic cleanup, formatting churn, refactors, generated files, and unrelated test changes.
   - Add or adjust a focused regression check only when it is necessary and allowed by repository policy.
   - Reinspect the diff to ensure scope and line endings are preserved.

6. **Verify and report**
   - Re-run the original reproduction and any directly relevant focused check, subject to execution approval rules.
   - Report: root cause, files changed, verification result, and any unverified risk or blocker.
   - Do not claim success unless the original failure no longer reproduces.

## Guardrails

- Never edit first and investigate afterward.
- Never treat a symptom suppression, broad catch, retry, or disabled assertion as a fix without proving it addresses the cause.
- Keep error logs and source evidence distinct from assumptions.
- Follow repository rules for submodules, commits, documentation, generated artifacts, and tool execution.
- Do not commit, push, build, or run tests unless the user and repository instructions permit it.

## Response checkpoints

Before an edit, use this concise format:

```text
Reproduction: <passed/failed/blocked and exact evidence>
Evidence: <log and code facts>
Root-cause hypothesis: <cause and causal link>
Proposed minimal change: <files and intent>
```

After verification, use:

```text
Root cause: <confirmed cause>
Changed: <file(s) and minimal correction>
Verified: <reproduction/check and result>
Unverified: <none or remaining limitation>
```
