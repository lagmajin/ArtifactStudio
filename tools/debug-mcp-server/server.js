const fs = require('fs');
const path = require('path');
const os = require('os');

const DEFAULT_STATE_FILE = path.join(os.tmpdir(), 'ArtifactStudio', 'debug-mcp-state.json');
const DEFAULT_BRIDGE_FILE = path.join(os.tmpdir(), 'ArtifactStudio', 'debug-bridge.json');
const STATE_FILE = process.env.ARTIFACT_DEBUG_MCP_STATE_FILE || DEFAULT_STATE_FILE;
const BRIDGE_FILE = process.env.ARTIFACT_DEBUG_BRIDGE_FILE || DEFAULT_BRIDGE_FILE;
const SERVER_NAME = 'ArtifactStudio Debug MCP Server';
const PROTOCOL_VERSION = '2024-11-05';

function nowIso() {
  return new Date().toISOString();
}

function deepClone(value) {
  return value === undefined ? undefined : JSON.parse(JSON.stringify(value));
}

function ensureDirFor(filePath) {
  const dir = path.dirname(filePath);
  fs.mkdirSync(dir, { recursive: true });
}

function readJsonFile(filePath, fallback) {
  try {
    if (!fs.existsSync(filePath)) {
      return fallback;
    }
    const raw = fs.readFileSync(filePath, 'utf8');
    if (!raw.trim()) {
      return fallback;
    }
    return JSON.parse(raw);
  } catch (error) {
    return fallback;
  }
}

function writeJsonFile(filePath, value) {
  ensureDirFor(filePath);
  const tmpPath = `${filePath}.tmp`;
  fs.writeFileSync(tmpPath, `${JSON.stringify(value, null, 2)}\n`, 'utf8');
  fs.renameSync(tmpPath, filePath);
}

function createDefaultSnapshot() {
  return {
    snapshotVersion: 1,
    timestamp: nowIso(),
    app: {
      name: 'Artifact',
      pid: 0
    },
    project: {
      id: '',
      name: 'No project loaded'
    },
    composition: {
      id: '',
      name: 'No composition',
      frameRate: 30,
      frameCount: 0
    },
    playback: {
      state: 'Stopped',
      frame: 0,
      speed: 1,
      looping: false
    },
    selection: {
      layerIds: [],
      layerNames: []
    },
    diagnostics: {
      healthState: 'unknown',
      summary: 'Bridge file not connected yet.'
    },
    trace: [],
    properties: []
  };
}

function createDefaultState() {
  return {
    version: 1,
    session: {
      paused: false,
      tickCount: 0,
      lastAction: 'idle',
      pauseReason: null,
      wasPlayingBeforePause: false,
      pausedAtFrame: null
    },
    nextConditionId: 1,
    breakConditions: [],
    lastBreakHit: null,
    history: [],
    mockSnapshot: createDefaultSnapshot(),
    sessionSummary: {
      text: 'running  mode=mock  bridge=<none>  reason=<none>  frame=<none>  ticks=0',
      status: 'running',
      mode: 'mock',
      source: 'mock',
      bridgePath: '<none>',
      reason: '<none>',
      frame: '<none>',
      lastAction: 'idle',
      recentAction: 'idle',
      tickCount: 0,
      breakConditionCount: 0,
      historyCount: 0,
      lastHit: '<none>'
    }
  };
}

function resetSessionState(state, clearBreakConditions = false) {
  state.session = createDefaultState().session;
  state.history = [];
  state.lastBreakHit = null;
  if (clearBreakConditions) {
    state.breakConditions = [];
    state.nextConditionId = 1;
  }
}

