// Regression harness for the packet parser. The parser below is the source of
// truth and is mirrored verbatim into index.html (only the return value differs:
// here it returns {queue,tossups,bonuses} for reporting; index.html returns the
// queue array). Run: `node test_packets/harness.js test_packets`.
// Status: 9/10 sample packets parse cleanly; two.txt (Question #N:, no ANSWER
// markers) needs a separate path — see the chat notes.
function Tossup(text, answer) { this.type = "tossup"; this.text = text; this.answer = answer; }
function Bonus(leadin, parts) { this.type = "bonus"; this.leadin = leadin; this.parts = parts; }

// ANSWER: / Answer. / ANS:  (no bare "A:" — it collides with "A." bonus parts).
// Colon optional so a typo'd "ANSWER the soul" still registers, but a bonus
// lead-in like "Answer the following ... For 10 points each:" is NOT an answer.
function isAnswer(ln) {
  if (/\banswer the following\b|for\s+(ten|10)\s+points/i.test(ln)) return false;
  return /^\s*(answers?|ans)\b\s*[:.]?(\s|$)/i.test(ln);
}
function stripAnswer(ln) { return ln.replace(/^\s*(answers?|ans)\b\s*[:.]?\s*/i, "").trim(); }
function isMeta(ln) { return /^\s*[<{]/.test(ln) || /^\s*category\s*:/i.test(ln); }
function isBlank(ln) { return /^\s*$/.test(ln); }
// Strip a trailing inline tag like <ES> or <VP, Mythology> from an answer/clue.
function clean(s) { return s.replace(/\s*<[^>]*>\s*$/, "").trim(); }

// Item-number prefix: "1." "1)" "12:" "TU3." -> { rest }. The number must be
// >= 1 and followed by whitespace/end, so a wrapped decimal like "0.01 cm" or
// "13.5 m" is not mistaken for an item.
function numStart(ln) {
  var m = ln.match(/^\s*(?:tossup|toss-?up|tu)?\s*([1-9]\d?)\s*[.):](?:\s+(.*))?\s*$/i);
  return m ? { rest: m[2] || "" } : null;
}
// Explicit bonus marker: "Bonus" / "Bonus 1)" / "Bonus 1." -> { rest }
function bonusStart(ln) {
  var m = ln.match(/^\s*bonus\b\s*\d*\s*[.):]?\s*(.*)$/i);
  return m ? { rest: m[1] } : null;
}
// Bonus part at line start: [10] [10h] [H] (10) (10m)  OR  A. B. C. D.
// Letter parts set .letter so the caller can require them to run in sequence
// (a lone "C." inside a wrapped clue must not be mistaken for a part).
function partStart(ln) {
  var m = ln.match(/^\s*[\[(]\s*(\d{1,2})?\s*([ehmEHM]?)\s*[\])]\s*(.*)$/);
  if (m && (m[1] || m[2])) return { value: m[1] ? parseInt(m[1], 10) : null, diff: (m[2] || "").toLowerCase(), rest: m[3], letter: null };
  var l = ln.match(/^\s*([A-Da-d])\s*[.)]\s+(.*)$/);
  if (l) return { value: null, diff: "", rest: l[2], letter: l[1].toUpperCase() };
  return null;
}

