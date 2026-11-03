(() => {
  'use strict';
  const byId = id => document.getElementById(id);
  const grid = byId('sensor-grid');
  const positions = ['Levi · zgoraj', 'Levi · sredina', 'Levi · spodaj', 'Desni · sredina', 'Desni · zgoraj', 'Desni · spodaj'];
  const romPattern = /^(?:[0-9A-F]{2}:){7}[0-9A-F]{2}$/i;
  const cards = new Map();
  let lastData;
  let timer;
  let controller;
  let pending = false;
  let stopped = false;

  function status(text, state) {
    byId('status').textContent = text;
    byId('status').dataset.state = state;
  }

  function render(data) {
    if (!data || !Array.isArray(data.sensors) || data.sensors.length !== 6 ||
        typeof data.sampled !== 'boolean' || !Number.isInteger(data.devices_at_boot) || data.devices_at_boot < 0 ||
        (data.sampled && (!Number.isFinite(data.sample_age_ms) || data.sample_age_ms < 0)))
      throw new Error('Neveljaven odgovor naprave.');
    const seen = new Set();
    for (const sensor of data.sensors) {
      if (!Number.isInteger(sensor.buffer) || sensor.buffer < 1 || sensor.buffer > 6 || seen.has(sensor.buffer) ||
          (sensor.index !== null && (!Number.isInteger(sensor.index) || sensor.index < 0 || sensor.index > 255)) ||
          (sensor.rom !== null && (typeof sensor.rom !== 'string' || !romPattern.test(sensor.rom))))
        throw new Error('Neveljavni podatki senzorja.');
      seen.add(sensor.buffer);
    }
    byId('empty-state')?.remove();
    let found = 0;
    for (const sensor of [...data.sensors].sort((a, b) => a.buffer - b.buffer)) {
      let card = cards.get(sensor.buffer);
      if (!card) {
        card = byId('sensor-template').content.firstElementChild.cloneNode(true);
        card.dataset.buffer = String(sensor.buffer);
        card.querySelector('h2').id = `buffer-${sensor.buffer}`;
        card.setAttribute('aria-labelledby', `buffer-${sensor.buffer}`);
        cards.set(sensor.buffer, card);
        grid.append(card);
      }
      const valid = data.sampled && sensor.rom !== null && typeof sensor.temperature === 'number' &&
        Number.isFinite(sensor.temperature) && sensor.temperature >= -55 && sensor.temperature <= 125;
      card.querySelector('.buffer-name').textContent = `Buffer ${sensor.buffer}`;
      card.querySelector('.position').textContent = positions[sensor.buffer - 1];
      card.querySelector('.index').textContent = sensor.index === null ? 'Ob zagonu ni zaznan' : `Ob zagonu #${sensor.index}`;
      card.querySelector('.rom').textContent = sensor.rom || 'Naslov ni prebran';
      card.querySelector('.temperature').textContent = valid ? sensor.temperature.toFixed(1).replace('.', ',') : '—';
      card.querySelector('.reading-state').textContent = !data.sampled ? 'Čakam na meritev' : !sensor.rom ? 'Ni naslova' : valid ? 'Meritev OK' : 'Ni veljavne meritve';
      card.dataset.valid = String(valid);
      if (sensor.rom) found++;
    }
    byId('device-count').textContent = String(data.devices_at_boot);
    byId('address-count').textContent = `${found} / 6`;
    byId('firmware-version').textContent = typeof data.firmware === 'string' ? data.firmware : '—';
    byId('last-sample').textContent = data.sampled ? new Date(Date.now() - data.sample_age_ms).toLocaleTimeString('sl-SI', {
      hour: '2-digit', minute: '2-digit', second: '2-digit'
    }) : '—';
    byId('count-notice').hidden = data.devices_at_boot === 6;
    byId('count-notice').textContent = `Ob zagonu je bilo zaznanih ${data.devices_at_boot} naprav. Spodaj je šest mest, ki jih trenutna koda spremlja.`;
    const stale = !data.sampled || data.sample_age_ms > 15000;
    grid.classList.toggle('is-stale', stale);
    status(!data.sampled ? 'Čakam na prvo meritev' : stale ? 'Meritve niso sveže' : 'Podatki so osveženi', stale ? 'waiting' : 'ok');
    byId('copy-all').disabled = !data.sampled;
    lastData = data;
  }

  async function poll() {
    clearTimeout(timer);
    if (pending || stopped || document.hidden) return;
    pending = true;
    byId('refresh').disabled = true;
    const request = new AbortController();
    controller = request;
    const timeout = setTimeout(() => request.abort(), 4500);
    try {
      const response = await fetch('/api/sensors', { cache: 'no-store', signal: request.signal });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const data = await response.json();
      if (!stopped) render(data);
    } catch (error) {
      if (!stopped && !document.hidden) {
        grid.classList.add('is-stale');
        status(lastData ? 'Ni povezave · prikazujem zadnje podatke' : 'Ni povezave z napravo · poskušam znova', 'error');
      }
    } finally {
      clearTimeout(timeout);
      pending = false;
      byId('refresh').disabled = false;
      if (controller === request) controller = null;
      if (!stopped && !document.hidden) timer = setTimeout(poll, 5000);
    }
  }

  byId('copy-all').addEventListener('click', async () => {
    if (!lastData) return;
    const text = [...lastData.sensors].sort((a, b) => a.buffer - b.buffer).map(sensor => {
      const value = typeof sensor.temperature === 'number' && Number.isFinite(sensor.temperature) ? `${sensor.temperature.toFixed(1)} °C` : 'ni meritve';
      const index = sensor.index === null ? 'ob zagonu ni zaznan' : `indeks ob zagonu ${sensor.index}`;
      return `buffer${sensor.buffer} | ${positions[sensor.buffer - 1]} | ${index} | ${sensor.rom || 'ni naslova'} | ${value}`;
    }).join('\n');
    let copied = false;
    if (navigator.clipboard && window.isSecureContext) {
      try { await navigator.clipboard.writeText(text); copied = true; } catch (error) { /* HTTP fallback below. */ }
    }
    if (!copied) {
      const textarea = byId('export-text');
      textarea.value = text;
      textarea.hidden = false;
      textarea.focus();
      textarea.select();
      try { copied = document.execCommand('copy'); } catch (error) { copied = false; }
      if (copied) { textarea.hidden = true; byId('copy-all').focus(); }
    }
    byId('copy-feedback').textContent = copied ? 'Seznam senzorjev je kopiran.' : 'Seznam je prikazan spodaj. Označi ga in kopiraj.';
  });
  byId('refresh').addEventListener('click', poll);
  document.addEventListener('visibilitychange', () => {
    clearTimeout(timer);
    if (document.hidden) controller?.abort();
    else timer = setTimeout(poll, 0);
  });
  window.addEventListener('pagehide', () => { stopped = true; clearTimeout(timer); controller?.abort(); });
  window.addEventListener('pageshow', event => { if (event.persisted) { stopped = false; poll(); } });
  poll();
})();
