/* ============================================================
   Music Plate — mockup app logic
   Everything under MockAPI simulates what will later be real:
     - MockAPI.ble.*      -> real BLE (Web Bluetooth / iOS CoreBluetooth)
     - MockAPI.spotify.*  -> real Spotify Web API calls
     - MockAPI.esp32.*    -> real HTTP/BLE comms with the reader
   The UI code below the mock layer shouldn't need to change
   when the real implementations are dropped in.
   ============================================================ */

const delay = (ms) => new Promise((r) => setTimeout(r, ms));

const MockAPI = {
  ble: {
    // Simulate scanning for readers in pairing mode
    async scanForReaders() {
      await delay(2200);
      return [
        { id: "mp-a4f2", name: "MusicPlate-A4F2", rssi: -48 },
        { id: "mp-c711", name: "MusicPlate-C711", rssi: -71 },
      ];
    },
  },

  esp32: {
    // Ask the reader to scan for wifi networks
    async scanWifi() {
      await delay(1800);
      return [
        { ssid: "MarshallHome", secured: true, strength: 3 },
        { ssid: "MarshallHome-5G", secured: true, strength: 3 },
        { ssid: "BT-Hub-9932", secured: true, strength: 2 },
        { ssid: "NextDoorWifi", secured: true, strength: 1 },
        { ssid: "CoffeeShopGuest", secured: false, strength: 1 },
      ];
    },
    // Push the full config blob to the reader
    async sendConfig(config) {
      console.log("[mock] sending config to reader:", config);
      // steps resolved one at a time by the send screen
      return true;
    },
    // Write an album ID to the tag currently on the reader
    async writeTag(albumId) {
      console.log("[mock] writing tag:", albumId);
      await delay(2600);
      return true;
    },
  },

  spotify: {
    // Real version: OAuth authorization-code flow w/ PKCE -> refresh token
    async authorize() {
      await delay(2000);
      return {
        user: "jerry",
        product: "premium",
        refreshToken: "AQDk3v…8fKw",
      };
    },
    // Real version: GET /v1/me/player/devices
    async getDevices() {
      await delay(600);
      return [
        { id: "dev1", name: "Living Room Speaker", type: "🔊", active: true },
        { id: "dev2", name: "Jerry's iPhone", type: "📱", active: false },
        { id: "dev3", name: "DESKTOP-PC", type: "💻", active: false },
        { id: "dev4", name: "Bedroom Echo", type: "🔊", active: false },
      ];
    },
    // Real version: GET /v1/search?type=album
    async searchAlbums(query) {
      await delay(350);
      const q = query.toLowerCase();
      return MOCK_ALBUMS.filter(
        (a) =>
          a.name.toLowerCase().includes(q) ||
          a.artist.toLowerCase().includes(q)
      );
    },
  },
};

// Mock album catalogue (stand-in for Spotify search results).
// Colors stand in for cover art until the real API provides images.
const MOCK_ALBUMS = [
  { id: "4LH4d3cOWNNsVw41Gqt2kv", name: "The Dark Side of the Moon", artist: "Pink Floyd", color: "linear-gradient(135deg,#1a1a2e,#4a1a5e)" },
  { id: "2guirTSEqLizK7j9i1MTTZ", name: "Nevermind", artist: "Nirvana", color: "linear-gradient(135deg,#0e5c8c,#3fa9f5)" },
  { id: "6dVIqQ8qmQ5GBnJ9shOYGE", name: "OK Computer", artist: "Radiohead", color: "linear-gradient(135deg,#b8c4cc,#5c7a8a)" },
  { id: "2widuo17g5CEC9IIkAlfme", name: "Whatever People Say I Am…", artist: "Arctic Monkeys", color: "linear-gradient(135deg,#2b2b2b,#6e6e6e)" },
  { id: "78bpIziExqiI9qztvNFlQu", name: "AM", artist: "Arctic Monkeys", color: "linear-gradient(135deg,#0d0d0d,#4a4a5e)" },
  { id: "4m2880jivSbbyEGAKfITCa", name: "Random Access Memories", artist: "Daft Punk", color: "linear-gradient(135deg,#c9a227,#8c6d1f)" },
  { id: "0ETFjACtuP2ADo6LFhL6HN", name: "Abbey Road", artist: "The Beatles", color: "linear-gradient(135deg,#2e5e3a,#87b891)" },
  { id: "5Z9iiGl2FcIfa3BMiv6OIw", name: "Currents", artist: "Tame Impala", color: "linear-gradient(135deg,#d94f70,#5e2b8a)" },
  { id: "7xl50xr9NDkd3i2kBbzsNZ", name: "Starboy", artist: "The Weeknd", color: "linear-gradient(135deg,#8c1f1f,#d94f2b)" },
  { id: "1To7kv722A8SpZF789MZy7", name: "channel ORANGE", artist: "Frank Ocean", color: "linear-gradient(135deg,#e8842b,#c94f1f)" },
  { id: "2nkto6YNI4rUYTLqEwWJ3o", name: "DAMN.", artist: "Kendrick Lamar", color: "linear-gradient(135deg,#8c1f2e,#1a1a1a)" },
  { id: "6QaVfG1pHYl1z15ZxkvVDW", name: "Rumours", artist: "Fleetwood Mac", color: "linear-gradient(135deg,#3a2e1f,#8a6d4a)" },
];

