'use strict';

const { spawn } = require('child_process');
const net = require('net');

class DapClient {
  constructor() {
    this.transport = null;
    this.process = null;
    this.buffer = Buffer.alloc(0);
    this.sequence = 1;
    this.pending = new Map();
    this.events = [];
    this.eventSequence = 0;
    this.eventWaiters = new Set();
    this.connected = false;
    this.initialized = false;
    this.adapterCapabilities = {};
  }

  async connect(options = {}) {
    await this.disconnect();
    if (options.host || options.port) {
      await this.connectTcp(options.host || '127.0.0.1', Number(options.port));
    } else {
      if (!options.adapter) {
        throw new Error('adapter is required for stdio DAP transport');
      }
      await this.connectStdio(options.adapter, Array.isArray(options.adapterArgs) ? options.adapterArgs : [], options.cwd);
    }
    const response = await this.request('initialize', {
      clientID: 'artifact-debug-mcp',
      clientName: 'ArtifactStudio Debug MCP',
      adapterID: options.adapterId || 'cppvsdbg',
      pathFormat: 'path',
      linesStartAt1: true,
      columnsStartAt1: true,
      supportsVariableType: true,
      supportsVariablePaging: true,
      supportsRunInTerminalRequest: false
    });
    this.adapterCapabilities = response && response.capabilities
      ? response.capabilities
      : {};
    this.initialized = true;
    return response;
  }

  connectTcp(host, port) {
    if (!Number.isInteger(port) || port <= 0) {
      return Promise.reject(new Error('a valid DAP TCP port is required'));
    }
    return new Promise((resolve, reject) => {
      const socket = net.createConnection({ host, port });
      socket.once('connect', () => {
        this.bindTransport(socket);
        resolve();
      });
      socket.once('error', reject);
    });
  }

  connectStdio(adapter, args, cwd) {
    return new Promise((resolve, reject) => {
      const child = spawn(adapter, args, {
        cwd: cwd || process.cwd(),
        stdio: ['pipe', 'pipe', 'pipe'],
        windowsHide: true,
        shell: false
      });
      child.once('error', reject);
      child.once('spawn', () => {
        this.process = child;
        this.bindTransport({
          write: (data) => child.stdin.write(data),
          destroy: () => child.kill(),
          on: (name, fn) => child.stdout.on(name, fn)
        });
        child.stderr.on('data', (data) => this.recordEvent({ event: 'adapterOutput', body: { category: 'stderr', output: data.toString('utf8') } }));
        child.on('exit', (code, signal) => this.onClosed(new Error(`DAP adapter exited (${code ?? signal ?? 'unknown'})`)));
        resolve();
      });
    });
  }

  bindTransport(transport) {
    this.transport = transport;
    this.connected = true;
    transport.on('data', (chunk) => this.onData(chunk));
    if (transport.on) {
      transport.on('close', () => this.onClosed(new Error('DAP transport closed')));
      transport.on('error', (error) => this.onClosed(error));
    }
  }

  onData(chunk) {
    this.buffer = Buffer.concat([this.buffer, chunk]);
    while (true) {
      const headerEnd = this.buffer.indexOf('\r\n\r\n');
      if (headerEnd < 0) return;
      const header = this.buffer.slice(0, headerEnd).toString('ascii');
      const match = header.match(/content-length:\s*(\d+)/i);
      if (!match) {
        this.buffer = this.buffer.slice(headerEnd + 4);
        continue;
      }
      const length = Number(match[1]);
      const end = headerEnd + 4 + length;
      if (this.buffer.length < end) return;
      const body = this.buffer.slice(headerEnd + 4, end).toString('utf8');
      this.buffer = this.buffer.slice(end);
      try {
        this.onMessage(JSON.parse(body));
      } catch (error) {
        this.recordEvent({ event: 'protocolError', body: { message: error.message } });
      }
    }
  }

  onMessage(message) {
    if (message.type === 'response') {
      const pending = this.pending.get(message.request_seq);
      if (!pending) return;
      this.pending.delete(message.request_seq);
      clearTimeout(pending.timer);
      if (message.success === false) {
        pending.reject(new Error(message.message || `${message.command} failed`));
      } else {
        pending.resolve(message.body || {});
      }
      return;
    }
    if (message.type === 'event') {
      this.recordEvent(message);
      return;
    }
    if (message.type === 'request') {
      this.send({
        type: 'response',
        request_seq: message.seq,
        command: message.command,
        success: false,
        message: `Client request ${message.command} is not supported`
      });
    }
  }

