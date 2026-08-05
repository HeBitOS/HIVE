#!/usr/bin/env node
import { spawn } from 'node:child_process';
import { mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const contentShell = process.env.HIVE_CONTENT_SHELL;
if (!contentShell) {
  console.error('set HIVE_CONTENT_SHELL to the built content_shell executable');
  process.exit(2);
}

const videoMode = process.argv[2] === '--video';
const targetUrl = videoMode
  ? (process.argv[3] || process.env.HIVE_BILIBILI_VIDEO_URL)
  : 'https://www.bilibili.com/';
if (!targetUrl || !/^https:\/\/([^.]+\.)*bilibili\.com\//.test(targetUrl)) {
  console.error('video mode requires an https://*.bilibili.com/ URL');
  process.exit(2);
}

const delay = milliseconds => new Promise(resolve => setTimeout(resolve, milliseconds));
const port = 19000 + Math.floor(Math.random() * 1000);
const profile = mkdtempSync(join(tmpdir(), 'hive-content-shell-'));
const diagnostics = [];
const child = spawn(contentShell, [
  '--remote-debugging-address=127.0.0.1',
  `--remote-debugging-port=${port}`,
  '--ozone-platform=headless',
  '--disable-gpu',
  '--no-sandbox',
  `--user-data-dir=${profile}`,
  'about:blank'
], { stdio: ['ignore', 'ignore', 'pipe'] });
child.stderr.setEncoding('utf8');
child.stderr.on('data', chunk => {
  diagnostics.push(chunk);
  if (diagnostics.length > 30) diagnostics.shift();
});

let socket;
const pending = new Map();
let nextId = 1;
const events = [];
const eventWaiters = [];

function failPending(error) {
  for (const { reject, timer } of pending.values()) {
    clearTimeout(timer);
    reject(error);
  }
  pending.clear();
}

function command(method, params = {}, timeoutMs = 30000) {
  const id = nextId++;
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      pending.delete(id);
      reject(new Error(`CDP timeout: ${method}`));
    }, timeoutMs);
    pending.set(id, { resolve, reject, timer });
    socket.send(JSON.stringify({ id, method, params }));
  });
}

function waitEvent(method, timeoutMs = 45000) {
  const queued = events.findIndex(event => event.method === method);
  if (queued >= 0) return Promise.resolve(events.splice(queued, 1)[0].params);
  return new Promise((resolve, reject) => {
    const waiter = { method, resolve, reject };
    waiter.timer = setTimeout(() => {
      const index = eventWaiters.indexOf(waiter);
      if (index >= 0) eventWaiters.splice(index, 1);
      reject(new Error(`CDP event timeout: ${method}`));
    }, timeoutMs);
    eventWaiters.push(waiter);
  });
}

async function discoverTarget() {
  const deadline = Date.now() + 25000;
  while (Date.now() < deadline) {
    try {
      const response = await fetch(`http://127.0.0.1:${port}/json/list`);
      const targets = await response.json();
      const page = targets.find(target => target.type === 'page');
      if (page?.webSocketDebuggerUrl) return page.webSocketDebuggerUrl;
    } catch {
      // Content shell is still starting.
    }
    await delay(200);
  }
  throw new Error('content_shell DevTools endpoint did not start');
}

const pageProbe = `(() => {
  const displays = Array.from(document.querySelectorAll('*')).slice(0, 500)
    .map(element => getComputedStyle(element).display);
  const video = document.querySelector('video');
  return {
    url: location.href,
    hostname: location.hostname,
    title: document.title,
    readyState: document.readyState,
    bodyTextLength: document.body ? document.body.innerText.length : 0,
    links: document.links.length,
    images: document.images.length,
    scripts: document.scripts.length,
    moduleScripts: document.querySelectorAll('script[type="module"]').length,
    piniaState: Boolean(globalThis.__pinia),
    fetchApi: typeof fetch === 'function',
    observers: typeof MutationObserver === 'function' &&
      typeof IntersectionObserver === 'function' && typeof ResizeObserver === 'function',
    flexOrGrid: displays.filter(value => value === 'flex' || value === 'grid').length,
    mediaSource: typeof MediaSource === 'function',
    worker: typeof Worker === 'function',
    videoCount: document.querySelectorAll('video').length,
    videoReadyState: video ? video.readyState : -1,
    videoCurrentTime: video ? video.currentTime : -1
  };
})()`;

let exitCode = 1;
try {
  const debuggerUrl = await discoverTarget();
  socket = new WebSocket(debuggerUrl);
  await new Promise((resolve, reject) => {
    socket.addEventListener('open', resolve, { once: true });
    socket.addEventListener('error', () => reject(new Error('CDP WebSocket failed')), { once: true });
  });
  socket.addEventListener('message', message => {
    const packet = JSON.parse(message.data);
    if (packet.id) {
      const item = pending.get(packet.id);
      if (!item) return;
      pending.delete(packet.id);
      clearTimeout(item.timer);
      if (packet.error) item.reject(new Error(packet.error.message));
      else item.resolve(packet.result);
      return;
    }
    if (!packet.method) return;
    const waiterIndex = eventWaiters.findIndex(waiter => waiter.method === packet.method);
    if (waiterIndex >= 0) {
      const waiter = eventWaiters.splice(waiterIndex, 1)[0];
      clearTimeout(waiter.timer);
      waiter.resolve(packet.params);
    } else {
      events.push(packet);
      if (events.length > 100) events.shift();
    }
  });
  socket.addEventListener('close', () => failPending(new Error('CDP WebSocket closed')));

  await command('Page.enable');
  await command('Runtime.enable');
  const loaded = waitEvent('Page.loadEventFired');
  await command('Page.navigate', { url: targetUrl });
  await loaded;
  await delay(videoMode ? 15000 : 8000);
  await command('Runtime.evaluate', {
    expression: 'scrollTo(0, Math.min(document.documentElement.scrollHeight, 1200))',
    returnByValue: true
  });
  await delay(1500);
  const evaluation = await command('Runtime.evaluate', {
    expression: pageProbe,
    returnByValue: true
  });
  const result = evaluation.result.value;
  console.log(JSON.stringify(result, null, 2));

  const commonPass = result.hostname.endsWith('bilibili.com') &&
    result.readyState === 'complete' && result.bodyTextLength > 500 &&
    result.links >= 5 && result.scripts >= 1 && result.fetchApi &&
    result.observers && result.mediaSource && result.worker;
  const pass = videoMode
    ? commonPass && result.videoCount >= 1
    : commonPass && result.images >= 3 && result.flexOrGrid >= 1;
  const marker = videoMode
    ? 'HIVE_BILIBILI_VIDEO_SMOKE'
    : 'HIVE_BILIBILI_HOME_SMOKE';
  console.log(`${marker}: ${pass ? 'PASS' : 'FAIL'}`);
  exitCode = pass ? 0 : 1;
} catch (error) {
  console.error(error.stack || error);
  console.error(diagnostics.join('').slice(-6000));
} finally {
  if (socket?.readyState === WebSocket.OPEN) socket.close();
  child.kill('SIGTERM');
  await Promise.race([
    new Promise(resolve => child.once('exit', resolve)),
    delay(2000)
  ]);
  if (child.exitCode === null && child.signalCode === null) child.kill('SIGKILL');
  rmSync(profile, { recursive: true, force: true });
}
process.exit(exitCode);