/* ============================================================
   State + persistence (localStorage stands in for app storage;
   the real system needs no server DB — tags carry the album ID)
   ============================================================ */

const store = {
  get players() { return JSON.parse(localStorage.getItem("mp_players") || "[]"); },
  set players(v) { localStorage.setItem("mp_players", JSON.stringify(v)); },
  get plates() { return JSON.parse(localStorage.getItem("mp_plates") || "[]"); },
  set plates(v) { localStorage.setItem("mp_plates", JSON.stringify(v)); },
};

let setupState = {};
let currentPlayerId = null;
let selectedAlbum = null;

/* ============================================================
   Screen routing
   ============================================================ */

function showScreen(id) {
  document.querySelectorAll(".screen").forEach((s) => s.classList.remove("active"));
  document.getElementById(id).classList.add("active");
  window.scrollTo(0, 0);
}

/* ============================================================
   Home
   ============================================================ */

function renderHome() {
  const players = store.players;
  const list = document.getElementById("player-list");
  document.getElementById("no-players").classList.toggle("hidden", players.length > 0);
  list.innerHTML = players
    .map(
      (p) => `
      <div class="player-card" onclick="openPlayer('${p.id}')">
        <div class="p-icon">📀</div>
        <div class="p-info">
          <b>${p.name}</b>
          <div class="p-status muted"><span class="status-dot"></span>Online · ${p.wifi}</div>
        </div>
        <span class="muted">›</span>
      </div>`
    )
    .join("");
}

/* ============================================================
   Setup wizard
   ============================================================ */

const WIZARD_STEPS = ["step-connect", "step-name", "step-wifi", "step-spotify", "step-device", "step-send"];
let wizardIndex = 0;

function startSetup() {
  setupState = {
    readerId: null,
    name: "",
    wifi: null,
    wifiPass: "",
    spotify: null,
    deviceMode: null,
    fixedDevice: null,
  };
  wizardIndex = 0;
  showWizardStep(0);
  showScreen("screen-setup");
  runBleScan();
}

function showWizardStep(i) {
  wizardIndex = i;
  WIZARD_STEPS.forEach((id, n) =>
    document.getElementById(id).classList.toggle("active", n === i)
  );
  const label = document.getElementById("wizard-step-label");
  const fill = document.getElementById("wizard-bar-fill");
  const footer = document.getElementById("setup-footer");
  if (i < 5) {
    label.textContent = `Step ${i + 1} of 5`;
    fill.style.width = `${((i + 1) / 5) * 100}%`;
    footer.style.display = "";
  } else {
    fill.style.width = "100%";
    label.textContent = "";
    footer.style.display = "none";
  }
  updateNextButton();
}

function setupBack() {
  if (wizardIndex === 0 || wizardIndex === 5) {
    showScreen("screen-home");
  } else {
    showWizardStep(wizardIndex - 1);
  }
}