function buildSessionSummary(state, source = 'mock', bridgePath = '<none>') {
  const session = state.session || {};
  const status = session.paused ? 'paused' : 'running';
  const mode = source === 'bridge' ? 'live' : 'mock';
  const reason = String(session.pauseReason || '').trim() || '<none>';
  const frame = Number.isFinite(Number(session.pausedAtFrame))
    ? String(Number(session.pausedAtFrame))
    : String(state.lastBreakHit && state.lastBreakHit.snapshot && state.lastBreakHit.snapshot.playback
        ? state.lastBreakHit.snapshot.playback.frame
        : '<none>');
  const lastAction = String(session.lastAction || 'idle').trim() || '<none>';
  const tickCount = Number.isFinite(Number(session.tickCount)) ? Number(session.tickCount) : 0;
  const breakConditionCount = Array.isArray(state.breakConditions) ? state.breakConditions.length : 0;
  const historyCount = Array.isArray(state.history) ? state.history.length : 0;
  const recentAction = state.history && state.history.length > 0
    ? historyActionLabel(state.history[state.history.length - 1].type)
    : '<none>';
  const lastHit = state.lastBreakHit
    ? historySummary({
        timestamp: state.lastBreakHit.matchedAt,
        type: 'break-hit',
        conditionId: state.lastBreakHit.conditionId,
        conditionKind: state.lastBreakHit.condition && state.lastBreakHit.condition.kind,
        tickCount
      })
    : '<none>';
  return {
    text: `${status}  mode=${mode}  bridge=${bridgePath}  reason=${reason}  frame=${frame}  ticks=${tickCount}  breaks=${breakConditionCount}  history=${historyCount}  hit=${lastHit}`,
    status,
    mode,
    source,
    bridgePath,
    reason,
    frame,
    lastAction,
    recentAction,
    tickCount,
    breakConditionCount,
    historyCount,
    lastHit
  };
}

function refreshDerivedState(state) {
  const source = readBridgeSnapshot() ? 'bridge' : 'mock';
  const bridgePath = source === 'bridge' ? BRIDGE_FILE : '<none>';
  state.sessionSummary = buildSessionSummary(state, source, bridgePath);
}

function normalizeArray(value) {
  return Array.isArray(value) ? value : [];
}

function normalizeSnapshot(snapshot) {
  const root = createDefaultSnapshot();
  if (!snapshot || typeof snapshot !== 'object') {
    return root;
  }

  return {
    ...root,
    ...snapshot,
    app: {
      ...root.app,
      ...(snapshot.app || {})
    },
    project: {
      ...root.project,
      ...(snapshot.project || {})
    },
    composition: {
      ...root.composition,
      ...(snapshot.composition || {})
    },
    playback: {
      ...root.playback,
      ...(snapshot.playback || {})
    },
    selection: {
      ...root.selection,
      ...(snapshot.selection || {}),
      layerIds: normalizeArray(snapshot.selection && snapshot.selection.layerIds).map(String),
      layerNames: normalizeArray(snapshot.selection && snapshot.selection.layerNames).map(String)
    },
    diagnostics: {
      ...root.diagnostics,
      ...(snapshot.diagnostics || {})
    },
    properties: normalizeArray(snapshot.properties).map((entry) => ({
      path: String(entry && entry.path ? entry.path : ''),
      ownerPath: String(entry && entry.ownerPath ? entry.ownerPath : ''),
      propertyName: String(entry && entry.propertyName ? entry.propertyName : ''),
      type: String(entry && entry.type ? entry.type : ''),
      value: entry && Object.prototype.hasOwnProperty.call(entry, 'value') ? entry.value : null,
      readOnly: entry && entry.readOnly !== false
    })),
    trace: normalizeArray(snapshot.trace).map((entry) => {
      if (typeof entry === 'string') {
        return { timestamp: nowIso(), message: entry };
      }
      return {
        timestamp: String(entry && entry.timestamp ? entry.timestamp : nowIso()),
        message: String(entry && entry.message ? entry.message : '')
      };
    })
  };
}

function loadState() {
  const state = readJsonFile(STATE_FILE, null);
  const fallback = createDefaultState();
  if (!state || typeof state !== 'object') {
    return fallback;
  }

  return {
    ...fallback,
    ...state,
    session: {
      ...fallback.session,
      ...(state.session || {})
    },
    breakConditions: normalizeArray(state.breakConditions).map((condition) => ({
      id: Number(condition && condition.id ? condition.id : 0),
      kind: String(condition && condition.kind ? condition.kind : 'frame_equals'),
      label: String(condition && condition.label ? condition.label : ''),
      enabled: condition && condition.enabled !== false,
      value: condition ? condition.value : undefined,
      createdAt: String(condition && condition.createdAt ? condition.createdAt : nowIso())
    })),
    lastBreakHit: state.lastBreakHit || null,
    history: normalizeArray(state.history),
    mockSnapshot: normalizeSnapshot(state.mockSnapshot || fallback.mockSnapshot),
    sessionSummary: state.sessionSummary && typeof state.sessionSummary === 'object'
      ? {
          ...fallback.sessionSummary,
          ...(state.sessionSummary || {})
        }
      : fallback.sessionSummary
  };
}

