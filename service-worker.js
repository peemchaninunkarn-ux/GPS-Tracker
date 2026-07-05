const CACHE_NAME = 'gps-tracking-pwa-path-fixed-v1';
const APP_SCOPE = '/GPS-Tracker/';
const APP_INDEX = '/GPS-Tracker/index.html';
const APP_ASSETS = [
  APP_INDEX,
  '/GPS-Tracker/manifest.webmanifest',
  '/GPS-Tracker/icon-192.png',
  '/GPS-Tracker/icon-512.png'
];

self.addEventListener('install', event => {
  self.skipWaiting();
  event.waitUntil(
    caches.open(CACHE_NAME).then(cache => cache.addAll(APP_ASSETS).catch(() => null))
  );
});

self.addEventListener('activate', event => {
  event.waitUntil(
    caches.keys()
      .then(keys => Promise.all(keys.map(key => key !== CACHE_NAME ? caches.delete(key) : null)))
      .then(() => self.clients.claim())
  );
});

self.addEventListener('fetch', event => {
  const req = event.request;
  const url = new URL(req.url);

  // อย่า cache ข้อมูลสด: Firebase, OpenStreetMap, Nominatim
  if (
    url.href.includes('firebaseio.com') ||
    url.href.includes('tile.openstreetmap.org') ||
    url.href.includes('nominatim.openstreetmap.org') ||
    url.href.includes('unpkg.com')
  ) {
    event.respondWith(fetch(req));
    return;
  }

  // เวลาเปิดจากไอคอน Home Screen ให้กลับมาที่หน้าเว็บของโปรเจกต์เสมอ ไม่หลุดไป root แล้ว 404
  if (req.mode === 'navigate') {
    event.respondWith(
      fetch(req).catch(() => caches.match(APP_INDEX)).then(response => response || caches.match(APP_INDEX))
    );
    return;
  }

  event.respondWith(
    fetch(req).then(response => {
      const copy = response.clone();
      if (url.pathname.startsWith(APP_SCOPE)) {
        caches.open(CACHE_NAME).then(cache => cache.put(req, copy)).catch(() => null);
      }
      return response;
    }).catch(() => caches.match(req))
  );
});
