// AutoShieldX Dashboard Client Logic

function updateClock() {
  const now = new Date();
  const timeStr = now.toTimeString().split(' ')[0] + '.' + String(Math.floor(now.getMilliseconds() / 100));
  const clockEl = document.getElementById('clock');
  if (clockEl) {
    clockEl.textContent = timeStr;
  }
}
setInterval(updateClock, 100);

async function fetchStatus() {
  try {
    const res = await fetch('/api/status');
    if (!res.ok) return;
    const data = await res.json();

    // 1. Update KPI counters
    const totalEl = document.getElementById('metric-total');
    if (totalEl) totalEl.textContent = data.total_messages.toLocaleString();
    const validEl = document.getElementById('metric-valid');
    if (validEl) validEl.textContent = data.valid_messages.toLocaleString();
    const rejEl = document.getElementById('metric-rejected');
    if (rejEl) rejEl.textContent = data.rejected_messages.toLocaleString();
    const hmacEl = document.getElementById('metric-hmac');
    if (hmacEl) hmacEl.textContent = data.hmac_failures.toLocaleString();
    const replayEl = document.getElementById('metric-replay');
    if (replayEl && data.replay_detections !== undefined) replayEl.textContent = data.replay_detections.toLocaleString();
    const mlEl = document.getElementById('metric-ml');
    if (mlEl) mlEl.textContent = data.ml_anomalies.toLocaleString();

    // 2. Update Live Telemetry
    const tel = data.latest_telemetry || {};
    const speed = tel.speed_kmh || 0;
    const rpm = tel.rpm || 0;
    const temp = tel.temperature_c || 25;
    const vib = tel.vibration_g || 0.08;
    const steer = tel.steering_deg || 0;
    const wheel = tel.wheel_speed_kmh || 0;

    document.getElementById('val-speed').textContent = speed.toFixed(1);
    document.getElementById('bar-speed').style.width = Math.min(100, (speed / 250) * 100) + '%';

    document.getElementById('val-rpm').textContent = Math.round(rpm);
    document.getElementById('bar-rpm').style.width = Math.min(100, (rpm / 8000) * 100) + '%';

    document.getElementById('val-temp').textContent = temp.toFixed(1);
    document.getElementById('bar-temp').style.width = Math.min(100, Math.max(0, ((temp + 40) / 165) * 100)) + '%';

    document.getElementById('val-vib').textContent = vib.toFixed(3);
    document.getElementById('bar-vib').style.width = Math.min(100, (vib / 3.0) * 100) + '%';

    document.getElementById('val-steer').textContent = steer.toFixed(1);
    document.getElementById('bar-steer').style.width = Math.min(100, Math.max(0, ((steer + 45) / 90) * 100)) + '%';

    document.getElementById('val-wheel').textContent = wheel.toFixed(1);
    document.getElementById('bar-wheel').style.width = Math.min(100, (wheel / 250) * 100) + '%';

    document.getElementById('val-can-id').textContent = '0x' + (tel.can_id || 256).toString(16).toUpperCase();
    document.getElementById('val-seq').textContent = tel.sequence_number || 0;
    
    if (tel.timestamp_us) {
      document.getElementById('val-timestamp').textContent = (tel.timestamp_us / 1000).toFixed(0) + ' ms';
    }

    // 3. Update Security Events Table
    const tbody = document.getElementById('logs-tbody');
    const events = data.recent_events || [];
    document.getElementById('event-count-badge').textContent = events.length + ' Events Logged';

    if (events.length > 0) {
      tbody.innerHTML = '';
      events.slice(0, 30).forEach(ev => {
        const tr = document.createElement('tr');

        let statusClass = 'status-valid';
        if (ev.message_status === 'REJECTED_DROPPED') statusClass = 'status-rejected';
        else if (ev.message_status === 'FLAGGED_MONITORED') statusClass = 'status-flagged';

        let sevClass = 'sev-info';
        if (ev.severity === 'WARNING') sevClass = 'sev-warning';
        else if (ev.severity === 'CRITICAL') sevClass = 'sev-critical';

        tr.innerHTML = `
          <td>${ev.timestamp_iso ? ev.timestamp_iso.replace('T', ' ').replace('Z', '') : '--'}</td>
          <td>${ev.can_id_hex || ('0x' + (ev.can_id || 0).toString(16))}</td>
          <td>${ev.sequence_number || 0}</td>
          <td><span class="status-badge ${statusClass}">${ev.message_status || 'UNKNOWN'}</span></td>
          <td>${ev.attack_type || 'NONE'}</td>
          <td><span class="${sevClass}">${ev.severity || 'INFO'}</span></td>
          <td>${escapeHtml(ev.details || '')}</td>
        `;
        tbody.appendChild(tr);
      });
    }
  } catch (err) {
    console.error('Failed to poll dashboard API:', err);
  }
}

function escapeHtml(str) {
  return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

async function resetCounters() {
  try {
    await fetch('/api/reset', { method: 'POST' });
    await fetchStatus();
  } catch (err) {
    console.error('Failed to reset counters:', err);
  }
}

// Poll every 500ms
setInterval(fetchStatus, 500);
fetchStatus();