function saveState(state) {
  refreshDerivedState(state);
  writeJsonFile(STATE_FILE, state);
}

function pushHistory(state, entry) {
  state.history.push({
    timestamp: nowIso(),
    ...entry
  });
  if (state.history.length > 100) {
    state.history.splice(0, state.history.length - 100);
  }
}

function readBridgeSnapshot() {
  const snapshot = readJsonFile(BRIDGE_FILE, null);
  if (!snapshot || typeof snapshot !== 'object') {
    return null;
  }
  return normalizeSnapshot(snapshot);
}

function effectiveSnapshot(state) {
  const bridgeSnapshot = readBridgeSnapshot();
  if (bridgeSnapshot) {
    return bridgeSnapshot;
  }
  return deepClone(state.mockSnapshot);
}

function snapshotFrame(snapshot) {
  const frame = snapshot && snapshot.playback ? snapshot.playback.frame : undefined;
  return Number.isFinite(Number(frame)) ? Number(frame) : 0;
}

function snapshotSelection(snapshot) {
  const selection = snapshot && snapshot.selection ? snapshot.selection : {};
  return {
    ids: normalizeArray(selection.layerIds).map(String),
    names: normalizeArray(selection.layerNames).map(String)
  };
}

function snapshotHealthState(snapshot) {
  return String(snapshot && snapshot.diagnostics && snapshot.diagnostics.healthState
    ? snapshot.diagnostics.healthState
    : 'unknown');
}

function snapshotTraceText(snapshot) {
  if (snapshot && typeof snapshot.traceText === 'string' && snapshot.traceText.trim()) {
    return snapshot.traceText;
  }
  return normalizeArray(snapshot && snapshot.trace)
    .map((entry) => {
      if (typeof entry === 'string') {
        return entry;
      }
      return `${entry.timestamp || ''} ${entry.message || ''}`.trim();
    })
    .join('\n');
}

function snapshotPropertiesText(snapshot) {
  return normalizeArray(snapshot && snapshot.properties)
    .map((entry) => {
      const pathText = String(entry && entry.path ? entry.path : '');
      const valueText = entry && Object.prototype.hasOwnProperty.call(entry, 'value')
        ? JSON.stringify(entry.value)
        : 'null';
      return `${pathText}=${valueText}`;
    })
    .join('\n');
}

function snapshotSignature(snapshot) {
  const selection = snapshotSelection(snapshot);
  return [
    `frame=${snapshotFrame(snapshot)}`,
    `selectionIds=${selection.ids.join(',')}`,
    `selectionNames=${selection.names.join(',')}`,
    `health=${snapshotHealthState(snapshot)}`,
    `trace=${snapshotTraceText(snapshot)}`,
    `properties=${snapshotPropertiesText(snapshot)}`
  ].join('|');
}

function conditionSummary(condition) {
  const label = condition.label ? ` (${condition.label})` : '';
  return `#${condition.id} ${condition.kind}${label}`;
}

function historyActionLabel(type) {
  const labelMap = {
    'break-hit': 'hit',
    'resume': 'resume',
    'step': 'step',
    'clear-history': 'clear',
    'reset-session': 'reset',
    'read-history': 'read',
    'read-session-summary': 'summary',
    'read-last-break-hit': 'last-hit',
    'snapshot-read': 'snapshot',
    'set-mock-snapshot': 'mock',
    'set-break-condition': 'set',
    'update-break-condition': 'update',
    'clear-break-condition': 'clear-one',
    'clear-all-break-conditions': 'clear-all',
    'list-break-conditions': 'list'
  };
  return labelMap[String(type || '')] || String(type || 'unknown');
}

