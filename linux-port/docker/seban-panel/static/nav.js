// Seban Panel navigation v2 (MT2009_PLUS_SEBAN_NAV_V2): drawer on small
// screens, remembered menu groups, quick search (Ctrl+K or "/") over pages,
// page sections and the individual settings of the settings pages, tabs on
// long detail pages (data-tabs) and the sticky "Na tej stronie" section bar.
// Vanilla JS, no dependencies. Every storage call is guarded: a private
// window or blocked storage only loses the remembered groups and history.
(() => {
  const ROOT = window.SEBAN_ROOT || '';
  const guard = area => ({
    get(key, fallback) { try { const v = JSON.parse(area().getItem(key)); return v == null ? fallback : v; } catch (_) { return fallback; } },
    set(key, value) { try { area().setItem(key, JSON.stringify(value)); } catch (_) {} },
  });
  const store = guard(() => localStorage);
  const session = guard(() => sessionStorage);
  // Lower case without Polish diacritics: "kupuja" finds "kupują".
  const norm = s => String(s || '').toLowerCase().normalize('NFD').replace(/\p{M}+/gu, '').replace(/ł/g, 'l');
  const tidy = s => String(s || '').replace(/\s+/g, ' ').trim();
  const stripIcon = s => tidy(s).replace(/^[^\p{L}\p{N}]+/u, '');
  const escapeHtml = s => String(s == null ? '' : s).replace(/[&<>"']/g, c => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'}[c]));
  const reduceMotion = window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  const scrollBehavior = reduceMotion ? 'auto' : 'smooth';

  // ---- drawer (phones/tablets) -------------------------------------------
  const toggle = document.getElementById('nav-toggle');
  const backdrop = document.getElementById('nav-backdrop');
  function closeNav() { document.body.classList.remove('nav-open'); if (toggle) toggle.setAttribute('aria-expanded', 'false'); }
  if (toggle) toggle.addEventListener('click', () => {
    const open = document.body.classList.toggle('nav-open');
    toggle.setAttribute('aria-expanded', open ? 'true' : 'false');
  });
  if (backdrop) backdrop.addEventListener('click', closeNav);
  document.querySelectorAll('aside.site-nav .sn-nav a').forEach(a => a.addEventListener('click', closeNav));

  // ---- remembered groups -------------------------------------------------
  const OPEN_KEY = 'seban.nav.open';
  const opened = new Set(store.get(OPEN_KEY, []));
  document.querySelectorAll('.sn-group[data-group]').forEach(group => {
    if (opened.has(group.dataset.group)) group.open = true;
    group.addEventListener('toggle', () => {
      if (group.open) opened.add(group.dataset.group); else opened.delete(group.dataset.group);
      store.set(OPEN_KEY, [...opened]);
    });
  });
  const activeLink = document.querySelector('.sn-link.is-active');
  const asideEl = document.getElementById('site-nav');
  if (activeLink && asideEl && activeLink.offsetTop > asideEl.clientHeight - 80) asideEl.scrollTop = activeLink.offsetTop - asideEl.clientHeight / 2;

  // ---- tabs on long detail pages (character card...) ----------------------
  // <div data-tabs><a data-tab="x" href="#karta-x">…</a></div> and
  // <div data-tab-panel="x" id="karta-x">…</div>. Without JS every panel
  // simply shows one under another.
  function initTabs() {
    const bars = [...document.querySelectorAll('main [data-tabs]')];
    document.body.classList.toggle('has-page-tabs', bars.length > 0);
    bars.forEach(bar => {
      const links = [...bar.querySelectorAll('[data-tab]')];
      const ids = links.map(a => a.dataset.tab);
      if (!ids.length) return;
      const key = 'seban.tab.' + location.pathname.replace(/\/\d+(?=\/|$)/g, '/#');
      const activate = (id, remember) => {
        if (!ids.includes(id)) id = ids[0];
        links.forEach(a => { const on = a.dataset.tab === id; a.classList.toggle('is-active', on); a.setAttribute('aria-selected', on ? 'true' : 'false'); });
        ids.forEach(other => { const panel = document.querySelector('[data-tab-panel="' + other + '"]'); if (panel) panel.hidden = other !== id; });
        if (remember) { session.set(key, id); history.replaceState(null, '', '#karta-' + id); }
        window.dispatchEvent(new Event('resize'));
      };
      bar._activate = activate;
      links.forEach(a => a.addEventListener('click', event => {
        if (event.ctrlKey || event.metaKey || event.shiftKey) return;
        event.preventDefault();
        activate(a.dataset.tab, true);
        const top = bar.getBoundingClientRect().top;
        if (top < 0 || top > window.innerHeight / 2) bar.scrollIntoView({behavior: scrollBehavior, block: 'start'});
      }));
      let initial = session.get(key, ids[0]);
      const hash = decodeURIComponent(location.hash.slice(1));
      if (hash.startsWith('karta-')) initial = hash.slice(6);
      else if (hash && !hash.startsWith('szukaj=')) {
        const target = document.getElementById(hash);
        const panel = target && target.closest('[data-tab-panel]');
        if (panel) initial = panel.dataset.tabPanel;
      }
      activate(initial, false);
    });
  }

  // Shows an element that may sit in a closed tab or a folded <details>.
  function reveal(el) {
    const panel = el.closest('[data-tab-panel]');
    if (panel && panel.hidden) {
      const bar = document.querySelector('main [data-tabs]');
      if (bar && bar._activate) bar._activate(panel.dataset.tabPanel, true);
    }
    for (let node = el.parentElement; node; node = node.parentElement) if (node.tagName === 'DETAILS') node.open = true;
  }

  function highlight(el) {
    reveal(el);
    const box = el.closest('label, .checkbox-row, .game-admin-card, .manage-form, section, .panel') || el;
    box.scrollIntoView({behavior: scrollBehavior, block: 'center'});
    box.classList.remove('search-hit');
    void box.offsetWidth;
    box.classList.add('search-hit');
    setTimeout(() => box.classList.remove('search-hit'), 3200);
    const control = box.matches('label') ? box.querySelector('input:not([type=hidden]),select,textarea') : null;
    if (control) setTimeout(() => control.focus({preventScroll: true}), 400);
  }

  // In-page links (#offline-shop...) that point into a closed tab.
  document.addEventListener('click', event => {
    const a = event.target.closest && event.target.closest('main a[href^="#"]');
    if (!a || a.closest('[data-tabs], #page-toc') || event.ctrlKey || event.metaKey) return;
    const target = document.getElementById(decodeURIComponent(a.getAttribute('href').slice(1)));
    if (!target || !target.closest('[data-tab-panel][hidden]')) return;
    event.preventDefault();
    reveal(target);
    target.scrollIntoView({behavior: scrollBehavior});
  });

  // ---- settings index for the quick search -------------------------------
  // Every heading and every labelled control of a page becomes an entry:
  // label text, its help (title attribute or <small>), the card it sits in
  // and an optional data-search alias (old wording players still use).
  const SKIP = 'dialog, #page-toc, .page-topbar, [data-tabs], .notif-dropdown, script, style, template';
  function entryText(el) {
    if (el.matches('label')) {
      const copy = el.cloneNode(true);
      copy.querySelectorAll('select, option, textarea, input, output, small, button, .muted, script, style').forEach(x => x.remove());
      return stripIcon(copy.textContent);
    }
    return stripIcon(el.textContent);
  }
  function extractEntries(doc) {
    const out = [], seen = new Set();
    doc.querySelectorAll('main h2, main h3, main label, main summary, main legend').forEach(el => {
      if (el.closest(SKIP)) return;
      const isLabel = el.matches('label');
      if (isLabel && !el.querySelector('input:not([type=hidden]), select, textarea')) return;
      const text = entryText(el);
      if (text.length < 3 || text.length > 140) return;
      let help = isLabel ? (el.getAttribute('title') || '') : '';
      if (isLabel && !help) { const small = el.querySelector('small'); if (small && tidy(small.textContent).length > 2) help = small.textContent; }
      const card = el.closest('section, article, .panel, form');
      let ctx = '';
      if (card) { const h = card.querySelector('h2') || card.querySelector('h3'); if (h && h !== el) ctx = stripIcon(h.textContent); }
      const id = norm(text) + '|' + norm(ctx);
      if (seen.has(id)) return;
      seen.add(id);
      out.push({el, l: text.slice(0, 110), h: tidy(help).slice(0, 170), c: ctx.slice(0, 70), x: el.getAttribute('data-search') || '', kind: isLabel ? 'setting' : 'section'});
    });
    return out;
  }
  function findEntry(label) {
    const key = norm(label);
    const entries = extractEntries(document);
    const hit = entries.find(e => norm(e.l) === key) || entries.find(e => norm(e.l).startsWith(key));
    return hit && hit.el;
  }

  const INDEX_KEY = 'seban.nav.index.v2';
  const INDEX_TTL = 10 * 60 * 1000;
  let indexState = 'idle', indexed = {};
  function loadIndex() {
    if (indexState !== 'idle') return;
    const cached = session.get(INDEX_KEY, null);
    if (cached && cached.root === ROOT && Date.now() - cached.t < INDEX_TTL) { indexed = cached.pages || {}; indexState = 'done'; return; }
    indexState = 'loading';
    const pages = (data.index || []).filter(p => p.u !== location.pathname);
    let left = pages.length;
    const finish = () => { if (--left > 0) return; indexState = 'done'; session.set(INDEX_KEY, {t: Date.now(), root: ROOT, pages: indexed}); if (dialog && dialog.open) render(true); };
    if (!left) { indexState = 'done'; return; }
    pages.forEach(page => {
      fetch(page.u, {credentials: 'same-origin'}).then(r => r.ok ? r.text() : '').then(html => {
        if (!html) return;
        const doc = new DOMParser().parseFromString(html, 'text/html');
        indexed[page.u] = {l: page.l, g: page.g, e: extractEntries(doc).map(({el, ...rest}) => rest)};
      }).catch(() => {}).finally(() => { finish(); if (dialog && dialog.open) render(true); });
    });
  }

  // ---- data for the quick search ------------------------------------------
  let data = {pages: [], search: [], index: [], current: {}};
  try { data = Object.assign(data, JSON.parse(document.getElementById('cmdk-data').textContent)); } catch (_) {}
  const RECENT_KEY = 'seban.nav.recent';
  const here = location.pathname + location.search;
  if (data.current && data.current.l) {
    const recent = store.get(RECENT_KEY, []).filter(r => r && r.u !== here);
    recent.unshift({l: data.current.l, g: data.current.g, i: data.current.i, u: here});
    store.set(RECENT_KEY, recent.slice(0, 8));
  }

  // ---- quick search (command palette) ------------------------------------
  const dialog = document.getElementById('cmdk');
  const input = document.getElementById('cmdk-input');
  const list = document.getElementById('cmdk-list');
  let results = [], selected = 0, userMoved = false;

  // How well the query words fit one entry: a word counts fully when it is
  // in the text, 0.7 when only its stem is (kupują ~ kupowania); one-letter
  // words (w, i, z) do not count. At least 60% of the words must fit.
  function fit(tokens, label, hay, query) {
    let got = 0, need = 0;
    tokens.forEach(t => {
      if (t.length < 2 && !/\d/.test(t)) return;
      need++;
      if (hay.includes(t)) got += 1;
      else if (t.length >= 5 && hay.includes(t.slice(0, Math.max(4, t.length - 2)))) got += .7;
    });
    if (!need) return label.includes(query) ? 1 : null;
    if (got < Math.max(.99, need * .6)) return null;
    let score = got / need;
    if (label.startsWith(query)) score += 2;
    else if (label.includes(query)) score += 1.5;
    else if (tokens.every(t => label.includes(t))) score += .8;
    return score;
  }

  function currentPageEntries() {
    return extractEntries(document).map(e => ({l: e.l, h: e.h, c: e.c, x: e.x, kind: e.kind, page: data.current.l || 'ta strona', u: ''}));
  }

  function allEntries() {
    const own = location.pathname;
    const out = [];
    // A heading that already has a hand-written anchor entry (the sections
    // of Gra i serwer / Panel webowy) is listed once, as that anchor.
    const anchors = data.pages.filter(p => p.i === '§').map(p => ({path: p.u.split('#')[0], l: norm(p.l)}));
    const hasAnchor = (url, label) => anchors.some(a => a.path === url && (a.l.startsWith(norm(label)) || norm(label).startsWith(a.l)));
    data.pages.forEach(p => out.push({kind: p.i === '§' ? 'anchor' : 'page', l: p.l, d: p.d, g: p.g, i: p.i, k: p.k, u: p.u}));
    currentPageEntries().forEach(e => { if (!(e.kind === 'section' && hasAnchor(own, e.l))) out.push(e); });
    Object.keys(indexed).forEach(url => {
      if (url === own) return;
      const page = indexed[url];
      page.e.forEach(e => { if (e.kind === 'section' && hasAnchor(url, e.l)) return; out.push({kind: e.kind, l: e.l, h: e.h, c: e.c, x: e.x, page: page.l, u: url}); });
    });
    return out;
  }

  function toResult(e) {
    if (e.kind === 'page' || e.kind === 'anchor') return {l: e.l, d: e.d, g: e.g, i: e.i, u: e.u};
    const where = (e.page || '') + (e.c && norm(e.c) !== norm(e.l) ? ' › ' + e.c : '');
    return {l: e.l, d: e.h || where, g: e.h ? where : (e.u ? '' : 'na tej stronie'), i: e.kind === 'setting' ? '⚙' : '§', u: e.u, find: e.l};
  }

  function search(query) {
    const q = norm(query).trim();
    const sections = [];
    if (!q) {
      const recent = store.get(RECENT_KEY, []).filter(r => r.u !== here).slice(0, 5);
      if (recent.length) sections.push({head: 'Ostatnio odwiedzane', items: recent.map(r => ({l: r.l, d: r.g, i: r.i, u: r.u}))});
      const toc = [...document.querySelectorAll('#page-toc .page-toc-track a[href^="#"]:not([hidden])')]
        .map(a => ({l: a.textContent.trim(), i: '§', u: a.getAttribute('href')}));
      if (toc.length) sections.push({head: 'Na tej stronie', items: toc});
      const byGroup = new Map();
      data.pages.filter(p => p.i !== '§').forEach(p => { if (!byGroup.has(p.g)) byGroup.set(p.g, []); byGroup.get(p.g).push({l: p.l, d: p.d, i: p.i, u: p.u}); });
      byGroup.forEach((items, head) => sections.push({head, items}));
      return sections;
    }
    const tokens = q.split(/\s+/).filter(Boolean);
    const pages = [], settings = [];
    allEntries().forEach(e => {
      const label = norm(e.l);
      const hay = label + ' ' + norm(e.d) + ' ' + norm(e.g) + ' ' + norm(e.k) + ' ' + norm(e.h) + ' ' + norm(e.c) + ' ' + norm(e.x) + ' ' + norm(e.page);
      const score = fit(tokens, label, hay, q);
      if (score == null) return;
      (e.kind === 'page' || e.kind === 'anchor' ? pages : settings).push({e, score: score + (e.kind === 'page' ? .3 : 0) + (e.u === '' ? .5 : 0)});
    });
    const order = (a, b) => b.score - a.score || a.e.l.localeCompare(b.e.l, 'pl');
    pages.sort(order); settings.sort(order);
    // The best kind of hit first: a setting whose own label fits the words
    // beats a page that only shares one of them.
    const pageBlock = pages.length ? {head: 'Strony', items: pages.slice(0, 8).map(s => toResult(s.e))} : null;
    const settingBlock = settings.length || indexState === 'loading'
      ? {head: 'Ustawienia i sekcje' + (indexState === 'loading' ? ' (wczytuję pozostałe strony…)' : ''), items: settings.slice(0, 14).map(s => toResult(s.e))} : null;
    if (settingBlock && (!pageBlock || (settings[0] && settings[0].score > pages[0].score))) { sections.push(settingBlock); if (pageBlock) sections.push(pageBlock); }
    else { if (pageBlock) sections.push(pageBlock); if (settingBlock) sections.push(settingBlock); }
    const raw = query.trim();
    sections.push({head: 'Szukaj „' + raw + '” w danych', items: data.search.map(s => ({l: s.l + ': ' + raw, i: s.i, u: s.u + (s.u.includes('?') ? '&' : '?') + 'q=' + encodeURIComponent(raw)}))});
    return sections;
  }

  function render(keepSelection) {
    // When more results arrive (settings of other pages), the top hit is
    // selected again unless the user already picked one with the arrows.
    const previous = keepSelection && userMoved ? results[selected] : null;
    const sections = search(input.value);
    results = [];
    let html = '';
    sections.forEach(section => {
      html += '<li class="cmdk-head" role="presentation">' + escapeHtml(section.head) + '</li>';
      if (!section.items.length) html += '<li class="cmdk-empty" role="presentation">…</li>';
      section.items.forEach(item => {
        const idx = results.push(item) - 1;
        const href = item.find ? (item.u || location.pathname) + '#szukaj=' + encodeURIComponent(item.find) : item.u;
        html += '<li class="cmdk-item" role="option" id="cmdk-opt-' + idx + '" data-idx="' + idx + '"><a href="' + escapeHtml(href) + '"><span class="cmdk-ico" aria-hidden="true">' + escapeHtml(item.i || '›') + '</span><span>' + escapeHtml(item.l) + (item.d ? '<small>' + escapeHtml(item.d) + '</small>' : '') + '</span>' + (item.g ? '<em>' + escapeHtml(item.g) + '</em>' : '') + '</a></li>';
      });
    });
    list.innerHTML = html || '<li class="cmdk-empty">Brak wyników.</li>';
    const keep = previous ? results.findIndex(r => r.l === previous.l && r.u === previous.u) : -1;
    select(keep >= 0 ? keep : 0);
  }

  function select(idx) {
    if (!results.length) return;
    selected = (idx + results.length) % results.length;
    list.querySelectorAll('.cmdk-item.is-sel').forEach(li => li.classList.remove('is-sel'));
    const li = list.querySelector('[data-idx="' + selected + '"]');
    if (li) { li.classList.add('is-sel'); li.scrollIntoView({block: 'nearest'}); input.setAttribute('aria-activedescendant', li.id); }
  }

  function go(item) {
    if (!item) return;
    if (item.find && (!item.u || item.u === location.pathname)) {
      dialog.close();
      const el = findEntry(item.find);
      if (el) highlight(el);
      return;
    }
    if (item.find) { window.location.href = item.u + '#szukaj=' + encodeURIComponent(item.find); return; }
    if (item.u.startsWith('#')) {
      dialog.close();
      const target = document.getElementById(item.u.slice(1));
      if (target) { reveal(target); target.scrollIntoView({behavior: scrollBehavior}); history.replaceState(null, '', item.u); }
      return;
    }
    window.location.href = item.u;
  }

  function openPalette() {
    if (!dialog || dialog.open) return;
    closeNav();
    input.value = '';
    userMoved = false;
    loadIndex();
    render();
    try { dialog.showModal(); } catch (_) { dialog.setAttribute('open', ''); }
    input.focus();
  }

  if (dialog && input && list) {
    document.addEventListener('click', event => {
      if (event.target.closest && event.target.closest('[data-cmdk-open]')) { event.preventDefault(); openPalette(); }
    });
    document.addEventListener('keydown', event => {
      const typing = event.target.closest && event.target.closest('input,textarea,select,[contenteditable="true"]');
      if ((event.ctrlKey || event.metaKey) && !event.altKey && (event.key === 'k' || event.key === 'K')) { event.preventDefault(); dialog.open ? dialog.close() : openPalette(); return; }
      if (event.key === '/' && !typing && !dialog.open && !event.ctrlKey && !event.metaKey && !event.altKey) { event.preventDefault(); openPalette(); }
      if (event.key === 'Escape' && document.body.classList.contains('nav-open')) closeNav();
    });
    input.addEventListener('input', () => { userMoved = false; render(); });
    input.addEventListener('keydown', event => {
      if (event.key === 'ArrowDown') { event.preventDefault(); userMoved = true; select(selected + 1); }
      else if (event.key === 'ArrowUp') { event.preventDefault(); userMoved = true; select(selected - 1); }
      else if (event.key === 'Enter') { event.preventDefault(); go(results[selected]); }
    });
    list.addEventListener('mousemove', event => {
      const li = event.target.closest('.cmdk-item');
      if (li && Number(li.dataset.idx) !== selected) select(Number(li.dataset.idx));
    });
    list.addEventListener('click', event => {
      const li = event.target.closest('.cmdk-item');
      if (!li || event.ctrlKey || event.metaKey || event.shiftKey || event.button) return;
      event.preventDefault();
      go(results[Number(li.dataset.idx)]);
    });
    dialog.addEventListener('click', event => { if (event.target === dialog) dialog.close(); });
  }

  // ---- "Na tej stronie" section bar ---------------------------------------
  const slug = s => norm(s).replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '').slice(0, 40);
  let tocLinks = [];

  function buildToc() {
    const main = document.querySelector('main');
    const slot = main && main.querySelector('#page-toc');
    tocLinks = [];
    if (!slot) return;
    if ('static' in slot.dataset) {
      // Server-written bar (settings pages): hide chips whose section is
      // switched off by a feature flag, and group labels left with nothing.
      slot.querySelectorAll('.page-toc-track a[href^="#"]').forEach(a => { a.hidden = !document.getElementById(a.getAttribute('href').slice(1)); });
      slot.querySelectorAll('.page-toc-part').forEach(part => {
        let node = part.nextElementSibling, any = false;
        while (node && !node.classList.contains('page-toc-part')) { if (!node.hidden) any = true; node = node.nextElementSibling; }
        part.hidden = !any;
      });
    } else {
      // Pages with their own tabs get no second navigation bar.
      if (main.querySelector('[data-tabs]') || document.body.dataset.endpoint === 'dashboard') { slot.hidden = true; slot.innerHTML = ''; return; }
      const seen = new Set(), items = [];
      main.querySelectorAll('h2').forEach(h => {
        if (h.closest('dialog,#page-toc,.page-topbar,[hidden],template')) return;
        const target = h.closest('section,article,.panel') || h;
        if (seen.has(target) || target === main) return;
        seen.add(target);
        let text = stripIcon(h.textContent);
        if (!text) return;
        if (text.length > 34) text = text.slice(0, 32).trim() + '…';
        if (!target.id) {
          const base = 'sekcja-' + (slug(text) || items.length + 1);
          let id = base, n = 2;
          while (document.getElementById(id)) id = base + '-' + n++;
          target.id = id;
        }
        items.push({id: target.id, text});
      });
      if (items.length < 4) { slot.hidden = true; slot.innerHTML = ''; return; }
      slot.innerHTML = '<span class="page-toc-title">Na tej stronie</span><div class="page-toc-track"></div>';
      const track = slot.lastElementChild;
      items.forEach(item => {
        const a = document.createElement('a');
        a.href = '#' + item.id;
        a.textContent = item.text;
        track.appendChild(a);
      });
      slot.hidden = false;
    }
    tocLinks = [...slot.querySelectorAll('.page-toc-track a[href^="#"]:not([hidden])')]
      .map(a => ({a, target: document.getElementById(a.getAttribute('href').slice(1))}))
      .filter(x => x.target);
    tocLinks.forEach(({a, target}) => a.addEventListener('click', event => {
      if (event.ctrlKey || event.metaKey || event.shiftKey) return;
      event.preventDefault();
      target.scrollIntoView({behavior: scrollBehavior});
      history.replaceState(null, '', a.getAttribute('href'));
    }));
    spy();
  }

  let spyQueued = false;
  function spy() {
    spyQueued = false;
    if (!tocLinks.length) return;
    const slot = document.getElementById('page-toc');
    // Sections scrolled to by a chip stop just under the bar (scroll-margin-top),
    // so the line sits a little below the bar's bottom edge.
    const line = (slot ? slot.getBoundingClientRect().bottom : 0) + 40;
    let current = null;
    tocLinks.forEach(x => { if (x.target.getBoundingClientRect().top <= line) current = x; });
    tocLinks.forEach(x => x.a.classList.toggle('is-active', x === current));
    if (current) {
      const track = current.a.parentElement;
      const left = current.a.offsetLeft - track.offsetLeft;
      if (left < track.scrollLeft || left + current.a.offsetWidth > track.scrollLeft + track.clientWidth) track.scrollLeft = Math.max(0, left - 24);
    }
  }
  const queueSpy = () => { if (!spyQueued) { spyQueued = true; requestAnimationFrame(spy); } };
  window.addEventListener('scroll', queueSpy, {passive: true});
  window.addEventListener('resize', queueSpy);

  function refreshMain() { initTabs(); buildToc(); }

  function init() {
    refreshMain();
    const hash = location.hash ? decodeURIComponent(location.hash.slice(1)) : '';
    if (hash.startsWith('szukaj=')) {
      // Arrived from the quick search on another page: find the setting by
      // its label, open its tab, scroll to it and light it up.
      const el = findEntry(hash.slice(7));
      history.replaceState(null, '', location.pathname + location.search);
      if (el) setTimeout(() => highlight(el), 60);
    } else if (hash && !hash.startsWith('karta-')) {
      const target = document.getElementById(hash);
      if (target) { reveal(target); target.scrollIntoView(); }
    }
  }
  // Pages build part of their content in their own scripts, so wait for the
  // whole document; ajax-forms.js swaps <main> after a form - refresh then.
  if (document.readyState === 'complete') init(); else window.addEventListener('load', init);
  const main = document.querySelector('main');
  if (main && window.MutationObserver) {
    let pending = null;
    new MutationObserver(() => { clearTimeout(pending); pending = setTimeout(refreshMain, 60); }).observe(main, {childList: true});
  }
  window.SebanNav = {open: openPalette, reveal, rebuild: refreshMain, root: ROOT};
})();
