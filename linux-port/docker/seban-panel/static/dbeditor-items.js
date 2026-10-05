// MT2009_PLUS_DB_EDITOR_V1: the item and skill editors' page script
// (templates/dbeditor/items_edit.html, skills_edit.html). ajax-forms.js swaps
// <main> and runs its scripts again after every save, so the listeners are
// bound once, on document, and init() only refreshes what is on the page.
(() => {
  const PREVIEW_DELAY = 300;
  const timers = {};

  function form() { return document.getElementById('dbe-skill-form'); }

  function sample() {
    const values = {};
    document.querySelectorAll('[data-dbe-sample]').forEach(input => { values[input.dataset.dbeSample] = input.value; });
    return values;
  }

  function esc(text) {
    return String(text).replace(/[&<>"]/g, ch => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[ch]));
  }

  function render(column, data) {
    const box = document.querySelector(`[data-dbe-preview="${column}"]`);
    const input = document.querySelector(`[data-dbe-poly="${column}"]`);
    if (!box) return;
    let html = '';
    if (data.error) html += `<p class="dbe-error">${esc(data.error)}</p>`;
    (data.warnings || []).forEach(w => { html += `<p class="dbe-warn">${esc(w)}</p>`; });
    if (data.ok && data.values && data.values.length) {
      const cls = v => (v.used ? '' : ' class="is-unused"');
      html += '<table class="dbe-table compact dbe-levels"><tr><th>Poziom</th>' +
        data.values.map(v => `<th${cls(v)}>${esc(v.label)}</th>`).join('') + '</tr><tr><td>Wynik</td>' +
        data.values.map(v => `<td${cls(v)}>${v.min === v.max ? v.min : esc(v.min + ' – ' + v.max)}</td>`).join('') +
        '</tr></table>';
    }
    box.innerHTML = html;
    if (input) input.classList.toggle('is-bad', !data.ok);
  }

  async function refresh(column) {
    const f = form();
    const input = document.querySelector(`[data-dbe-poly="${column}"]`);
    if (!f || !input) return;
    input.classList.toggle('is-changed', input.value !== (input.dataset.original || ''));
    try {
      const res = await fetch(f.dataset.previewUrl, {
        method: 'POST', credentials: 'same-origin', headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ column, formula: input.value, max_level: f.dataset.maxLevel, sample: sample() }),
      });
      render(column, await res.json());
    } catch (err) {
      render(column, { ok: false, error: 'Nie udało się policzyć podglądu (brak połączenia z panelem).' });
    }
  }

  function schedule(column) {
    clearTimeout(timers[column]);
    timers[column] = setTimeout(() => refresh(column), PREVIEW_DELAY);
  }

  function mirror(checkbox) {
    const [from, to] = (checkbox.dataset.dbeMirror || '').split(':');
    const source = document.querySelector(`[data-dbe-poly="${from}"]`);
    const target = document.querySelector(`[data-dbe-poly="${to}"]`);
    if (!source || !target) return;
    target.readOnly = checkbox.checked;
    if (checkbox.checked && target.value !== source.value) {
      target.value = source.value;
      schedule(to);
    }
  }

  function syncUnit(select) {
    const unit = document.querySelector(`[data-dbe-unit="${select.dataset.dbeBonus}"]`);
    const option = select.selectedOptions[0];
    if (unit) unit.textContent = option ? (option.dataset.unit || '') : '';
  }

  // MT2009_PLUS_ITEM_EXTRA_APPLY_V1: "Dodatkowe bonusy (ponad 3)" - rows added
  // from the <template>, removed by their cross; the unit follows the kind.
  function syncExtraUnit(select) {
    const unit = select.closest('[data-dbe-extra-row]')?.querySelector('[data-dbe-extra-unit]');
    const option = select.selectedOptions[0];
    if (unit) unit.textContent = option ? (option.dataset.unit || '') : '';
  }

  function addExtraRow() {
    const box = document.querySelector('[data-dbe-extra-rows]');
    const tpl = document.querySelector('[data-dbe-extra-template]');
    if (!box || !tpl) return;
    const row = tpl.content.firstElementChild.cloneNode(true);
    box.appendChild(row);
    renumberExtra();
    row.querySelector('select')?.focus();
  }

  function renumberExtra() {
    document.querySelectorAll('[data-dbe-extra-rows] [data-dbe-extra-row] .dbe-num').forEach((num, i) => {
      num.textContent = `${i + 4}.`;
    });
  }

  function syncFamily() {
    const box = document.querySelector('[data-dbe-family]');
    const mode = document.querySelector('[data-dbe-family-mode]');
    if (box && mode) mode.hidden = !box.checked;
  }

  function init() {
    document.querySelectorAll('[data-dbe-mirror]').forEach(mirror);
    document.querySelectorAll('[data-dbe-bonus]').forEach(syncUnit);
    document.querySelectorAll('[data-dbe-extra-type]').forEach(syncExtraUnit);
    syncFamily();
  }

  if (!window.__dbeItemsBound) {
    window.__dbeItemsBound = true;
    document.addEventListener('input', e => {
      const poly = e.target.closest('[data-dbe-poly]');
      if (poly) {
        schedule(poly.dataset.dbePoly);
        document.querySelectorAll('[data-dbe-mirror]').forEach(box => {
          if (box.checked && box.dataset.dbeMirror.split(':')[0] === poly.dataset.dbePoly) mirror(box);
        });
      }
      if (e.target.closest('[data-dbe-sample]')) {
        document.querySelectorAll('[data-dbe-poly]').forEach(input => schedule(input.dataset.dbePoly));
      }
    });
    document.addEventListener('change', e => {
      if (e.target.matches('[data-dbe-mirror]')) mirror(e.target);
      if (e.target.matches('[data-dbe-bonus]')) syncUnit(e.target);
      if (e.target.matches('[data-dbe-extra-type]')) syncExtraUnit(e.target);
      if (e.target.matches('[data-dbe-family]')) syncFamily();
    });
    document.addEventListener('click', e => {
      if (e.target.closest('[data-dbe-extra-add]')) {
        addExtraRow();
        return;
      }
      const removeExtra = e.target.closest('[data-dbe-extra-remove]');
      if (removeExtra) {
        removeExtra.closest('[data-dbe-extra-row]')?.remove();
        renumberExtra();
        return;
      }
      const scale = e.target.closest('[data-dbe-scale]');
      const reset = e.target.closest('[data-dbe-reset]');
      const button = scale || reset;
      if (!button) return;
      const input = button.closest('[data-dbe-formula]')?.querySelector('[data-dbe-poly]');
      if (!input || input.readOnly) return;
      if (reset) {
        input.value = input.dataset.original || '';
      } else if (input.value.trim()) {
        input.value = `(${input.value.trim()})*${scale.dataset.dbeScale}`;
      }
      input.dispatchEvent(new Event('input', { bubbles: true }));
    });
  }
  init();
})();
