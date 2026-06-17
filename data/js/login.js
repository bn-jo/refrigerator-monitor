// login.js — submit credentials, set session cookie via server
document.getElementById('loginForm').addEventListener('submit', async (e) => {
  e.preventDefault();
  const err = document.getElementById('err');
  err.hidden = true;
  try {
    const res = await fetch('/api/login', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        user: document.getElementById('user').value,
        pass: document.getElementById('pass').value
      })
    });
    if (res.ok) {
      location.href = '/';
    } else {
      err.textContent = window.I18N ? window.I18N.t('login.error') : 'Invalid username or password.';
      err.hidden = false;
    }
  } catch (_) {
    err.textContent = window.I18N ? window.I18N.t('login.connError') : 'Connection error.';
    err.hidden = false;
  }
});