function updateNextButton() {
  const btn = document.getElementById("btn-setup-next");
  let ok = false;
  switch (WIZARD_STEPS[wizardIndex]) {
    case "step-connect": ok = !!setupState.readerId; break;
    case "step-name":    ok = document.getElementById("input-name").value.trim().length > 0; break;
    case "step-wifi":
      ok = !!setupState.wifi &&
        (!setupState.wifi.secured || document.getElementById("input-wifi-pass").value.length > 0);
      break;
    case "step-spotify": ok = !!setupState.spotify; break;
    case "step-device":
      ok = setupState.deviceMode === "active" ||
        (setupState.deviceMode === "fixed" && !!setupState.fixedDevice);
      break;
  }
  btn.disabled = !ok;
}

function setupNext() {
  const step = WIZARD_STEPS[wizardIndex];
  if (step === "step-name") {
    setupState.name = document.getElementById("input-name").value.trim();
  }
  if (step === "step-wifi") {
    setupState.wifiPass = document.getElementById("input-wifi-pass").value;
  }
  if (step === "step-wifi") loadSpotifyStep();
  if (step === "step-device") { runSendConfig(); showWizardStep(5); return; }
  showWizardStep(wizardIndex + 1);
  if (WIZARD_STEPS[wizardIndex] === "step-wifi" && !setupState.wifiScanned) runWifiScan();
}

/* --- step 1: BLE scan --- */
async function runBleScan() {
  const scanner = document.getElementById("ble-scanner");
  const results = document.getElementById("ble-results");
  scanner.classList.remove("hidden");
  results.classList.add("hidden");
  results.innerHTML = "";

  const readers = await MockAPI.ble.scanForReaders();
  scanner.classList.add("hidden");
  results.classList.remove("hidden");
  results.innerHTML = readers
    .map(
      (r) => `
      <div class="device-result" id="reader-${r.id}" onclick="selectReader('${r.id}')">
        <span>📶</span>
        <b>${r.name}</b>
        <span class="signal">${r.rssi} dBm</span>
      </div>`
    )
    .join("");
}

function selectReader(id) {
  setupState.readerId = id;
  document.querySelectorAll(".device-result").forEach((el) =>
    el.classList.toggle("selected", el.id === `reader-${id}`)
  );
  updateNextButton();
}

/* --- step 2: name --- */
function fillName(name) {
  document.getElementById("input-name").value = name;
  updateNextButton();
}

/* --- step 3: wifi --- */
async function runWifiScan() {
  setupState.wifiScanned = true;
  const list = document.getElementById("wifi-list");
  const networks = await MockAPI.esp32.scanWifi();
  list.innerHTML = networks
    .map((n, i) => {
      return `
      <div class="wifi-row" id="wifi-${i}" onclick='selectWifi(${i}, ${JSON.stringify(JSON.stringify(n))})'>
        <b>${n.ssid}</b>
        ${n.secured ? '<span class="lock">🔒</span>' : ""}
        <span class="bars">${"▂▄▆▇".slice(3 - n.strength)}</span>
      </div>`;
    })
    .join("");
}

function selectWifi(i, json) {
  const network = JSON.parse(json);
  setupState.wifi = network;
  document.querySelectorAll(".wifi-row").forEach((el, n) =>
    el.classList.toggle("selected", n === i)
  );
  const passField = document.getElementById("wifi-password-field");
  passField.style.display = network.secured ? "" : "none";
  document.getElementById("wifi-chosen-name").textContent = network.ssid;
  document.getElementById("input-wifi-pass").value = "";
  updateNextButton();
}

function togglePassword() {
  const input = document.getElementById("input-wifi-pass");
  input.type = input.type === "password" ? "text" : "password";
}

/* --- step 4: spotify --- */
function loadSpotifyStep() {
  // reset visual state each time we arrive
  if (!setupState.spotify) {
    document.getElementById("spotify-disconnected").classList.remove("hidden");
    document.getElementById("spotify-connecting").classList.add("hidden");
    document.getElementById("spotify-connected").classList.add("hidden");
  }
}

