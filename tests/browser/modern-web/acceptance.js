import { computed, createSSRApp, nextTick, ref } from './node_modules/vue/dist/vue.esm-browser.prod.js';

const results = [];
const check = (name, condition) => {
  results.push(`${condition ? 'PASS' : 'FAIL'} ${name}`);
  if (!condition) throw new Error(name);
};

if (globalThis.testRunner) {
  testRunner.dumpAsText();
  testRunner.waitUntilDone();
}

try {
  check('html-dom', document.querySelector('h1').textContent.includes('HIVE'));
  const gridStyle = getComputedStyle(document.querySelector('#grid'));
  check('css-grid-custom-properties',
        gridStyle.display === 'grid' && gridStyle.color === 'rgb(39, 174, 96)');

  const count = ref(0);
  const doubled = computed(() => count.value * 2);
  const app = createSSRApp({
    setup() {
      return { count, doubled };
    },
    template: '<button id="counter" @click="count++">{{ count }}</button><span id="doubled">{{ doubled }}</span>'
  });
  app.mount('#app');
  document.querySelector('#counter').click();
  await nextTick();
  check('vue3-hydration-reactivity',
        document.querySelector('#counter').textContent === '1' &&
        document.querySelector('#doubled').textContent === '2');

  const response = await fetch('./fixture.json', { cache: 'no-store' });
  const fixture = await response.json();
  check('fetch-promise-json', response.ok && fixture.value === 42);

  const wasm = await WebAssembly.instantiate(new Uint8Array([
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00
  ]));
  check('webassembly', wasm.instance instanceof WebAssembly.Instance);
  check('media-source-api', 'MediaSource' in globalThis);
  check('worker-api', 'Worker' in globalThis);

  results.push('HIVE_MODERN_WEB: PASS');
} catch (error) {
  results.push(`ERROR ${error && error.message ? error.message : error}`);
  results.push('HIVE_MODERN_WEB: FAIL');
} finally {
  const output = document.querySelector('#results');
  output.textContent = results.join('\n');
  output.className = results.at(-1).endsWith('PASS') ? 'pass' : 'fail';
  if (globalThis.testRunner) testRunner.notifyDone();
}
