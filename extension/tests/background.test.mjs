// Tests for background.js with a fake `chrome` object. Run with: node --test extension/tests
import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import vm from "node:vm";

const SOURCE = readFileSync(new URL("../background.js", import.meta.url), "utf8");

// Loads background.js. `answer(message)` plays the service: return a reply object, or undefined to
// stay silent.
function load(answer) {
  const listeners = {};
  const updates = [];
  const posted = [];
  const port = {
    onMessage: { addListener: (f) => { port.reply = f; } },
    onDisconnect: { addListener: (f) => { port.disconnect = f; } },
    postMessage(message) {
      posted.push(message);
      const reply = answer(message);
      if (reply !== undefined) queueMicrotask(() => port.reply({ id: message.id, ...reply }));
    },
  };
  const chrome = {
    runtime: {
      id: "sirenproof",
      connectNative: () => port,
      getManifest: () => ({ version: "0.1.0" }),
      getURL: (path) => `chrome-extension://sirenproof/${path}`,
      onMessage: { addListener: (f) => { listeners.message = f; } },
    },
    webNavigation: {
      onBeforeNavigate: { addListener: (f) => { listeners.before = f; } },
      onHistoryStateUpdated: { addListener: (f) => { listeners.history = f; } },
    },
    tabs: {
      update: async (tabId, props) => { updates.push({ tabId, ...props }); },
      query: async () => [],
    },
  };
  const context = vm.createContext({
    chrome, console, queueMicrotask, setTimeout, clearTimeout, setInterval: () => 0,
  });
  vm.runInContext(SOURCE, context);
  return { listeners, updates, posted };
}

const settle = () => new Promise((resolve) => setTimeout(resolve, 5));

const verdict = (action, extra = {}) => ({
  ok: true,
  result: { action, rule: "work", rule_name: "Work hours", goal: "Ship the report", until: "2026-11-02T11:30:00Z",
            pause_seconds: 15, continue_minutes: 5, ...extra },
});

function decidingService(action) {
  return (m) => (m.op === "ext.decide" ? verdict(action) : { ok: true, result: {} });
}

test("says hello to the service when it starts", () => {
  const { posted } = load(() => ({ ok: true, result: {} }));
  assert.equal(posted[0].op, "ext.hello");
  assert.equal(posted[0].args.version, "0.1.0");
});

test("a blocked page is replaced by the block page, carrying the goal", async () => {
  const { listeners, updates } = load(decidingService("block"));
  listeners.before({ tabId: 7, frameId: 0, url: "https://www.reddit.com/r/all" });
  await settle();
  assert.equal(updates.length, 1);
  assert.equal(updates[0].tabId, 7);
  assert.ok(updates[0].url.startsWith("chrome-extension://sirenproof/block.html#"));
  const data = JSON.parse(decodeURIComponent(updates[0].url.split("#")[1]));
  assert.equal(data.action, "block");
  assert.equal(data.goal, "Ship the report");
  assert.equal(data.url, "https://www.reddit.com/r/all");
});

test("an allowed page is left alone", async () => {
  const { listeners, updates } = load(decidingService("allow"));
  listeners.before({ tabId: 7, frameId: 0, url: "https://example.com/" });
  await settle();
  assert.equal(updates.length, 0);
});

test("in-page navigation (YouTube's feed to Shorts) is checked too", async () => {
  const { listeners, updates, posted } = load(decidingService("nudge"));
  listeners.history({ tabId: 3, frameId: 0, url: "https://www.youtube.com/shorts/abc" });
  await settle();
  assert.ok(posted.some((m) => m.op === "ext.decide" && m.args.url === "https://www.youtube.com/shorts/abc"));
  assert.equal(updates.length, 1);
});

test("browser pages and frames inside a page are never sent to the service", async () => {
  const { listeners, posted } = load(decidingService("block"));
  listeners.before({ tabId: 1, frameId: 0, url: "chrome://settings" });
  listeners.before({ tabId: 1, frameId: 2, url: "https://ads.example.com/frame" });
  await settle();
  assert.equal(posted.filter((m) => m.op === "ext.decide").length, 0);
});

test("if the service can't answer, browsing keeps working", async () => {
  const { listeners, updates } = load((m) => (m.op === "ext.decide" ? { ok: false, error: "the Sirenproof service isn't running" } : { ok: true, result: {} }));
  listeners.before({ tabId: 7, frameId: 0, url: "https://www.reddit.com/" });
  await settle();
  assert.equal(updates.length, 0);
});

test("continuing after a nudge asks the service, then opens the page", async () => {
  const { listeners, updates, posted } = load((m) =>
    m.op === "ext.continue" ? { ok: true, result: { ok: true, message: "continue until ..." } } : { ok: true, result: {} });
  const replies = [];
  const keepOpen = listeners.message(
    { type: "continue", rule: "gentle", url: "https://www.reddit.com/" },
    { id: "sirenproof", tab: { id: 9 } },
    (r) => replies.push(r));
  assert.equal(keepOpen, true);
  await settle();
  assert.ok(posted.some((m) => m.op === "ext.continue" && m.args.rule === "gentle"));
  assert.deepEqual(updates, [{ tabId: 9, url: "https://www.reddit.com/" }]);
  assert.equal(replies[0].ok, true);
});

test("the block page can only send the tab back to a web address", async () => {
  const { listeners, updates, posted } = load(() => ({ ok: true, result: { ok: true } }));
  const replies = [];
  listeners.message({ type: "continue", rule: "x", url: "javascript:alert(1)" }, { id: "sirenproof", tab: { id: 9 } },
                    (r) => replies.push(r));
  await settle();
  assert.equal(replies[0].ok, false);
  assert.equal(updates.length, 0);
  assert.equal(posted.filter((m) => m.op === "ext.continue").length, 0);
});

test("messages from other extensions are ignored", async () => {
  const { listeners, updates } = load(() => ({ ok: true, result: { ok: true } }));
  const answered = listeners.message({ type: "continue", rule: "x", url: "https://reddit.com/" },
                                     { id: "someone-else", tab: { id: 9 } }, () => {});
  await settle();
  assert.equal(answered, false);
  assert.equal(updates.length, 0);
});
