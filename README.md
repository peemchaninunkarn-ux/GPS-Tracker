GPS Tracking - PWA Path Fixed

ให้อัปโหลดไฟล์ทั้งหมดนี้ทับไฟล์เดิมใน GitHub Repository: GPS-Tracker

ไฟล์ที่ต้องอยู่ระดับ root ของ repository:
- index.html
- manifest.webmanifest
- service-worker.js
- icon-192.png
- icon-512.png

หลังอัปโหลด:
1. ลบไอคอน GPS Tracking เดิมออกจากมือถือ
2. เปิด Chrome แล้วเข้า https://peemchaninunkarn-ux.github.io/GPS-Tracker/index.html
3. กดเมนู ⋮ แล้วเลือก Install app หรือ Add to Home screen ใหม่

หมายเหตุ: ชุดนี้แก้เฉพาะ PWA path/start_url/scope/service-worker ไม่แตะ logic GPS, ความเร็ว, marker, Firebase
