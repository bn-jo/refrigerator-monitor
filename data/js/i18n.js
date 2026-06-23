// =====================================================================
//  i18n.js — English / Hebrew translations, persisted in localStorage,
//  with automatic RTL handling for Hebrew.
// =====================================================================
(function () {
  const LANG_KEY = 'fm_lang';

  const translations = {
    en: {
      'nav.dashboard': 'Dashboard', 'nav.settings': 'Settings', 'nav.logs': 'Logs',
      'logout': 'Logout', 'restart': '⟳ Restart Device',
      'live.title': 'Live Temperatures', 'chart.title': 'History', 'sys.title': 'System',
      'range.1h': 'Last hour', 'range.24h': '24 hours', 'range.7d': '7 days', 'range.30d': '30 days',
      'chart.resetZoom': 'Reset zoom', 'chart.fullscreen': '⛶ Fullscreen',
      'chart.tip': 'Tip: scroll / pinch to zoom, drag to pan, click a legend item to show/hide a line.',
      'sys.connection': 'Connection', 'sys.ip': 'IP address', 'sys.internet': 'Internet',
      'sys.signal': 'WiFi signal', 'sys.datetime': 'Date & time', 'sys.timesync': 'Time sync',
      'sys.uptime': 'Uptime', 'sys.firmware': 'Firmware', 'sys.records': 'History records',
      'sys.storage': 'Storage', 'sys.freemem': 'Free memory',
      'val.connected': 'Connected', 'val.unavailable': 'Unavailable',
      'val.syncedNtp': 'Synced (NTP)', 'val.notSynced': 'Not synced',
      'val.ok': 'OK', 'val.fault': 'FAULT', 'val.wifiFallback': ' (WiFi fallback)',
      'status.normal': 'Normal', 'status.near': 'Near', 'status.alarm': 'Alarm',
      'sensor.ok': 'ok', 'sensor.fault': 'FAULT', 'sensor.missing': 'MISSING', 'sensor.outofrange': 'OUT OF RANGE',
      'live.ambient': 'Ambient', 'live.thr': 'thr', 'live.updated': 'updated', 'live.sensor': 'Sensor',
      'live.outdoor': 'Outdoor', 'fridge.n': 'Refrigerator {n}', 'settings.reset': 'Reset to default',
      'acq.prefix': 'Data acquisition: ', 'acq.ok': 'OK', 'acq.degraded': 'DEGRADED',
      'acq.alarmActive': ' · ⚠ ALARM ACTIVE',
      'settings.fridges': 'Refrigerators', 'settings.general': 'General',
      'settings.deviceName': 'Device name', 'settings.outdoorName': 'Outdoor sensor name',
      'settings.timezone': 'Timezone (POSIX TZ)',
      'settings.ipTitle': 'IP Address',
      'settings.ipHint': 'The device always joins via DHCP first, then switches to the static address below only on networks where it is valid — so it works on any network. Changes take effect after a restart.',
      'settings.ipMode': 'Addressing', 'settings.ipDhcp': 'Automatic (DHCP)',
      'settings.ipStatic': 'Prefer static address', 'settings.ipAddr': 'Static IP',
      'settings.ipGw': 'Gateway', 'settings.ipMask': 'Subnet mask',
      'settings.ipDns1': 'DNS 1', 'settings.ipDns2': 'DNS 2',
      'settings.network': 'WiFi Fallback', 'settings.wifiSsid': 'WiFi network (SSID)',
      'settings.wifiPass': 'WiFi password',
      'settings.wifiHint': 'Used only when no Ethernet link is detected. Changes take effect on the next WiFi fallback or after a restart.',
      'settings.email': 'E-mail Alerts',
      'settings.emailEnable': 'Enable e-mail alerts', 'settings.smtpServer': 'SMTP server',
      'settings.port': 'Port', 'settings.smtpUser': 'SMTP username', 'settings.smtpPass': 'SMTP password',
      'settings.sender': 'Sender address', 'settings.recipient': 'Recipient address',
      'settings.sendTo': 'Send alerts to', 'settings.sendTo2': 'Second address (optional)',
      'settings.smtpProvider': 'E-mail Provider (SMTP) — configured',
      'settings.provider': 'Provider', 'settings.custom': 'Custom',
      'settings.emailLogin': 'Login (e-mail / username)', 'settings.emailPass': 'Password / SMTP key',
      'settings.fromAddr': 'From address',
      'settings.testEmail': 'Send test e-mail', 'settings.security': 'Security',
      'settings.newPass': 'New password (leave blank to keep)', 'settings.save': 'Save settings',
      'settings.fridgeName': 'Refrigerator name', 'settings.threshold': 'Threshold (°C)',
      'settings.enabled': 'Enabled', 'settings.yes': 'Yes', 'settings.no': 'No',
      'settings.unchanged': '(unchanged)',
      'settings.users': 'Users', 'settings.username': 'Username', 'settings.password': 'Password',
      'settings.addUser': 'Add user', 'settings.delete': 'Delete',
      'settings.setAll': 'Set all thresholds (°C)', 'settings.apply': 'Apply to all',
      'settings.thrHint': 'Alarm e-mail is sent when a refrigerator rises ABOVE its threshold.',
      'toast.userAdded': 'User saved', 'toast.userRemoved': 'User removed',
      'toast.userFailed': 'Failed: ', 'confirm.delUser': 'Remove user "{u}"?',
      'logs.all': 'All levels', 'logs.info': 'Info', 'logs.warn': 'Warn', 'logs.error': 'Error',
      'logs.filterCat': 'Filter category (e.g. net)', 'logs.refresh': 'Refresh',
      'logs.download': 'Download', 'logs.clear': 'Clear', 'logs.none': 'No log entries.',
      'login.subtitle': 'Sign in to continue', 'login.username': 'Username',
      'login.password': 'Password', 'login.signin': 'Sign in',
      'login.error': 'Invalid username or password.', 'login.connError': 'Connection error.',
      'toast.saved': 'Settings saved', 'toast.saveFailed': 'Save failed: ',
      'toast.sendingTest': 'Sending test e-mail…', 'toast.testSent': 'Test e-mail sent',
      'toast.testFailed': 'Test failed (check SMTP settings & logs)',
      'toast.restarting': 'Device is restarting…',
      'confirm.restart': 'Restart the monitoring device now?\nMonitoring will pause for ~20 seconds.',
      'confirm.clearLogs': 'Clear all logs? This cannot be undone.'
    },
    he: {
      'nav.dashboard': 'לוח בקרה', 'nav.settings': 'הגדרות', 'nav.logs': 'יומן',
      'logout': 'התנתק', 'restart': '⟳ אתחל מכשיר',
      'live.title': 'טמפרטורות בזמן אמת', 'chart.title': 'היסטוריה', 'sys.title': 'מערכת',
      'range.1h': 'שעה אחרונה', 'range.24h': '24 שעות', 'range.7d': '7 ימים', 'range.30d': '30 יום',
      'chart.resetZoom': 'אפס זום', 'chart.fullscreen': '⛶ מסך מלא',
      'chart.tip': 'טיפ: גלגלו או צבטו לזום, גררו להזזה, לחצו על פריט במקרא להצגה/הסתרה.',
      'sys.connection': 'חיבור', 'sys.ip': 'כתובת IP', 'sys.internet': 'אינטרנט',
      'sys.signal': 'עוצמת WiFi', 'sys.datetime': 'תאריך ושעה', 'sys.timesync': 'סנכרון זמן',
      'sys.uptime': 'זמן פעילות', 'sys.firmware': 'גרסת קושחה', 'sys.records': 'רשומות היסטוריה',
      'sys.storage': 'אחסון', 'sys.freemem': 'זיכרון פנוי',
      'val.connected': 'מחובר', 'val.unavailable': 'לא זמין',
      'val.syncedNtp': 'מסונכרן (NTP)', 'val.notSynced': 'לא מסונכרן',
      'val.ok': 'תקין', 'val.fault': 'תקלה', 'val.wifiFallback': ' (גיבוי WiFi)',
      'status.normal': 'תקין', 'status.near': 'קרוב לסף', 'status.alarm': 'אזעקה',
      'sensor.ok': 'תקין', 'sensor.fault': 'תקלה', 'sensor.missing': 'חסר', 'sensor.outofrange': 'מחוץ לטווח',
      'live.ambient': 'סביבה', 'live.thr': 'סף', 'live.updated': 'עודכן', 'live.sensor': 'חיישן',
      'live.outdoor': 'חוץ', 'fridge.n': 'מקרר {n}', 'settings.reset': 'אפס לברירת מחדל',
      'acq.prefix': 'איסוף נתונים: ', 'acq.ok': 'תקין', 'acq.degraded': 'מופחת',
      'acq.alarmActive': ' · ⚠ אזעקה פעילה',
      'settings.fridges': 'מקררים', 'settings.general': 'כללי',
      'settings.deviceName': 'שם המכשיר', 'settings.outdoorName': 'שם חיישן חוץ',
      'settings.timezone': 'אזור זמן (POSIX TZ)',
      'settings.ipTitle': 'כתובת IP',
      'settings.ipHint': 'המכשיר מתחבר תמיד תחילה באמצעות DHCP, ועובר לכתובת הסטטית שלמטה רק ברשתות שבהן היא תקפה — כך שהוא עובד בכל רשת. השינויים ייכנסו לתוקף לאחר אתחול.',
      'settings.ipMode': 'הקצאת כתובת', 'settings.ipDhcp': 'אוטומטי (DHCP)',
      'settings.ipStatic': 'העדף כתובת סטטית', 'settings.ipAddr': 'כתובת IP סטטית',
      'settings.ipGw': 'שער (Gateway)', 'settings.ipMask': 'מסכת רשת',
      'settings.ipDns1': 'DNS 1', 'settings.ipDns2': 'DNS 2',
      'settings.network': 'גיבוי WiFi', 'settings.wifiSsid': 'רשת WiFi (SSID)',
      'settings.wifiPass': 'סיסמת WiFi',
      'settings.wifiHint': 'בשימוש רק כאשר אין חיבור Ethernet. השינויים ייכנסו לתוקף בגיבוי ה-WiFi הבא או לאחר אתחול.',
      'settings.email': 'התראות דוא"ל',
      'settings.emailEnable': 'הפעל התראות דוא"ל', 'settings.smtpServer': 'שרת SMTP',
      'settings.port': 'פורט', 'settings.smtpUser': 'שם משתמש SMTP', 'settings.smtpPass': 'סיסמת SMTP',
      'settings.sender': 'כתובת שולח', 'settings.recipient': 'כתובת נמען',
      'settings.sendTo': 'שלח התראות אל', 'settings.sendTo2': 'כתובת שנייה (אופציונלי)',
      'settings.smtpProvider': 'ספק דוא"ל (SMTP) — מוגדר',
      'settings.provider': 'ספק', 'settings.custom': 'מותאם אישית',
      'settings.emailLogin': 'התחברות (דוא"ל / שם משתמש)', 'settings.emailPass': 'סיסמה / מפתח SMTP',
      'settings.fromAddr': 'כתובת שולח (From)',
      'settings.testEmail': 'שלח דוא"ל בדיקה', 'settings.security': 'אבטחה',
      'settings.newPass': 'סיסמה חדשה (השאר ריק לשמירה)', 'settings.save': 'שמור הגדרות',
      'settings.fridgeName': 'שם מקרר', 'settings.threshold': 'סף (°C)',
      'settings.enabled': 'מופעל', 'settings.yes': 'כן', 'settings.no': 'לא',
      'settings.unchanged': '(ללא שינוי)',
      'settings.users': 'משתמשים', 'settings.username': 'שם משתמש', 'settings.password': 'סיסמה',
      'settings.addUser': 'הוסף משתמש', 'settings.delete': 'מחק',
      'settings.setAll': 'קבע סף לכולם (°C)', 'settings.apply': 'החל על כולם',
      'settings.thrHint': 'התראת דוא"ל נשלחת כאשר מקרר עולה מעל הסף שלו.',
      'toast.userAdded': 'המשתמש נשמר', 'toast.userRemoved': 'המשתמש הוסר',
      'toast.userFailed': 'נכשל: ', 'confirm.delUser': 'להסיר את המשתמש "{u}"?',
      'logs.all': 'כל הרמות', 'logs.info': 'מידע', 'logs.warn': 'אזהרה', 'logs.error': 'שגיאה',
      'logs.filterCat': 'סנן קטגוריה (לדוגמה net)', 'logs.refresh': 'רענן',
      'logs.download': 'הורד', 'logs.clear': 'נקה', 'logs.none': 'אין רשומות ביומן.',
      'login.subtitle': 'התחבר כדי להמשיך', 'login.username': 'שם משתמש',
      'login.password': 'סיסמה', 'login.signin': 'התחבר',
      'login.error': 'שם משתמש או סיסמה שגויים.', 'login.connError': 'שגיאת חיבור.',
      'toast.saved': 'ההגדרות נשמרו', 'toast.saveFailed': 'השמירה נכשלה: ',
      'toast.sendingTest': 'שולח דוא"ל בדיקה…', 'toast.testSent': 'דוא"ל בדיקה נשלח',
      'toast.testFailed': 'הבדיקה נכשלה (בדוק הגדרות SMTP ויומן)',
      'toast.restarting': 'המכשיר מאותחל…',
      'confirm.restart': 'לאתחל את מכשיר הניטור כעת?\nהניטור ייעצר לכ-20 שניות.',
      'confirm.clearLogs': 'לנקות את כל היומן? לא ניתן לבטל פעולה זו.'
    }
  };

  function getLang() { return localStorage.getItem(LANG_KEY) || 'en'; }
  function t(key) {
    const l = getLang();
    return (translations[l] && translations[l][key]) || translations.en[key] || key;
  }
  function applyLang(lang) {
    localStorage.setItem(LANG_KEY, lang);
    document.documentElement.lang = lang;
    document.documentElement.dir = (lang === 'he') ? 'rtl' : 'ltr';
    document.querySelectorAll('[data-i18n]').forEach(el => {
      const txt = t(el.dataset.i18n);
      if (el.children.length === 0) {
        el.textContent = txt;                 // leaf element: safe to replace
      } else {
        // Element wraps children (e.g. a <label> around an <input>): update
        // ONLY the leading text node so inputs/selects are never destroyed.
        let node = Array.prototype.find.call(el.childNodes,
          n => n.nodeType === 3 && n.textContent.trim().length);
        if (node) node.textContent = txt + ' ';
        else el.insertBefore(document.createTextNode(txt + ' '), el.firstChild);
      }
    });
    document.querySelectorAll('[data-i18n-ph]').forEach(el => { el.placeholder = t(el.dataset.i18nPh); });
    if (typeof window.onLangChange === 'function') window.onLangChange();
  }

  window.I18N = { t, getLang, applyLang };

  document.addEventListener('DOMContentLoaded', () => {
    const sel = document.getElementById('langSel');
    if (sel) {
      sel.value = getLang();
      sel.addEventListener('change', () => applyLang(sel.value));
    }
    applyLang(getLang());
  });
})();
