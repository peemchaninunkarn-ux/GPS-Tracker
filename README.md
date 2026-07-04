# GPS Tracker Enterprise V8 Final

ไฟล์สำหรับอัปโหลดขึ้น GitHub Pages / Netlify

## วิธีใช้
1. อัปโหลดไฟล์ทั้งหมดในโฟลเดอร์นี้ขึ้น GitHub repository
2. เปิด GitHub Pages จาก Settings > Pages
3. เปิดลิงก์บนมือถือ แล้วกด Add to Home Screen
4. อัปโหลดไฟล์ `.ino` ลงบอร์ด ESP32 + A7670E

## ไฟล์สำคัญ
- `index.html` หน้าเว็บหลัก / PWA
- `manifest.webmanifest` ข้อมูลติดตั้งเป็นแอป
- `service-worker.js` ระบบ PWA cache
- `icon-192.png` และ `icon-512.png` ไอคอนแอป
- `GPS_Firebase_A7670E_V6_ENTERPRISE_FINAL.ino` โค้ด ESP32

## ฟีเจอร์
- Real-time tracking
- Smart GPS drift filter
- Raw GPS speed when moving
- Online/offline status
- Live route
- History from Firebase
- Place name by OpenStreetMap Nominatim
- Route replay
- CSV export
- Dark mode