async function mockSpotifyAuth() {
  document.getElementById("spotify-disconnected").classList.add("hidden");
  document.getElementById("spotify-connecting").classList.remove("hidden");

  const auth = await MockAPI.spotify.authorize();
  setupState.spotify = auth;

  document.getElementById("spotify-connecting").classList.add("hidden");
  document.getElementById("spotify-connected").classList.remove("hidden");
  document.getElementById("spotify-user-name").textContent = auth.user;
  document.getElementById("token-snippet").textContent = auth.refreshToken;
  updateNextButton();
}

/* --- step 5: playback device --- */
async function selectDeviceMode(mode) {
  setupState.deviceMode = mode;
  document.getElementById("mode-active").classList.toggle("selected", mode === "active");
  document.getElementById("mode-fixed").classList.toggle("selected", mode === "fixed");

  const listEl = document.getElementById("fixed-device-list");
  if (mode === "fixed") {
    listEl.classList.remove("hidden");
    if (!listEl.dataset.loaded) {
      listEl.innerHTML = '<div class="spinner-row"><div class="spinner"></div><span class="muted">Fetching your Spotify devices…</span></div>';
      const devices = await MockAPI.spotify.getDevices();
      listEl.dataset.loaded = "1";
      listEl.innerHTML = devices
        .map(
          (d) => `
          <div class="device-row" id="sdev-${d.id}" onclick="selectFixedDevice('${d.id}', '${d.name.replace(/'/g, "\\'")}')">
            <span class="d-type">${d.type}</span>
            <b>${d.name}</b>
            ${d.active ? '<span class="d-active">● active now</span>' : ""}
          </div>`
        )
        .join("");
    }
  } else {
    listEl.classList.add("hidden");
    setupState.fixedDevice = null;
  }
  updateNextButton();
}

function selectFixedDevice(id, name) {
  setupState.fixedDevice = { id, name };
  document.querySelectorAll(".device-row").forEach((el) =>
    el.classList.toggle("selected", el.id === `sdev-${id}`)
  );
  updateNextButton();
}

/* --- step 6: send config --- */
let sendingConfig = false;
async function runSendConfig() {
  if (sendingConfig) return;
  sendingConfig = true;
  const items = document.querySelectorAll("#send-checklist li");
  items.forEach((li) => li.classList.remove("doing", "done"));
  document.getElementById("send-done").classList.add("hidden");
  document.getElementById("send-checklist").classList.remove("hidden");
  document.getElementById("send-title").textContent = "Sending to reader…";

  await MockAPI.esp32.sendConfig({
    name: setupState.name,
    ssid: setupState.wifi.ssid,
    password: setupState.wifiPass,
    spotifyRefreshToken: setupState.spotify.refreshToken,
    deviceMode: setupState.deviceMode,
    fixedDeviceId: setupState.fixedDevice?.id ?? null,
  });

  for (const li of items) {
    li.classList.add("doing");
    await delay(1100);
    li.classList.remove("doing");
    li.classList.add("done");
  }

  // save player (replace any existing entry for this reader)
  const players = store.players.filter((p) => p.id !== setupState.readerId);
  players.push({
    id: setupState.readerId,
    name: setupState.name,
    wifi: setupState.wifi.ssid,
    spotifyUser: setupState.spotify.user,
    deviceMode: setupState.deviceMode,
    fixedDevice: setupState.fixedDevice,
  });
  store.players = players;

  document.getElementById("send-title").textContent = "All set";
  document.getElementById("send-checklist").classList.add("hidden");
  document.getElementById("send-done").classList.remove("hidden");
  document.getElementById("send-done-name").textContent = setupState.name;
  sendingConfig = false;
}

function finishSetup() {
  renderHome();
  showScreen("screen-home");
}

/* ============================================================
   Player detail
   ============================================================ */

function openPlayer(id) {
  const player = store.players.find((p) => p.id === id);
  if (!player) return;
  currentPlayerId = id;

  document.getElementById("player-detail-name").textContent = player.name;
  document.getElementById("detail-wifi").textContent = player.wifi;
  document.getElementById("detail-spotify").textContent = "@" + player.spotifyUser;
  document.getElementById("detail-device").textContent =
    player.deviceMode === "fixed" ? player.fixedDevice.name : "Active device";

  renderPlates();
  renderNowPlaying(null);
  showScreen("screen-player");
}

