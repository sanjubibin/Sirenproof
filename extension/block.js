// The block and pause page. Everything shown comes from the address the extension opened this page
// with, and every value is set as text, never as markup.
"use strict";

const CIRCUMFERENCE = 326.7;

function readDetails() {
  try {
    return JSON.parse(decodeURIComponent(location.hash.slice(1)));
  } catch (error) {
    return {};
  }
}

function clock(iso) {
  const date = iso ? new Date(iso) : null;
  return date && !Number.isNaN(date.getTime())
    ? date.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" })
    : null;
}

const details = readDetails();
const $ = (id) => document.getElementById(id);
const nudge = details.action === "nudge";
const ruleLabel = details.ruleName || details.rule || "your rule";
const until = clock(details.until);

$("goal").textContent = details.goal || (nudge ? "Is this what you meant to do?" : "Back to what matters.");
$("address").textContent = details.url || "";
$("leave").addEventListener("click", () => chrome.runtime.sendMessage({ type: "leave" }));

if (!nudge) {
  $("kind").textContent = "Blocked";
  $("detail").textContent = until ? `${ruleLabel} keeps this closed until ${until}.` : `${ruleLabel} keeps this closed.`;
  if (until) {
    const label = $("ring-label");
    label.classList.add("time");
    const caption = document.createElement("small");
    caption.textContent = "until";
    label.append(caption, document.createTextNode(until));
  }
} else {
  document.body.classList.add("pause");
  $("kind").textContent = "Pause";
  $("detail").textContent = `${ruleLabel} asks you to take a breath first.`;

  const continueButton = $("continue");
  continueButton.hidden = false;
  const total = Math.max(1, Math.round(details.pause || 15));
  let left = total;
  const ring = $("ring");

  const tick = () => {
    $("ring-label").textContent = left > 0 ? String(left) : "";
    ring.style.strokeDashoffset = String(CIRCUMFERENCE * (left / total));
    if (left > 0) {
      continueButton.textContent = `Continue in ${left} s`;
      left -= 1;
      setTimeout(tick, 1000);
    } else {
      continueButton.disabled = false;
      continueButton.textContent = `Continue for ${details.continueMinutes || 5} minutes`;
    }
  };
  tick();

  continueButton.addEventListener("click", () => {
    continueButton.disabled = true;
    chrome.runtime.sendMessage({ type: "continue", rule: details.rule, url: details.url }, (reply) => {
      if (!reply || !reply.ok) {
        $("status").textContent = (reply && reply.message) || "Sirenproof couldn't be reached.";
        continueButton.disabled = false;
      }
    });
  });
}

$("leave").focus();
