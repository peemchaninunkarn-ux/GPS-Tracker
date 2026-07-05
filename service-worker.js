// GPS Tracking - realtime friendly service worker
// ไม่ cache ข้อมูล Firebase/แผนที่ เพื่อไม่ให้ตำแหน่งค้าง
self.addEventListener('install', event => {
  self.skipWaiting();
});

self.addEventListener('activate', event => {
  event.waitUntil(
    caches.keys().then(keys => Promise.all(keys.map(key => caches.delete(key))))
  );
  self.clients.claim();
});

self.addEventListener('fetch', event => {
  event.respondWith(fetch(event.request));
});
