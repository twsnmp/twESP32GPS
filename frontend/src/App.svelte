<script>
  import { onMount } from 'svelte';
  import SkyPlot from './components/SkyPlot.svelte';
  import { i18n } from './i18n.svelte.js';
  import { Sun, Moon, Compass, Radio, Clock, Wifi, Satellite } from '@lucide/svelte';

  // ==========================================================================
  //  Reactive state (mirrors twgps App.svelte structure)
  // ==========================================================================

  let gpsState = $state({
    hasFix:        false,
    time:          '',
    date:          '',
    latitude:      0,
    longitude:     0,
    altitude:      0,
    speedKmh:      0,
    numSatellites: 0,
    numSatsInView: 0,
    stratum:       16,
    uptime:        0,
    ntpClients:    0,
    satellites:    [],
  });

  let isDarkMode   = $state(true);
  let systemTime   = $state({ utc: '', local: '' });
  let fetchError   = $state(false);

  // ---- Derived: GPS-preferred UTC time (mirrors twgps timeUTC derived) -----
  let timeUTC = $derived.by(() => {
    if (gpsState.date && gpsState.time) {
      const clean = gpsState.time.split('.')[0];
      return `${gpsState.date} ${clean}`;
    }
    return systemTime.utc;
  });

  // ---- Derived: local time from GPS UTC ------------------------------------
  let timeLocal = $derived.by(() => {
    if (gpsState.date && gpsState.time) {
      try {
        const dt = new Date(`${gpsState.date}T${gpsState.time}Z`);
        if (!isNaN(dt)) {
          const pad = n => String(n).padStart(2, '0');
          return `${dt.getFullYear()}-${pad(dt.getMonth()+1)}-${pad(dt.getDate())} ${pad(dt.getHours())}:${pad(dt.getMinutes())}:${pad(dt.getSeconds())}`;
        }
      } catch {}
    }
    return systemTime.local;
  });

  // ---- Derived: satellite count per constellation -------------------------
  let satsBySystem = $derived.by(() => {
    const map = {};
    for (const sat of (gpsState.satellites || [])) {
      if (!map[sat.system]) map[sat.system] = [];
      map[sat.system].push(sat);
    }
    return map;
  });

  let totalSatsInView = $derived((gpsState.satellites || []).length);

  // ---- Derived: uptime format ---------------------------------------------
  let uptimeStr = $derived.by(() => {
    const s = gpsState.uptime || 0;
    const h = Math.floor(s / 3600);
    const m = Math.floor((s % 3600) / 60);
    const sec = s % 60;
    if (h > 0) return `${h}h ${m}m`;
    if (m > 0) return `${m}m ${sec}s`;
    return `${sec}s`;
  });

  // System color palette (matches twgps and SkyPlot.svelte)
  const systemColors = {
    'GPS':     '#3b82f6',
    'GLONASS': '#ef4444',
    'Galileo': '#ec4899',
    'BeiDou':  '#a855f7',
    'QZSS':    '#10b981',
    'SBAS':    '#6b7280',
  };

  // ==========================================================================
  //  Data fetch from /api/gps (1-second polling, replaces Wails IPC)
  // ==========================================================================

  async function fetchGPS() {
    try {
      const res = await fetch('/api/gps', { cache: 'no-store' });
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      const data = await res.json();
      gpsState = {
        hasFix:        data.hasFix        ?? false,
        time:          data.time          ?? '',
        date:          data.date          ?? '',
        latitude:      data.latitude      ?? 0,
        longitude:     data.longitude     ?? 0,
        altitude:      data.altitude      ?? 0,
        speedKmh:      data.speedKmh      ?? 0,
        numSatellites: data.numSatellites ?? 0,
        numSatsInView: data.numSatsInView ?? 0,
        stratum:       data.stratum       ?? 16,
        uptime:        data.uptime        ?? 0,
        ntpClients:    data.ntpClients    ?? 0,
        satellites:    Array.isArray(data.satellites) ? data.satellites : [],
      };
      fetchError = false;
    } catch (e) {
      fetchError = true;
      console.warn('[twESP32GPS] API fetch error:', e);
    }
  }

  // ==========================================================================
  //  Theme toggle
  // ==========================================================================

  function toggleTheme() {
    isDarkMode = !isDarkMode;
    document.documentElement.setAttribute('data-theme', isDarkMode ? 'dark' : 'light');
  }

  // ==========================================================================
  //  Lifecycle
  // ==========================================================================

  onMount(() => {
    // Apply initial theme
    document.documentElement.setAttribute('data-theme', 'dark');

    // Initial fetch
    fetchGPS();

    // 1-second polling loop
    const pollTimer = setInterval(fetchGPS, 1000);

    // System clock tick (for fallback display when GPS not fixed)
    const clockTimer = setInterval(() => {
      const now = new Date();
      const pad = n => String(n).padStart(2, '0');
      systemTime.local = `${now.getFullYear()}-${pad(now.getMonth()+1)}-${pad(now.getDate())} ${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`;
      systemTime.utc   = `${now.getUTCFullYear()}-${pad(now.getUTCMonth()+1)}-${pad(now.getUTCDate())} ${pad(now.getUTCHours())}:${pad(now.getUTCMinutes())}:${pad(now.getUTCSeconds())}`;
    }, 1000);

    return () => {
      clearInterval(pollTimer);
      clearInterval(clockTimer);
    };
  });