  recordEvent(message) {
    let body = message.body || {};
    if (message.event === 'adapterOutput' && typeof body.output === 'string' && body.output.length > 4096) {
      body = { ...body, output: body.output.slice(-4096) };
    }
    this.events.push({ sequence: ++this.eventSequence, event: message.event, body, receivedAt: new Date().toISOString() });
    if (this.events.length > 200) this.events.splice(0, this.events.length - 200);
    const latest = this.events[this.events.length - 1];
    for (const waiter of this.eventWaiters) {
      if (latest.sequence > waiter.afterSequence && latest.event === waiter.eventName) {
        clearTimeout(waiter.timer);
        this.eventWaiters.delete(waiter);
        waiter.resolve();
      }
    }
  }

  waitForEvent(eventName, timeoutMs = 15000, afterSequence = 0) {
    if (this.events.some((item) => item.sequence > afterSequence && item.event === eventName)) return Promise.resolve();
    return new Promise((resolve, reject) => {
      if (!this.connected) {
        reject(new Error(`DAP disconnected while waiting for ${eventName}`));
        return;
      }
      const waiter = { eventName, afterSequence, resolve, reject, timer: null };
      waiter.timer = setTimeout(() => {
        this.eventWaiters.delete(waiter);
        reject(new Error(`DAP event ${eventName} timed out after ${timeoutMs}ms`));
      }, timeoutMs);
      this.eventWaiters.add(waiter);
      if (this.events.some((item) => item.sequence > afterSequence && item.event === eventName)) {
        clearTimeout(waiter.timer);
        this.eventWaiters.delete(waiter);
        resolve();
      };
    });
  }

  send(message) {
    if (!this.transport || !this.connected) throw new Error('DAP is not connected');
    const envelope = { seq: this.sequence++, ...message };
    const body = Buffer.from(JSON.stringify(envelope), 'utf8');
    this.transport.write(Buffer.concat([Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'), body]));
    return envelope.seq;
  }

  request(command, args = {}, timeoutMs = 15000) {
    return new Promise((resolve, reject) => {
      let seq;
      try {
        seq = this.send({ type: 'request', command, arguments: args });
      } catch (error) {
        reject(error);
        return;
      }
      const timer = setTimeout(() => {
        this.pending.delete(seq);
        reject(new Error(`DAP ${command} timed out after ${timeoutMs}ms`));
      }, timeoutMs);
      this.pending.set(seq, { resolve, reject, timer });
    });
  }

  async start(kind, configuration) {
    if (!this.initialized) throw new Error('call dap_connect first');
    const eventSequence = this.eventSequence;
    const responsePromise = this.request(kind, configuration || {}, 30000);
    // Ignore initialized events retained from an earlier launch/attach.
    const responseOutcome = responsePromise.then(
      (response) => ({ type: 'response', response }),
      (error) => ({ type: 'error', error })
    );
    const eventOutcome = this.waitForEvent('initialized', 15000, eventSequence).then(
      () => ({ type: 'event' }),
      (error) => ({ type: 'error', error })
    );
    let outcome = await Promise.race([responseOutcome, eventOutcome]);
    if (outcome.type === 'error') throw outcome.error;
    if (outcome.type === 'response') {
      outcome = await eventOutcome;
      if (outcome.type === 'error') throw outcome.error;
    }
    if (this.adapterCapabilities.supportsConfigurationDoneRequest) {
      await this.request('configurationDone', {});
    }
    return responsePromise;
  }

  status() {
    return {
      connected: this.connected,
      initialized: this.initialized,
      adapterPid: this.process ? this.process.pid : null,
      recentEvents: this.events.slice(-20)
    };
  }

  async disconnect() {
    if (this.connected) {
      try { await this.request('disconnect', { terminateDebuggee: false }, 2000); } catch (_) {}
    }
    if (this.transport && this.transport.destroy) this.transport.destroy();
    if (this.process && !this.process.killed) this.process.kill();
    this.onClosed(new Error('DAP disconnected'));
  }

  onClosed(error) {
    this.connected = false;
    this.initialized = false;
    this.adapterCapabilities = {};
    this.transport = null;
    this.process = null;
    for (const waiter of this.eventWaiters) {
      clearTimeout(waiter.timer);
      waiter.reject(error);
    }
    this.eventWaiters.clear();
    for (const pending of this.pending.values()) {
      clearTimeout(pending.timer);
      pending.reject(error);
    }
    this.pending.clear();
  }
}

module.exports = { DapClient };
