// =====================================================================
//  settings.js — load/save configuration
// =====================================================================
window.Settings = (function () {
  const SMTP_PRESETS = {
    gmail:   ['smtp.gmail.com', 465],
    outlook: ['smtp-mail.outlook.com', 587],
    brevo:   ['smtp-relay.brevo.com', 587]
  };
  function detectProvider(host) {
    for (const k in SMTP_PRESETS) if (SMTP_PRESETS[k][0] === host) return k;
    return host ? 'custom' : 'gmail';
  }
  function toggleCustom() {
    document.getElementById('emCustom').hidden =
      document.getElementById('emProvider').value !== 'custom';
  }

  function load() {
    API.get('/api/settings').then(s => {
      const tt = window.I18N ? window.I18N.t : (k)=>k;

      // Each refrigerator is its own block so its threshold is unmistakable.
      const box = document.getElementById('fridgeSettings');
      box.innerHTML = s.fridges.map((f, i) => `
        <div class="fridge-setting">
          <div class="fridge-setting-h">🧊 #${i+1}</div>
          <div class="row" style="align-items:flex-end">
            <label>${tt('settings.fridgeName')} <input data-f="name" data-i="${i}" value="${esc(f.name)}"></label>
            <label style="flex:0 0 130px">${tt('settings.threshold')} <input data-f="thr" data-i="${i}" type="number" step="0.5" value="${f.thr}"></label>
            <label style="flex:0 0 110px">${tt('settings.enabled')}
              <select data-f="en" data-i="${i}">
                <option value="1" ${f.en?'selected':''}>${tt('settings.yes')}</option>
                <option value="0" ${!f.en?'selected':''}>${tt('settings.no')}</option>
              </select></label>
          </div>
        </div>`).join('');

      document.getElementById('setDeviceName').value  = s.deviceName || '';
      document.getElementById('setOutdoorName').value = s.outdoorName || '';
      document.getElementById('setTz').value          = s.tz || '';

      const e = s.email || {};
      document.getElementById('emEnabled').checked   = !!e.enabled;
      document.getElementById('emProvider').value    = detectProvider(e.host || '');
      document.getElementById('emHost').value        = e.host || '';
      document.getElementById('emPort').value        = e.port || 465;
      document.getElementById('emUser').value        = e.user || '';
      document.getElementById('emPass').placeholder  = e.hasPass ? tt('settings.unchanged') : '';
      document.getElementById('emRecipient').value   = e.recipient || '';
      // From address only when it differs from the login (relay services).
      document.getElementById('emFrom').value =
        (e.sender && e.sender !== e.user) ? e.sender : '';
      toggleCustom();

      // User list with delete buttons (last account can't be removed).
      const ul = document.getElementById('userList');
      const names = s.users || [];
      ul.innerHTML = names.map(n => `
        <div class="userrow">
          <span>👤 ${esc(n)}</span>
          ${names.length>1?`<button class="btn btn-danger btn-sm" data-deluser="${esc(n)}">${tt('settings.delete')}</button>`:''}
        </div>`).join('');
      ul.querySelectorAll('[data-deluser]').forEach(b =>
        b.addEventListener('click', () => delUser(b.dataset.deluser)));
    });
  }
  function esc(s){ return (s||'').replace(/"/g,'&quot;'); }

  async function delUser(name) {
    if (!confirm(T('confirm.delUser').replace('{u}', name))) return;
    const r = await API.post('/api/settings', { delUser: name });
    const j = await r.json().catch(()=>({}));
    if (r.ok) { toast(T('toast.userRemoved')); load(); }
    else toast(T('toast.userFailed') + (j.error || r.status));
  }

  function collect() {
    const fridges = [];
    for (let i = 0; i < 5; i++) {
      fridges.push({
        name: q(`[data-f="name"][data-i="${i}"]`).value,
        thr:  parseFloat(q(`[data-f="thr"][data-i="${i}"]`).value),
        en:   q(`[data-f="en"][data-i="${i}"]`).value === '1'
      });
    }
    const prov = document.getElementById('emProvider').value;
    let host, port;
    if (prov === 'custom') {
      host = document.getElementById('emHost').value;
      port = parseInt(document.getElementById('emPort').value, 10);
    } else {
      [host, port] = SMTP_PRESETS[prov];
    }
    const user = document.getElementById('emUser').value;
    // From address: defaults to the login, but a relay (Custom) can override
    // it (e.g. SMTP2GO username differs from the address you send as).
    let sender = user;
    if (prov === 'custom') {
      const from = document.getElementById('emFrom').value.trim();
      if (from) sender = from;
    }
    const body = {
      fridges,
      deviceName:  document.getElementById('setDeviceName').value,
      outdoorName: document.getElementById('setOutdoorName').value,
      tz:          document.getElementById('setTz').value,
      email: {
        enabled:   document.getElementById('emEnabled').checked,
        host, port,
        user,
        sender,
        recipient: document.getElementById('emRecipient').value
      }
    };
    const pass = document.getElementById('emPass').value;
    if (pass) body.email.pass = pass;
    const np = document.getElementById('setPass').value;
    if (np) body.newPassword = np;
    return body;
  }
  function q(s){ return document.querySelector(s); }

  document.getElementById('saveSettings').addEventListener('click', async () => {
    const r = await API.post('/api/settings', collect());
    if (r.ok) { toast(T('toast.saved')); load(); }
    else { const e = await r.json().catch(()=>({})); toast(T('toast.saveFailed') + (e.error||r.status)); }
  });

  // Fill every per-fridge threshold input with one value and save immediately,
  // so "Apply to all" actually applies without a separate Save click.
  document.getElementById('applyAllThr').addEventListener('click', async () => {
    const v = document.getElementById('setAllThr').value;
    if (v === '') return;
    document.querySelectorAll('[data-f="thr"]').forEach(i => { i.value = v; });
    const r = await API.post('/api/settings', collect());
    if (r.ok) { toast(T('toast.saved')); load(); }
    else { const e = await r.json().catch(()=>({})); toast(T('toast.saveFailed') + (e.error||r.status)); }
  });

  document.getElementById('addUserBtn').addEventListener('click', async () => {
    const u = document.getElementById('newUser').value.trim();
    const p = document.getElementById('newUserPass').value;
    if (!u || !p) { toast(T('toast.userFailed') + 'user & pass'); return; }
    const r = await API.post('/api/settings', { addUser: { user: u, pass: p } });
    const j = await r.json().catch(()=>({}));
    if (r.ok) {
      toast(T('toast.userAdded'));
      document.getElementById('newUser').value = '';
      document.getElementById('newUserPass').value = '';
      load();
    } else toast(T('toast.userFailed') + (j.error || r.status));
  });

  document.getElementById('emProvider').addEventListener('change', toggleCustom);

  // Reset the form back to the saved (default) settings, discarding edits.
  document.getElementById('emReset').addEventListener('click', () => {
    load();
    toast(T('settings.reset'));
  });

  document.getElementById('emTest').addEventListener('click', async () => {
    toast(T('toast.sendingTest'));
    await API.post('/api/settings', collect());   // save current form first
    const r = await API.post('/api/email/test');
    const j = await r.json().catch(()=>({}));
    toast(j.ok ? T('toast.testSent') : T('toast.testFailed'));
  });

  return { load };
})();
