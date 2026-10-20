// Interface only: every array operation is in PlaylistManager.cpp
const enc = new TextEncoder(), dec = new TextDecoder();
let w;               // the WebAssembly exports
let highlight = -1;  // row found by the last search

const $ = id => document.getElementById(id);
const bytes = () => new Uint8Array(w.memory.buffer);

function say(text, ok = true) {
  const m = $("message");
  m.textContent = text;
  m.className = ok ? "ok" : "err";
}

function putText(ptr, str) {                 // write text into a C++ array
  const b = enc.encode(str.trim()).subarray(0, w.text_len() - 1);
  bytes().set(b, ptr);
  bytes()[ptr + b.length] = 0;
}
function getText(ptr) {                      // read text out of a C++ array
  const m = bytes();
  let e = ptr; while (m[e]) e++;
  return dec.decode(m.subarray(ptr, e));
}
const mmss = s => `${Math.floor(s / 60)}:${String(s % 60).padStart(2, "0")}`;
function total(s) {
  const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), sec = s % 60;
  return h ? `${h}:${String(m).padStart(2, "0")}:${String(sec).padStart(2, "0")}` : mmss(s);
}
function parseLength(str) {                  // "3:45" -> 225 seconds, or 0 if invalid
  const m = /^(\d{1,3}):([0-5]\d)$/.exec(str.trim());
  return m ? Number(m[1]) * 60 + Number(m[2]) : 0;
}

function draw() {
  const n = w.song_count(), cur = w.current_index();
  $("counter").textContent = n ? `(${n} of ${w.max_songs()} slots used, ${total(w.total_seconds())} total)`
                               : `(0 of ${w.max_songs()} slots used)`;
  if (n) {
    $("npTitle").textContent = getText(w.get_title(cur));
    $("npMeta").textContent = `${getText(w.get_artist(cur))} \u2022 ${mmss(w.get_duration(cur))} \u2022 song ${cur + 1} of ${n} (index ${cur})`;
  } else {
    $("npTitle").textContent = "Nothing yet";
    $("npMeta").textContent = "Add a song or load the sample playlist.";
  }
  const body = $("tbody");
  body.innerHTML = "";
  if (n === 0) {
    body.innerHTML = `<tr><td class="empty" colspan="5">The playlist is empty.</td></tr>`;
    return;
  }
  for (let i = 0; i < n; i++) {
    const tr = document.createElement("tr");
    tr.className = (i === cur ? "playing " : "") + (i === highlight ? "hit" : "");
    [i, getText(w.get_title(i)), getText(w.get_artist(i)), mmss(w.get_duration(i))].forEach((v, k) => {
      const td = document.createElement("td");
      td.textContent = i === cur && k === 1 ? "\u25B6 " + v : v;
      if (k === 0 || k === 3) td.className = "num";
      tr.appendChild(td);
    });
    const td = document.createElement("td");
    const wrap = document.createElement("div");
    wrap.className = "acts";
    td.appendChild(wrap);
    const act = (label, cls, fn, aria) => {
      const b = document.createElement("button");
      b.className = cls; b.textContent = label; b.setAttribute("aria-label", aria);
      b.onclick = () => { fn(); highlight = -1; draw(); };
      wrap.appendChild(b);
    };
    act("\u25B2", "small", () => w.move_up(i), "Move up");
    act("\u25BC", "small", () => w.move_down(i), "Move down");
    act("Play", "small", () => w.set_current(i), "Play this song");
    act("Remove", "danger", () => { w.remove_song(i); say("Song removed."); }, "Remove song");
    tr.appendChild(td);
    body.appendChild(tr);
  }
}

function addSong(title, artist, seconds, nextUp) {
  putText(w.title_buffer(), title);
  putText(w.artist_buffer(), artist);
  return nextUp ? w.play_next(seconds) : w.add_song(seconds);
}

function onAdd(nextUp) {
  const seconds = parseLength($("length").value);
  const r = addSong($("title").value, $("artist").value, seconds, nextUp);
  const msgs = [nextUp ? "Added right after the current song." : "Added to the end of the playlist.",
                "The playlist is full: the array has no free slot.",
                "Enter a song title.",
                "Enter the length like 3:45 (minutes:seconds)."];
  say(msgs[r], r === 0);
  if (r === 0) {
    ["title", "artist", "length"].forEach(id => $(id).value = "");
    highlight = -1; $("title").focus();
  }
  draw();
}

function onFind() {
  const q = $("search").value;
  putText(w.title_buffer(), q);
  highlight = w.find_song();
  if (highlight === -1) say("No match.", false);
  else say(`Found "${getText(w.get_title(highlight))}" at index ${highlight}.`);
  draw();
}

function onShuffle() {
  if (w.song_count() < 2) { say("Add at least 2 songs to shuffle.", false); return; }
  w.set_seed(crypto.getRandomValues(new Uint32Array(1))[0]);
  w.shuffle(); highlight = -1;
  say("Shuffled. The playing song moved to the top.");
  draw();
}

function onSample() {
  if (w.song_count() > 0) { say("The sample loads into an empty playlist only. Remove songs first.", false); return; }
  const sample = [["Essence", "Wizkid", 248], ["Last Last", "Burna Boy", 172], ["Kwaku the Traveller", "Black Sherif", 195],
                  ["Peru", "Fireboy DML", 170], ["Calm Down", "Rema", 239], ["Rush", "Ayra Starr", 190],
                  ["Soweto", "Victony", 210], ["Unavailable", "Davido", 185]];
  sample.forEach(([t, a, s]) => addSong(t, a, s, false));
  say(`Loaded ${sample.length} sample songs (lengths are rounded demo values).`);
  draw();
}

async function start() {
  const url = "playlist_manager.wasm";
  try {
    w = (await WebAssembly.instantiateStreaming(fetch(url), {})).instance.exports;
  } catch (e) {                              // server without the wasm MIME type
    const buf = await (await fetch(url)).arrayBuffer();
    w = (await WebAssembly.instantiate(buf, {})).instance.exports;
  }
  $("addBtn").onclick = () => onAdd(false);
  $("nextUpBtn").onclick = () => onAdd(true);
  $("findBtn").onclick = onFind;
  $("shuffleBtn").onclick = onShuffle;
  $("sampleBtn").onclick = onSample;
  $("nextBtn").onclick = () => { w.next_song(); highlight = -1; draw(); };
  $("prevBtn").onclick = () => { w.prev_song(); highlight = -1; draw(); };
  $("search").onkeydown = e => { if (e.key === "Enter") onFind(); };
  ["title", "artist", "length"].forEach(id => $(id).onkeydown = e => { if (e.key === "Enter") onAdd(false); });
  draw();
}
start().catch(err => say("Could not load the C++ module: " + err.message, false));
