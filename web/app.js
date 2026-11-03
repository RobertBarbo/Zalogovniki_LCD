(() => {
  'use strict';
  const byId = id => document.getElementById(id);
  const fields = [...document.querySelectorAll('[data-temp]')];
  const scene = document.querySelector('.scene');
  const dialog = byId('settings-dialog');
  const restartDialog = byId('restart-dialog');
  const restartButton = byId('open-restart');
  const fileInput = byId('lcd-image');
  const imagePreview = byId('image-preview');
  const uploadButton = byId('upload-image');
  let pollTimer;
  let pollController;
  let polling = false;
  let uploading = false;
  let selectedFile = null;
  let selectionId = 0;
  let imageUrl;
  let receivedData = false;
  let stopped = false;
  let restarting = false;
  let restartBootId = null;
  let lastBootId = null;
  let restartStartedAt = 0;
  const palette = getComputedStyle(document.documentElement);
  const rgb = name => palette.getPropertyValue(name).trim().slice(1).match(/../g).map(part => parseInt(part, 16));
  const temperatureStops = [
    { value: 30, color: rgb('--temp-cold') },
    { value: 60, color: rgb('--temp-mid') },
    { value: 90, color: rgb('--temp-hot') }
  ];

  function temperatureColor(value) {
    const clamped = Math.max(30, Math.min(90, value));
    const lower = clamped <= 60 ? temperatureStops[0] : temperatureStops[1];
    const upper = clamped <= 60 ? temperatureStops[1] : temperatureStops[2];
    const fraction = (clamped - lower.value) / (upper.value - lower.value);
    return `rgb(${lower.color.map((channel, i) => Math.round(channel + (upper.color[i] - channel) * fraction)).join(', ')})`;
  }

  function updateControls() {
    restartButton.disabled = !receivedData || uploading || restarting;
    byId('confirm-restart').disabled = uploading || restarting;
    fileInput.disabled = uploading || restarting;
    uploadButton.disabled = !selectedFile || uploading || restarting;
  }

  function connection(label, detail, state) {
    byId('connection-label').textContent = label;
    byId('connection-detail').textContent = detail;
    byId('device-connection').dataset.state = state;
  }

  function render(data) {
    if (!data || !Array.isArray(data.temperatures) || data.temperatures.length !== 6 ||
        !data.wifi || typeof data.wifi.connected !== 'boolean' || typeof data.sampled !== 'boolean' ||
        typeof data.boot_id !== 'string' || !data.boot_id ||
        (data.sampled && (!Number.isFinite(data.sample_age_ms) || data.sample_age_ms < 0)))
      throw new Error('Neveljaven odgovor naprave.');
    const stale = !data.sampled || data.sample_age_ms > 15000;
    scene.classList.toggle('is-stale', stale);
    let invalid = 0;
    fields.forEach(field => {
      const value = data.temperatures[Number(field.dataset.temp)];
      const valid = data.sampled && typeof value === 'number' && Number.isFinite(value) && value >= -55 && value <= 125;
      field.textContent = valid ? value.toFixed(1).replace('.', ',') : '—';
      field.parentElement.style.color = valid ? temperatureColor(value) : '#173c61';
      field.parentElement.classList.toggle('is-invalid', !valid);
      field.parentElement.title = valid ? '' : 'Ni veljavne meritve senzorja';
      if (!valid) invalid++;
    });
    byId('wifi-ssid').textContent = data.wifi.ssid || 'Ni povezave';
    byId('wifi-ip').textContent = data.wifi.ip || '—';
    byId('wifi-state').textContent = data.wifi.connected ? 'Povezano' : 'Ni povezave';
    byId('wifi-pill').textContent = data.wifi.connected ? 'Povezano' : 'Brez povezave';
    const wifiIcon = document.querySelector('.lcd-wifi');
    wifiIcon.dataset.connected = String(data.wifi.connected);
    wifiIcon.setAttribute('aria-label', data.wifi.connected ? 'Wi-Fi povezan' : 'Wi-Fi ni povezan');
    const rssi = data.wifi.connected && Number.isFinite(data.wifi.rssi) ? data.wifi.rssi : null;
    byId('wifi-rssi').textContent = rssi === null ? '—' : String(rssi).replace('-', '−');
    const strength = rssi === null ? 0 : rssi >= -50 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : 1;
    byId('signal-quality').textContent = ['Ni povezave', 'Šibek signal', 'Zadovoljiv signal', 'Dober signal', 'Odličen signal'][strength];
    [...byId('signal-bars').children].forEach((bar, index) => {
      bar.classList.toggle('active', index < strength);
      bar.classList.toggle('inactive', index >= strength);
    });
    byId('firmware-version').textContent = data.firmware || '—';
    byId('mqtt-state').textContent = data.mqtt_connected ? 'Povezano' : 'Ni povezave';
    if (Number.isFinite(data.uptime_s) && data.uptime_s >= 0) {
      const minutes = Math.floor(data.uptime_s / 60);
      byId('device-uptime').textContent = `${Math.floor(minutes / 1440)} d ${Math.floor(minutes / 60) % 24} h ${minutes % 60} min`;
    }
    if (data.sampled) {
      byId('last-update').textContent = new Date(Date.now() - data.sample_age_ms).toLocaleTimeString('sl-SI', {
        hour: '2-digit', minute: '2-digit', second: '2-digit'
      });
    } else byId('last-update').textContent = '—';
    if (stale) {
      connection(data.sampled ? 'Meritve niso sveže' : 'Čakam na prvo meritev', 'Povezava z napravo deluje', 'waiting');
      byId('refresh-status').textContent = data.sampled ? 'Prikazane so starejše meritve. Čakam na nove podatke.' : 'Senzorji še niso opravili prve meritve.';
    } else {
      connection(data.wifi.connected ? 'Naprava je povezana' : 'Wi-Fi ni povezan',
        invalid ? `${invalid} merilnih mest brez veljavne temperature` : 'Vsi podatki so osveženi',
        data.wifi.connected ? 'ok' : 'error');
      byId('refresh-status').textContent = 'Podatki se osvežujejo vsakih 5 sekund.';
    }
    receivedData = true;
    lastBootId = data.boot_id;
    if (restarting) {
      if (restartBootId && data.boot_id !== restartBootId) {
        restarting = false;
        byId('restart-feedback').textContent = 'ESP je ponovno zagnan.';
      } else {
        connection('Čakam na ponovni zagon', 'Ponovno vzpostavljam povezavo z napravo', 'waiting');
        byId('refresh-status').textContent = 'Čakam na nov zagon naprave …';
      }
    }
    updateControls();
  }

  async function poll() {
    clearTimeout(pollTimer);
    if (stopped || polling || uploading || document.hidden) return;
    polling = true;
    const controller = new AbortController();
    pollController = controller;
    const timeout = setTimeout(() => controller.abort(), 4500);
    try {
      const response = await fetch('/api/status', { cache: 'no-store', signal: controller.signal });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const data = await response.json();
      if (!uploading && !stopped) render(data);
    } catch (error) {
      if (!uploading && !stopped && !document.hidden) {
        scene.classList.add('is-stale');
        if (restarting) {
          connection('Čakam na ponovni zagon', 'Ponovno vzpostavljam povezavo z napravo', 'waiting');
          byId('refresh-status').textContent = 'Čakam na nov zagon naprave …';
        } else {
          connection('Ni povezave z napravo', receivedData ? 'Prikazani so zadnji prejeti podatki' : 'Čakam na odgovor ESP-ja', 'error');
          byId('refresh-status').textContent = 'Podatki niso sveži. Samodejno poskušam znova.';
        }
      }
    } finally {
      clearTimeout(timeout);
      polling = false;
      if (pollController === controller) pollController = null;
      if (restarting && Date.now() - restartStartedAt > 60000) {
        restarting = false;
        byId('restart-feedback').textContent = 'Ponovni zagon ni potrjen. Preveri napravo pred ponovitvijo.';
        connection('Ponovni zagon ni potrjen', 'Preveri povezavo z napravo', 'error');
        updateControls();
      }
      if (!stopped && !uploading && !document.hidden) pollTimer = setTimeout(poll, 5000);
    }
  }

  function jpegDimensions(buffer) {
    const view = new DataView(buffer);
    if (view.byteLength < 4 || view.getUint16(0) !== 0xffd8) throw new Error('Izberi sliko JPG.');
    let offset = 2;
    while (offset + 4 <= view.byteLength) {
      if (view.getUint8(offset++) !== 0xff) throw new Error('Neveljavna JPG slika.');
      while (offset < view.byteLength && view.getUint8(offset) === 0xff) offset++;
      if (offset + 3 > view.byteLength) break;
      const marker = view.getUint8(offset++);
      if (marker === 0xc2) throw new Error('Progressive JPG ni podprt. Izvozi baseline JPG.');
      if (marker === 0xda || marker === 0xd9) break;
      const length = view.getUint16(offset);
      if (length < 2 || offset + length > view.byteLength) break;
      if (marker === 0xc0 && length >= 8) return { height: view.getUint16(offset + 3), width: view.getUint16(offset + 5) };
      offset += length;
    }
    throw new Error('Potrebna je baseline JPG slika.');
  }

  byId('open-settings').addEventListener('click', () => dialog.showModal());
  restartButton.addEventListener('click', () => restartDialog.showModal());
  ['close-restart', 'cancel-restart'].forEach(id => byId(id).addEventListener('click', () => restartDialog.close()));
  restartDialog.addEventListener('click', event => {
    const rect = restartDialog.getBoundingClientRect();
    if (event.target === restartDialog && (event.clientX < rect.left || event.clientX > rect.right || event.clientY < rect.top || event.clientY > rect.bottom)) restartDialog.close();
  });
  byId('confirm-restart').addEventListener('click', async () => {
    if (uploading || restarting || !receivedData) return;
    restarting = true;
    restartBootId = lastBootId;
    restartStartedAt = Date.now();
    restartDialog.close();
    updateControls();
    clearTimeout(pollTimer);
    if (pollController) pollController.abort();
    byId('restart-feedback').textContent = 'Pošiljam zahtevo za ponovni zagon …';
    const request = new AbortController();
    const timeout = setTimeout(() => request.abort(), 7000);
    try {
      const response = await fetch('/api/restart', {
        method: 'POST', headers: { 'X-Requested-With': 'ZalogovnikiLCD', 'Content-Type': 'text/plain' },
        body: 'RESTART', signal: request.signal
      });
      if (!response.ok) {
        restarting = false;
        const result = await response.json();
        throw new Error(result.message || 'Zahteva za ponovni zagon je bila zavrnjena.');
      }
      const result = await response.json();
      if (!result.ok || typeof result.boot_id !== 'string' || !result.boot_id)
        throw new Error('Naprava ni potrdila zahteve. Čakam na stanje naprave.');
      restartBootId = result.boot_id;
      byId('restart-feedback').textContent = 'Zahteva je sprejeta. Čakam na nov zagon naprave …';
    } catch (error) {
      byId('restart-feedback').textContent = restarting ? 'Odgovor ni potrjen. Čakam na stanje naprave; ukaza ne ponavljam.' : error.message;
    } finally {
      clearTimeout(timeout);
      updateControls();
      clearTimeout(pollTimer);
      if (!stopped && !document.hidden) pollTimer = setTimeout(poll, 1000);
    }
  });
  ['close-settings', 'done-settings'].forEach(id => byId(id).addEventListener('click', () => dialog.close()));
  dialog.addEventListener('click', event => {
    const rect = dialog.getBoundingClientRect();
    if (event.target === dialog && (event.clientX < rect.left || event.clientX > rect.right || event.clientY < rect.top || event.clientY > rect.bottom)) dialog.close();
  });
  byId('current-image').addEventListener('error', () => { byId('current-image').alt = 'Trenutnega ozadja ni mogoče prebrati.'; });

  fileInput.addEventListener('change', async () => {
    const ticket = ++selectionId;
    selectedFile = null;
    uploadButton.disabled = true;
    imagePreview.hidden = true;
    imagePreview.removeAttribute('src');
    byId('file-status').textContent = '';
    if (imageUrl) { URL.revokeObjectURL(imageUrl); imageUrl = undefined; }
    const file = fileInput.files[0];
    if (!file) return;
    try {
      if (file.size > 128 * 1024) throw new Error('Slika je prevelika. Največ 128 KB.');
      const dimensions = jpegDimensions(await file.arrayBuffer());
      if (dimensions.width !== 480 || dimensions.height !== 320) throw new Error('Slika mora biti velika točno 480 × 320 px.');
      if (ticket !== selectionId) return;
      imageUrl = URL.createObjectURL(file);
      imagePreview.src = imageUrl;
      await imagePreview.decode();
      if (ticket !== selectionId) return;
      imagePreview.hidden = false;
      selectedFile = file;
      updateControls();
      byId('file-status').textContent = `480 × 320 px · ${(file.size / 1024).toFixed(1)} KB · pripravljeno za nalaganje`;
    } catch (error) {
      if (ticket === selectionId) byId('file-status').textContent = error.message;
    }
  });

  uploadButton.addEventListener('click', async () => {
    if (!selectedFile || uploading || restarting) return;
    uploading = true;
    clearTimeout(pollTimer);
    if (pollController) pollController.abort();
    updateControls();
    byId('file-status').textContent = 'Nalagam sliko na LCD …';
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 20000);
    try {
      const body = new FormData();
      body.append('upload', selectedFile);
      const response = await fetch('/upload', {
        method: 'POST', headers: { 'X-Requested-With': 'ZalogovnikiLCD' }, body, signal: controller.signal
      });
      const result = await response.json();
      if (!response.ok || !result.ok) throw new Error(result.message || 'Slike ni bilo mogoče naložiti.');
      byId('file-status').textContent = result.message;
      byId('current-image').src = `/api/lcd-image?t=${Date.now()}`;
      selectedFile = null;
    } catch (error) {
      byId('file-status').textContent = error.name === 'AbortError' ? 'Naprava ni pravočasno odgovorila. Preveri prikaz na LCD-ju, preden ponoviš prenos.' : error.message;
    } finally {
      clearTimeout(timeout);
      uploading = false;
      updateControls();
      pollTimer = setTimeout(poll, 250);
    }
  });

  document.addEventListener('visibilitychange', () => {
    clearTimeout(pollTimer);
    if (document.hidden) { if (pollController) pollController.abort(); }
    else pollTimer = setTimeout(poll, 0);
  });
  window.addEventListener('pagehide', () => {
    stopped = true;
    clearTimeout(pollTimer);
    if (pollController) pollController.abort();
    if (imageUrl) URL.revokeObjectURL(imageUrl);
  });
  window.addEventListener('pageshow', event => {
    if (event.persisted) { stopped = false; poll(); }
  });
  poll();
})();