function historySummary(entry) {
  const timestamp = String(entry && entry.timestamp ? entry.timestamp : nowIso());
  const type = String(entry && entry.type ? entry.type : 'unknown');
  const conditionId = Object.prototype.hasOwnProperty.call(entry || {}, 'conditionId')
    ? String(entry.conditionId)
    : '-';
  const conditionKind = String(entry && entry.conditionKind ? entry.conditionKind : '-');
  const tickCount = Object.prototype.hasOwnProperty.call(entry || {}, 'tickCount')
    ? String(entry.tickCount)
    : '-';
  const label = historyActionLabel(type);
  const parts = [`${timestamp}`, label];
  if (conditionId !== '-' || conditionKind !== '-') {
    parts.push(`condition=${conditionId}/${conditionKind}`);
  }
  if (tickCount !== '-') {
    parts.push(`tick=${tickCount}`);
  }
  return parts.join('  ');
}

function matchesCondition(condition, snapshot) {
  if (!condition || condition.enabled === false) {
    return false;
  }

  switch (condition.kind) {
    case 'frame_equals':
      return snapshotFrame(snapshot) === Number(condition.value);

    case 'frame_range': {
      const frame = snapshotFrame(snapshot);
      const value = condition.value && typeof condition.value === 'object' ? condition.value : {};
      const min = Number(value.min);
      const max = Number(value.max);
      const lowerOk = Number.isFinite(min) ? frame >= min : true;
      const upperOk = Number.isFinite(max) ? frame <= max : true;
      return lowerOk && upperOk;
    }

    case 'selection_contains': {
      const selection = snapshotSelection(snapshot);
      const targets = Array.isArray(condition.value)
        ? condition.value.map(String)
        : [String(condition.value)];
      return targets.some((target) =>
        selection.ids.includes(target) ||
        selection.names.includes(target)
      );
    }

    case 'health_is':
      return snapshotHealthState(snapshot).toLowerCase() === String(condition.value).toLowerCase();

    case 'trace_contains': {
      const haystack = snapshotTraceText(snapshot).toLowerCase();
      const needle = String(condition.value).toLowerCase();
      return needle.length > 0 && haystack.includes(needle);
    }

    case 'property_equals': {
      const value = condition.value && typeof condition.value === 'object' ? condition.value : {};
      const pathKey = String(value.path || condition.path || '');
      if (!pathKey) {
        return false;
      }
      const properties = normalizeArray(snapshot && snapshot.properties);
      return properties.some((entry) => {
        const entryPath = String(entry && entry.path ? entry.path : '');
        if (entryPath !== pathKey) {
          return false;
        }
        return JSON.stringify(entry && entry.value) === JSON.stringify(value.value);
      });
    }

    default:
      return false;
  }
}

function evaluateBreakConditions(state, snapshot) {
  const hits = [];
  for (const condition of state.breakConditions) {
    if (matchesCondition(condition, snapshot)) {
      hits.push(condition);
    }
  }

  if (hits.length === 0) {
    return hits;
  }

  const hit = hits[0];
  const signature = snapshotSignature(snapshot);
  if (state.lastBreakHit &&
      state.lastBreakHit.conditionId === hit.id &&
      state.lastBreakHit.signature === signature) {
    return [];
  }

  state.session.paused = true;
  state.session.lastAction = 'break-hit';
  state.session.pauseReason = 'breakpoint';
  state.session.wasPlayingBeforePause = snapshot && snapshot.playback
    ? String(snapshot.playback.state || '').toLowerCase() === 'playing'
    : false;
  state.session.pausedAtFrame = snapshotFrame(snapshot);
  state.lastBreakHit = {
    conditionId: hit.id,
    condition: hit,
    reason: `Matched ${conditionSummary(hit)}`,
    matchedAt: nowIso(),
    signature,
    snapshot
  };
  pushHistory(state, {
    type: 'break-hit',
    conditionId: hit.id,
    conditionKind: hit.kind
  });
  saveState(state);
  return hits;
}

function formatSnapshotForTool(snapshot, state) {
  return {
    source: readBridgeSnapshot() ? 'bridge-file' : 'mock',
    session: deepClone(state.session),
    sessionSummary: deepClone(state.sessionSummary),
    snapshot,
    breakConditions: state.breakConditions.map((condition) => ({
      id: condition.id,
      kind: condition.kind,
      label: condition.label,
      enabled: condition.enabled,
      value: condition.value,
      createdAt: condition.createdAt
    })),
    lastBreakHit: state.lastBreakHit
  };
}

function jsonText(value) {
  return JSON.stringify(value, null, 2);
}

function makeTool(name, description, inputSchema) {
  return {
    name,
    description,
    inputSchema
  };
}

