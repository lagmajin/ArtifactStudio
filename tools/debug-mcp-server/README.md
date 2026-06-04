# ArtifactStudio Debug MCP Server

Standalone MCP server for pseudo-breakpoint debugging.

This prototype is intentionally small:

- reads a bridge snapshot from a JSON file if present
- keeps break conditions in a local state file
- exposes snapshot, break condition, session summary, break history, and resume, step, reset, and clear tools over stdio MCP frames

## Start

```bash
cd tools/debug-mcp-server
npm start
npm run check
```

- `npm start` runs the MCP server.
- `npm run check` verifies `server.js` syntax before or after a change.

## Optional bridge file

The default bridge file is `"%TEMP%/ArtifactStudio/debug-bridge.json"` on Windows
and the equivalent temp directory path on other platforms.
Set `ARTIFACT_DEBUG_BRIDGE_FILE` to point at a different JSON file if needed.
If no bridge file is present, the server falls back to the local mock snapshot.

## Runtime state

The server stores its own state in `"%TEMP%/ArtifactStudio/debug-mcp-state.json"`
on Windows and the equivalent temp directory path on other platforms.
Set `ARTIFACT_DEBUG_MCP_STATE_FILE` to point at a different JSON file if needed.
The Artifact app polls the same file and will cooperatively pause or resume
playback when a breakpoint condition matches.

## Tools

- `get_debug_snapshot`
- `set_break_condition`
- `update_break_condition`
- `list_break_conditions`
- `clear_break_condition`
- `clear_all_break_conditions`
- `resume_debug_session`
- `step_one_tick`
- `get_last_break_hit`
- `get_session_summary`
- `get_break_history`
- `clear_history`
- `reset_debug_session`
- `set_mock_snapshot`

## Example Flow

Use the mock snapshot if the app is not connected yet:

```json
{
  "snapshot": {
    "playback": { "state": "Playing", "frame": 120 },
    "selection": { "layerIds": ["layer-7"], "layerNames": ["Glow"] },
    "diagnostics": { "healthState": "issues", "summary": "sample" },
    "properties": [
      { "path": "layer-7.transform.opacity", "type": "number", "value": 0.5 }
    ]
  }
}
```

Then add a breakpoint condition:

```json
{
  "condition": {
    "kind": "property_equals",
    "label": "Opacity dipped",
    "value": {
      "path": "layer-7.transform.opacity",
      "value": 0.5
    }
  }
}
```

Cleanup tools:

- `clear_break_condition` removes one entry by id
- `clear_all_break_conditions` clears the full list

Edit tool:

- `update_break_condition` patches an existing condition without re-creating it

Inspection:

- `get_last_break_hit` returns the last matched condition and snapshot
- `get_session_summary` returns `sessionSummary` with the current mode and history summary
- `get_break_history` returns recent break-history events plus `summary`, counts, and window bounds

Reset:

- `clear_history` clears the stored pseudo-breakpoint history
- `reset_debug_session` clears history and last-hit state, and can also clear all conditions

`get_break_history.summary` feeds `summaryPreview`, and the raw history list is the fallback.

`list_break_conditions` also returns a compact `summary` array for quick scanning.

Example cleanup flow:

```json
{ "clearBreakConditions": true }
```

Call `reset_debug_session` after a repro run if you want to start from a blank
session, then re-seed the mock snapshot or live bridge as needed.

## Notes

This is the initial contract-only implementation.
The Artifact app still needs a bridge hook before break conditions can reflect live UI and playback state.
