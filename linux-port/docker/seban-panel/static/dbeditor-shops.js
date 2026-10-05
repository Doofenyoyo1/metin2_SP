// MT2009_PLUS_DB_EDITOR_V1: the database editor's "Sklepy NPC" and
// "Ulepszanie u Kowala" pages (templates/dbeditor/shops*, refine*). Forms
// marked .dbs-form get: the item picker (VNUM or name -> name and icon, from
// /db/api/items) on every .dbs-item input, "add a line" from the shop's
// <template>, the line price recomputed while typing, the quick ×N buttons
// and confirmations. Everything also works without it - the server checks
// and computes again. Re-run safely by ajax-forms.js (an IIFE, flags per form).
(() => {
  const fmt = (n) => Math.round(n).toLocaleString('pl-PL');

  function picker(form) {
    const api = form.dataset.api;
    const list = form.querySelector('#dbs-items') || document.getElementById('dbs-items');
    if (!api) return;
    let timer = null;
    form.addEventListener('input', ev => {
      const input = ev.target;
      if (!input.classList || !input.classList.contains('dbs-item')) return;
      const q = input.value.trim();
      const holder = input.closest('.dbr-mat') || input.closest('tr') || input.parentElement;
      const cell = holder ? holder.querySelector('.dbs-name') : null;
      const iconCell = holder ? holder.querySelector('.dbr-mat-icon') : null;
      clearTimeout(timer);
      if (!q) { if (cell) cell.textContent = ''; if (iconCell) iconCell.innerHTML = ''; return; }
      if (q.length < 2 && !/^\d+$/.test(q)) return;
      timer = setTimeout(() => fetch(api + '?q=' + encodeURIComponent(q), {credentials: 'same-origin'})
        .then(r => r.json()).then(d => {
          const items = d.items || [];
          if (list) {
            list.innerHTML = '';
            items.forEach(it => { const o = document.createElement('option'); o.value = it.vnum; o.label = it.vnum + ' – ' + it.name; o.textContent = o.label; list.appendChild(o); });
          }
          const hit = items.find(it => String(it.vnum) === q);
          if (cell) {
            cell.innerHTML = '';
            const span = document.createElement('span');
            if (hit) { span.textContent = hit.name; }
            else if (/^\d+$/.test(q)) { span.className = 'dbe-error'; span.textContent = 'NIEZNANY PRZEDMIOT'; }
            else { span.textContent = 'wybierz z listy (VNUM)'; }
            cell.appendChild(span);
          }
          if (iconCell) {
            iconCell.innerHTML = '';
            if (hit && hit.icon) { const img = document.createElement('img'); img.src = hit.icon; img.alt = ''; iconCell.appendChild(img); }
          }
          if (holder && holder.classList.contains('dbr-mat')) holder.classList.toggle('is-empty', !q);
        }).catch(() => {}), 250);
    });
  }

  function shop(form) {
    const add = form.querySelector('.dbs-add');
    const template = form.querySelector('template.dbs-new-template');
    const counter = form.querySelector('.dbs-new-n');
    const body = form.querySelector('tbody.dbs-new');
    if (add && template && counter && body) {
      add.addEventListener('click', () => {
        const j = parseInt(counter.value, 10) || 0;
        counter.value = j + 1;
        const holder = document.createElement('tbody');
        holder.innerHTML = template.innerHTML.replace(/__j__/g, String(j));
        const tr = holder.firstElementChild;
        body.appendChild(tr);
        const input = tr.querySelector('.dbs-item'); if (input) input.focus();
      });
    }
    // The line price: the item's price (its first line holds the input) × count.
    form.addEventListener('input', ev => {
      const t = ev.target;
      if (!t.classList || !(t.classList.contains('dbs-count') || t.classList.contains('dbs-gold'))) return;
      form.querySelectorAll('table.dbs-goods > tbody:first-of-type > tr').forEach(tr => {
        const vnum = (tr.querySelector('input[name^="item_"]') || {}).value;
        const gold = form.querySelector('input[name="gold_' + vnum + '"]');
        const count = tr.querySelector('.dbs-count');
        const out = tr.querySelector('.dbs-price');
        if (!gold || !count || !out) return;
        const g = parseInt(gold.value, 10), c = parseInt(count.value, 10);
        if (isFinite(g) && isFinite(c)) out.textContent = fmt(g * c).replace(/ /g, ' ') + ' Yang';
      });
    });
    form.querySelectorAll('input[name^="del_"]').forEach(box => box.addEventListener('change', () => {
      const tr = box.closest('tr'); if (tr) tr.classList.toggle('dbs-del', box.checked);
    }));
  }

  function refine(form) {
    const check = () => {
      form.querySelectorAll('.dbr-prob').forEach(i => { const v = parseInt(i.value, 10); i.classList.toggle('is-bad', !(v >= 1 && v <= 100)); });
      form.querySelectorAll('.dbr-cost').forEach(i => { const v = parseInt(i.value, 10); i.classList.toggle('is-warn', v > parseInt(i.dataset.guild || '0', 10)); });
    };
    form.addEventListener('input', check);
    check();
  }

  function init() {
    document.querySelectorAll('form.dbs-form').forEach(form => {
      if (form.dataset.dbsReady) return;
      form.dataset.dbsReady = '1';
      picker(form);
      if (form.querySelector('table.dbs-goods')) shop(form);
      if (form.classList.contains('dbr-form')) refine(form);
    });
    document.querySelectorAll('form.dbs-mass').forEach(form => {
      if (form.dataset.dbsReady) return;
      form.dataset.dbsReady = '1';
      form.querySelectorAll('[data-factor]').forEach(btn => btn.addEventListener('click', () => {
        const f = form.querySelector('input[name=factor]'); if (f) f.value = btn.dataset.factor;
      }));
      form.querySelectorAll('[data-confirm]').forEach(btn => btn.addEventListener('click', ev => {
        if (!window.confirm(btn.dataset.confirm)) ev.preventDefault();
      }));
    });
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init); else init();
})();
