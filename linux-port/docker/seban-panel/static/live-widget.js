(() => {
  const css = document.createElement('link'); css.rel = 'stylesheet'; css.href = '/static/live-overrides.css'; document.head.appendChild(css);
  let snapshot = [], globalTopId = null, currentLevel = 'all';
  const $ = id => document.getElementById(id);
  const map = $('world-map'), select = $('map-select'), search = $('bot-search');
  const filters = document.querySelector('.live-filters');
  const mode = document.createElement('select');
  mode.id = 'live-mode';
  mode.innerHTML = '<option value="live">Pozycje botów na żywo</option><option value="deaths">Mapa cieplna: zgony botów</option><option value="metins">Mapa cieplna: rozbite Metiny</option><option value="bosses">Mapa cieplna: zabite bossy</option>';
  filters.insertBefore(mode, search);
  const autoplay = document.createElement('label');
  autoplay.className = 'map-autoplay';
  autoplay.innerHTML = '<input id="map-autoplay" type="checkbox"> Automatycznie zmieniaj mapy po bezczynności';
  filters.insertBefore(autoplay, search);
  const restartInfo = document.createElement('small');
  restartInfo.id = 'last-restart';
  document.querySelector('.live-shell header > div').appendChild(restartInfo);
  [...document.querySelectorAll('.live-shell footer span')].filter(node => node.textContent.includes('Podkład graficzny')).forEach(node => node.remove());
  const mapLabels = {21:'Chunjo M1 — Joan',23:'Chunjo M2 — Bokjung',24:'Chunjo M3 — Waryong',25:'Łatwy Loch Małp',61:'Góra Sohan',64:'Dolina Orków',63:'Pustynia Yongbi',104:'Loch Pająków V1',108:'Loch Małp Normalny',109:'Loch Małp Trudny',65:'Świątynia Hwang',4:'Shinsoo M3 — Jungrang',44:'Jinno M3 — Imha',5:'Loch Małp Shinsoo',45:'Loch Małp Jinno',1:'Shinsoo M1 — Yongan',3:'Shinsoo M2 — Jayang',41:'Jinno M1 — Pyongmoo',43:'Jinno M2 — Bakra',67:'Las',68:'Czerwony Las',66:'Wieża Demonów'};
  Object.entries(mapLabels).forEach(([id,label]) => {
    const option = select.querySelector(`option[value="${id}"]`);
    if (option) option.textContent = label;
  });
  const overview = document.querySelector('.world-overview');
  let overviewMaps = null;
  if (overview) {
    overviewMaps = document.createElement('section');
    overviewMaps.className = 'overview-maps';
    overviewMaps.innerHTML = '<h4>🗺 Boty na mapach</h4><div class="overview-maps-list"><div class="muted">Ładowanie…</div></div>';
    overview.insertBefore(overviewMaps, overview.querySelector('.overview-restart'));
  }
  function renderOverviewMaps() {
    if (!overviewMaps) return;
    const counts = snapshot.reduce((all, bot) => { all[bot.map_index] = (all[bot.map_index] || 0) + 1; return all; }, Object.fromEntries(Object.keys(mapLabels).map(id => [id, 0])));
    const entries = Object.entries(counts).sort((a,b)=>b[1]-a[1]);
    // Scrollable inner list -- with 20+ maps now tracked, the plain list
    // used to spill past the fixed-height world-overview sidebar box.
    overviewMaps.innerHTML = '<h4>🗺 Boty na mapach</h4><div class="overview-maps-list">' + (entries.map(([id,count]) => `<div><span>${escape(mapLabels[id] || `Mapa #${id}`)}</span><b>${count}</b></div>`).join('') || '<div class="muted">Brak botów online.</div>') + '</div>';
  }
  const levelOK = (level) => { if (currentLevel === 'all') return true; if (currentLevel.endsWith('+')) return level >= Number.parseInt(currentLevel, 10); const [from, to] = currentLevel.split('-').map(Number); return level >= from && level <= to; };
  const escape = value => String(value).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  const portrait = job => { const files = ["warrior_m.bmp","assassin_w.bmp","sura_m.bmp","shaman_w.bmp","warrior_w.bmp","assassin_m.bmp","sura_w.bmp","shaman_m.bmp"]; const index = Number.isInteger(Number(job)) && Number(job) >= 0 && Number(job) < files.length ? Number(job) : 0; return `/static/class-portraits/${files[index]}`; };
  function activityGroup(bot) {
    const status = String(bot.action_label || '').toLowerCase();
    if (/łow|low|ryb|fishing|branie/.test(status)) return 'Łowi ryby';
    const goals = {2:'Wybiera profesję',3:'Zdobywa ekwipunek',4:'Uzupełnia zapasy',5:'Ulepsza ekwipunek',6:'Rozwija umiejętności',7:'Poluje na Metiny',8:'Gra w grupie',9:'Robi Biologa',10:'Misje polowania',11:'Rozwija konia'};
    if (goals[bot.goal] !== undefined) return goals[bot.goal];
    const actions = {2:'Expi / walczy',3:'Zbiera łup',4:'Regeneruje się',5:'Wybiera profesję',6:'Handluje',7:'Ulepsza ekwipunek',8:'Rozwija umiejętności',9:'Ulepsza ekwipunek',10:'Gra w grupie',11:'Robi Biologa',12:'Rozwija konia',13:'Prowadzi stragan',15:'Przegląda stragany',16:'Wabi potwory',17:'Odpoczywa w mieście',18:'Kopie rudę'};
    return actions[bot.action] || (bot.action === 1 ? 'Przemieszcza się' : 'Expi / walczy');
  }
  function renderActivities(bots) {
    const box = $('live-activity');
    if (!box) return;
    const grouped = bots.reduce((all, bot) => { const label = activityGroup(bot); all[label] = (all[label] || 0) + 1; return all; }, {});
    let entries = Object.entries(grouped).sort((a,b)=>b[1]-a[1]);
    if (entries.length > 6) { const rest = entries.slice(5).reduce((sum, entry)=>sum+entry[1],0); entries = entries.slice(0,5); if (rest) entries.push(['Pozostałe aktywności', rest]); }
    const total = bots.length || 1;
    box.innerHTML = entries.map(([label,count]) => `<div class="activity-line"><span title="${escape(label)}">${escape(label)}</span><b>${count}</b><i style="--share:${Math.max(4,Math.round(count/total*100))}%"></i></div>`).join('') || '<p class="muted">Brak aktywnych botów na tej mapie.</p>';
  }
  function render() {
    if (mode.value !== 'live') return;
    const mapId = Number(select.value), needle = search.value.trim().toLowerCase();
    map.dataset.mapIndex = String(mapId);
    const bots = snapshot.filter(b => b.map_index === mapId && levelOK(b.level) && (!$('party-only').checked || b.in_party) && (!needle || b.name.toLowerCase().includes(needle)));
    map.querySelectorAll('.bot-point,.heat-point').forEach(node => node.remove());
    bots.forEach(bot => {
      const point = document.createElement('a'); point.className = `bot-point ${bot.in_party ? 'is-pt' : ''}${bot.stuck ? ' is-stuck' : ''}${bot.fighting_metin ? ' is-metin' : ''}`;
      point.href = `/player/${bot.id}`; point.style.left = `${Math.max(1,Math.min(99,bot.px))}%`; point.style.top = `${Math.max(1,Math.min(99,bot.py))}%`;
      point.title = `${bot.name} · poziom ${bot.level}${bot.in_party ? ' · PT' : ''}${bot.stuck ? ' · możliwie zablokowany' : ''}${bot.fighting_metin ? ' · walczy z Metinem' : ''}`;
      if ($('show-names').checked) point.innerHTML = `<em>${escape(bot.name)} (${bot.level})</em>`;
      map.appendChild(point);
    });
    const average = bots.length ? (bots.reduce((sum,b)=>sum+b.level,0)/bots.length).toFixed(1) : '—';
    $('stat-visible').textContent = bots.length; $('stat-pt').textContent = bots.filter(b=>b.in_party).length; $('stat-avg').textContent = average; $('stat-max').textContent = bots.length ? Math.max(...bots.map(b=>b.level)) : '—';
    $('live-count').textContent = `${bots.length} botów na mapie`;
    $('map-caption').textContent = select.options[select.selectedIndex].text;
$('live-ranking').innerHTML = bots.sort((a,b)=>b.level-a.level||a.name.localeCompare(b.name)).slice(0,10).map((b,i)=>`<a class="${b.id === globalTopId ? 'is-global-leader' : ''}" href="/player/${b.id}"><b>#${i+1}</b><img class="class-portrait class-portrait--live" src="${portrait(b.job)}" alt=""> ${escape(b.name)}${b.in_party?'<mark class="pt-mark">PT</mark>':''}${b.stuck?'<mark class="stuck-mark">⚠</mark>':''} <span>Lv ${b.level}</span></a>`).join('') || '<p class="muted">Brak botów spełniających filtr.</p>';
    renderActivities(bots);
  }
  async function load() {
    try {
      const response = await fetch('/api/live-bots', {cache:'no-store'}), data = await response.json();
      if (!data.ok) return;
      globalTopId = data.global_top_id; snapshot = data.bots.map(bot => { const b = data.bounds[String(bot.map_index)] || data.bounds[bot.map_index]; return b ? {...bot, px:(bot.x-b[0])/b[2]*100, py:(bot.y-b[1])/b[3]*100} : bot; });
      const globalAverage = snapshot.length ? (snapshot.reduce((sum,bot)=>sum+bot.level,0)/snapshot.length).toFixed(1) : '0';
      if ($('overview-bots')) $('overview-bots').textContent = snapshot.length;
      if ($('overview-avg')) $('overview-avg').textContent = globalAverage;
      if ($('overview-party')) $('overview-party').textContent = snapshot.filter(bot=>bot.in_party).length;
      if ($('overview-max')) $('overview-max').textContent = snapshot.length ? Math.max(...snapshot.map(bot=>bot.level)) : '0';
      renderOverviewMaps();
      if (mode.value === 'live') render();
    } catch (_) { $('live-count').textContent = 'Brak danych live'; }
  }
  async function loadHeat() {
    try {
      const response = await fetch(`/api/heat-events?type=${encodeURIComponent(mode.value)}`, {cache:'no-store'}), data = await response.json();
      if (!data.ok) return;
      const mapId = Number(select.value), bound = data.bounds[String(mapId)] || data.bounds[mapId];
      const events = data.events.filter(event => event.map_index === mapId);
      map.dataset.mapIndex = String(mapId);
      map.querySelectorAll('.bot-point,.heat-point').forEach(node => node.remove());
      events.forEach(event => { const dot=document.createElement('i'); dot.className='heat-point'; dot.style.left=`${Math.max(1,Math.min(99,(event.x-bound[0])/bound[2]*100))}%`; dot.style.top=`${Math.max(1,Math.min(99,(event.y-bound[1])/bound[3]*100))}%`; dot.title=`${event.name||'Zdarzenie'} · ${event.time}`; map.appendChild(dot); });
    const label = ({deaths:'zgonów botów',metins:'rozbitych Metinów',bosses:'zabitych bossów'})[mode.value] || 'zdarzeń';
      $('live-count').textContent = `${events.length} ${label} / 24 h`;
      $('map-caption').textContent = `${select.options[select.selectedIndex].text} · ${label}`;
      $('stat-visible').textContent = events.length; $('stat-pt').textContent = '—'; $('stat-avg').textContent = '24 h'; $('stat-max').textContent = '●';
      $('live-ranking').innerHTML = events.slice(0,15).map((event,i)=>`<a href="#"><b>#${i+1}</b> ${escape(event.name||'Zdarzenie')} <span>${String(event.time).slice(11,16)}</span></a>`).join('') || '<p class="muted">Brak zdarzeń na tej mapie.</p>';
      const activity = $('live-activity'); if (activity) activity.innerHTML = '<p class="muted">W trybie mapy cieplnej aktywności nie są wyświetlane.</p>';
    } catch (_) { $('live-count').textContent = 'Brak danych heatmapy'; }
  }
  async function refreshRestartInfo() {
    try { const data = await fetch('/api/manage-status',{cache:'no-store'}).then(r=>r.json()); const time = Number(data.last_restart_time || 0); const label=time ? new Date(time * 1000).toLocaleString('pl-PL') : 'Brak danych'; restartInfo.textContent = time ? `Ostatni restart: ${label}` : ''; if ($('overview-restart')) $('overview-restart').textContent=label; Object.entries(data.rates || {}).forEach(([name,value]) => { const node=$(`overview-rate-${name}`); if (node) node.textContent=`${value}%`; }); } catch (_) {}
  }
  let lastInteraction = Date.now();
  const markInteraction = () => { lastInteraction = Date.now(); };
  document.querySelectorAll('[data-level]').forEach(button => button.addEventListener('click', () => { markInteraction(); currentLevel = button.dataset.level; document.querySelectorAll('[data-level]').forEach(x=>x.classList.toggle('active',x===button)); render(); }));
  [select, search, $('show-names'), $('party-only')].forEach(node => node.addEventListener('input', () => mode.value === 'live' ? render() : loadHeat()));
  mode.addEventListener('input', () => mode.value === 'live' ? render() : loadHeat());
  [select, search, $('show-names'), $('party-only'), mode, $('map-autoplay')].forEach(node => node.addEventListener('input', markInteraction));
  window.addEventListener('pointerdown', markInteraction, {passive:true});
  setInterval(() => { if (!$('map-autoplay').checked || Date.now() - lastInteraction < 15000) return; const next=(select.selectedIndex+1)%select.options.length; select.selectedIndex=next; mode.value === 'live' ? render() : loadHeat(); }, 8000);
  load(); refreshRestartInfo(); setInterval(load, 1500); setInterval(refreshRestartInfo, 30000);
})();
