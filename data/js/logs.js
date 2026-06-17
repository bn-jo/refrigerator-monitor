// =====================================================================
//  logs.js — view / filter / clear / download event logs
// =====================================================================
window.Logs = (function () {
  function lineClass(l) {
    if (l.includes('[ERROR]')) return 'ERROR';
    if (l.includes('[WARN]'))  return 'WARN';
    return 'INFO';
  }
  async function refresh() {
    const lvl = document.getElementById('logLevel').value;
    const cat = document.getElementById('logCat').value.trim();
    let lines;
    try {
      lines = await API.get('/api/logs?level=' + encodeURIComponent(lvl) +
                            '&cat=' + encodeURIComponent(cat));
    } catch(_) { return; }
    const box = document.getElementById('logbox');
    if (!lines.length) { box.innerHTML = '<div class="muted">' + T('logs.none') + '</div>'; return; }
    box.innerHTML = lines.map(l =>
      `<div class="logline ${lineClass(l)}">${escapeHtml(l)}</div>`).join('');
    box.scrollTop = box.scrollHeight;
  }
  function escapeHtml(s){ return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'); }

  document.getElementById('logRefresh').addEventListener('click', refresh);
  document.getElementById('logLevel').addEventListener('change', refresh);
  document.getElementById('logCat').addEventListener('input', () => {
    clearTimeout(window.Logs._t); window.Logs._t = setTimeout(refresh, 300);
  });
  document.getElementById('logDownload').addEventListener('click', () => {
    window.location = '/api/logs/download';
  });
  document.getElementById('logClear').addEventListener('click', async () => {
    if (!confirm(T('confirm.clearLogs'))) return;
    await API.post('/api/logs/clear'); refresh();
  });

  return { refresh };
})();
