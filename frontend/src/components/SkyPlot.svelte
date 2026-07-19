<script>
  import { onMount } from 'svelte';

  // ==========================================================================
  //  SkyPlot — Canvas 2D Satellite Sky Plot
  //  Renders an azimuth/elevation sky dome, mirroring the concept of
  //  twgps Globe.svelte but using 2D canvas for ESP32 memory constraints
  // ==========================================================================

  /** @type {{ satellites: Array, hasFix: boolean, latitude: number, longitude: number }} */
  let { gpsState = { satellites: [], hasFix: false, latitude: 0, longitude: 0 } } = $props();

  let canvas = $state(null);
  let tooltip = $state({ visible: false, x: 0, y: 0, text: '' });

  // Constellation color palette — matches twgps systemColors
  const systemColors = {
    'GPS':     '#3b82f6',
    'GLONASS': '#ef4444',
    'Galileo': '#ec4899',
    'BeiDou':  '#a855f7',
    'QZSS':    '#10b981',
    'SBAS':    '#6b7280',
  };

  // -------------------------------------------------------------------------
  //  Coordinate conversion: azimuth + elevation → canvas x, y
  //  Elevation 90° = center, 0° = outer ring edge
  // -------------------------------------------------------------------------
  function skyToCanvas(az, el, cx, cy, radius) {
    const r = radius * (1 - el / 90);
    const theta = (az - 90) * (Math.PI / 180); // 0° az = North = top
    return {
      x: cx + r * Math.cos(theta),
      y: cy + r * Math.sin(theta),
    };
  }

  // -------------------------------------------------------------------------
  //  Draw sky plot
  // -------------------------------------------------------------------------
  function draw() {
    if (!canvas) return;
    const ctx = canvas.getContext('2d');

    // Reset transform matrix to identity first, then scale by devicePixelRatio
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    const dpr = window.devicePixelRatio || 1;
    ctx.scale(dpr, dpr);

    // Use logical CSS dimensions instead of physical pixel canvas.width/height
    const rect = canvas.getBoundingClientRect();
    const w = rect.width;
    const h = rect.height;
    const cx = w / 2;
    const cy = h / 2;
    const radius = Math.min(w, h) / 2 - 24;

    // Detect theme
    const isDark = document.documentElement.getAttribute('data-theme') !== 'light';
    const bgColor    = isDark ? '#000814' : '#f0f4ff';
    const ringColor  = isDark ? 'rgba(0,217,255,0.15)' : 'rgba(2,132,199,0.15)';
    const ringStroke = isDark ? 'rgba(0,217,255,0.25)' : 'rgba(2,132,199,0.25)';
    const textColor  = isDark ? 'rgba(148,163,184,0.8)' : 'rgba(71,85,105,0.9)';
    const cardinalColor = isDark ? 'rgba(0,217,255,0.7)' : 'rgba(2,132,199,0.9)';
    const crossColor = isDark ? 'rgba(255,255,255,0.08)' : 'rgba(15,23,42,0.08)';

    ctx.clearRect(0, 0, w, h);

    // Background fill
    ctx.fillStyle = bgColor;
    ctx.beginPath();
    ctx.arc(cx, cy, radius + 2, 0, Math.PI * 2);
    ctx.fill();

    // Elevation rings: 0°, 30°, 60°, 90°
    [0, 30, 60].forEach(el => {
      const r = radius * (1 - el / 90);
      ctx.beginPath();
      ctx.arc(cx, cy, r, 0, Math.PI * 2);
      ctx.strokeStyle = ringStroke;
      ctx.lineWidth = 1;
      ctx.stroke();

      // Elevation label
      if (el > 0) {
        ctx.fillStyle = textColor;
        ctx.font = '9px monospace';
        ctx.textAlign = 'center';
        ctx.fillText(`${el}°`, cx + 4, cy - r + 12);
      }
    });

    // Center dot (zenith = 90°)
    ctx.beginPath();
    ctx.arc(cx, cy, 3, 0, Math.PI * 2);
    ctx.fillStyle = ringStroke;
    ctx.fill();

    // Crosshair lines (N-S / E-W)
    ctx.strokeStyle = crossColor;
    ctx.lineWidth = 1;
    ctx.setLineDash([4, 4]);
    ctx.beginPath();
    ctx.moveTo(cx, cy - radius); ctx.lineTo(cx, cy + radius);
    ctx.moveTo(cx - radius, cy); ctx.lineTo(cx + radius, cy);
    ctx.stroke();
    ctx.setLineDash([]);

    // Cardinal direction labels
    ctx.fillStyle = cardinalColor;
    ctx.font = 'bold 11px Nunito, sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText('N', cx,          cy - radius - 8);
    ctx.fillText('S', cx,          cy + radius + 16);
    ctx.fillText('E', cx + radius + 14, cy + 4);
    ctx.fillText('W', cx - radius - 14, cy + 4);

    // Outer border circle
    ctx.beginPath();
    ctx.arc(cx, cy, radius, 0, Math.PI * 2);
    ctx.strokeStyle = ringStroke;
    ctx.lineWidth = 1.5;
    ctx.stroke();

    // Draw satellites
    const sats = gpsState.satellites || [];
    sats.forEach(sat => {
      if (!sat || sat.elevation === undefined) return;
      const color = systemColors[sat.system] || '#ffffff';
      const pos = skyToCanvas(sat.azimuth, sat.elevation, cx, cy, radius);
      const active = sat.snr > 0;
      const dotRadius = active ? 7 : 5;

      // Glow effect for active satellites
      if (active) {
        const grd = ctx.createRadialGradient(pos.x, pos.y, 0, pos.x, pos.y, dotRadius * 2.5);
        grd.addColorStop(0, color + 'aa');
        grd.addColorStop(1, color + '00');
        ctx.beginPath();
        ctx.arc(pos.x, pos.y, dotRadius * 2.5, 0, Math.PI * 2);
        ctx.fillStyle = grd;
        ctx.fill();
      }

      // Satellite dot
      ctx.beginPath();
      ctx.arc(pos.x, pos.y, dotRadius, 0, Math.PI * 2);
      if (active) {
        ctx.fillStyle = color;
        ctx.fill();
        ctx.strokeStyle = 'rgba(255,255,255,0.5)';
        ctx.lineWidth = 1;
        ctx.stroke();
      } else {
        ctx.setLineDash([2, 2]);
        ctx.strokeStyle = color + '80';
        ctx.lineWidth = 1.5;
        ctx.stroke();
        ctx.setLineDash([]);
      }

      // PRN label
      ctx.fillStyle = active ? '#ffffff' : textColor;
      ctx.font = `bold ${active ? 8 : 7}px monospace`;
      ctx.textAlign = 'center';
      ctx.fillText(sat.prn, pos.x, pos.y + 3);
    });
  }

  // Resize canvas to match its CSS size
  function resizeCanvas() {
    if (!canvas) return;
    const rect = canvas.getBoundingClientRect();
    canvas.width  = rect.width  * window.devicePixelRatio;
    canvas.height = rect.height * window.devicePixelRatio;
    draw();
  }

  // -------------------------------------------------------------------------
  //  Tooltip on hover
  // -------------------------------------------------------------------------
  function handleMouseMove(e) {
    if (!canvas) return;
    const rect = canvas.getBoundingClientRect();
    const mx = e.clientX - rect.left;
    const my = e.clientY - rect.top;
    const cx = rect.width / 2;
    const cy = rect.height / 2;
    const radius = Math.min(rect.width, rect.height) / 2 - 24;

    const sats = gpsState.satellites || [];
    let found = null;
    for (const sat of sats) {
      if (!sat || sat.elevation === undefined) continue;
      const pos = skyToCanvas(sat.azimuth, sat.elevation, cx, cy, radius);
      const dist = Math.hypot(mx - pos.x, my - pos.y);
      if (dist < 12) { found = sat; break; }
    }

    if (found) {
      const snrLabel = found.snr > 0 ? `${found.snr} dBHz` : 'No Signal';
      tooltip = {
        visible: true,
        x: e.clientX - rect.left + 12,
        y: e.clientY - rect.top - 8,
        text: `${found.system} No.${found.prn}\nEl: ${found.elevation}°  Az: ${found.azimuth}°\nSNR: ${snrLabel}`
      };
    } else {
      tooltip = { ...tooltip, visible: false };
    }
  }

  function handleMouseLeave() {
    tooltip = { ...tooltip, visible: false };
  }

  // -------------------------------------------------------------------------
  //  Reactive redraw when gpsState changes
  // -------------------------------------------------------------------------
  $effect(() => {
    if (gpsState && canvas) {
      draw();
    }
  });

  onMount(() => {
    resizeCanvas();
    const ro = new ResizeObserver(resizeCanvas);
    ro.observe(canvas.parentElement);
    return () => ro.disconnect();
  });
