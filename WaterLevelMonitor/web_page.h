#pragma once

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="id">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Water Level Monitor</title>
<style>
:root{--bg:#0f172a;--card:#1e293b;--text:#e2e8f0;--muted:#94a3b8;--water:#38bdf8;--ok:#22c55e;--warn:#eab308;--bad:#ef4444}
*{box-sizing:border-box}
body{margin:0;font-family:system-ui,-apple-system,sans-serif;background:var(--bg);color:var(--text)}
main{max-width:900px;margin:0 auto;padding:16px}
h1{font-size:1.3rem;margin:8px 0 16px}
.grid{display:grid;gap:16px;grid-template-columns:1fr 1fr}
@media(max-width:640px){.grid{grid-template-columns:1fr}}
.card{background:var(--card);border-radius:12px;padding:16px}
.card h2{font-size:.85rem;text-transform:uppercase;letter-spacing:.05em;color:var(--muted);margin:0 0 12px}
.tank-wrap{display:flex;gap:20px;align-items:center}
.tank{position:relative;width:110px;height:200px;border:3px solid var(--muted);border-top:none;border-radius:0 0 14px 14px;overflow:hidden;flex:none}
.water{position:absolute;bottom:0;left:0;right:0;height:0;background:var(--water);transition:height .8s ease}
.pct{font-size:2.6rem;font-weight:700;font-variant-numeric:tabular-nums}
.badge{display:inline-block;padding:4px 10px;border-radius:999px;font-size:.8rem;font-weight:600;margin-top:6px;white-space:nowrap}
.LOW,.SENSOR_ERROR{background:var(--bad)}.MEDIUM{background:var(--warn);color:#111}.FULL{background:var(--ok);color:#111}
dl{display:grid;grid-template-columns:auto 1fr;gap:6px 12px;margin:0}
dt{color:var(--muted)}dd{margin:0;text-align:right;font-variant-numeric:tabular-nums}
.btns{display:flex;gap:8px;margin-top:12px}
button{flex:1;padding:10px;border:1px solid #334155;border-radius:8px;background:#0f172a;color:var(--text);font-size:.95rem;cursor:pointer}
button.active{background:var(--water);color:#0f172a;border-color:var(--water);font-weight:600}
form{display:grid;gap:10px}
label{display:flex;justify-content:space-between;align-items:center;gap:8px;font-size:.9rem}
input[type=number]{width:110px;padding:8px;border-radius:6px;border:1px solid #334155;background:#0f172a;color:var(--text)}
input[type=text],input[type=password]{width:60%;max-width:260px;padding:8px;border-radius:6px;border:1px solid #334155;background:#0f172a;color:var(--text)}
canvas{width:100%;height:160px}
#msg,#wmsg{font-size:.85rem;color:var(--muted);min-height:1.2em}
.list{list-style:none;margin:0 0 12px;padding:0;display:grid;gap:6px}
.list li{display:flex;justify-content:space-between;align-items:center;gap:8px;padding:8px 10px;border:1px solid #334155;border-radius:8px;font-size:.9rem}
.list li.pick{cursor:pointer}.list li.pick:hover{border-color:var(--water)}
.list small{color:var(--muted)}
.list button{flex:none;padding:4px 10px;font-size:.8rem}
#wstat{margin:0 0 12px}
</style>
</head>
<body>
<main>
<h1>Smart Water Level Monitor</h1>
<div class="grid">
  <section class="card">
    <h2>Level Air</h2>
    <div class="tank-wrap">
      <div class="tank"><div class="water" id="water"></div></div>
      <div>
        <div class="pct" id="pct">--%</div>
        <div id="liters">-- L</div>
        <span class="badge" id="status">--</span>
      </div>
    </div>
  </section>
  <section class="card">
    <h2>Detail</h2>
    <dl>
      <dt>Jarak sensor</dt><dd id="dist">--</dd>
      <dt>Tinggi air</dt><dd id="lvl">--</dd>
      <dt>Pompa</dt><dd id="pump">--</dd>
      <dt>Sinyal WiFi</dt><dd id="rssi">--</dd>
      <dt>IP</dt><dd id="ip">--</dd>
      <dt>Uptime</dt><dd id="up">--</dd>
    </dl>
    <div class="btns">
      <button data-mode="auto">Auto</button>
      <button data-mode="on">ON</button>
      <button data-mode="off">OFF</button>
    </div>
  </section>
  <section class="card" style="grid-column:1/-1">
    <h2>Riwayat (10 menit terakhir)</h2>
    <canvas id="chart"></canvas>
  </section>
  <section class="card" style="grid-column:1/-1">
    <h2>Pengaturan Tangki</h2>
    <form id="cfg">
      <label>Jarak sensor ke dasar tangki (cm)<input type="number" step="0.1" name="tank_height_cm"></label>
      <label>Jarak sensor ke air saat penuh (cm)<input type="number" step="0.1" name="full_distance_cm"></label>
      <label>Kapasitas tangki (liter)<input type="number" step="1" name="capacity_liters"></label>
      <label>Batas rendah / pompa ON (%)<input type="number" name="low_percent"></label>
      <label>Batas penuh / pompa OFF (%)<input type="number" name="high_percent"></label>
      <label>Buzzer alarm<input type="checkbox" name="buzzer"></label>
      <button type="submit">Simpan</button>
      <div id="msg"></div>
    </form>
  </section>
  <section class="card" style="grid-column:1/-1" id="wifiCard">
    <h2>WiFi</h2>
    <p id="wstat">--</p>
    <ul class="list" id="saved"></ul>
    <button type="button" id="scanBtn">Cari WiFi di sekitar</button>
    <ul class="list" id="nets" style="margin-top:12px"></ul>
    <form id="wifi">
      <label>Nama WiFi (SSID)<input type="text" name="ssid" maxlength="32" required></label>
      <label>Password<input type="password" name="password" maxlength="63"></label>
      <button type="submit">Simpan &amp; Hubungkan</button>
      <div id="wmsg"></div>
    </form>
  </section>
</div>
</main>
<script>
const $ = id => document.getElementById(id);
const LABEL = {LOW:'RENDAH',MEDIUM:'SEDANG',FULL:'PENUH',SENSOR_ERROR:'SENSOR ERROR'};
const points = [];
const MAX_POINTS = 300; // 300 x 2 detik = 10 menit

function render(s) {
  $('water').style.height = s.percent + '%';
  $('pct').textContent = s.sensor_ok ? s.percent.toFixed(0) + '%' : '--%';
  $('liters').textContent = s.sensor_ok ? s.liters + ' L' : '-- L';
  $('status').textContent = LABEL[s.status];
  $('status').className = 'badge ' + s.status;
  $('dist').textContent = s.sensor_ok ? s.distance_cm.toFixed(1) + ' cm' : 'error';
  $('lvl').textContent = s.level_cm.toFixed(1) + ' cm';
  $('pump').textContent = (s.pump_on ? 'ON' : 'OFF') + ' (' + s.pump_mode + ')';
  $('rssi').textContent = s.rssi ? s.rssi + ' dBm' : 'mode AP';
  $('ip').textContent = s.ip;
  const u = s.uptime_s;
  $('up').textContent = Math.floor(u / 3600) + 'j ' + Math.floor(u % 3600 / 60) + 'm';
  document.querySelectorAll('[data-mode]').forEach(b => b.classList.toggle('active', b.dataset.mode === s.pump_mode));
}

function drawChart() {
  const c = $('chart'), ctx = c.getContext('2d');
  const w = c.width = c.clientWidth * devicePixelRatio, h = c.height = c.clientHeight * devicePixelRatio;
  ctx.clearRect(0, 0, w, h);
  ctx.strokeStyle = '#334155'; ctx.lineWidth = 1;
  [0, 25, 50, 75, 100].forEach(p => { const y = h - p / 100 * h; ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(w, y); ctx.stroke(); });
  if (points.length < 2) return;
  ctx.strokeStyle = '#38bdf8'; ctx.lineWidth = 2 * devicePixelRatio; ctx.beginPath();
  points.forEach((v, i) => { const x = i / (MAX_POINTS - 1) * w, y = h - v / 100 * h; i ? ctx.lineTo(x, y) : ctx.moveTo(x, y); });
  ctx.stroke();
}

async function poll() {
  try {
    const s = await (await fetch('/api/status')).json();
    render(s);
    if (s.sensor_ok) { points.push(s.percent); if (points.length > MAX_POINTS) points.shift(); }
    drawChart();
  } catch (e) { $('status').textContent = 'OFFLINE'; $('status').className = 'badge LOW'; }
}

async function loadConfig() {
  const c = await (await fetch('/api/config')).json();
  const f = $('cfg');
  for (const k in c) {
    if (!f[k]) continue;
    if (f[k].type === 'checkbox') f[k].checked = c[k]; else f[k].value = c[k];
  }
}

$('cfg').addEventListener('submit', async e => {
  e.preventDefault();
  const f = e.target, body = new URLSearchParams();
  ['tank_height_cm','full_distance_cm','capacity_liters','low_percent','high_percent'].forEach(k => body.append(k, f[k].value));
  body.append('buzzer', f.buzzer.checked);
  const r = await fetch('/api/config', {method: 'POST', body});
  const j = await r.json();
  $('msg').textContent = r.ok ? 'Tersimpan.' : j.error;
});

document.querySelectorAll('[data-mode]').forEach(b => b.addEventListener('click', async () => {
  const r = await fetch('/api/pump', {method: 'POST', body: new URLSearchParams({mode: b.dataset.mode})});
  render(await r.json());
}));

const sleep = ms => new Promise(r => setTimeout(r, ms));
const getJson = async url => (await fetch(url)).json();

function item(text, sub, onClick, btnLabel, onBtn) {
  const li = document.createElement('li'), span = document.createElement('span');
  span.textContent = text;
  if (sub) { const s = document.createElement('small'); s.textContent = ' ' + sub; span.append(s); }
  li.append(span);
  if (onClick) { li.className = 'pick'; li.onclick = onClick; }
  if (btnLabel) { const b = document.createElement('button'); b.type = 'button'; b.textContent = btnLabel; b.onclick = onBtn; li.append(b); }
  return li;
}

async function loadWifi() {
  const w = await getJson('/api/wifi');
  $('wstat').textContent = w.connected
    ? 'Terhubung ke ' + w.ssid + ' (' + w.ip + ', ' + w.rssi + ' dBm)'
    : w.state === 'connecting' ? 'Menghubungkan ke ' + w.ssid + '...'
    : 'Tidak terhubung. Portal setup: ' + w.ap_ssid + ' (' + w.ap_ip + ')';
  $('saved').replaceChildren(...w.saved.map(s => item(s, w.connected && s === w.ssid ? '(aktif)' : '', null, 'Hapus', async () => {
    await fetch('/api/wifi/remove', {method: 'POST', body: new URLSearchParams({ssid: s})});
    loadWifi();
  })));
  return w;
}

$('scanBtn').onclick = async () => {
  const btn = $('scanBtn');
  btn.disabled = true; btn.textContent = 'Mencari...';
  try {
    await fetch('/api/wifi/scan', {method: 'POST'});
    for (let i = 0; i < 25; i++) {
      await sleep(1000);
      const r = await getJson('/api/wifi/scan');
      if (r.scanning) continue;
      $('nets').replaceChildren(...r.networks.map(n => item(n.ssid, n.rssi + ' dBm' + (n.secure ? '' : ' terbuka') + (n.saved ? ' - tersimpan' : ''), () => {
        $('wifi').ssid.value = n.ssid; $('wifi').password.focus();
      })));
      break;
    }
  } catch (e) {}
  btn.disabled = false; btn.textContent = 'Cari WiFi di sekitar';
};

$('wifi').addEventListener('submit', async e => {
  e.preventDefault();
  const f = e.target, ssid = f.ssid.value;
  const r = await fetch('/api/wifi/add', {method: 'POST', body: new URLSearchParams({ssid, password: f.password.value})});
  if (!r.ok) { $('wmsg').textContent = (await r.json()).error; return; }
  f.password.value = '';
  $('wmsg').textContent = 'Menghubungkan ke ' + ssid + '...';
  for (let i = 0; i < 25; i++) {
    await sleep(1000);
    try {
      const w = await loadWifi();
      if (w.connected && w.ssid === ssid) {
        $('wmsg').textContent = 'Berhasil! Sambungkan HP ke WiFi "' + ssid + '", lalu buka http://waterlevel.local atau http://' + w.ip;
        return;
      }
      if (w.state === 'idle') break;
    } catch (err) {} // koneksi ke portal bisa putus sebentar saat ESP32 pindah channel
  }
  $('wmsg').textContent = 'Gagal terhubung ke ' + ssid + '. Cek password dan pastikan WiFi 2.4 GHz.';
});

loadConfig();
loadWifi().then(w => { if (!w.connected) $('wifiCard').scrollIntoView(); });
setInterval(() => loadWifi().catch(() => {}), 10000);
poll();
setInterval(poll, 2000);
window.addEventListener('resize', drawChart);
</script>
</body>
</html>
)rawliteral";
