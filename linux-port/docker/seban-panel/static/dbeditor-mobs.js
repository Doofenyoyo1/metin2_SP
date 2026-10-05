// MT2009_PLUS_DB_EDITOR_V1: the respawn pages (templates/dbeditor/spawns_map.html,
// spawns_special.html): a click on the map picture puts its cell coordinates
// into the row the operator last typed in (a new row when none), and the
// "kind of time" switch of a timed boss shows only its own fields.
(() => {
  let active = null;
  document.querySelectorAll('[data-dbe-row]').forEach(row => {
    row.addEventListener('focusin', () => {
      document.querySelectorAll('[data-dbe-row].is-active').forEach(r => r.classList.remove('is-active'));
      row.classList.add('is-active');
      active = row;
    });
  });
  document.querySelectorAll('[data-dbe-map]').forEach(map => {
    const width = Number(map.dataset.width), height = Number(map.dataset.height);
    const cursor = map.querySelector('[data-dbe-map-cursor]');
    map.addEventListener('click', event => {
      const box = map.getBoundingClientRect();
      const x = Math.round((event.clientX - box.left) / box.width * width);
      const y = Math.round((event.clientY - box.top) / box.height * height);
      let row = active;
      if (!row) {
        row = [...document.querySelectorAll('[data-dbe-row].dbe-new-row')].find(r => !r.querySelector('[data-x]').value);
      }
      if (!row) return;
      row.querySelector('[data-x]').value = x;
      row.querySelector('[data-y]').value = y;
      row.classList.add('is-active');
      active = row;
      if (cursor) {
        cursor.hidden = false;
        cursor.style.left = (x / width * 100) + '%';
        cursor.style.top = (y / height * 100) + '%';
      }
    });
  });
  const timeType = document.querySelector('[data-dbe-timetype]');
  if (timeType) {
    const update = () => document.querySelectorAll('[data-dbe-when]').forEach(el => {
      el.hidden = el.dataset.dbeWhen !== timeType.value;
    });
    timeType.addEventListener('change', update);
    update();
  }
})();
