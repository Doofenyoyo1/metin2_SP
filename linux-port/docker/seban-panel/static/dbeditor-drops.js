// MT2009_PLUS_DB_EDITOR_V1: the database editor's Drop and Szkatułki pages.
// Forms marked .dbd-form get: "add a line" from their <template>, the item
// picker (VNUM or name -> name and icon, from /db/api/items), the chance of
// every line recomputed while typing, a strike-through for deleted lines,
// the quick ×N buttons and confirmations. Everything also works without it -
// the server checks and computes again.
(() => {
  const fmt = (v) => {
    if (!isFinite(v) || v <= 0) return '0%';
    if (v >= 100) return '100%';
    return String(+v.toPrecision(4)).replace('.', ',') + '%';
  };
  const chance = (v) => {
    if (!isFinite(v) || v <= 0) return '0% (nigdy)';
    if (v >= 100) return '100% (zawsze)';
    const n = 100 / v;
    return n < 1.5 ? fmt(v) : fmt(v) + ' (ok. 1 na ' + Math.round(n).toLocaleString('pl-PL') + ')';
  };
  const num = (v) => { const n = parseFloat(String(v || '').replace(',', '.')); return isFinite(n) && n > 0 ? n : 0; };

  function initForm(form) {
    if (form.dataset.dbdReady) return;
    form.dataset.dbdReady = '1';
    const mode = form.dataset.mode || 'pct';
    const body = form.querySelector('.dbd-rows');
    const counter = form.querySelector('.dbd-row-count');
    const template = form.querySelector('template.dbd-row-template');
    const api = form.dataset.api;
    const list = document.getElementById('dbd-items');
    const killDrop = form.querySelector('.dbd-killdrop');
    const typeSelect = form.querySelector('.dbd-type');

    function live(tr) {
      const del = tr.querySelector('.dbd-delete'), item = tr.querySelector('.dbd-item');
      tr.classList.toggle('dbd-del', !!(del && del.checked));
      return !!(item && item.value.trim()) && !(del && del.checked);
    }
    function recalc() {
      if (!body) return;
      const trs = [...body.querySelectorAll('tr')];
      let m = mode;
      if (typeSelect) m = typeSelect.value === 'pct' ? 'chest-pct' : 'chest-weight';
      let total = 0;
      trs.forEach(tr => { const w = tr.querySelector('.dbd-weight'); if (w && live(tr)) total += num(w.value); });
      const every = killDrop ? Math.max(1, parseInt(killDrop.value || '1', 10) || 1) : 1;
      trs.forEach(tr => {
        const out = tr.querySelector('.dbd-out');
        if (!out) return;
        if (!live(tr)) { out.textContent = ''; return; }
        let c = 0;
        if (m === 'pct') c = Math.min(100, num((tr.querySelector('.dbd-pct') || {}).value));
        else if (m === 'chest-pct') c = Math.min(100, num((tr.querySelector('.dbd-weight') || {}).value));
        else { const w = num((tr.querySelector('.dbd-weight') || {}).value); c = total ? 100 * w / total / (m === 'kill' ? every : 1) : 0; }
        out.textContent = chance(c);
        const bar = tr.querySelector('.dbd-bar'); if (bar) bar.style.width = Math.max(2, Math.min(100, c) * 1.2) + 'px';
      });
    }
    form.addEventListener('input', recalc);
    form.addEventListener('change', recalc);

    const add = form.querySelector('.dbd-add');
    if (add && template && counter && body) {
      add.addEventListener('click', () => {
        const i = parseInt(counter.value, 10) || 0;
        counter.value = i + 1;
        const holder = document.createElement('tbody');
        holder.innerHTML = template.innerHTML.replace(/__i__/g, String(i));
        const tr = holder.firstElementChild;
        body.appendChild(tr);
        const item = tr.querySelector('.dbd-item'); if (item) item.focus();
        recalc();
      });
    }

    let timer = null;
    if (body && api) {
      body.addEventListener('input', ev => {
        if (!ev.target.classList.contains('dbd-item')) return;
        const input = ev.target, q = input.value.trim().replace(/^s/i, '');
        const cell = input.closest('tr').querySelector('.dbd-name');
        clearTimeout(timer);
        if (q.length < 2 && !/^\d+$/.test(q)) return;
        timer = setTimeout(() => fetch(api + '?q=' + encodeURIComponent(q), {credentials: 'same-origin'})
          .then(r => r.json()).then(d => {
            const items = d.items || [];
            if (list) {
              list.innerHTML = '';
              items.forEach(it => { const o = document.createElement('option'); o.value = it.vnum; o.label = it.vnum + ' – ' + it.name; o.textContent = o.label; list.appendChild(o); });
            }
            if (!cell) return;
            const hit = items.find(it => String(it.vnum) === q);
            cell.innerHTML = '';
            if (hit) {
              if (hit.icon) { const img = document.createElement('img'); img.src = hit.icon; img.alt = ''; cell.appendChild(img); }
              const span = document.createElement('span'); span.textContent = hit.name; cell.appendChild(span);
            } else if (/^\d+$/.test(q)) {
              const span = document.createElement('span'); span.className = 'dbd-bad'; span.textContent = 'NIEZNANY PRZEDMIOT'; cell.appendChild(span);
            } else {
              const span = document.createElement('span'); span.className = 'muted'; span.textContent = 'wybierz z listy (VNUM)'; cell.appendChild(span);
            }
          }).catch(() => {}), 250);
      });
    }

    form.querySelectorAll('[data-factor]').forEach(btn => btn.addEventListener('click', () => {
      const f = form.querySelector('input[name=factor]'); if (f) f.value = btn.dataset.factor;
    }));
    form.querySelectorAll('[data-confirm]').forEach(btn => btn.addEventListener('click', ev => {
      if (!window.confirm(btn.dataset.confirm)) ev.preventDefault();
    }));
    const filter = form.querySelector('.dbd-filter');
    if (filter && body) {
      filter.addEventListener('input', () => {
        const q = filter.value.trim().toLowerCase();
        body.querySelectorAll('tr').forEach(tr => { tr.style.display = !q || tr.textContent.toLowerCase().includes(q) ||
          [...tr.querySelectorAll('input')].some(i => i.value.toLowerCase().includes(q)) ? '' : 'none'; });
      });
    }
    form.querySelectorAll('.dbd-all').forEach(box => box.addEventListener('change', () => {
      form.querySelectorAll(box.dataset.target).forEach(other => { other.checked = box.checked; });
    }));
    recalc();
  }

  function init() { document.querySelectorAll('form.dbd-form').forEach(initForm); }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init); else init();
})();