function deletePlayer() {
  const player = store.players.find((p) => p.id === currentPlayerId);
  if (!confirm(`Remove "${player.name}"? The reader will need to be set up again.`)) return;
  store.players = store.players.filter((p) => p.id !== currentPlayerId);
  renderHome();
  showScreen("screen-home");
}

/* --- edit playback device (bottom sheet) --- */
let sheetState = { mode: null, fixedDevice: null };

async function openDeviceSheet() {
  const player = store.players.find((p) => p.id === currentPlayerId);
  if (!player) return;
  sheetState = {
    mode: player.deviceMode,
    fixedDevice: player.fixedDevice || null,
  };

  const listEl = document.getElementById("sheet-device-list");
  listEl.dataset.loaded = "";
  paintSheetMode();

  document.getElementById("device-sheet").classList.add("open");

  // if already fixed, load the device list so the current choice shows
  if (sheetState.mode === "fixed") await loadSheetDevices();
}

function paintSheetMode() {
  document.getElementById("sheet-mode-active").classList.toggle("selected", sheetState.mode === "active");
  document.getElementById("sheet-mode-fixed").classList.toggle("selected", sheetState.mode === "fixed");
  document.getElementById("sheet-device-list").classList.toggle("hidden", sheetState.mode !== "fixed");
  updateSheetSave();
}

async function sheetSelectMode(mode) {
  sheetState.mode = mode;
  if (mode === "active") sheetState.fixedDevice = null;
  paintSheetMode();
  if (mode === "fixed") await loadSheetDevices();
}

async function loadSheetDevices() {
  const listEl = document.getElementById("sheet-device-list");
  if (listEl.dataset.loaded) { paintSheetDevices(); return; }
  listEl.innerHTML = '<div class="spinner-row"><div class="spinner"></div><span class="muted">Fetching your Spotify devices…</span></div>';
  const devices = await MockAPI.spotify.getDevices();
  listEl.dataset.loaded = "1";
  listEl._devices = devices;
  paintSheetDevices();
}

function paintSheetDevices() {
  const listEl = document.getElementById("sheet-device-list");
  const devices = listEl._devices || [];
  listEl.innerHTML = devices
    .map(
      (d) => `
      <div class="device-row ${sheetState.fixedDevice?.id === d.id ? "selected" : ""}"
           id="shdev-${d.id}" onclick="sheetSelectDevice('${d.id}', '${d.name.replace(/'/g, "\\'")}')">
        <span class="d-type">${d.type}</span>
        <b>${d.name}</b>
        ${d.active ? '<span class="d-active">● active now</span>' : ""}
      </div>`
    )
    .join("");
  updateSheetSave();
}

function sheetSelectDevice(id, name) {
  sheetState.fixedDevice = { id, name };
  paintSheetDevices();
}

function updateSheetSave() {
  const ok = sheetState.mode === "active" ||
    (sheetState.mode === "fixed" && !!sheetState.fixedDevice);
  document.getElementById("sheet-save").disabled = !ok;
}

async function saveDeviceSheet() {
  const players = store.players;
  const player = players.find((p) => p.id === currentPlayerId);
  if (!player) return;

  player.deviceMode = sheetState.mode;
  player.fixedDevice = sheetState.mode === "fixed" ? sheetState.fixedDevice : null;
  store.players = players;

  // real version: POST /api/config/device to the reader
  await MockAPI.esp32.sendConfig({ deviceMode: player.deviceMode, fixedDeviceId: player.fixedDevice?.id ?? null });

  document.getElementById("detail-device").textContent =
    player.deviceMode === "fixed" ? player.fixedDevice.name : "Active device";
  closeDeviceSheet();
}

function closeDeviceSheet() {
  document.getElementById("device-sheet").classList.remove("open");
}

function onSheetBackdrop(e) {
  if (e.target.id === "device-sheet") closeDeviceSheet();
}

