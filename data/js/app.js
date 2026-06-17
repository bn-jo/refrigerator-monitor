// =====================================================================
//  app.js — shared helpers, tab routing, dashboard, system status
// =====================================================================
const API = {
  async get(path)        { const r = await fetch(path); if (r.status === 401) return redirectLogin(); return r.json(); },
  async post(path, body) { const r = await fetch(path, { method:'POST',
      headers:{'Content-Type':'application/json'}, body: body?JSON.stringify(body):undefined });
      if (r.status === 401) return redirectLogin(); return r; }
};
function redirectLogin(){ location.href='/login.html'; throw new Error('unauth'); }
const T = (k) => window.I18N ? window.I18N.t(k) : k;

function toast(msg) {
  const t = document.getElementById('toast');
  t.textContent = msg; t.classList.add('show');
  clearTimeout(t._t); t._t = setTimeout(()=>t.classList.remove('show'), 2600);
}

// ---- Tab routing ----
document.querySelectorAll('nav.tabs button').forEach(btn => {
  btn.addEventListener('click', () => {
    document.querySelectorAll('nav.tabs button').forEach(b=>b.classList.remove('active'));
    document.querySelectorAll('.view').forEach(v=>v.classList.remove('active'));
    btn.classList.add('active');
    document.getElementById(btn.dataset.view).classList.add('active');
    if (btn.dataset.view === 'settings') window.Settings?.load();
    if (btn.dataset.view === 'logs')     window.Logs?.refresh();
  });
});

document.getElementById('logoutBtn').addEventListener('click', async () => {
  await API.post('/api/logout'); location.href='/login.html';
});

document.getElementById('restartBtn').addEventListener('click', async () => {
  if (!confirm(T('confirm.restart'))) return;
  await API.post('/api/restart');
  toast(T('toast.restarting'));
});

// ---- helpers ----
function tile(label, value) {
  return `<div class="card"><div class="kv">${label}</div><div><b>${value}</b></div></div>`;
}
function cap(s){ return s.charAt(0).toUpperCase()+s.slice(1); }

// Translate the default names ("Refrigerator N" / "Outdoor") for display;
// custom names the user typed are shown as-is.
function dispName(name) {
  const m = /^Refrigerator (\d+)$/.exec(name || '');
  if (m) return T('fridge.n').replace('{n}', m[1]);
  if (name === 'Outdoor') return T('live.outdoor');
  return name;
}

// Map an RSSI (dBm) to 0..4 signal bars.
function rssiBars(dbm) {
  if (dbm >= -55) return 4;
  if (dbm >= -65) return 3;
  if (dbm >= -75) return 2;
  if (dbm >= -85) return 1;
  return 0;
}

let lastStatus = null, lastTemps = null;

