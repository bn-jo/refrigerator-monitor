// =====================================================================
//  charts.js — historical chart for the 5 refrigerators (Chart.js)
// =====================================================================
window.Charts = (function () {
  const COLORS = ['#60a5fa','#f87171','#34d399','#fbbf24','#c084fc'];
  let chart = null;
  let range = '24h';
  let loadedOnce = false;

  function build() {
    const ctx = document.getElementById('histChart');
    chart = new Chart(ctx, {
      type: 'line',
      data: { datasets: [] },
      options: {
        responsive: true, maintainAspectRatio: false,
        parsing: false, animation: false,
        interaction: { mode: 'nearest', axis: 'x', intersect: false },
        scales: {
          x: { type: 'time',
               time: { tooltipFormat: 'yyyy-MM-dd HH:mm' },
               ticks: { color: '#94a3b8', maxRotation: 0, autoSkip: true },
               grid: { color: '#1e293b' } },
          y: { title: { display: true, text: '°C', color: '#94a3b8' },
               ticks: { color: '#94a3b8' }, grid: { color: '#1e293b' } }
        },
        plugins: {
          legend: { labels: { color: '#e2e8f0', usePointStyle: true } },
          tooltip: { callbacks: { label: c => `${c.dataset.label}: ${c.parsed.y?.toFixed(1)} °C` } },
          zoom: {
            pan:  { enabled: true, mode: 'x' },
            zoom: { wheel: { enabled: true }, pinch: { enabled: true }, mode: 'x' }
          }
        }
      }
    });
  }

  async function load() {
    if (!chart) build();
    let names = [];
    try { const s = await API.get('/api/settings'); names = s.fridges.map(f=>f.name); } catch(_){}
    let d; try { d = await API.get('/api/history?range=' + range); } catch(_) { return; }

    const sets = [];
    for (let i = 0; i < 5; i++) {
      sets.push({
        label: (typeof dispName === 'function' ? dispName(names[i] || ('Refrigerator ' + (i+1))) : (names[i] || ('Refrigerator ' + (i+1)))),
        borderColor: COLORS[i], backgroundColor: COLORS[i],
        borderWidth: 2, pointRadius: 0, tension: 0.25, spanGaps: true,
        data: d.points.map(p => ({ x: p.t * 1000, y: p.v[i] }))
      });
    }
    chart.data.datasets = sets;
    chart.resetZoom?.();
    chart.update();
  }

  // Wire range buttons
  document.querySelectorAll('.range-btns button').forEach(b => {
    b.addEventListener('click', () => {
      document.querySelectorAll('.range-btns button').forEach(x=>x.classList.remove('active'));
      b.classList.add('active'); range = b.dataset.range; load();
    });
  });
  document.getElementById('resetZoom').addEventListener('click', ()=>chart?.resetZoom());

  // Fullscreen toggle
  document.getElementById('fsBtn').addEventListener('click', () => {
    document.getElementById('chartWrap').classList.toggle('fullscreen');
    setTimeout(()=>chart?.resize(), 50);
  });
  document.addEventListener('keydown', e => {
    if (e.key === 'Escape')
      document.getElementById('chartWrap').classList.remove('fullscreen');
  });

  // The chart lives on the dashboard (always visible), so load on startup
  // and refresh it on the regular dashboard cadence.
  document.addEventListener('DOMContentLoaded', () => { load(); setInterval(load, 60000); });

  return { load, onShow() { load(); } };
})();
