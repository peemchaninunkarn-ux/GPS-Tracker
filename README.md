# GPS Tracking

เวอร์ชัน Stable Motion Logic

ไฟล์สำหรับอัปโหลดขึ้น GitHub Pages:
- index.html
- manifest.webmanifest
- service-worker.js
- icon-192.png
- icon-512.png

วิธีใช้:
1. แตก ZIP
2. อัปโหลดไฟล์ทั้งหมดขึ้น GitHub Repository ที่ root
3. Commit
4. เปิดลิงก์ https://peemchaninunkarn-ux.github.io/GPS-Tracker/index.html

หมายเหตุ:
- service-worker.js เวอร์ชันนี้ไม่ cache ข้อมูล Firebase เพื่อไม่ให้ตำแหน่งค้าง
- รถวิ่ง = Online ตลอด
- รถหยุดเกิน 3 นาที = Offline ตามที่ตั้งไว้
