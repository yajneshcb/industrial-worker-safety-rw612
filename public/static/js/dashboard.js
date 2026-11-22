(() => {
  'use strict';
  const $ = (selector) => document.querySelector(selector);
  const fmt = (value, digits = 1) => Number.isFinite(Number(value)) ? Number(value).toFixed(digits) : '—';
  const escapeHTML = (value) => String(value ?? '').replace(/[&<>"']/g, (char) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[char]));

  const state = {
    data: null,
    lastSafetyState: null,
    lastHazard: null,
    incidents: [],
    audioEnabled: false,
    audioContext: null,
    alertTimer: null,
    emergencyOsc: null,
    emergencyGain: null,
    toastTimer: null,
    pollTimer: null,
    serialPort: null,
    serialReader: null,
    serialConnected: false
  };

  const themeColors = {
    dark: { temperature: '#45d292', gas: '#55cbb1', motion: '#a193ff', pointStroke: '#101720', placeholder: '#334155' },
    light: { temperature: '#16a34a', gas: '#0d9488', motion: '#7c3aed', pointStroke: '#ffffff', placeholder: '#cbd5e1' }
  };

  function getCurrentTheme() {
    return document.documentElement.getAttribute('data-theme') === 'light' ? 'light' : 'dark';
  }

  function applyTheme(theme) {
    document.documentElement.setAttribute('data-theme', theme);
    const meta = $('meta[name="theme-color"]');
    if (meta) meta.setAttribute('content', theme === 'light' ? '#f8fafc' : '#090c11');
    const label = $('#themeToggleLabel');
    if (label) label.textContent = theme === 'light' ? 'DARK' : 'LIGHT';
    try {
      localStorage.setItem('rw612-theme', theme);
    } catch (_) { }

    if (state.data) {
      renderStatus(state.data);
    }
  }

  function localClock() {
    const now = new Date();
    // 12-hour AM/PM format without milliseconds: e.g. "05:42:15 PM"
    const timeStr = now.toLocaleTimeString('en-US', {
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
      hour12: true
    }).toUpperCase();
    $('#liveClock').textContent = timeStr;
    $('#liveDate').textContent = now.toLocaleDateString('en-US', {
      weekday: 'short',
      year: 'numeric',
      month: 'short',
      day: '2-digit'
    }).toUpperCase();
  }

  function showToast(message) {
    const toast = $('#toast');
    toast.textContent = message;
    toast.classList.add('show');
    clearTimeout(state.toastTimer);
    state.toastTimer = setTimeout(() => toast.classList.remove('show'), 2600);
  }

  function formatTime(iso) {
    if (!iso) return '—';
    const date = new Date(iso);
    if (Number.isNaN(date.getTime())) return String(iso);
    return date.toLocaleTimeString(undefined, { hour12: false }) + '.' + String(date.getMilliseconds()).padStart(3, '0');
  }

  function drawEmptySparkline(svg, placeholderText) {
    if (!svg) return;
    const width = 280, height = 58;
    const theme = getCurrentTheme();
    const strokeColor = themeColors[theme].placeholder;
    svg.innerHTML = `
      <line x1="4" y1="${height / 2}" x2="${width - 4}" y2="${height / 2}" stroke="${strokeColor}" stroke-dasharray="4 5" stroke-width="1.2" opacity="0.6"/>
      <text x="${width / 2}" y="${height / 2 + 14}" fill="${strokeColor}" font-size="9" font-family="'DM Mono', monospace" text-anchor="middle" letter-spacing="0.08em">${escapeHTML(placeholderText || 'AWAITING SENSOR LINK')}</text>
    `;
  }

  function drawSparkline(svg, values, color, threshold = null, pointStroke = null) {
    if (!svg) return;
    const width = 280, height = 58, pad = 4;
    const list = (values || []).slice(-40).map(Number).filter(Number.isFinite);
    if (!list.length) {
      drawEmptySparkline(svg, 'AWAITING STREAM');
      return;
    }
    let low = Math.min(...list), high = Math.max(...list);
    if (threshold !== null) { low = Math.min(low, threshold); high = Math.max(high, threshold); }
    if (high - low < 0.01) { high += 0.5; low -= 0.5; }
    const x = (i) => pad + (list.length <= 1 ? 0 : i * (width - pad * 2) / (list.length - 1));
    const y = (v) => height - pad - ((v - low) / (high - low)) * (height - pad * 2);
    const points = list.map((v, i) => `${x(i).toFixed(1)},${y(v).toFixed(1)}`).join(' ');
    const area = `${pad},${height - pad} ${points} ${width - pad},${height - pad}`;
    const thresholdLine = threshold === null ? '' : `<line x1="${pad}" y1="${y(threshold)}" x2="${width - pad}" y2="${y(threshold)}" stroke="#dba93e" stroke-width="1" stroke-dasharray="3 4" opacity=".75" vector-effect="non-scaling-stroke"/>`;
    const stroke = pointStroke || (getCurrentTheme() === 'light' ? '#ffffff' : '#101720');
    svg.innerHTML = `
      <defs>
        <linearGradient id="grad-${svg.id}" x1="0" x2="0" y1="0" y2="1">
          <stop offset="0" stop-color="${color}" stop-opacity=".55"/>
          <stop offset="1" stop-color="${color}" stop-opacity="0"/>
        </linearGradient>
      </defs>
      ${thresholdLine}
      <polygon class="spark-area" points="${area}" fill="url(#grad-${svg.id})"/>
      <polyline class="spark-line" points="${points}" stroke="${color}"/>
      <circle class="spark-point" cx="${x(list.length - 1)}" cy="${y(list[list.length - 1])}" r="2.5" fill="${color}" stroke="${stroke}"/>
    `;
  }

  function setAxisBar(selector, value) {
    const el = $(selector);
    if (!el) return;
    if (value === null || value === undefined || !Number.isFinite(Number(value))) {
      el.style.width = '0%';
      return;
    }
    el.style.width = `${Math.min(100, Math.max(3, Math.abs(Number(value)) * 32))}%`;
  }

  function signed(value) {
    const n = Number(value);
    return Number.isFinite(n) ? `${n >= 0 ? '+' : ''}${n.toFixed(2)}` : '—';
  }

  function renderStatus(data) {
    state.data = data;
    const isConnected = data.link_state === 'Connected';
    const hasTelemetry = Boolean(data.has_telemetry && data.temperature_c !== null);
    const isActive = Boolean(isConnected && hasTelemetry);

    const badge = $('#connectionBadge');
    const banner = $('#masterBanner');
    const themeKey = getCurrentTheme();
    const c = themeColors[themeKey];

    // ==========================================
    // CASE 1: OFFLINE / DISCONNECTED STATE
    // ==========================================
    if (!isActive) {
      // Top connection badge
      badge.dataset.link = 'Disconnected';
      $('#connectionText').textContent = 'DISCONNECTED';

      // Master status banner
      banner.className = 'master-banner state-offline';
      $('#stateLabel').textContent = 'OFFLINE';
      $('#bannerKicker').textContent = 'EDGE CLASSIFIER STANDBY';
      $('#bannerTitle').textContent = 'NODE OFFLINE — AWAITING SENSOR TELEMETRY';
      $('#bannerDetail').textContent = 'Awaiting real-time telemetry over Wi-Fi / USB VCOM.';

      // Section 01: Ambient Temperature card
      $('#temperatureValue').textContent = '—';
      $('#temperatureGaugeValue').textContent = '—';
      $('#temperatureArc').style.strokeDasharray = '0 383';
      $('#temperatureArc').className = 'gauge-value';
      $('#temperatureState').textContent = 'NO SIGNAL';
      $('#temperatureState').className = 'mini-state';
      $('#temperatureMin').textContent = '—';
      $('#temperatureMax').textContent = '—';
      drawEmptySparkline($('#temperatureSpark'), 'Awaiting P3T1755 sensor link…');

      // Section 01: Motion & Impact card
      $('#accelValue').textContent = '—';
      const motionCondition = $('#motionCondition');
      motionCondition.className = 'motion-condition';
      motionCondition.lastElementChild.textContent = 'NO SIGNAL';
      $('#axisX').textContent = '—';
      $('#axisY').textContent = '—';
      $('#axisZ').textContent = '—';
      setAxisBar('#axisBarX', null);
      setAxisBar('#axisBarY', null);
      setAxisBar('#axisBarZ', null);
      $('#fallCount').textContent = '—';
      drawEmptySparkline($('#motionSpark'), 'Awaiting MPU-6050 IMU link…');

      // Section 01: Gas & Smoke card
      $('#gasValue').textContent = '—';
      const gasQuality = $('#gasQuality');
      gasQuality.textContent = 'OFFLINE';
      gasQuality.className = 'aqi-label';
      const gasNeedle = $('#gasNeedle');
      gasNeedle.className = 'gas-needle';
      gasNeedle.style.left = '0%';
      $('#gasSensorLive').className = 'sensor-live';
      drawEmptySparkline($('#gasSpark'), 'Awaiting MQ-2 sensor link…');

      // Section 01: Actuator Hardware Twin
      $('#ledNormal').className = 'hardware-led green';
      $('#ledWarning').className = 'hardware-led amber';
      $('#ledEmergency').className = 'hardware-led red';
      $('#buzzerVisual').className = 'buzzer-dial';
      $('#buzzerStatus').textContent = 'BUZZER OFF';
      $('#buzzerMode').textContent = 'AWAITING LINK';
      $('#actuatorState').textContent = 'OFFLINE';
      $('#actuatorState').className = 'offline';

      // Section 02: Incident Log
      renderIncidents(data.incidents || []);

      // Section 03: Node Link Diagnostics
      $('#nodeIndicator').className = 'node-indicator';
      $('#nodeIndicatorText').textContent = 'OFFLINE';
      $('#linkStateValue').textContent = 'DISCONNECTED';
      $('#linkSourceValue').textContent = '—';
      $('#nodePacketCount').textContent = '0';
      $('#messageIdValue').textContent = '—';
      $('#queueValue').innerHTML = '0 <small>PACKETS</small>';
      $('#queueBar').style.width = '0%';
      $('#serialStatusValue').textContent = (data.serial_status || 'Disabled').toUpperCase();
      $('#nodeFootnote').textContent = 'Node disconnected. Awaiting telemetry stream from NXP FRDM-RW612.';
      $('#packetCount').textContent = '—';
      $('#sampleAge').textContent = '—';

      stopAudio();
      updateAudioForState('OFFLINE');
      state.lastSafetyState = 'OFFLINE';
      state.lastHazard = 'NONE';
      return;
    }

    // ==========================================
    // CASE 2: ACTIVE HARDWARE TELEMETRY
    // ==========================================
    const safety = ['NORMAL', 'WARNING', 'EMERGENCY'].includes(data.safety_state) ? data.safety_state : 'NORMAL';
    const hazard = String(data.hazard_type || 'NONE');

    // Connection badge
    const link = ['Connected', 'Reconnecting', 'Disconnected', 'Buffered'].includes(data.link_state) ? data.link_state : 'Connected';
    const badgeLink = data.buffered || Number(data.offline_queue_count || 0) > 0 ? 'Buffered' : link;
    badge.dataset.link = badgeLink;
    $('#connectionText').textContent = badgeLink === 'Connected' ? 'LIVE · CONNECTED' : badgeLink === 'Buffered' ? 'BUFFERED' : badgeLink.toUpperCase();

    // Banner
    banner.className = `master-banner state-${safety.toLowerCase()}`;
    $('#stateLabel').textContent = safety;
    const banners = {
      NORMAL: ['EDGE CLASSIFIER ONLINE', 'SYSTEM NORMAL — ALL WORKER PARAMETERS SECURE', 'No active hazards detected. Continuous telemetry is monitored locally and mirrored here.'],
      WARNING: ['ADVISORY · OPERATOR ATTENTION', 'ADVISORY WARNING — ELEVATED HAZARD DETECTED', detailFor(hazard)],
      EMERGENCY: ['CRITICAL · IMMEDIATE RESPONSE', 'CRITICAL EMERGENCY — IMMEDIATE ACTION REQUIRED', detailFor(hazard)]
    };
    const copy = banners[safety];
    $('#bannerKicker').textContent = copy[0];
    $('#bannerTitle').textContent = copy[1];
    $('#bannerDetail').textContent = copy[2];

    // Card 1: Temperature
    const temp = Number(data.temperature_c);
    $('#temperatureValue').textContent = fmt(temp, 1);
    $('#temperatureGaugeValue').textContent = fmt(temp, 1);
    const tempPercent = Math.max(0, Math.min(100, (temp || 0) / 60 * 100));
    const circumference = 2 * Math.PI * 61;
    const arc = $('#temperatureArc');
    const gaugeLength = circumference * 0.75;
    arc.style.strokeDasharray = `${(gaugeLength * tempPercent / 100).toFixed(1)} ${circumference.toFixed(1)}`;
    arc.className = `gauge-value${temp >= 50 ? ' emergency' : temp >= 40 ? ' warning' : ''}`;
    const tempCondition = temp >= 50 ? 'emergency' : temp >= 40 ? 'warning' : 'normal';
    $('#temperatureState').textContent = temp >= 50 ? 'CRITICAL' : temp >= 40 ? 'WARNING' : 'NORMAL';
    $('#temperatureState').className = `mini-state ${tempCondition}`;
    $('#temperatureMin').textContent = data.temperature_min_c !== null ? `${fmt(data.temperature_min_c, 1)}°` : '—';
    $('#temperatureMax').textContent = data.temperature_max_c !== null ? `${fmt(data.temperature_max_c, 1)}°` : '—';

    // Card 2: Motion
    const accel = Number(data.accel_magnitude_g);
    $('#accelValue').textContent = fmt(accel, 2);
    $('#axisX').textContent = signed(data.accel_x);
    $('#axisY').textContent = signed(data.accel_y);
    $('#axisZ').textContent = signed(data.accel_z);
    setAxisBar('#axisBarX', data.accel_x);
    setAxisBar('#axisBarY', data.accel_y);
    setAxisBar('#axisBarZ', data.accel_z);
    $('#fallCount').textContent = data.fall_event_count ?? 0;
    const motionCondition = $('#motionCondition');
    const impact = accel > 2.5 || hazard === 'FALL_DETECTED';
    motionCondition.className = `motion-condition ${impact ? 'impact' : 'stable'}`;
    motionCondition.lastElementChild.textContent = impact ? 'IMPACT EVENT' : 'STABLE';

    // Card 3: Gas
    const gas = Number(data.gas_ppm || 0);
    $('#gasValue').textContent = fmt(gas, gas % 1 ? 1 : 0);
    const quality = $('#gasQuality');
    quality.textContent = gas >= 600 ? 'DANGER ZONE' : gas >= 300 ? 'ELEVATED' : 'CLEAN AIR';
    quality.className = `aqi-label ${gas >= 600 ? 'danger' : gas >= 300 ? 'warning' : 'clean'}`;
    const needle = $('#gasNeedle');
    needle.style.left = `${Math.min(100, Math.max(0, gas / 800 * 100))}%`;
    needle.className = `gas-needle active ${gas >= 600 ? 'danger' : gas >= 300 ? 'warning' : ''}`;
    $('#gasSensorLive').className = 'sensor-live active';

    // Card 4: Actuator Mirror
    renderActuators(data);

    // Section 02 & 03
    renderLink(data);
    renderIncidents(data.incidents || []);

    // Sparklines
    const samples = (data.history || []).filter((item) => item.temperature_c !== null);
    if (samples.length) {
      drawSparkline($('#temperatureSpark'), samples.map((item) => item.temperature_c), c.temperature, 40, c.pointStroke);
      drawSparkline($('#gasSpark'), samples.map((item) => item.gas_ppm), c.gas, 300, c.pointStroke);
      drawSparkline($('#motionSpark'), samples.map((item) => item.accel_magnitude_g), c.motion, 2.5, c.pointStroke);
    } else {
      drawEmptySparkline($('#temperatureSpark'), 'Awaiting P3T1755 samples…');
      drawEmptySparkline($('#gasSpark'), 'Awaiting MQ-2 samples…');
      drawEmptySparkline($('#motionSpark'), 'Awaiting MPU-6050 samples…');
    }

    // Packet stats
    $('#packetCount').textContent = String(data.packets_received || 0).padStart(6, '0');
    const age = data.received_at_ms ? Math.max(0, Date.now() - Number(data.received_at_ms)) : null;
    $('#sampleAge').textContent = age === null ? '—' : age < 1000 ? `${age}ms` : `${(age / 1000).toFixed(1)}s`;

    updateAudioForState(safety);
    state.lastSafetyState = safety;
    state.lastHazard = hazard;
  }

  function detailFor(hazard) {
    const copy = {
      HIGH_TEMP: 'Ambient temperature is above the advisory threshold. Check ventilation and worker exposure.',
      GAS_LEAK: 'Combustible gas / smoke concentration is elevated. Verify the source and follow plant response procedure.',
      FALL_DETECTED: 'Free-fall followed by a high-impact signature. Verify worker condition immediately.',
      MANUAL_SOS: 'Worker activated the manual emergency SOS button. Initiate the plant response team.',
      SENSOR_FAULT: 'A sensor input channel is faulted or disconnected. Check hardware wiring.',
      NONE: 'The edge node reports an elevated safety condition. Verify sensor readings.'
    };
    return copy[hazard] || `Active hazard reported by the edge node: ${hazard}.`;
  }

  function renderActuators(data) {
    const normal = $('#ledNormal'), warning = $('#ledWarning'), emergency = $('#ledEmergency');
    normal.className = `hardware-led green${data.led_normal ? ' active' : ''}`;
    warning.className = `hardware-led amber${data.led_warning ? ' active' : ''}${data.led_warning_mode === 'PULSE_1HZ' ? ' pulse' : ''}`;
    emergency.className = `hardware-led red${data.led_emergency ? ' active strobe' : ''}`;

    const buzzer = $('#buzzerVisual');
    buzzer.className = `buzzer-dial${data.safety_state === 'EMERGENCY' ? ' active' : data.safety_state === 'WARNING' ? ' warning' : ''}`;
    $('#buzzerStatus').textContent = data.buzzer_active ? 'BUZZER ACTIVE' : 'BUZZER OFF';
    $('#buzzerMode').textContent = data.buzzer_mode === 'WARNING_BEEP_200MS' ? '200MS WARNING BEEP' : data.buzzer_mode === 'CONTINUOUS_RAPID_ALARM' ? 'CONTINUOUS RAPID ALARM' : 'OUTPUT IDLE';

    const actuator = $('#actuatorState');
    actuator.textContent = data.safety_state === 'EMERGENCY' ? 'EMERGENCY' : data.safety_state === 'WARNING' ? 'WARNING' : 'NOMINAL';
    actuator.className = data.safety_state === 'EMERGENCY' ? 'emergency' : data.safety_state === 'WARNING' ? 'warning' : 'nominal';
  }

  function renderLink(data) {
    const link = ['Connected', 'Reconnecting', 'Disconnected', 'Buffered'].includes(data.link_state) ? data.link_state : 'Disconnected';
    const node = $('#nodeIndicator');
    node.className = `node-indicator ${link.toLowerCase()}`;
    $('#nodeIndicatorText').textContent = link.toUpperCase();

    $('#linkStateValue').textContent = link.toUpperCase();
    $('#linkSourceValue').textContent = data.link_source || '—';
    $('#nodePacketCount').textContent = data.packets_received || 0;
    $('#messageIdValue').textContent = data.id ?? '—';
    const queued = Math.max(0, Number(data.offline_queue_count || 0));
    $('#queueValue').innerHTML = `${queued} <small>PACKETS</small>`;
    $('#queueBar').style.width = `${Math.min(100, queued / 1000 * 100)}%`;
    $('#serialStatusValue').textContent = (data.serial_status || 'Disabled').toUpperCase();
    $('#nodeFootnote').textContent = link === 'Connected' ? `Telemetry source ${data.link_source || 'unknown'} · latest message ID: ${data.id ?? 'pending'}.` : link === 'Reconnecting' ? 'Wireless link interrupted — buffered packets may be queued on the edge node.' : link === 'Buffered' ? `${queued} packet(s) buffered locally while the wireless link is unavailable.` : 'Awaiting node telemetry packet.';
  }

  function renderIncidents(incidents) {
    state.incidents = incidents;
    const filter = $('#hazardFilter').value;
    const visible = filter === 'ALL' ? incidents : incidents.filter((item) => item.hazard_type === filter);
    $('#eventCount').textContent = incidents.length;
    const body = $('#incidentRows');
    if (!visible.length) {
      body.innerHTML = `<tr><td colspan="4" class="empty-row">${incidents.length ? 'No events match this hazard filter.' : 'Waiting for FRDM-RW612 telemetry events…'}</td></tr>`;
      return;
    }
    body.innerHTML = visible.slice(0, 100).map((item) => {
      const severity = String(item.severity || 'INFO').toLowerCase();
      const hazard = item.hazard_type === 'NONE' ? 'SYSTEM / CLEAR' : item.hazard_type || 'SYSTEM';
      return `<tr><td>${escapeHTML(formatTime(item.timestamp))}</td><td><span class="severity-tag ${escapeHTML(severity)}">${escapeHTML(severity.toUpperCase())}</span></td><td>${escapeHTML(hazard)} <span style="opacity:0.75">— ${escapeHTML(item.message || '')}</span></td><td>${escapeHTML(item.packet_id ?? '—')}</td></tr>`;
    }).join('');
  }

  async function fetchStatus() {
    try {
      const response = await fetch('/api/status', { cache: 'no-store', headers: { 'Accept': 'application/json' } });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      renderStatus(await response.json());
    } catch (error) {
      $('#connectionBadge').dataset.link = 'Reconnecting';
      $('#connectionText').textContent = 'RECONNECTING';
      $('#nodeIndicator').className = 'node-indicator reconnecting';
      $('#nodeIndicatorText').textContent = 'RECONNECTING';
      $('#nodeFootnote').textContent = 'Dashboard API unavailable — retrying connection.';
    }
  }

  function downloadFile(filename, text, mime) {
    const blob = new Blob([text], { type: mime });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement('a');
    anchor.href = url; anchor.download = filename; document.body.appendChild(anchor); anchor.click(); anchor.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }

  function exportCSV() {
    const filter = $('#hazardFilter').value;
    const rows = filter === 'ALL' ? state.incidents : state.incidents.filter((item) => item.hazard_type === filter);
    const columns = ['event_id', 'timestamp', 'severity', 'hazard_type', 'safety_state', 'message', 'packet_id', 'source'];
    const quote = (value) => `"${String(value ?? '').replaceAll('"', '""')}"`;
    const csv = [columns.join(','), ...rows.map((row) => columns.map((key) => quote(row[key])).join(','))].join('\r\n');
    downloadFile(`rw612-incidents-${new Date().toISOString().slice(0, 10)}.csv`, csv, 'text/csv;charset=utf-8');
    showToast(`Exported ${rows.length} event(s) to CSV.`);
  }

  function exportJSON() {
    downloadFile(`rw612-incidents-${new Date().toISOString().slice(0, 10)}.json`, JSON.stringify(state.incidents, null, 2), 'application/json');
    showToast(`Exported ${state.incidents.length} event(s) to JSON.`);
  }

  function stopAudio() {
    clearInterval(state.alertTimer); state.alertTimer = null;
    if (state.emergencyOsc) {
      try { state.emergencyOsc.stop(); } catch (_) { }
      state.emergencyOsc.disconnect(); state.emergencyOsc = null;
    }
    if (state.emergencyGain) { state.emergencyGain.disconnect(); state.emergencyGain = null; }
    $('#audioButton').classList.remove('alerting');
  }

  function playWarningChirp() {
    if (!state.audioContext || state.audioContext.state !== 'running') return;
    const osc = state.audioContext.createOscillator();
    const gain = state.audioContext.createGain();
    osc.type = 'triangle'; osc.frequency.setValueAtTime(740, state.audioContext.currentTime);
    gain.gain.setValueAtTime(0.0001, state.audioContext.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.08, state.audioContext.currentTime + 0.018);
    gain.gain.exponentialRampToValueAtTime(0.0001, state.audioContext.currentTime + 0.18);
    osc.connect(gain); gain.connect(state.audioContext.destination); osc.start(); osc.stop(state.audioContext.currentTime + 0.2);
  }

  function startWarning() {
    stopAudio();
    playWarningChirp();
    state.alertTimer = setInterval(playWarningChirp, 850);
  }

  function startEmergency() {
    stopAudio();
    if (!state.audioContext || state.audioContext.state !== 'running') return;
    const osc = state.audioContext.createOscillator();
    const gain = state.audioContext.createGain();
    osc.type = 'sawtooth'; osc.frequency.setValueAtTime(440, state.audioContext.currentTime);
    gain.gain.setValueAtTime(0.0001, state.audioContext.currentTime);
    gain.gain.exponentialRampToValueAtTime(0.075, state.audioContext.currentTime + 0.12);
    osc.connect(gain); gain.connect(state.audioContext.destination); osc.start();
    state.emergencyOsc = osc; state.emergencyGain = gain;
    let high = false;
    state.alertTimer = setInterval(() => {
      if (!state.emergencyOsc || !state.audioContext) return;
      high = !high;
      state.emergencyOsc.frequency.setTargetAtTime(high ? 760 : 440, state.audioContext.currentTime, 0.07);
    }, 420);
  }

  function updateAudioForState(safety) {
    const button = $('#audioButton');
    button.classList.toggle('enabled', state.audioEnabled);
    button.classList.toggle('alerting', state.audioEnabled && (safety === 'WARNING' || safety === 'EMERGENCY'));
    $('#audioButtonText').textContent = state.audioEnabled
      ? (safety === 'NORMAL' || safety === 'OFFLINE' ? 'AUDIO ARMED · CLICK TO MUTE' : 'ACK / MUTE ACTIVE ALERT')
      : 'AUDIO MUTED · ENABLE';
    if (!state.audioEnabled) { stopAudio(); return; }
    if (safety !== state.lastSafetyState) {
      if (safety === 'EMERGENCY') startEmergency();
      else if (safety === 'WARNING') startWarning();
      else stopAudio();
    } else if (safety === 'NORMAL' || safety === 'OFFLINE') stopAudio();
  }

  async function toggleAudio() {
    if (state.audioEnabled) {
      state.audioEnabled = false; stopAudio(); updateAudioForState(state.data?.safety_state || 'OFFLINE'); showToast('Audio muted. Visual safety alerts remain active.'); return;
    }
    try {
      const AudioContextClass = window.AudioContext || window.webkitAudioContext;
      if (!AudioContextClass) throw new Error('Web Audio is unavailable in this browser.');
      if (!state.audioContext) state.audioContext = new AudioContextClass();
      await state.audioContext.resume();
      state.audioEnabled = true;
      updateAudioForState(state.data?.safety_state || 'OFFLINE');
      const current = state.data?.safety_state || 'OFFLINE';
      if (current === 'EMERGENCY') startEmergency(); else if (current === 'WARNING') startWarning();
      showToast(current === 'NORMAL' || current === 'OFFLINE' ? 'Audio alerts enabled for future hazards.' : 'Audio alert acknowledged and enabled.');
    } catch (error) { showToast(error.message); }
  }

  async function triggerSimulation(action) {
    try {
      const resp = await fetch('/api/simulate', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ action })
      });
      const res = await resp.json();
      if (!resp.ok) throw new Error(res.error || `HTTP ${resp.status}`);
      if (res.data) renderStatus(res.data);
      showToast(`Simulation triggered: ${action.replace('_', ' ').toUpperCase()}`);
    } catch (err) {
      showToast(`Simulation error: ${err.message}`);
    }
  }

  async function disconnectWebSerial() {
    if (state.serialReader) {
      try {
        await state.serialReader.cancel();
      } catch (_) {}
      state.serialReader = null;
    }
    if (state.serialPort) {
      try {
        await state.serialPort.close();
      } catch (_) {}
      state.serialPort = null;
    }
    state.serialConnected = false;
    const btn = $('#serialConnectBtn');
    if (btn) {
      btn.classList.remove('connected');
      $('#serialBtnText').textContent = 'USB DIRECT';
    }
    showToast('USB Serial disconnected.');
  }

  async function toggleWebSerial() {
    if (state.serialConnected) {
      await disconnectWebSerial();
      return;
    }
    if (!('serial' in navigator)) {
      showToast('Web Serial API requires Chrome, Edge, or Opera over HTTPS / localhost.');
      return;
    }
    try {
      const port = await navigator.serial.requestPort();
      await port.open({ baudRate: 115200 });
      state.serialPort = port;
      state.serialConnected = true;

      const btn = $('#serialConnectBtn');
      if (btn) {
        btn.classList.add('connected');
        $('#serialBtnText').textContent = 'CONNECTED';
      }
      showToast('USB Serial connected @ 115200 baud.');

      const textDecoder = new TextDecoderStream();
      port.readable.pipeTo(textDecoder.writable).catch(() => {});
      const reader = textDecoder.readable.getReader();
      state.serialReader = reader;

      let lineBuf = '';
      while (state.serialConnected) {
        const { value, done } = await reader.read();
        if (done) break;
        lineBuf += value;
        const lines = lineBuf.split('\n');
        lineBuf = lines.pop();
        for (const raw of lines) {
          const line = raw.trim();
          if (!line.includes('[TELEMETRY]')) continue;
          const jsonText = line.split('[TELEMETRY]')[1].trim();
          try {
            const payload = JSON.parse(jsonText);
            fetch('/api/telemetry', {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify(payload)
            }).then(r => r.json()).then(res => {
              if (res.data) renderStatus(res.data);
            }).catch(() => {});

            payload.link_state = 'Connected';
            payload.link_source = 'USB Serial (Web Serial)';
            payload.has_telemetry = true;
            payload.received_at_ms = Date.now();
            payload.safety_state = payload.state || payload.safety_state || 'NORMAL';
            payload.hazard_type = payload.hazard || payload.hazard_type || 'NONE';
            payload.temperature_c = payload.temp ?? payload.temperature_c;
            payload.gas_ppm = payload.gas ?? payload.gas_ppm;
            payload.accel_magnitude_g = payload.accel ?? payload.accel_magnitude_g;
            payload.sos_active = payload.sos ?? payload.sos_active ?? false;
            renderStatus(payload);
          } catch (pe) {
            console.debug('JSON parse error from serial line:', pe);
          }
        }
      }
    } catch (err) {
      if (err.name !== 'NotFoundError') {
        showToast(`Serial error: ${err.message}`);
      }
      await disconnectWebSerial();
    }
  }

  function bindEvents() {
    $('#hazardFilter').addEventListener('change', () => renderIncidents(state.incidents));
    $('#exportCsv').addEventListener('click', exportCSV);
    $('#exportJson').addEventListener('click', exportJSON);
    $('#audioButton').addEventListener('click', toggleAudio);

    const serialBtn = $('#serialConnectBtn');
    if (serialBtn) {
      serialBtn.addEventListener('click', toggleWebSerial);
    }

    document.querySelectorAll('.sim-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const action = btn.dataset.action;
        if (action) triggerSimulation(action);
      });
    });

    const themeBtn = $('#themeToggle');
    if (themeBtn) {
      themeBtn.addEventListener('click', () => {
        const next = getCurrentTheme() === 'light' ? 'dark' : 'light';
        applyTheme(next);
        showToast(`Theme switched to ${next.toUpperCase()} mode.`);
      });
    }

    window.addEventListener('online', fetchStatus);
    window.addEventListener('offline', () => {
      $('#connectionBadge').dataset.link = 'Reconnecting';
      $('#connectionText').textContent = 'BROWSER OFFLINE';
    });
  }

  // Initialize theme from storage or preference
  let initialTheme = 'dark';
  try {
    const stored = localStorage.getItem('rw612-theme');
    if (stored === 'light' || stored === 'dark') {
      initialTheme = stored;
    }
  } catch (_) { }
  applyTheme(initialTheme);

  localClock();
  setInterval(localClock, 1000);
  bindEvents();
  fetchStatus();
  state.pollTimer = setInterval(fetchStatus, 1000);
})();