function toolCatalog() {
  return [
    makeTool(
      'get_debug_snapshot',
      'Return the current app-level debug snapshot, merged with the optional bridge file.',
      {
        type: 'object',
        additionalProperties: false,
        properties: {}
      }
    ),
    makeTool(
      'set_break_condition',
      'Register one semantic condition that can trigger a pseudo breakpoint.',
      {
        type: 'object',
        additionalProperties: false,
        properties: {
          condition: {
            type: 'object',
            additionalProperties: true,
            properties: {
              kind: {
                type: 'string',
                enum: [
                  'frame_equals',
                  'frame_range',
                  'selection_contains',
                  'health_is',
                  'trace_contains',
                  'property_equals'
                ]
              },
              label: { type: 'string' },
              enabled: { type: 'boolean' },
              value: {}
            },
            required: ['kind', 'value']
          }
        },
        required: ['condition']
      }
    ),
    makeTool(
      'update_break_condition',
      'Patch an existing break condition by id.',
      {
        type: 'object',
        additionalProperties: false,
        properties: {
          id: {
            oneOf: [{ type: 'number' }, { type: 'string' }]
          },
          condition: {
            type: 'object',
            additionalProperties: true,
            properties: {
              kind: {
                type: 'string',
                enum: [
                  'frame_equals',
                  'frame_range',
                  'selection_contains',
                  'health_is',
                  'trace_contains',
                  'property_equals'
                ]
              },
              label: { type: 'string' },
              enabled: { type: 'boolean' },
              value: {}
            }
          }
        },
        required: ['id', 'condition']
      }
    ),
    makeTool(
      'list_break_conditions',
      'List all currently registered break conditions.',
      {
        type: 'object',
        additionalProperties: false,
        properties: {}
      }
    ),
    makeTool(
      'clear_break_condition',
      'Remove one break condition by id.',
      {
        type: 'object',
        additionalProperties: false,
        properties: {
          id: {
            oneOf: [{ type: 'number' }, { type: 'string' }]
          }
        },
        required: ['id']
      }
    ),
    makeTool(
      'clear_all_break_conditions',
      'Remove every registered break condition.',
      {
        type: 'object',
        additionalProperties: false,
        properties: {}
      }
    ),
    makeTool(
      'resume_debug_session',
      'Resume a paused debug session.',
      {
        type: 'object',
        additionalProperties: false,
        properties: {}
      }
    ),
    makeTool(
      'step_one_tick',
      'Advance one cooperative tick and re-evaluate the registered break conditions.',
      {
        type: 'object',
        additionalProperties: false,
        properties: {}
      }
    ),
    makeTool(
      'get_last_break_hit',
      'Return the last matched break condition and captured snapshot.',
      {
        type: 'object',
        additionalProperties: false,
        properties: {}
      }
    ),
    makeTool(
      'get_session_summary',
      'Return the current derived session summary.',
      {
        type: 'object',
        additionalProperties: false,
        properties: {}
      }
    ),
    makeTool(
      'get_break_history',
      'Return recent pseudo-breakpoint history entries.',
      {
        type: 'object',
        additionalProperties: false,
        properties: {
          limit: {
            type: 'number',
            minimum: 1,
            maximum: 100
          }
        }
      }
    ),
    makeTool(
      'clear_history',
      'Clear the stored pseudo-breakpoint history entries.',
      {
        type: 'object',
        additionalProperties: false,
        properties: {}
      }
    ),
    makeTool(
      'reset_debug_session',
      'Reset the paused state, history, and last hit; optionally clear all conditions.',
      {
        type: 'object',
        additionalProperties: false,
        properties: {
          clearBreakConditions: {
            type: 'boolean'
          }
        }
      }
    ),
    makeTool(
      'set_mock_snapshot',
      'Replace the fallback snapshot used when the bridge file is unavailable.',
      {
        type: 'object',
        additionalProperties: false,
        properties: {
          snapshot: {
            type: 'object',
            additionalProperties: true
          }
        },
        required: ['snapshot']
      }
    )
  ];
}

function contentResult(text, structuredContent) {
  const result = {
    content: [
      {
        type: 'text',
        text
      }
    ]
  };
  if (structuredContent !== undefined) {
    result.structuredContent = structuredContent;
  }
  return result;
}

