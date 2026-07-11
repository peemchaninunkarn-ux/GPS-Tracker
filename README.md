# GPS Tracking System – Professional Single-file UI

เวอร์ชันนี้ปรับเฉพาะหน้าตาและการจัดเมนู โดยยังคง CSS และ JavaScript ทั้งหมดไว้ภายใน `index.html` ไฟล์เดียว

## สิ่งที่ปรับ
- Header: GPS Tracking System
- Dashboard คง 5 การ์ดเดิม
- Sidebar เปลี่ยนเป็น “เมนูระบบ” และไม่เพิ่มข้อมูลทะเบียน/คนขับ/ข้อมูลรถส่วนเกิน
- เมนู “อื่น ๆ” รวม ติดตามรถ / แสดงหรือซ่อนเส้นทาง / ล้างเส้นทาง
- Popup และแผนที่ปรับให้ดูเป็นทางการและทันสมัย
- รองรับมือถือ แท็บเล็ต คอมพิวเตอร์ และ PWA
- ไม่แยก `style.css`, `app.js`, `history.js`, `ui.js`
- ไม่แก้ Logic GPS, Firebase, ความเร็ว, สถานะ, Marker หรือรอบอัปเดต

## ไฟล์ที่ให้อัปโหลดขึ้น GitHub root
- `index.html`
- `manifest.webmanifest`
- `service-worker.js`
- `icon-192.png`
- `icon-512.png`
- `GPS_Firebase_A7670E_GPS_TRACKING_STABLE.ino`

หลังอัปโหลด ให้รอ GitHub Pages deploy แล้วรีเฟรชแบบไม่ใช้แคช หรือลบแอปเดิมและติดตั้งใหม่เพื่อให้ UI ใหม่แสดงผล