</script>

<div class="skyplot-wrapper">
  <canvas
    bind:this={canvas}
    class="skyplot-canvas"
    onmousemove={handleMouseMove}
    onmouseleave={handleMouseLeave}
  ></canvas>

  {#if tooltip.visible}
    <div
      class="sat-tooltip"
      style="left: {tooltip.x}px; top: {tooltip.y}px;"
    >
      {#each tooltip.text.split('\n') as line}
        <div>{line}</div>
      {/each}
    </div>
  {/if}
</div>

<style>
  .skyplot-wrapper {
    width: 100%;
    height: 100%;
    position: relative;
    border-radius: 16px;
    overflow: hidden;
    background: var(--skyplot-bg, #000814);
  }

  .skyplot-canvas {
    width: 100%;
    height: 100%;
    display: block;
    cursor: crosshair;
  }

  .sat-tooltip {
    position: absolute;
    background: rgba(6, 12, 28, 0.92);
    border: 1px solid rgba(0, 217, 255, 0.3);
    color: #f8fafc;
    font-size: 11px;
    font-family: monospace;
    line-height: 1.6;
    padding: 6px 10px;
    border-radius: 8px;
    pointer-events: none;
    white-space: nowrap;
    backdrop-filter: blur(8px);
    z-index: 100;
    box-shadow: 0 4px 16px rgba(0, 0, 0, 0.5);
  }
</style>
