const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const { webcrypto } = require('node:crypto');

const source = fs.readFileSync(path.join(__dirname, '../Examples/WebController/www/openctrl.js'), 'utf8');
function controller(search = '') {
    const sent = [], pending = [], timers = new Map();
    const context = {
        location: { protocol: 'http:', origin: 'http://controller', search },
        localStorage: { getItem() { return 'a'.repeat(32); }, setItem() {} },
        crypto: webcrypto, AbortController, Event, URLSearchParams,
        document: { hidden: false, addEventListener() {}, querySelectorAll() { return []; } },
        window: { addEventListener() {} },
        setTimeout(fn) { const id = Symbol(); timers.set(id, fn); return id; },
        clearTimeout(id) { timers.delete(id); },
        fetch: async (url, options) => {
            sent.push({ path: url.slice(1), body: options.body ? JSON.parse(options.body) : undefined, headers: options.headers });
            return new Promise((resolve, reject) => pending.push({ resolve, reject }));
        },
    };
    vm.createContext(context);
    vm.runInContext(source, context);
    const run = (code) => vm.runInContext(code, context);
    run('sessionActive = true');
    const tick = () => new Promise((resolve) => setImmediate(resolve));
    async function reply(status = 200, body = {}) {
        assert.ok(pending.length, 'Expected an outstanding request');
        pending.shift().resolve({ ok: status === 200, status, json: async () => body });
        await tick();
    }
    async function drain() { while (pending.length) await reply(); }
    return { sent, pending, run, reply, drain, tick, timers };
}

test('button down and up survive an in-flight axis request', async () => {
    const c = controller();
    c.run('setGamepad({x:50}); setGamepad({buttons:1}); setGamepad({buttons:0})');
    await c.drain();
    assert.deepEqual(c.sent.map((r) => r.body.buttons), [0, 1, 0]);
});

test('continuous movement coalesces between discrete transitions', async () => {
    const c = controller();
    c.run('setGamepad({x:1}); setGamepad({x:2}); setGamepad({x:3}); setGamepad({buttons:1}); setGamepad({x:4}); setGamepad({buttons:0})');
    await c.drain();
    assert.deepEqual(c.sent.map((r) => [r.body.x, r.body.buttons]), [[1, 0], [3, 0], [4, 1], [4, 0]]);
});

test('pen contact and release cannot be coalesced away', async () => {
    const c = controller();
    c.run('pen.x=1; sendPen(); pen.tip=true; sendPen(); pen.x=2; sendPen(); pen.tip=false; sendPen()');
    await c.drain();
    assert.deepEqual(c.sent.map((r) => r.body.tip), [false, true, false]);
    assert.equal(c.sent[1].body.x, 2);
});

test('mouse clicks and movement share one order', async () => {
    const c = controller();
    c.run('clickMouse(1); mouse.moveX=10; sendMouseMove()');
    assert.equal(c.sent.length, 1);
    await c.drain();
    assert.deepEqual(c.sent.map((r) => [r.body.buttons, r.body.dx || 0]), [[1, 0], [0, 0], [0, 10]]);
});

test('secondary mouse uses its own endpoint and retains click/movement order', async () => {
    const c = controller('?mouse=secondary');
    c.run('clickMouse(1); mouse.moveX=10; sendMouseMove()');
    await c.drain();
    assert.deepEqual(c.sent.map((r) => r.path), ['mouse/secondary', 'mouse/secondary', 'mouse/secondary']);
    assert.deepEqual(c.sent.map((r) => [r.body.buttons, r.body.dx || 0]), [[1, 0], [0, 0], [0, 10]]);
    c.run("stopSession('Leaving page')");
    await c.drain();
    assert.equal(c.sent.at(-1).path, 'release');
});

