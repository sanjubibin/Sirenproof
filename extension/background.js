// Sirenproof extension.
//
// Asks the Sirenproof service on this PC about every page you open, including in-page navigation on
// sites like YouTube, and shows the block or pause page when a rule says so. All decisions are made
// by the service; the extension stores nothing and sends nothing anywhere else.
//
// Permissions: nativeMessaging (to reach the service), webNavigation (to see page loads) and tabs (to
// re-check pages that were already open when a lock began).
"use strict";

const HOST = "com.sirenproof.host";
const HEARTBEAT_MS = 10_000;
const REQUEST_TIMEOUT_MS = 3_000;
const RECONNECT_MS = 2_000;
const CACHE_MS = 2_000;

let port = null;
let nextId = 1;
const waiting = new Map();
const recent = new Map();  // url -> {verdict, at}: one navigation fires several events

function connect() {
  try {
    port = chrome.runtime.connectNative(HOST);
  } catch (error) {
    port = null;
    setTimeout(connect, RECONNECT_MS);
    return;
  }
  port.onMessage.addListener(onReply);
  port.onDisconnect.addListener(() => {
    port = null;
    for (const resolve of waiting.values()) resolve({ ok: false, error: "disconnected" });
    waiting.clear();
    setTimeout(connect, RECONNECT_MS);
  });
  send("ext.hello", { version: chrome.runtime.getManifest().version });
}

function onReply(message) {
  const resolve = waiting.get(message && message.id);
  if (resolve) {
    waiting.delete(message.id);
    resolve(message);
  }
}

function send(op, args = {}) {
  return new Promise((resolve) => {
    if (!port) connect();
    if (!port) {
      resolve({ ok: false, error: "not connected" });
      return;
    }
    const id = nextId++;
    waiting.set(id, resolve);
    try {
      port.postMessage({ id, op, args });
    } catch (error) {
      waiting.delete(id);
      resolve({ ok: false, error: String(error) });
      return;
    }
    setTimeout(() => {
      if (waiting.delete(id)) resolve({ ok: false, error: "timeout" });
    }, REQUEST_TIMEOUT_MS);
  });
}

function isWebAddress(url) {
  return typeof url === "string" && /^https?:\/\//i.test(url);
}

async function decide(url) {
  const cached = recent.get(url);
  if (cached && Date.now() - cached.at < CACHE_MS) return cached.verdict;
  const reply = await send("ext.decide", { url });
  // If the service can't be reached, never break browsing. During a lock the service itself closes
  // browsers whose extension isn't working.
  const verdict = reply.ok && reply.result ? reply.result : { action: "allow" };
  recent.set(url, { verdict, at: Date.now() });
  if (recent.size > 200) recent.delete(recent.keys().next().value);
  return verdict;
}

function blockPageUrl(url, verdict) {
  const data = {
    url,
    action: verdict.action,
    rule: verdict.rule || "",
    ruleName: verdict.rule_name || "",
    goal: verdict.goal || "",
    until: verdict.until || null,
    pause: Number(verdict.pause_seconds) || 15,
    continueMinutes: Number(verdict.continue_minutes) || 5,
  };
  return chrome.runtime.getURL("block.html") + "#" + encodeURIComponent(JSON.stringify(data));
}

async function check(tabId, url, frameId) {
  if (frameId !== 0 || tabId < 0 || !isWebAddress(url)) return;
  const verdict = await decide(url);
  if (verdict.action === "allow") return;
  await chrome.tabs.update(tabId, { url: blockPageUrl(url, verdict) });
}

async function checkOpenTabs() {
  const tabs = await chrome.tabs.query({});
  for (const tab of tabs) {
    if (tab.id !== undefined && tab.url) check(tab.id, tab.url, 0);
  }
}

// Requests from the block page.
chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  if (sender.id !== chrome.runtime.id || !sender.tab || !message) return false;

  if (message.type === "continue") {
    if (!isWebAddress(message.url)) {
      sendResponse({ ok: false, message: "that isn't a web address" });
      return false;
    }
    send("ext.continue", { rule: String(message.rule) }).then((reply) => {
      const ok = Boolean(reply.ok && reply.result && reply.result.ok);
      if (ok) {
        recent.clear();
        chrome.tabs.update(sender.tab.id, { url: message.url });
      }
      sendResponse({ ok, message: reply.ok ? reply.result && reply.result.message : reply.error });
    });
    return true;  // answered asynchronously
  }

  if (message.type === "leave") {
    chrome.tabs.update(sender.tab.id, { url: "chrome://newtab/" });
  }
  return false;
});

chrome.webNavigation.onBeforeNavigate.addListener((d) => check(d.tabId, d.url, d.frameId));
chrome.webNavigation.onHistoryStateUpdated.addListener((d) => check(d.tabId, d.url, d.frameId));

setInterval(() => {
  send("ext.heartbeat");
  recent.clear();
  checkOpenTabs();
}, HEARTBEAT_MS);

connect();
checkOpenTabs();
