'use strict';

// Protocol and state-isolation tests for the standalone debug MCP server.
// Run with `npm test`. Each test spawns a real stdio server with scratch files;
// no bridge file is present, so handlers use the mock snapshot path.

const assert = require('node:assert/strict');
const { spawn } = require('node:child_process');
const { once } = require('node:events');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const { test } = require('node:test');

const SERVER_PATH = path.join(__dirname, 'server.js');
const RESPONSE_TIMEOUT_MS = 5000;

function createScratch() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'artifact-debug-mcp-test-'));
  return {
    dir,
    stateFile: path.join(dir, 'debug-mcp-state.json'),
    bridgeFile: path.join(dir, 'debug-bridge.json')
  };
}

function frame(message) {
  const body = Buffer.from(JSON.stringify(message), 'utf8');
  return Buffer.concat([
    Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'),
    body
  ]);
}

function attachReader(child) {
  const reader = { buffer: Buffer.alloc(0), waiters: [], unsolicited: [] };
  child.stdout.on('data', (chunk) => {
    reader.buffer = Buffer.concat([reader.buffer, chunk]);
    while (true) {
      const headerEnd = reader.buffer.indexOf('\r\n\r\n', 0, 'ascii');
      if (headerEnd < 0) return;
      const header = reader.buffer.subarray(0, headerEnd).toString('ascii');
      const match = /content-length:\s*(\d+)/i.exec(header);
      if (!match) {
        throw new Error(`Malformed response header: ${header}`);
      }
      const length = Number(match[1]);
      const bodyStart = headerEnd + 4;
      if (reader.buffer.length < bodyStart + length) return;
      const body = reader.buffer.subarray(bodyStart, bodyStart + length).toString('utf8');
      reader.buffer = reader.buffer.subarray(bodyStart + length);
      const message = JSON.parse(body);
      const waiter = reader.waiters.shift();
      if (waiter) waiter.resolve(message);
      else reader.unsolicited.push(message);
    }
  });
  child.stderr.on('data', () => {
    // stderr is intentionally drained so a chatty child cannot block.
  });
  return reader;
}

async function nextResponse(reader) {
  if (reader.unsolicited.length > 0) {
    return reader.unsolicited.shift();
  }
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      const index = reader.waiters.indexOf(waiter);
      if (index >= 0) reader.waiters.splice(index, 1);
      reject(new Error('Timed out waiting for an MCP response frame'));
    }, RESPONSE_TIMEOUT_MS);
    const waiter = {
      resolve: (message) => {
        clearTimeout(timer);
        resolve(message);
      }
    };
    reader.waiters.push(waiter);
  });
}

async function withServer(seed, run) {
  const scratch = createScratch();
  if (seed) seed(scratch);
  const child = spawn(process.execPath, [SERVER_PATH], {
    env: {
      ...process.env,
      ARTIFACT_DEBUG_MCP_STATE_FILE: scratch.stateFile,
      ARTIFACT_DEBUG_BRIDGE_FILE: scratch.bridgeFile
    },
    stdio: ['pipe', 'pipe', 'pipe']
  });
  const reader = attachReader(child);

  const send = (message) => {
    child.stdin.write(frame(message));
    return nextResponse(reader);
  };
  const notify = (message) => {
    child.stdin.write(frame(message));
  };
  const writeRaw = (bytes) => {
    child.stdin.write(bytes);
  };
  const receive = () => nextResponse(reader);

  try {
    await once(child, 'spawn');
    await run({ child, reader, scratch, send, notify, writeRaw, receive });
  } finally {
    child.kill();
    fs.rmSync(scratch.dir, { recursive: true, force: true });
  }
}

function callTool(id, name, args = {}) {
  return { jsonrpc: '2.0', id, method: 'tools/call', params: { name, arguments: args } };
}

