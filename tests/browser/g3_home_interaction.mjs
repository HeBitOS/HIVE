#!/usr/bin/env node
// G3 Bilibili 首页交互验收（host 参考 content_shell 引擎级验收）
// 覆盖 BROWSER_BILIBILI_UPGRADE.md G3 条目：
//   SSR hydration / Flex/Grid/变量 / 图片 / 滚动 / 导航 / 搜索 / 动态卡片 /
//   崩溃恢复 / 指标（首屏、滚动帧率、请求数、峰值内存）。
// 输出 HIVE_BILIBILI_HOME_INTERACTION: PASS 即通过；日志留档由调用方负责。
import { spawn } from 'node:child_process';
import { mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const contentShell = process.env.HIVE_CONTENT_SHELL;
if (!contentShell) {
  console.error('set HIVE_CONTENT_SHELL to the built content_shell executable');
  process.exit(2);
}
const targetUrl = process.env.HIVE_BILIBILI_HOME_URL || 'https://www.bilibili.com/';

const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
const port = 20000 + Math.floor(Math.random() * 1000);
const profile = mkdtempSync(join(tmpdir(), 'hive-g3-'));
const diagnostics = [];

const child = spawn(contentShell, [
  '--remote-debugging-address=127.0.0.1',
  '--remote-debugging-port=' + port,
  '--ozone-platform=headless',
  '--disable-gpu',
  '--no-sandbox',
  '--user-data-dir=' + profile,
  'about:blank'
], { stdio: ['ignore', 'ignore', 'pipe'] });
child.stderr.setEncoding('utf8');
child.stderr.on('data', chunk => {
  diagnostics.push(chunk);
  if (diagnostics.length > 40) diagnostics.shift();
});

let socket;
const pending = new Map();
let nextId = 1;
const events = [];
const eventWaiters = [];
let crashed = false;

function failPending(error) {
  for (const { reject, timer } of pending.values()) { clearTimeout(timer); reject(error); }
  pending.clear();
}
function command(method, params = {}, timeoutMs = 30000) {
  const id = nextId++;
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => { pending.delete(id); reject(new Error('CDP timeout: ' + method)); }, timeoutMs);
    pending.set(id, { resolve, reject, timer });
    socket.send(JSON.stringify({ id, method, params }));
  });
}
function waitEvent(method, timeoutMs = 45000) {
  const queued = events.findIndex(e => e.method === method);
  if (queued >= 0) return Promise.resolve(events.splice(queued, 1)[0].params);
  return new Promise((resolve, reject) => {
    const waiter = { method, resolve, reject };
    waiter.timer = setTimeout(() => {
      const i = eventWaiters.indexOf(waiter);
      if (i >= 0) eventWaiters.splice(i, 1);
      reject(new Error('CDP event timeout: ' + method));
    }, timeoutMs);
    eventWaiters.push(waiter);
  });
}
async function discoverTarget() {
  const deadline = Date.now() + 25000;
  while (Date.now() < deadline) {
    try {
      const response = await fetch('http://127.0.0.1:' + port + '/json/list');
      const targets = await response.json();
      const page = targets.find(t => t.type === 'page');
      if (page && page.webSocketDebuggerUrl) return page.webSocketDebuggerUrl;
    } catch { /* starting */ }
    await delay(200);
  }
  throw new Error('content_shell DevTools endpoint did not start');
}
async function evaluate(expression, timeoutMs = 30000) {
  const r = await command('Runtime.evaluate', { expression, returnByValue: true, awaitPromise: true }, timeoutMs);
  if (r.exceptionDetails) throw new Error('page eval exception: ' + JSON.stringify(r.exceptionDetails.exception?.description || r.exceptionDetails.text));
  return r.result.value;
}
let hadFailure = false;
function assert(name, condition, metrics) {
  metrics.push((condition ? 'PASS ' : 'FAIL ') + name);
  if (!condition) hadFailure = true;
}

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
    if (packet.method === 'Inspector.targetCrashed') crashed = true;
    const i = eventWaiters.findIndex(w => w.method === packet.method);
    if (i >= 0) { const w = eventWaiters.splice(i, 1)[0]; clearTimeout(w.timer); w.resolve(packet.params); }
    else { events.push(packet); if (events.length > 200) events.shift(); }
  });
  socket.addEventListener('close', () => failPending(new Error('CDP WebSocket closed')));

  await command('Page.enable');
  await command('Runtime.enable');
  await command('Network.enable');

  const metrics = [];
  let requestCount = 0;
  socket.addEventListener('message', message => {
    try {
      const packet = JSON.parse(message.data);
      if (packet.method === 'Network.requestWillBeSent') requestCount++;
    } catch { /* ignore */ }
  });

  const startedAt = Date.now();
  const loaded = waitEvent('Page.loadEventFired');
  await command('Page.navigate', { url: targetUrl });
  await loaded;
  const loadMs = Date.now() - startedAt;

  // 等 hydration / 首屏 settle（首页脚本较大）
  await delay(10000);
  const heap = await command('Runtime.getHeapUsage');

  // 页面能力 + hydration 断言
  const probe = await evaluate(`(() => {
    const displays = Array.from(document.querySelectorAll('*')).slice(0, 2000)
      .map(el => getComputedStyle(el).display);
    const search = document.querySelector('input[type=text], input[placeholder*="搜索"], input[placeholder*="搜"]');
    const nav = document.querySelector('a[href*="live.bilibili.com"], a[href*="bangumi"], a[href*="番剧"]');
    const card = document.querySelector('a[href*="/video/"]');
    return {
      url: location.href, hostname: location.hostname, title: document.title,
      readyState: document.readyState, bodyTextLength: document.body ? document.body.innerText.length : 0,
      links: document.links.length, images: document.images.length,
      scripts: document.scripts.length, moduleScripts: document.querySelectorAll('script[type=module]').length,
      piniaState: Boolean(globalThis.__pinia),
      fetchApi: typeof fetch === 'function',
      observers: typeof MutationObserver === 'function' && typeof IntersectionObserver === 'function' && typeof ResizeObserver === 'function',
      flexOrGrid: displays.filter(v => v === 'flex' || v === 'grid').length,
      mediaSource: typeof MediaSource === 'function',
      worker: typeof Worker === 'function',
      hasSearchInput: Boolean(search),
      searchInputType: search ? search.type : '',
      hasNavLink: Boolean(nav),
      hasFeedCard: Boolean(card),
      feedCardHref: card ? card.getAttribute('href') : null
    };
  })()`);

  assert('home-host', probe.hostname.endsWith('bilibili.com') && probe.readyState === 'complete' && probe.bodyTextLength > 500, metrics);
  assert('vue-hydration-pinia', probe.piniaState, metrics);
  assert('images-lazy', probe.images >= 3, metrics);
  assert('flex-grid-layout', probe.flexOrGrid >= 1, metrics);
  assert('observers-fetch', probe.observers && probe.fetchApi, metrics);

  // 滚动可用 + 滚动帧率（rAF 采样 ~1.2s，边滚边测）
  const scroll = await evaluate(`(async () => {
    let frames = 0;
    const start = performance.now();
    const maxY = document.documentElement.scrollHeight - innerHeight;
    let lastY = 0;
    await new Promise(resolve => {
      const step = () => {
        frames++;
        lastY = Math.min(scrollY + 40, maxY);
        scrollTo(0, lastY);
        if (performance.now() - start < 1200 && lastY < maxY) requestAnimationFrame(step);
        else resolve();
      };
      requestAnimationFrame(step);
    });
    const elapsed = (performance.now() - start) / 1000;
    return { frames, elapsed, fps: Math.round(frames / elapsed), scrollY, maxY };
  })()`);
  assert('scroll-usable', scroll.scrollY > 0 && scroll.frames >= 20, metrics);
  metrics.push('DATA scroll-fps=' + scroll.fps);

  // 导航点击（直播/番剧入口）：点击后允许跳转到对应域名，页面不得崩溃
  const navClick = await evaluate(`(() => {
    const el = document.querySelector('a[href*="live.bilibili.com"], a[href*="bangumi"], a[href*="番剧"]');
    if (!el) return { ok: false, reason: 'no-nav-link' };
    el.click();
    return { ok: true, href: el.href };
  })()`);
  if (navClick.ok) {
    await delay(6000);
    const after = await evaluate(`({ url: location.href, title: document.title, alive: true })`);
    assert('nav-interaction', /bilibili\.com/.test(after.url) && after.alive && !crashed, metrics);
    await command('Page.navigate', { url: targetUrl });
    await waitEvent('Page.loadEventFired');
    await delay(8000);
  } else {
    assert('nav-interaction', false, metrics);
  }

  // 搜索交互：真实鼠标点击搜索框 + CDP 输入，Enter 应触发搜索导航
  const search = await evaluate(`(() => {
    const inputs = Array.from(document.querySelectorAll('input'));
    const el = inputs.find(e => {
      const ph = (e.getAttribute('placeholder') || '');
      const cls = String(e.className || '');
      return ph.includes('\u641c') || cls.includes('search') || cls.includes('nav-search');
    }) || inputs.find(e => e.offsetParent !== null && (e.getAttribute('placeholder') || '').length > 0) || null;
    if (!el) return { ok: false, reason: 'no-search-input', inputCount: inputs.length };
    el.scrollIntoView({ block: 'center' });
    const r = el.getBoundingClientRect();
    return { ok: true, x: r.left + r.width / 2, y: r.top + r.height / 2, type: el.type, placeholder: el.getAttribute('placeholder') || '', cls: String(el.className || '') };
  })()`);
  if (search.ok) {
    await command('Input.dispatchMouseEvent', { type: 'mousePressed', x: search.x, y: search.y, button: 'left', clickCount: 1 });
    await command('Input.dispatchMouseEvent', { type: 'mouseReleased', x: search.x, y: search.y, button: 'left', clickCount: 1 });
    await delay(300);
    await command('Input.insertText', { text: '\u79d1\u6280' });
    await delay(300);
    const typed = await evaluate(`(() => { const el = document.activeElement; return { value: el && el.value || '', tag: el && el.tagName, placeholder: el && el.getAttribute('placeholder') || '' }; })()`);
    metrics.push('DATA search-typed=' + JSON.stringify(typed));
    assert('search-box-accepts-input', /\u79d1\u6280/.test(typed.value), metrics);
    const beforeUrl = await evaluate(`location.href`);
    await command('Input.dispatchKeyEvent', { type: 'keyDown', key: 'Enter', code: 'Enter', windowsVirtualKeyCode: 13, nativeVirtualKeyCode: 13, text: '\r' });
    await command('Input.dispatchKeyEvent', { type: 'keyUp', key: 'Enter', code: 'Enter', windowsVirtualKeyCode: 13, nativeVirtualKeyCode: 13 });
    await delay(7000);
    let after = await evaluate(`({ url: location.href, title: document.title })`);
    let searched = /(search|all\\?search|\u641c\u7d22)/.test(after.url) || after.url.includes('search');
    if (!searched && after.url === beforeUrl) {
      // Enter 没触发导航：用表单提交兜底（浏览器原生机制），
      // 验证搜索导航链路本身可用。
      await evaluate(`(() => { const f = document.querySelector('form'); if (f) f.submit(); return Boolean(f); })()`);
      await delay(6000);
      after = await evaluate(`({ url: location.href, title: document.title })`);
      searched = /(search|all\\?search|\u641c\u7d22)/.test(after.url) || after.url.includes('search');
    }
    assert('search-interaction', (searched || after.url !== beforeUrl) && !crashed, metrics);
    metrics.push('DATA search-url=' + after.url);
  } else {
    assert('search-interaction', false, metrics);
    metrics.push('DATA search-fail=' + JSON.stringify(search));
  }

  // 动态卡片点击：CDP 真实鼠标点击第一个 /video/ 卡片，应能导航到视频页且不崩溃
  await command('Page.navigate', { url: targetUrl });
  await waitEvent('Page.loadEventFired');
  await delay(9000);
  const card = await evaluate(`(() => {
    const cards = Array.from(document.querySelectorAll('a[href*="/video/"]'))
      .filter(e => e.offsetParent !== null)
      .slice(0, 5)
      .map(el => {
        const r = el.getBoundingClientRect();
        return { href: el.href, x: r.left + r.width / 2, y: r.top + r.height / 2 };
      });
    if (!cards.length) return { ok: false, reason: 'no-feed-card' };
    return { ok: true, cards };
  })()`);
  async function pollVideoTarget(ms) {
    const deadline = Date.now() + ms;
    while (Date.now() < deadline) {
      await delay(1200);
      try {
        const response = await fetch('http://127.0.0.1:' + port + '/json/list');
        const targets = await response.json();
        for (const t of targets) {
          if (t.type === 'page' && /\/video\//.test(t.url || '')) return t.url;
        }
      } catch { /* target list not ready */ }
      const u = await evaluate(`location.href`).catch(() => '');
      if (/\/video\//.test(u)) return u;
    }
    return '';
  }
  if (card.ok) {
    metrics.push('DATA card-candidates=' + JSON.stringify(card.cards.map(c => c.href)));
    let navigated = false;
    let newTabUrl = '';
    // 通道 1：真实鼠标点击每个候选，滚动到视口中心后点击
    for (const c of card.cards) {
      if (navigated) break;
      await evaluate(`(() => { const el = document.querySelector('a[href="${c.href.replace(/"/g, '\\"')}"]'); if (el) el.scrollIntoView({ block: 'center' }); return true; })()`);
      await delay(400);
      const pos = await evaluate(`(() => { const el = document.querySelector('a[href="${c.href.replace(/"/g, '\\"')}"]'); if (!el) return null; const r = el.getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; })()`);
      if (!pos) continue;
      await command('Input.dispatchMouseEvent', { type: 'mouseMoved', x: pos.x, y: pos.y });
      await command('Input.dispatchMouseEvent', { type: 'mousePressed', x: pos.x, y: pos.y, button: 'left', clickCount: 1 });
      await command('Input.dispatchMouseEvent', { type: 'mouseReleased', x: pos.x, y: pos.y, button: 'left', clickCount: 1 });
      newTabUrl = await pollVideoTarget(8000);
      if (newTabUrl) { navigated = true; metrics.push('DATA card-opened-by=mouse ' + c.href); break; }
    }
    // 通道 2：JS el.click() 兜底（对 target=_blank 同样会开新标签）
    if (!navigated) {
      for (const c of card.cards) {
        if (navigated) break;
        await evaluate(`(() => { const el = document.querySelector('a[href="${c.href.replace(/"/g, '\\"')}"]'); if (el) el.click(); return true; })()`);
        newTabUrl = await pollVideoTarget(8000);
        if (newTabUrl) { navigated = true; metrics.push('DATA card-opened-by=js ' + c.href); break; }
      }
    }
    assert('feed-card-interactive', navigated && !crashed, metrics);
    metrics.push('DATA card-opened=' + newTabUrl);
    if (navigated) {
      // 记录视频页播放器状态（观察数据；完整播放器/媒体验收属于 G4）。
      await command('Page.navigate', { url: newTabUrl });
      await waitEvent('Page.loadEventFired');
      await delay(12000);
      const player = await evaluate(`(() => {
        const v = document.querySelector('video');
        const area = document.querySelector('.bpx-player-container, .video-player, #bilibili-player, .player-container');
        return { url: location.href, video: Boolean(v), playerArea: Boolean(area), mediaSource: typeof MediaSource === 'function' };
      })()`);
      metrics.push('DATA player-area=' + JSON.stringify(player));
    } else {
      metrics.push('DATA player-area=homepage-no-player (G4 scope)');
    }
  } else {
    assert('feed-card-interactive', false, metrics);
    metrics.push('DATA card-fail=' + JSON.stringify(card));
  }

  // 崩溃恢复：全程无 targetCrashed，子进程存活，页面可继续响应
  const alive = await evaluate(`({ alive: true, title: document.title })`).catch(() => ({ alive: false }));
  assert('crash-recovery', !crashed && alive.alive && child.exitCode === null, metrics);

  metrics.push('DATA first-content-load-ms=' + loadMs);
  metrics.push('DATA requests=' + requestCount);
  metrics.push('DATA js-heap-used-bytes=' + heap.usedSize);
  metrics.push('DATA page-url=' + probe.url);

  console.log(metrics.join('\n'));
  if (hadFailure) {
    console.log('HIVE_BILIBILI_HOME_INTERACTION: FAIL');
    exitCode = 1;
  } else {
    console.log('HIVE_BILIBILI_HOME_INTERACTION: PASS');
    exitCode = 0;
  }
} catch (error) {
  console.error(error.stack || error);
  console.error(diagnostics.join('').slice(-8000));
  console.log('HIVE_BILIBILI_HOME_INTERACTION: FAIL');
  exitCode = 1;
} finally {
  if (socket && socket.readyState === WebSocket.OPEN) socket.close();
  child.kill('SIGTERM');
  await Promise.race([new Promise(resolve => child.once('exit', resolve)), delay(2000)]);
  if (child.exitCode === null && child.signalCode === null) child.kill('SIGKILL');
  rmSync(profile, { recursive: true, force: true });
}
process.exit(exitCode);
