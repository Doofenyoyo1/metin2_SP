// MT2009_PLUS_DB_EDITOR_V1: the database editor's "Spawny potworów" pages
// (dbeditor/regen.py). Table filters, confirmations, a click on the map's
// picture fills the "Dodaj spawn" coordinates, and the monster/group search
// of that form. Everything also works without it - the forms carry plain
// type/number fields and the server checks every value again.
(() => {
  const fold = (s) => String(s || '').toLocaleLowerCase('pl-PL').normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/ł/g, 'l');

  document.addEventListener('click', (event) => {
    const button = event.target.closest('[data-confirm]');
    if (button && !window.confirm(button.dataset.confirm)) event.preventDefault();
  });

  document.querySelectorAll('[data-dbr-filter]').forEach((input) => {
    const rows = () => document.querySelectorAll(input.dataset.dbrFilter);
    input.addEventListener('input', () => {
      const words = fold(input.value).split(/\s+/).filter(Boolean);
      rows().forEach((tr) => {
        const hay = fold(tr.dataset.text || tr.textContent);
        tr.hidden = !words.every((w) => hay.includes(w));
      });
    });
  });

  const map = document.getElementById('dbr-map');
  const add = document.querySelector('.dbr-add form');
  if (map && add) {
    const pick = map.querySelector('.dbr-pick');
    map.addEventListener('click', (event) => {
      if (event.target.closest('.dbr-mk')) return;
      const box = map.getBoundingClientRect();
      const fx = (event.clientX - box.left) / box.width, fy = (event.clientY - box.top) / box.height;
      const x = Math.round(fx * Number(map.dataset.w)), y = Math.round(fy * Number(map.dataset.h));
      add.querySelector('.dbr-x').value = x;
      add.querySelector('.dbr-y').value = y;
      if (pick) { pick.hidden = false; pick.style.left = (fx * 100) + '%'; pick.style.top = (fy * 100) + '%'; }
      [add.querySelector('.dbr-x'), add.querySelector('.dbr-y')].forEach((el) => {
        el.animate([{ background: '#d6a74a66' }, { background: 'transparent' }], { duration: 900 });
      });
    });
  }

  if (add && add.dataset.search) {
    const input = add.querySelector('.dbr-search'), box = add.querySelector('.dbr-results');
    const type = add.querySelector('.dbr-type'), vnum = add.querySelector('.dbr-vnum'), chosen = add.querySelector('.dbr-chosen');
    let timer = null, asked = '';
    const choose = (kind, number, text) => {
      type.value = kind; vnum.value = number;
      chosen.textContent = 'Wybrano: ' + text; chosen.classList.add('set');
      box.hidden = true;
    };
    const item = (kind, number, text, note) => {
      const b = document.createElement('button');
      b.type = 'button';
      b.innerHTML = '<b></b> <small></small>';
      b.querySelector('b').textContent = text;
      b.querySelector('small').textContent = note;
      b.addEventListener('click', () => choose(kind, number, text + ' (' + note + ')'));
      return b;
    };
    const search = async () => {
      const q = input.value.trim();
      if (q === asked) return;
      asked = q;
      if (q.length < 2 && !/^\d+$/.test(q)) { box.hidden = true; return; }
      let data;
      try { data = await fetch(add.dataset.search + '?q=' + encodeURIComponent(q), { cache: 'no-store' }).then((r) => r.json()); }
      catch (e) { return; }
      if (q !== asked) return;
      box.innerHTML = '';
      (data.groups || []).forEach((g) => box.appendChild(item(g.type, g.vnum, (g.type === 'r' ? 'Losowa grupa: ' : 'Grupa: ') + g.text, 'nr ' + g.vnum)));
      (data.mobs || []).forEach((m) => box.appendChild(item('m', m.vnum, m.name, m.kind + ', poz. ' + m.level + ', nr ' + m.vnum)));
      if (!box.children.length) { const p = document.createElement('small'); p.textContent = 'Nic nie znaleziono.'; box.appendChild(p); }
      box.hidden = false;
    };
    input.addEventListener('input', () => { clearTimeout(timer); timer = setTimeout(search, 250); });
    input.addEventListener('keydown', (e) => { if (e.key === 'Enter') { e.preventDefault(); search(); } });
  }

})();
