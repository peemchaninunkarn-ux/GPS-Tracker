const APP_SCOPE = '/GPS-Tracker/';

self.addEventListener('install', event => {
  self.skipWaiting();
});

self.addEventListener('activate', event => {
  event.waitUntil(
    caches.keys().then(keys => Promise.all(keys.map(k => caches.delete(k))))
  );
  self.clients.claim();
});

self.addEventListener('fetch', event => {
  const request = event.request;
  const url = new URL(request.url);

  if (request.mode === 'navigate') {
    if (url.pathname === '/GPS-Tracker' || url.pathname === '/GPS-Tracker/') {
      event.respondWith(fetch('/GPS-Tracker/index.html', { cache: 'no-store' }));
      return;
    }
    event.respondWith(fetch(request, { cache: 'no-store' }).catch(() => fetch('/GPS-Tracker/index.html', { cache: 'no-store' })));
    return;
  }

  event.respondWith(fetch(request, { cache: 'no-store' }));
});