function renderNowPlaying(album) {
  const art = document.getElementById("np-art");
  const status = document.getElementById("np-status");
  if (album) {
    art.style.background = album.color;
    art.textContent = "";
    document.getElementById("np-track").textContent = album.name;
    document.getElementById("np-artist").textContent = album.artist;
    status.innerHTML = '<div class="eq"><span></span><span></span><span></span></div>';
  } else {
    art.style.background = "";
    art.textContent = "♪";
    document.getElementById("np-track").textContent = "Nothing playing";
    document.getElementById("np-artist").textContent = "Place a plate on the reader";
    status.innerHTML = "";
  }
}

function renderPlates() {
  const plates = store.plates;
  const list = document.getElementById("plate-list");
  if (plates.length === 0) {
    list.innerHTML = '<p class="muted small" style="padding:12px 0">No plates written yet.</p>';
    return;
  }
  list.innerHTML = plates
    .map(
      (p, i) => `
      <div class="plate-card" onclick="mockTapPlate(${i})" title="Tap to simulate placing this plate on the reader">
        <div class="album-art" style="background:${p.color}">${initials(p.name)}</div>
        <div style="min-width:0">
          <b>${p.name}</b>
          <div class="muted">${p.artist}</div>
        </div>
        <span class="muted" style="margin-left:auto">▶</span>
      </div>`
    )
    .join("");
}

// Tapping a plate in the list simulates dropping it on the reader
function mockTapPlate(i) {
  renderNowPlaying(store.plates[i]);
}

function initials(name) {
  return name.split(" ").filter(Boolean).slice(0, 2).map((w) => w[0]).join("").toUpperCase();
}

/* ============================================================
   Write plate
   ============================================================ */

let searchTimer = null;

function onAlbumSearch() {
  clearTimeout(searchTimer);
  const q = document.getElementById("input-album-search").value.trim();
  const results = document.getElementById("album-results");
  if (!q) {
    results.innerHTML = '<p class="muted center" style="padding:32px 0">Search for an album to put on this plate.</p>';
    return;
  }
  searchTimer = setTimeout(async () => {
    const albums = await MockAPI.spotify.searchAlbums(q);
    if (albums.length === 0) {
      results.innerHTML = '<p class="muted center" style="padding:32px 0">No albums found.</p>';
      return;
    }
    results.innerHTML = albums
      .map(
        (a, i) => `
        <div class="album-row" onclick="chooseAlbum('${a.id}')">
          <div class="album-art" style="background:${a.color}">${initials(a.name)}</div>
          <div class="a-info">
            <div class="a-name">${a.name}</div>
            <div class="muted small">${a.artist}</div>
          </div>
        </div>`
      )
      .join("");
  }, 300);
}

async function chooseAlbum(id) {
  selectedAlbum = MOCK_ALBUMS.find((a) => a.id === id);

  document.getElementById("write-search-phase").classList.add("hidden");
  document.getElementById("write-confirm-phase").classList.remove("hidden");
  document.getElementById("write-waiting").classList.remove("hidden");
  document.getElementById("write-success").classList.add("hidden");

  const art = document.getElementById("write-art");
  art.style.background = selectedAlbum.color;
  art.textContent = initials(selectedAlbum.name);
  document.getElementById("write-album-name").textContent = selectedAlbum.name;
  document.getElementById("write-album-artist").textContent = selectedAlbum.artist;
  document.getElementById("write-album-id").textContent = selectedAlbum.id;

  await MockAPI.esp32.writeTag(selectedAlbum.id);

  // record the plate (mock convenience — real system needs no DB)
  const plates = store.plates;
  if (!plates.some((p) => p.id === selectedAlbum.id)) {
    plates.push(selectedAlbum);
    store.plates = plates;
  }
  renderPlates();

  document.getElementById("write-waiting").classList.add("hidden");
  document.getElementById("write-success").classList.remove("hidden");
}

function resetWriteFlow() {
  selectedAlbum = null;
  document.getElementById("write-confirm-phase").classList.add("hidden");
  document.getElementById("write-search-phase").classList.remove("hidden");
  document.getElementById("input-album-search").value = "";
  onAlbumSearch();
}

/* ============================================================
   Init
   ============================================================ */

document.getElementById("input-name").addEventListener("input", updateNextButton);
document.getElementById("input-wifi-pass").addEventListener("input", updateNextButton);

renderHome();
