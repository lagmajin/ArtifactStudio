'use strict';

const { spawn } = require('child_process');

function runOperation(program, args = [], options = {}) {
  return new Promise((resolve, reject) => {
    if (!program) {
      reject(new Error('program is required'));
      return;
    }
    const output = [];
    let outputBytes = 0;
    let outputTruncated = false;
    let timedOut = false;
    const maxOutputBytes = 64 * 1024;
    const appendOutput = (stream, data) => {
      const text = data.toString('utf8');
      output.push({ stream, text });
      outputBytes += Buffer.byteLength(text, 'utf8');
      while (outputBytes > maxOutputBytes && output.length > 0) {
        const excess = outputBytes - maxOutputBytes;
        const first = output[0];
        const firstBytes = Buffer.byteLength(first.text, 'utf8');
        outputTruncated = true;
        if (firstBytes <= excess) {
          output.shift();
          outputBytes -= firstBytes;
        } else {
          first.text = Buffer.from(first.text, 'utf8').subarray(excess).toString('utf8');
          outputBytes -= excess;
        }
      }
    };
    const child = spawn(program, args, {
      cwd: options.cwd || process.cwd(),
      env: { ...process.env, ...(options.env || {}) },
      shell: false,
      windowsHide: true,
      stdio: ['ignore', 'pipe', 'pipe']
    });
    const timeoutMs = Math.max(100, Math.min(300000, Number(options.timeoutMs) || 30000));
    const timer = setTimeout(() => {
      timedOut = true;
      child.kill();
    }, timeoutMs);
    child.stdout.on('data', (data) => appendOutput('stdout', data));
    child.stderr.on('data', (data) => appendOutput('stderr', data));
    child.once('error', (error) => {
      clearTimeout(timer);
      reject(error);
    });
    child.once('close', (code, signal) => {
      clearTimeout(timer);
      resolve({
        ok: code === 0,
        exitCode: code,
        signal,
        timedOut,
        output,
        outputTruncated
      });
    });
  });
}

function readPath(root, dottedPath) {
  return String(dottedPath || '').split('.').filter(Boolean).reduce(
    (value, key) => value !== null && value !== undefined ? value[key] : undefined,
    root
  );
}

function checkAssertions(snapshot, assertions = []) {
  return assertions.map((assertion) => {
    const actual = readPath(snapshot, assertion.path);
    let passed = false;
    const operator = assertion.operator || 'equals';
    switch (operator) {
      case 'exists': passed = actual !== undefined && actual !== null; break;
      case 'notEquals': passed = actual !== assertion.value; break;
      case 'contains': passed = Array.isArray(actual) ? actual.includes(assertion.value) : String(actual ?? '').includes(String(assertion.value)); break;
      case 'greaterThan': passed = Number(actual) > Number(assertion.value); break;
      case 'lessThan': passed = Number(actual) < Number(assertion.value); break;
      case 'equals': passed = JSON.stringify(actual) === JSON.stringify(assertion.value); break;
      default: passed = false; break;
    }
    return { path: assertion.path, operator, expected: assertion.value, actual, passed };
  });
}

module.exports = { runOperation, checkAssertions };
