GPS Tracking Final Stable System

ไฟล์ที่ต้องอัปโหลดขึ้น GitHub Pages:
- index.html
- manifest.webmanifest
- service-worker.js
- icon-192.png
- icon-512.png
- README.md

ไฟล์ที่ต้องอัปโหลดลงบอร์ด Arduino:
- GPS_Firebase_A7670E_GPS_TRACKING_FINAL_STABLE.ino

เป้าหมายการทำงาน:
- รถออกตัว: เว็บเปลี่ยนเป็นกำลังวิ่งภายใน 3-5 วินาที
- รถวิ่ง: ส่งตำแหน่งล่าสุดขึ้น Firebase ทุก 5 วินาที
- รถจอด: speed กลับเป็น 0 ภายใน 3-10 วินาที
- เว็บอ่าน Firebase ทุก 2 วินาที เพื่อจับข้อมูลใหม่ทันทีเมื่อบอร์ดส่งมา

หมายเหตุ:
- ถ้าความเร็วเพี้ยนคงที่ ให้ปรับ SPEED_SCALE ในไฟล์ .ino เช่น 0.90 หรือ 1.10
- service-worker.js ไม่ cache ข้อมูล เพื่อไม่ให้ข้อมูล GPS ค้างบนมือถือ