function callTool(state, name, args) {
  const snapshot = effectiveSnapshot(state);
  switch (name) {
    case 'get_debug_snapshot':
      state.session.lastAction = 'get_debug_snapshot';
      pushHistory(state, { type: 'snapshot-read' });
      saveState(state);
      return contentResult(
        jsonText(formatSnapshotForTool(snapshot, state)),
        formatSnapshotForTool(snapshot, state)
      );

    case 'set_break_condition': {
      const conditionInput = args && typeof args.condition === 'object' ? args.condition : {};
      const condition = {
        id: state.nextConditionId++,
        kind: String(conditionInput.kind || 'frame_equals'),
        label: String(conditionInput.label || ''),
        enabled: conditionInput.enabled !== false,
        value: deepClone(conditionInput.value),
        createdAt: nowIso()
      };
      state.breakConditions.push(condition);
      state.session.lastAction = 'set_break_condition';
      pushHistory(state, {
        type: 'set-break-condition',
        conditionId: condition.id,
        conditionKind: condition.kind
      });
      saveState(state);
      return contentResult(
        jsonText({ ok: true, condition }),
        { ok: true, condition }
      );
    }

    case 'update_break_condition': {
      const id = Number(args && args.id);
      const conditionInput = args && typeof args.condition === 'object' ? args.condition : {};
      const index = state.breakConditions.findIndex((condition) => condition.id === id);
      if (index < 0) {
        state.session.lastAction = 'update_break_condition';
        pushHistory(state, { type: 'update-break-condition', conditionId: id, found: false });
        saveState(state);
        return contentResult(
          jsonText({ ok: false, id, reason: 'not-found' }),
          { ok: false, id, reason: 'not-found' }
        );
      }

      const current = state.breakConditions[index];
      const updated = {
        ...current,
        kind: conditionInput.kind ? String(conditionInput.kind) : current.kind,
        label: conditionInput.label !== undefined ? String(conditionInput.label) : current.label,
        enabled: conditionInput.enabled !== undefined ? Boolean(conditionInput.enabled) : current.enabled,
        value: conditionInput.value !== undefined ? deepClone(conditionInput.value) : current.value
      };
      state.breakConditions[index] = updated;
      state.session.lastAction = 'update_break_condition';
      pushHistory(state, {
        type: 'update-break-condition',
        conditionId: id,
        found: true,
        conditionKind: updated.kind
      });
      saveState(state);
      return contentResult(
        jsonText({ ok: true, condition: updated }),
        { ok: true, condition: updated }
      );
    }

    case 'list_break_conditions': {
      state.session.lastAction = 'list_break_conditions';
      pushHistory(state, { type: 'list-break-conditions' });
      saveState(state);
      const conditions = state.breakConditions.map((condition) => ({
        id: condition.id,
        kind: condition.kind,
        label: condition.label,
        enabled: condition.enabled,
        value: condition.value,
        createdAt: condition.createdAt
      }));
      const summary = conditions.map(conditionSummary);
      return contentResult(
        jsonText({ summary, conditions }),
        { summary, conditions }
      );
    }

    case 'clear_break_condition': {
      const id = Number(args && args.id);
      const before = state.breakConditions.length;
      state.breakConditions = state.breakConditions.filter((condition) => condition.id !== id);
      const removed = before !== state.breakConditions.length;
      state.session.lastAction = 'clear_break_condition';
      pushHistory(state, { type: 'clear-break-condition', conditionId: id, removed });
      saveState(state);
      return contentResult(
        jsonText({ ok: removed, id }),
        { ok: removed, id }
      );
    }

    case 'clear_all_break_conditions': {
      const cleared = state.breakConditions.length;
      state.breakConditions = [];
      state.session.lastAction = 'clear_all_break_conditions';
      pushHistory(state, { type: 'clear-all-break-conditions', cleared });
      saveState(state);
      return contentResult(
        jsonText({ ok: true, cleared }),
        { ok: true, cleared }
      );
    }

    case 'resume_debug_session':
      state.session.paused = false;
      state.session.lastAction = 'resume';
      state.session.pauseReason = null;
      state.session.pausedAtFrame = null;
      state.lastBreakHit = state.lastBreakHit
        ? {
            ...state.lastBreakHit,
            resumedAt: nowIso()
          }
        : null;
      pushHistory(state, { type: 'resume' });
      saveState(state);
      return contentResult(
        jsonText({ ok: true, paused: state.session.paused }),
        { ok: true, paused: state.session.paused }
      );

    case 'step_one_tick': {
      state.session.tickCount += 1;
      state.session.lastAction = 'step_one_tick';
      pushHistory(state, { type: 'step', tickCount: state.session.tickCount });
      const hits = evaluateBreakConditions(state, snapshot);
      saveState(state);
      return contentResult(
        jsonText({
          ok: true,
          tickCount: state.session.tickCount,
          paused: state.session.paused,
          breakHits: hits.map(conditionSummary)
        }),
        {
          ok: true,
          tickCount: state.session.tickCount,
          paused: state.session.paused,
          breakHits: hits.map((condition) => ({
            id: condition.id,
            kind: condition.kind,
            label: condition.label
          }))
        }
      );
    }

    case 'get_last_break_hit':
      state.session.lastAction = 'get_last_break_hit';
      pushHistory(state, { type: 'read-last-break-hit' });
      saveState(state);
      return contentResult(
        jsonText({ lastBreakHit: state.lastBreakHit }),
        { lastBreakHit: state.lastBreakHit }
      );

    case 'get_session_summary':
      state.session.lastAction = 'get_session_summary';
      pushHistory(state, { type: 'read-session-summary' });
      saveState(state);
      return contentResult(
        jsonText({ sessionSummary: state.sessionSummary }),
        { sessionSummary: state.sessionSummary }
      );

    case 'get_break_history': {
      state.session.lastAction = 'get_break_history';
      pushHistory(state, { type: 'read-history' });
      const requestedLimit = Number(args && args.limit);
      const limit = Number.isFinite(requestedLimit) ? Math.max(1, Math.min(100, Math.floor(requestedLimit))) : 20;
      const history = state.history.slice(Math.max(0, state.history.length - limit));
      const recentAction = state.sessionSummary && typeof state.sessionSummary.recentAction === 'string'
        ? state.sessionSummary.recentAction
        : history.length > 0
          ? historyActionLabel(history[history.length - 1].type)
          : '<none>';
      const mode = state.sessionSummary && typeof state.sessionSummary.mode === 'string'
        ? state.sessionSummary.mode
        : state.sessionSummary && state.sessionSummary.source === 'bridge'
          ? 'live'
          : 'mock';
      const bridgePath = state.sessionSummary && typeof state.sessionSummary.bridgePath === 'string'
        ? state.sessionSummary.bridgePath
        : '<none>';
      const summary = history.map(historySummary);
      saveState(state);
      return contentResult(
        jsonText({
          mode,
          bridgePath,
          recentAction,
          historyCount: history.length,
          totalHistoryCount: state.history.length,
          requestedLimit: limit,
          history,
          summary
        }),
        {
          mode,
          bridgePath,
          recentAction,
          historyCount: history.length,
          totalHistoryCount: state.history.length,
          requestedLimit: limit,
          history,
          summary
        }
      );
    }

    case 'clear_history': {
      const cleared = state.history.length;
      state.history = [];
      state.session.lastAction = 'clear_history';
      saveState(state);
      return contentResult(
        jsonText({ ok: true, cleared }),
        { ok: true, cleared }
      );
    }

    case 'reset_debug_session': {
      const clearBreakConditions = Boolean(args && args.clearBreakConditions);
      const clearedHistory = state.history.length;
      const clearedConditions = clearBreakConditions ? state.breakConditions.length : 0;
      resetSessionState(state, clearBreakConditions);
      state.session.lastAction = 'reset_debug_session';
      pushHistory(state, {
        type: 'reset-session',
        clearedHistory,
        clearedConditions
      });
      saveState(state);
      return contentResult(
        jsonText({
          ok: true,
          clearedHistory,
          clearedConditions,
          clearBreakConditions
        }),
        {
          ok: true,
          clearedHistory,
          clearedConditions,
          clearBreakConditions
        }
      );
    }

    case 'set_mock_snapshot': {
      const snapshotInput = args && typeof args.snapshot === 'object' ? args.snapshot : {};
      state.mockSnapshot = normalizeSnapshot(snapshotInput);
      state.session.lastAction = 'set_mock_snapshot';
      pushHistory(state, { type: 'set-mock-snapshot' });
      saveState(state);
      return contentResult(
        jsonText({ ok: true, mockSnapshot: state.mockSnapshot }),
        { ok: true, mockSnapshot: state.mockSnapshot }
      );
    }

    default:
      return {
        isError: true,
        content: [
          {
            type: 'text',
            text: `Unknown tool: ${name}`
          }
        ]
      };
  }
}