test('controller metadata exposes the second mouse only in mixed mode', async () => {
    for (const mixed of [false, true]) {
        const c = controller();
        c.run(`globalThis.link = {hidden:true}; globalThis.modeLabel = {}; globalThis.mouseLabel = {};
            document.querySelectorAll = (selector) => {
                if (selector === '[data-secondary-mouse]') return [link];
                if (selector === '[data-connection-mode]') return [modeLabel];
                if (selector === '[data-mouse-name]') return [mouseLabel];
                return [];
            };`);
        const loading = c.run('loadControllerInfo()');
        await c.reply(200, { mode: mixed ? 'Mixed USB / Bluetooth' : 'USB', primaryMouse:'USB mouse', secondaryMouse:mixed });
        await loading;
        assert.equal(c.run('link.hidden'), !mixed);
        assert.equal(c.run('mouseLabel.textContent'), 'USB mouse');
        assert.equal(c.run('modeLabel.textContent'), mixed ? 'Mixed USB / Bluetooth' : 'USB');
        assert.equal(c.run('sessionActive'), true);
    }
});

test('typing sends an explicit release after every key', async () => {
    const c = controller();
    const typing = c.run("typeText('Aa')");
    await c.drain();
    await typing;
    assert.deepEqual(c.sent.map((r) => r.body), [
        {modifiers:2,keys:[4]}, {modifiers:0,keys:[]},
        {modifiers:0,keys:[4]}, {modifiers:0,keys:[]},
    ]);
});

test('USB errors cancel stale reports and reconnect without replay', async () => {
    const c = controller();
    c.run('setGamepad({buttons:1}); setGamepad({buttons:0}); setGamepad({buttons:2})');
    await c.reply(503);
    await c.drain();
    assert.equal(c.run('sessionActive'), false);
    assert.equal(c.run('gamepad.buttons'), 0);
    assert.deepEqual(c.sent.map((r) => r.path), ['gamepad', 'release']);
    const reconnect = c.run('heartbeat()');
    await c.reply();
    await reconnect;
    assert.equal(c.run('sessionActive'), true);
    assert.deepEqual(c.sent.map((r) => r.path), ['gamepad', 'release', 'heartbeat']);
});

test('network failure clears input and attempts a session release', async () => {
    const c = controller();
    c.run('clickMouse(1)');
    c.pending.shift().reject(new Error('Connection lost'));
    await c.tick();
    await c.drain();
    assert.equal(c.run('sessionActive'), false);
    assert.equal(c.sent[1].path, 'release');
});

test('every request carries the token and session ID', async () => {
    const c = controller();
    c.run('setGamepad({x:1})');
    assert.equal(c.sent[0].headers.Authorization, `Bearer ${'a'.repeat(32)}`);
    assert.match(c.sent[0].headers['X-OpenCtrl-Session'], /^[0-9a-f]{32}$/);
    await c.drain();
});

test('stale acknowledgements cannot reactivate a stopped session', async () => {
    const c = controller();
    const heartbeat = c.run('heartbeat()');
    c.run("stopSession('Paused', false)");
    await c.reply();
    await heartbeat;
    assert.equal(c.run('sessionActive'), false);
});

test('queue overflow releases controls instead of replaying a long backlog', async () => {
    const c = controller();
    c.run('for(let i=0;i<132;i++) post("keyboard", {keys:[4],modifiers:0})');
    assert.equal(c.run('sessionActive'), false);
    assert.equal(c.run('reports.length'), 0);
    await c.drain();
    assert.deepEqual(c.sent.map((r) => r.path), ['keyboard', 'release']);
});

test('reconnection resets local state changed while inactive', async () => {
    const c = controller();
    c.run('sessionActive=false; mouse.buttons=1; gamepad.buttons=4; heldKeys.set(4,2)');
    const reconnect = c.run('heartbeat()');
    await c.reply();
    await reconnect;
    assert.equal(c.run('mouse.buttons'), 0);
    assert.equal(c.run('gamepad.buttons'), 0);
    assert.equal(c.run('heldKeys.size'), 0);
    assert.equal(c.run('sessionActive'), true);
});