function parsePacket(raw) {
  var lines = raw.split(/\r?\n/);

  // Boilerplate: lines repeated across page breaks, plus page/licensing/section
  // headers. Repeated long lines catch most PDF page headers/footers.
  var freq = {};
  lines.forEach(function (l) { var t = l.trim(); if (t) freq[t] = (freq[t] || 0) + 1; });
  function isNoise(ln) {
    var t = ln.trim();
    if (!t) return false;
    if (t.length >= 25 && freq[t] >= 2) return true;                                          // repeated header/footer
    if (/©|\blicensing\b|may not be used|terms for usage|do not use these questions/i.test(t)) return true;
    if (/\bpage \d+\b/i.test(t) || /^\d{1,3}$/.test(t)) return true;                          // page number (incl. bare)
    if (/^note to (players?|moderators?|teams?|readers?)\b/i.test(t)) return true;            // moderator note
    if (/^[_*\s]+$/.test(t) || /^\W*halftime\W*$/i.test(t)) return true;                      // rules / **HALFTIME**
    if (/^(tossups?|bonuses|round\b.*|packet\b.*|.*\bsection\b|toss-?up questions)$/i.test(t)) return true; // section header
    return false;
  }

  // Skip the front matter: start at the first non-noise line that looks like an item.
  var start = 0;
  for (var i = 0; i < lines.length; i++) {
    if (!isNoise(lines[i]) && (numStart(lines[i]) || bonusStart(lines[i]) || partStart(lines[i]))) { start = i; break; }
  }

  // Build segments. start: "num" | "bonus" | "part" | "text" (unmarked leadin).
  var segs = [], cur = null, field = null, lastLetter = null; // field: "text" | "answer" | null
  function open(s, extra, first) {
    cur = { start: s, value: extra ? extra.value : null, diff: extra ? extra.diff : null, text: [], answer: [] };
    if (first) cur.text.push(first);
    segs.push(cur); field = "text";
  }
  for (var j = start; j < lines.length; j++) {
    var ln = lines[j];
    if (isBlank(ln) || isNoise(ln) || isMeta(ln)) { if (field === "answer") field = null; continue; }
    if (isAnswer(ln)) { if (cur) { field = "answer"; cur.answer.push(stripAnswer(ln)); } continue; }
    var ps = partStart(ln);
    if (ps && ps.letter) {
      // Letter parts must run A, B, C, ... so a wrapped "C." inside a clue isn't a part.
      var expect = lastLetter ? String.fromCharCode(lastLetter.charCodeAt(0) + 1) : null;
      if (ps.letter === "A" || ps.letter === expect) { lastLetter = ps.letter; open("part", ps, ps.rest); continue; }
      ps = null; // reject; fall through to text
    } else if (ps) { lastLetter = null; open("part", ps, ps.rest); continue; }
    var bs = bonusStart(ln);
    if (bs) { lastLetter = null; open("bonus", null, bs.rest); continue; }
    var ns = numStart(ln);
    if (ns) { lastLetter = null; open("num", null, ns.rest); continue; }
    if (cur && field === "text") cur.text.push(ln.trim());
    else if (cur && field === "answer") cur.answer.push(ln.trim());
    else { lastLetter = null; open("text", null, ln.trim()); } // unmarked leadin/clue after an answer ended
  }

  // Group: a leadin (num/bonus/text seg) followed by part segs is a bonus;
  // a seg with its own answer and no following parts is a tossup.
  function mkpart(s) { return { value: s.value, diff: s.diff, text: clean(s.text.join(" ").trim()), answer: clean(s.answer.join(" ").trim()) }; }
  function leadOf(s) { return clean(s.text.join(" ").trim().replace(/^\s*(?:\d{1,2}\s*[.):]|bonus\b\s*\d*\s*[.):]?)\s*/i, "")); }
  var items = [];
  for (var k = 0; k < segs.length;) {
    var s = segs[k];
    if (s.start === "part") {
      var p0 = []; while (k < segs.length && segs[k].start === "part") { p0.push(mkpart(segs[k])); k++; }
      items.push(new Bonus("", p0));
    } else if (k + 1 < segs.length && segs[k + 1].start === "part") {
      var lead = leadOf(s), pp = []; k++;
      while (k < segs.length && segs[k].start === "part") { pp.push(mkpart(segs[k])); k++; }
      items.push(new Bonus(lead, pp));
    } else if (s.start === "bonus") {
      items.push(new Bonus(leadOf(s), [])); k++;
    } else {
      items.push(new Tossup(leadOf(s), clean(s.answer.join(" ").trim()))); k++;
    }
  }

  // Pair the Nth tossup with the Nth bonus and interleave into the play queue.
  var tossups = items.filter(function (q) { return q.type === "tossup"; });
  var bonuses = items.filter(function (q) { return q.type === "bonus"; });
  var queue = [];
  var rounds = Math.max(tossups.length, bonuses.length);
  for (var r = 0; r < rounds; r++) { if (tossups[r]) queue.push(tossups[r]); if (bonuses[r]) queue.push(bonuses[r]); }
  return { queue: queue, tossups: tossups, bonuses: bonuses };
}

var fs = require("fs"), path = require("path");
var dir = process.argv[2] || ".";
fs.readdirSync(dir).filter(function (f) { return /\.txt$/.test(f); }).sort().forEach(function (f) {
  var out = parsePacket(fs.readFileSync(path.join(dir, f), "utf8"));
  var flags = [];
  var badBonus = out.bonuses.filter(function (b) { return b.parts.length !== 3; }).length;
  var emptyAns = out.queue.filter(function (q) { return q.type === "tossup" ? !q.answer : q.parts.some(function (p) { return !p.answer; }); }).length;
  var shortTU = out.tossups.filter(function (t) { return t.text.length < 60; }).length;
  var emptyLead = out.bonuses.filter(function (b) { return !b.leadin; }).length;
  if (out.tossups.length === 0) flags.push("NO TOSSUPS");
  if (badBonus) flags.push(badBonus + " bonus!=3parts");
  if (emptyAns) flags.push(emptyAns + " empty-answer");
  if (shortTU) flags.push(shortTU + " short-tossup");
  if (emptyLead) flags.push(emptyLead + " empty-leadin");
  console.log(
    f.padEnd(11) + " TU=" + String(out.tossups.length).padStart(3) +
    " B=" + String(out.bonuses.length).padStart(3) +
    "  " + (flags.length ? "⚠ " + flags.join(", ") : "ok"));
});