function handleRequest(state, request) {
  const id = Object.prototype.hasOwnProperty.call(request, 'id') ? request.id : undefined;
  const method = String(request.method || '').trim();
  const params = request.params && typeof request.params === 'object' ? request.params : {};

  const makeResponse = (result) => ({
    jsonrpc: '2.0',
    id,
    result
  });

  const makeError = (code, message, data) => ({
    jsonrpc: '2.0',
    id,
    error: data === undefined
      ? { code, message }
      : { code, message, data }
  });

  if (!method) {
    return makeError(-32600, 'Invalid request: missing method');
  }

  if (method === 'initialize') {
    const bridgeSnapshot = readBridgeSnapshot();
    const result = {
      protocolVersion: PROTOCOL_VERSION,
      serverInfo: {
        name: SERVER_NAME,
        version: '0.1.0'
      },
      capabilities: {
        tools: {
          listChanged: false
        }
      },
      bridge: {
        available: Boolean(bridgeSnapshot),
        path: BRIDGE_FILE
      }
    };
    return makeResponse(result);
  }

  if (method === 'notifications/initialized') {
    return null;
  }

  if (method === 'tools/list') {
    return makeResponse({
      tools: toolCatalog()
    });
  }

  if (method === 'tools/call') {
    const name = String(params.name || params.tool || '').trim();
    const args = params.arguments && typeof params.arguments === 'object' ? params.arguments : {};
    if (!name) {
      return makeError(-32602, 'tools/call is missing a tool name');
    }
    return makeResponse(callTool(state, name, args));
  }

  if (method === 'ping') {
    return makeResponse({ pong: true, timestamp: nowIso() });
  }

  return makeError(-32601, `Method not found: ${method}`);
}