// ---- System info + connectivity ----
async function refreshStatus() {
  let s; try { s = await API.get('/api/status'); } catch(_) { return; }
  lastStatus = s;
  // The header title and browser tab reflect the configured device name.
  const dn = s.device || 'Refrigerator Monitor';
  document.getElementById('deviceName').textContent = dn;
  document.title = dn;

  const pNet = document.getElementById('pillNet');
  pNet.textContent = s.mode + (s.ethUp || s.wifi ? ' ✓' : ' ✕');
  pNet.className = 'pill ' + ((s.ethUp || s.wifi) ? 'ok' : 'bad');

  // WiFi signal pill (only meaningful when connected over WiFi).
  const pRssi = document.getElementById('pillRssi');
  if (s.wifi && typeof s.rssi === 'number' && s.rssi !== 0) {
    const bars = rssiBars(s.rssi);
    pRssi.hidden = false;
    pRssi.querySelector('.bars').dataset.level = bars;
    document.getElementById('rssiTxt').textContent = s.rssi + ' dBm';
    pRssi.className = 'pill signal ' + (bars >= 2 ? 'ok' : 'bad');
  } else {
    pRssi.hidden = true;
  }

  const pInet = document.getElementById('pillInet');
  pInet.textContent = T('sys.internet') + ' ' + (s.internet ? '✓' : '✕');
  pInet.className = 'pill ' + (s.internet ? 'ok' : 'bad');

  document.getElementById('pillTime').textContent = s.time;

  document.getElementById('sysinfo').innerHTML =
    tile(T('sys.connection'), s.mode + (s.wifi?T('val.wifiFallback'):'')) +
    tile(T('sys.ip'), s.ip) +
    tile(T('sys.internet'), s.internet ? T('val.connected') : T('val.unavailable')) +
    (s.wifi && s.rssi ? tile(T('sys.signal'), s.rssi + ' dBm') : '') +
    tile(T('sys.datetime'), s.time) +
    tile(T('sys.timesync'), s.timeSynced ? T('val.syncedNtp') : T('val.notSynced')) +
    tile(T('sys.uptime'), s.uptime) +
    tile(T('sys.firmware'), 'v' + s.fw) +
    tile(T('sys.records'), s.records) +
    tile(T('sys.storage'), s.storageOk ? (T('val.ok')+' ('+s.writeFails+')') : T('val.fault')) +
    tile(T('sys.freemem'), (s.heapFree/1024|0) + ' KB');

  const acq = document.getElementById('acqStatus');
  acq.textContent = T('acq.prefix') + (s.dataAcqOk ? T('acq.ok') : T('acq.degraded')) +
    (s.alarmActive ? T('acq.alarmActive') : '');
  acq.style.color = s.alarmActive ? 'var(--red)' : (s.dataAcqOk ? 'var(--muted)' : 'var(--yellow)');
}

// ---- Live temperatures (fridges in grid, outdoor in header pill) ----
async function refreshTemps() {
  let d; try { d = await API.get('/api/temperatures'); } catch(_) { return; }
  lastTemps = d;

  // Outdoor → header pill.
  const outdoor = d.sensors.find(s => s.outdoor);
  const pOut = document.getElementById('pillOutdoor');
  if (outdoor) {
    const ot = (outdoor.temp===null||outdoor.temp===undefined) ? '--' : outdoor.temp.toFixed(1)+'°C';
    pOut.textContent = '🌡️ ' + dispName(outdoor.name || 'Outdoor') + ' ' + ot;
    pOut.className = 'pill outdoor ' + (outdoor.sensor === 'ok' ? 'ok' : 'bad');
  }

  // Fridges only → grid.
  const fridges = d.sensors.filter(s => !s.outdoor);
  const box = document.getElementById('fridges');
  box.innerHTML = fridges.map(s => {
    const t = (s.temp===null||s.temp===undefined) ? '--' : s.temp.toFixed(1);
    const cls = s.status || 'normal';
    const sensorBad = s.sensor !== 'ok';
    const stateCls = sensorBad ? s.sensor : cls;
    const last = s.lastValid ? new Date(s.lastValid*1000).toLocaleTimeString() : '—';
    const statusLabel = sensorBad ? T('sensor.'+s.sensor) : T('status.'+cls);
    return `<div class="card fridge ${stateCls}">
      <h3>${dispName(s.name)}</h3>
      <div class="temp">${t}<span class="unit">°C</span></div>
      <div class="kv"><span class="dot ${stateCls}"></span>${statusLabel}
        ${(s.threshold!==undefined)?` · ${T('live.thr')} ${s.threshold}°C`:''}</div>
      <div class="kv">${T('live.sensor')}: ${T('sensor.'+s.sensor)} · ${T('live.updated')} ${last}</div>
    </div>`;
  }).join('');
}

async function tick() { await refreshStatus(); await refreshTemps(); }

// Re-render immediately when language changes (uses cached last responses).
window.onLangChange = () => {
  if (lastStatus) refreshStatus();
  if (lastTemps) refreshTemps();
  window.Charts && window.Charts.load();
};

tick();
setInterval(tick, 10000);   // dashboard auto-refresh every 10 s