</script>

<div class="app-layout">

  <!-- =====================================================================
       Top Glassmorphic Navigation Bar (mirrors twgps navbar)
  ====================================================================== -->
  <header class="navbar">
    <div class="logo-area">
      <div class="logo-circle"></div>
      <h1>twESP32GPS <span class="badge">Stratum {gpsState.stratum}</span></h1>
    </div>
    <div class="nav-controls">
      <!-- GPS fix status indicator -->
      <div class="status-indicator {gpsState.hasFix ? 'active' : 'searching'}">
        <span class="pulse-dot"></span>
        {gpsState.hasFix ? i18n.t('status.locked') : i18n.t('status.searching')}
      </div>
      <!-- Language selector -->
      <div class="lang-selector">
        <select
          class="lang-select"
          value={i18n.currentLocale}
          onchange={e => i18n.setLocale(e.target.value)}
          title={i18n.t('nav.lang')}
          aria-label="Language"
        >
          <option value="en">EN</option>
          <option value="ja">JA</option>
        </select>
      </div>
      <!-- Theme toggle -->
      <button class="icon-btn theme-toggle" onclick={toggleTheme} title={i18n.t('nav.theme')} aria-label="Toggle theme">
        {#if isDarkMode}
          <Sun size={20} color="var(--accent-cyan)" />
        {:else}
          <Moon size={20} color="#0d1527" />
        {/if}
      </button>
    </div>
  </header>

  <!-- =====================================================================
       Main Content: Left Sidebar + Right Visual Area
  ====================================================================== -->
  <div class="main-content">

    <!-- ===================================================================
         Left Sidebar Panel (mirrors twgps sidebar-left)
    ==================================================================== -->
    <aside class="sidebar-left card">

      <!-- Position Matrix -->
      <div class="card-header">
        <Compass class="card-icon cyan" size={16} />
        <h2>{i18n.t('panels.position')}</h2>
      </div>
      <div class="coord-matrix">
        <div class="coord-row">
          <div class="coord-val">
            <span class="label">{i18n.t('pos.latitude')}</span>
            <span class="value">{gpsState.hasFix ? gpsState.latitude.toFixed(6) + '°' : '---.------'}</span>
          </div>
          <div class="coord-val">
            <span class="label">{i18n.t('pos.longitude')}</span>
            <span class="value">{gpsState.hasFix ? gpsState.longitude.toFixed(6) + '°' : '---.------'}</span>
          </div>
        </div>
        <div class="coord-row mt-4">
          <div class="coord-val">
            <span class="label">{i18n.t('pos.altitude')}</span>
            <span class="value">{gpsState.hasFix ? gpsState.altitude.toFixed(1) + ' m' : '----.- m'}</span>
          </div>
          <div class="coord-val">
            <span class="label">{i18n.t('pos.speed')}</span>
            <span class="value">{gpsState.hasFix ? gpsState.speedKmh.toFixed(1) + ' km/h' : '--.- km/h'}</span>
          </div>
        </div>
      </div>

      <hr class="separator" />

      <!-- Time Matrix -->
      <div class="card-header">
        <Clock class="card-icon cyan" size={16} />
        <h2>{i18n.t('panels.time')}</h2>
      </div>
      <div class="coord-matrix">
        <div class="coord-row">
          <div class="coord-val">
            <span class="label">{i18n.t('time.local')}</span>
            <span class="value">{timeLocal || '--:--:--'}</span>
          </div>
        </div>
        <div class="coord-row mt-4">
          <div class="coord-val">
            <span class="label">{i18n.t('time.utc')}</span>
            <span class="value">{timeUTC || '--:--:--'}</span>
          </div>
        </div>
      </div>

      <hr class="separator" />

      <!-- NTP Server Status -->
      <div class="card-header">
        <Clock class="card-icon emerald" size={16} />
        <h2>{i18n.t('panels.ntp')}</h2>
      </div>
      <div class="ntp-panel">
        <div class="ntp-status-box {gpsState.stratum === 1 ? 'stratum1' : 'no-fix'}">
          {gpsState.stratum === 1 ? i18n.t('ntp.online') : i18n.t('ntp.offline')}
        </div>
        <div class="stats-row">
          <div class="stat-unit">
            <span class="stat-label">{i18n.t('ntp.stratum')}</span>
            <span class="stat-value" style="color: {gpsState.stratum === 1 ? 'var(--accent-emerald)' : 'var(--accent-amber)'}">
              {gpsState.stratum}
            </span>
          </div>
          <div class="stat-unit">
            <span class="stat-label">{i18n.t('ntp.clients')}</span>
            <span class="stat-value">{gpsState.ntpClients}</span>
          </div>
          <div class="stat-unit">
            <span class="stat-label">{i18n.t('ntp.refId')}</span>
            <span class="stat-value" style="color: var(--accent-cyan)">GPS</span>
          </div>
        </div>
        <div class="field-info" style="margin-bottom:0">
          <span class="label">{i18n.t('ntp.uptime')}</span>
          <span class="value">{uptimeStr}</span>
        </div>
      </div>

      <hr class="separator" />

      <!-- Network Info -->
      <div class="card-header">
        <Wifi class="card-icon amber" size={16} />
        <h2>{i18n.t('panels.network')}</h2>
      </div>
      <div class="field-info">
        <span class="label">{i18n.t('net.satsFix')}</span>
        <span class="value">{gpsState.numSatellites}</span>
      </div>
      <div class="field-info">
        <span class="label">{i18n.t('net.satsView')}</span>
        <span class="value">{totalSatsInView}</span>
      </div>
      {#if fetchError}
        <div class="fetch-error">⚠ API unreachable</div>
      {/if}

    </aside>

    <!-- ===================================================================
         Right Visual Area: SkyPlot + Satellite Matrix
    ==================================================================== -->
    <div class="visual-area">

      <!-- Sky Plot (replaces twgps Globe) -->
      <main class="skyplot-section card" style="padding: 8px;">
        <SkyPlot {gpsState} />
      </main>

      <!-- Satellite Matrix (identical to twgps satellite section) -->
      <section class="satellites-section card">
        <div class="card-header">
          <Radio class="card-icon pink" size={20} />
          <h2>
            {i18n.t('panels.satellites')}
            ({gpsState.numSatellites}/{totalSatsInView})
          </h2>
        </div>

        <div class="satellites-rows-container">
          {#if Object.keys(satsBySystem).length === 0}
            <div class="empty-state">
              <span class="pulse-dot large"></span>
              <p>{i18n.t('sat.empty')}</p>
            </div>
          {:else}
            {#each Object.entries(satsBySystem) as [system, sats]}
              {#if sats.length > 0}
                <div class="constellation-row">
                  <h3>
                    <span class="indicator" style="background-color: {systemColors[system] || '#fff'}"></span>
                    {i18n.t('systems.' + system)} ({sats.length})
                  </h3>
                  <div class="sat-horizontal-list">
                    {#each sats as sat}
                      <div class="sat-card {sat.snr > 0 ? 'active' : 'inactive'}">
                        <div class="sat-header">
                          <span class="prn">No.{sat.prn}</span>
                          <span class="snr-val" style="color: {systemColors[system] || '#fff'}">
                            {sat.snr > 0 ? sat.snr + ' dB' : i18n.t('sat.noSig')}
                          </span>
                        </div>
                        <div class="snr-bar-bg">
                          <div
                            class="snr-bar-fg"
                            style="width: {Math.min(100, (sat.snr / 50) * 100)}%; background-color: {systemColors[system] || '#fff'}"
                          ></div>
                        </div>
                        <div class="sat-meta">
                          <span>El: {sat.elevation}°</span>
                          <span>Az: {sat.azimuth}°</span>
                        </div>
                      </div>
                    {/each}
                  </div>
                </div>
              {/if}
            {/each}
          {/if}
        </div>
      </section>

    </div>
  </div>
</div>

<style>
  .fetch-error {
    font-size: 11px;
    color: var(--accent-amber);
    text-align: center;
    padding: 4px 8px;
    border: 1px solid var(--accent-amber);
    border-radius: 6px;
    opacity: 0.85;
    margin-top: 4px;
  }
</style>