test('initialize advertises MCP 2024-11-05 with listChanged and JSON Schema tools', async () => {
  await withServer(null, async ({ send }) => {
    const init = await send({ jsonrpc: '2.0', id: 1, method: 'initialize', params: {} });
    assert.equal(init.result.protocolVersion, '2024-11-05');
    assert.equal(init.result.capabilities.tools.listChanged, false);

    const listed = await send({ jsonrpc: '2.0', id: 2, method: 'tools/list', params: {} });
    assert.ok(Array.isArray(listed.result.tools));
    assert.ok(listed.result.tools.length >= 37);
    const names = listed.result.tools.map((tool) => tool.name);
    assert.equal(new Set(names).size, names.length);
    assert.ok(names.includes('set_diagnostic_breakpoint'));
    for (const tool of listed.result.tools) {
      assert.equal(typeof tool.name, 'string');
      assert.ok(tool.inputSchema && typeof tool.inputSchema === 'object');
      assert.equal(tool.inputSchema.type, 'object');
    }
  });
});

test('notifications never produce a response frame', async () => {
  await withServer(null, async ({ send, notify, reader }) => {
    await send({ jsonrpc: '2.0', id: 1, method: 'initialize', params: {} });
    notify({ jsonrpc: '2.0', method: 'notifications/initialized', params: {} });
    notify({ jsonrpc: '2.0', method: 'notifications/cancelled', params: {} });

    const ping = await send({ jsonrpc: '2.0', id: 2, method: 'ping', params: {} });
    assert.equal(ping.result.pong, true);
    assert.equal(reader.unsolicited.length, 0);
  });
});

test('requests without a JSON-RPC version are rejected', async () => {
  await withServer(null, async ({ send }) => {
    const bad = await send({ id: 1, method: 'ping', params: {} });
    assert.equal(bad.error.code, -32600);
    assert.equal(bad.id, 1);
  });
});

test('oversized frames are rejected instead of buffered without bound', async () => {
  await withServer(null, async ({ writeRaw, send, receive }) => {
    writeRaw(Buffer.from('Content-Length: 20000000\r\n\r\n', 'ascii'));
    const rejected = await receive();
    assert.equal(rejected.error.code, -32700);
    const aliveAfterHeader = await send({ jsonrpc: '2.0', id: 99, method: 'ping', params: {} });
    assert.equal(aliveAfterHeader.result.pong, true);

    writeRaw(Buffer.alloc(17 * 1024 * 1024, 0x78));
    const rejectedAgain = await receive();
    assert.equal(rejectedAgain.error.code, -32700);
    const aliveAfterBuffer = await send({ jsonrpc: '2.0', id: 100, method: 'ping', params: {} });
    assert.equal(aliveAfterBuffer.result.pong, true);
  });
});

test('read tools leave the shared state file byte-for-byte unchanged', async () => {
  const seed = (scratch) => {
    fs.writeFileSync(scratch.stateFile, JSON.stringify({
      version: 1,
      session: { paused: false, lastAction: 'seeded' },
      breakConditions: [],
      watchDescriptors: [],
      history: [{ timestamp: '2026-01-01T00:00:00.000Z', type: 'break-hit' }]
    }, null, 2));
  };

  await withServer(seed, async ({ send, scratch }) => {
    const before = fs.readFileSync(scratch.stateFile, 'utf8');
    for (let i = 0; i < 5; i += 1) {
      const snapshot = await send(callTool(10 + i, 'get_debug_snapshot'));
      assert.equal(snapshot.result.isError, undefined);
      await send(callTool(20 + i, 'get_break_history'));
      await send(callTool(30 + i, 'get_session_summary'));
    }
    const after = fs.readFileSync(scratch.stateFile, 'utf8');
    assert.equal(after, before);
    const state = JSON.parse(after);
    assert.equal(state.history.length, 1);
    assert.equal(state.session.lastAction, 'seeded');
  });
});

test('break history no longer reports a synthetic read event', async () => {
  const seed = (scratch) => {
    fs.writeFileSync(scratch.stateFile, JSON.stringify({
      version: 1,
      session: { paused: false, lastAction: 'break-hit' },
      history: [{ timestamp: '2026-01-01T00:00:00.000Z', type: 'break-hit' }]
    }, null, 2));
  };

  await withServer(seed, async ({ send }) => {
    const response = await send(callTool(1, 'get_break_history'));
    const result = response.result.structuredContent;
    assert.equal(result.includesReadEvent, false);
    assert.equal(result.history.length, 1);
    assert.equal(result.history[0].type, 'break-hit');
    assert.equal(result.recentAction, 'hit');
  });
});