function encodeFrame(message) {
  const body = Buffer.from(JSON.stringify(message), 'utf8');
  return Buffer.concat([
    Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'),
    body
  ]);
}

function findHeaderEnd(buffer) {
  return buffer.indexOf(Buffer.from('\r\n\r\n', 'ascii'));
}

function parseFrame(buffer) {
  const headerEnd = findHeaderEnd(buffer);
  if (headerEnd < 0) {
    return null;
  }

  const headerText = buffer.slice(0, headerEnd).toString('ascii');
  const contentLengthMatch = headerText.match(/content-length:\s*(\d+)/i);
  if (!contentLengthMatch) {
    return { error: new Error('Missing Content-Length header'), consume: headerEnd + 4 };
  }

  const contentLength = Number(contentLengthMatch[1]);
  const bodyStart = headerEnd + 4;
  const bodyEnd = bodyStart + contentLength;
  if (buffer.length < bodyEnd) {
    return null;
  }

  const body = buffer.slice(bodyStart, bodyEnd);
  try {
    const message = JSON.parse(body.toString('utf8'));
    return { message, consume: bodyEnd };
  } catch (error) {
    return { error, consume: bodyEnd };
  }
}

function main() {
  let state = loadState();
  let buffer = Buffer.alloc(0);

  process.stdin.on('data', (chunk) => {
    buffer = Buffer.concat([buffer, chunk]);
    while (true) {
      const parsed = parseFrame(buffer);
      if (!parsed) {
        break;
      }

      buffer = buffer.slice(parsed.consume);
      if (parsed.error) {
        const response = {
          jsonrpc: '2.0',
          error: {
            code: -32700,
            message: parsed.error.message || 'Parse error'
          }
        };
        process.stdout.write(encodeFrame(response));
        continue;
      }

      const response = handleRequest(state, parsed.message);
      if (response) {
        process.stdout.write(encodeFrame(response));
      }
    }
  });

  process.stdin.on('end', () => {
    try {
      saveState(state);
    } catch (error) {
      // Ignore shutdown write errors.
    }
  });

  process.stdin.resume();
}

main();
