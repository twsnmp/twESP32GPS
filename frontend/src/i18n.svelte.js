// =============================================================================
//  i18n — EN / JA bilingual support
//  Mirrors twgps i18n.svelte.js, extended with ESP32-specific labels
// =============================================================================

const translations = {
  en: {
    status: {
      locked:    'GPS Locked',
      searching: 'Searching Satellites...',
      noWifi:    'No WiFi'
    },
    nav: {
      theme: 'Toggle Theme',
      lang:  'Language'
    },
    panels: {
      position:   'Position Matrix',
      time:       'Time Matrix',
      ntp:        'NTP Server',
      network:    'Network',
      satellites: 'Satellite Matrix'
    },
    pos: {
      latitude:  'Latitude',
      longitude: 'Longitude',
      altitude:  'Altitude (MSL)',
      speed:     'Speed'
    },
    time: {
      local: 'Local Time',
      utc:   'UTC Time'
    },
    ntp: {
      status:   'Server Status',
      online:   'ONLINE',
      offline:  'OFFLINE',
      stratum:  'Stratum',
      clients:  'NTP Clients',
      refId:    'Reference ID',
      uptime:   'Uptime'
    },
    net: {
      ip:       'IP Address',
      satsView: 'Sats In View',
      satsFix:  'Sats Tracked'
    },
    sat: {
      empty:  'Searching for satellite signals...',
      noSig:  'No Sig'
    },
    systems: {
      'GPS':     'GPS',
      'GLONASS': 'GLONASS',
      'Galileo': 'Galileo',
      'BeiDou':  'BeiDou',
      'QZSS':    'QZSS',
      'SBAS':    'SBAS'
    }
  },
  ja: {
    status: {
      locked:    'GPS ロック完了',
      searching: '衛星を探索中...',
      noWifi:    'WiFi 未接続'
    },
    nav: {
      theme: 'テーマ切り替え',
      lang:  '言語'
    },
    panels: {
      position:   '位置マトリクス',
      time:       '時間マトリクス',
      ntp:        'NTP サーバー',
      network:    'ネットワーク',
      satellites: '衛星マトリクス'
    },
    pos: {
      latitude:  '緯度',
      longitude: '経度',
      altitude:  '高度 (平均海面)',
      speed:     '速度'
    },
    time: {
      local: 'ローカル時刻',
      utc:   '協定世界時 (UTC)'
    },
    ntp: {
      status:  'サーバー状態',
      online:  'オンライン',
      offline:  'オフライン',
      stratum: 'ストラタム',
      clients: 'NTP クライアント',
      refId:   '参照元 ID',
      uptime:  '稼働時間'
    },
    net: {
      ip:       'IP アドレス',
      satsView: '捕捉衛星数',
      satsFix:  '測位利用衛星'
    },
    sat: {
      empty:  '衛星信号を探索中...',
      noSig:  '信号なし'
    },
    systems: {
      'GPS':     'GPS',
      'GLONASS': 'GLONASS',
      'Galileo': 'Galileo',
      'BeiDou':  'BeiDou',
      'QZSS':    'みちびき(QZSS)',
      'SBAS':    '補正信号(SBAS)'
    }
  }
};

function detectOSLanguage() {
  const lang = typeof navigator !== 'undefined'
    ? (navigator.language || navigator.languages?.[0] || 'en')
    : 'en';
  return lang.startsWith('ja') ? 'ja' : 'en';
}

class I18nManager {
  currentLocale = $state(detectOSLanguage());

  setLocale(lang) {
    if (translations[lang]) {
      this.currentLocale = lang;
    }
  }

  t(path) {
    const keys = path.split('.');
    let node = translations[this.currentLocale];
    for (const key of keys) {
      if (node && node[key] !== undefined) {
        node = node[key];
      } else {
        return keys[keys.length - 1]; // fallback: return last key
      }
    }
    return node;
  }
}

export const i18n = new I18nManager();
