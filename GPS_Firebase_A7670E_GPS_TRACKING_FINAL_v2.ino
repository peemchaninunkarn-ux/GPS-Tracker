// GPS Tracking System
// Board: LilyGO T-A7670E / ESP32 + A7670E
// Network: True 4G
// Database: Firebase Realtime Database
//
// GPS Tracking:
// - อ่าน GPS เร็วขึ้น
// - ส่ง Firebase เร็วขึ้นแต่ยังเสถียร
// - แก้ speed ติดลบ
// - ลด ERROR จาก HTTPTERM/HTTPINIT
// - ไม่เปิด/ปิด HTTP ซ้ำทุกครั้ง
// - เพิ่ม sequence และ deviceMillis ให้เว็บรู้ว่าข้อมูลใหม่จริง
// - ใช้ร่วมกับ index_v4_final.html
//
// หมายเหตุ:
// V5 History ใช้ร่วมกับ index_v5_enterprise_history.html
// ส่งตำแหน่งล่าสุด + บันทึกประวัติลง Firebase history
// FINAL v2: แก้ความเร็ว + History ไม่เขียนทับหลังบอร์ดรีสตาร์ต

#define TINY_GSM_MODEM_SIM7600

#include <TinyGsmClient.h>

#define SerialMon Serial
#define SerialAT Serial1

#define MODEM_TX   26
#define MODEM_RX   25
#define MODEM_PWRKEY 4
#define MODEM_RESET  27
#define MODEM_BAUD  115200

const char apn[] = "internet";
const char user[] = "";
const char pass[] = "";

#define FIREBASE_HOST "https://gps-tracker-a20ca-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_PATH "/vehicle/location.json"
#define FIREBASE_HISTORY_BASE "/vehicle/history"

TinyGsm modem(SerialAT);

unsigned long lastGPSRead = 0;
unsigned long lastUpload = 0;
unsigned long lastHistoryUpload = 0;
unsigned long sequenceNo = 0;

const unsigned long GPS_INTERVAL = 1000;       // อ่าน GPS ทุก 1 วิ
const unsigned long UPLOAD_INTERVAL = 5000;    // ส่งตำแหน่งล่าสุดทุก 5 วิ ตามที่ต้องการ
const unsigned long HISTORY_INTERVAL = 30000;  // บันทึกประวัติทุก 30 วิ เพื่อลดภาระ HTTP ไม่ให้ realtime ช้า

const double MIN_DISPLAY_SPEED_KMH = 3.0;      // ต่ำกว่านี้ให้ถือว่าจอด
const double GPS_DRIFT_THRESHOLD_M = 8.0;      // ระยะต่ำกว่านี้มองเป็น GPS drift ตอนจอด
const double MIN_MOVE_DISTANCE_M = 6.0;        // ถ้าพิกัดเปลี่ยนเกินนี้ภายในรอบล่าสุด ให้ถือว่าเริ่มเคลื่อน
const double KNOT_TO_KMH = 1.852;             // แปลงความเร็ว GNSS เป็น km/h
const double SPEED_CALIBRATION = 1.0;         // ปรับละเอียดได้ภายหลังหากทดสอบจริงยังคลาดเล็กน้อย

double latestLat = 0;
double latestLon = 0;
double latestSpeed = 0;
double latestRawSpeed = 0;
int latestSatellites = 0;
String latestLocalDate = "";
String latestLocalTime = "";
unsigned long latestSortKey = 0;

double lastAcceptedLat = 0;
double lastAcceptedLon = 0;
bool hasAcceptedPoint = false;
unsigned long lastAcceptedTime = 0;

bool gpsFixed = false;
bool httpReady = false;
bool hasNewGPS = false;

String sendAT(String cmd, uint32_t timeout = 3000, String expected = "OK") {
 String res = "";

 while (SerialAT.available()) SerialAT.read();

 SerialMon.print(">> ");
 SerialMon.println(cmd);
 SerialAT.println(cmd);

 uint32_t start = millis();
 while (millis() - start < timeout) {
  while (SerialAT.available()) {
   res += (char)SerialAT.read();
  }

  // สำคัญ: ถ้าได้คำตอบที่ต้องการแล้วให้ออกทันที ไม่รอ timeout เต็ม
  // ช่วยลดดีเลย์จาก 10-12 วินาที เหลือไม่กี่วินาที
  if (expected.length() > 0 && res.indexOf(expected) >= 0) break;
  if (res.indexOf("ERROR") >= 0 && expected == "OK") break;

  delay(1);
 }

 SerialMon.println(res);
 return res;
}

void powerOnModem() {
 pinMode(MODEM_PWRKEY, OUTPUT);
 pinMode(MODEM_RESET, OUTPUT);

 digitalWrite(MODEM_RESET, HIGH);
 delay(100);

 digitalWrite(MODEM_PWRKEY, HIGH);
 delay(1000);
 digitalWrite(MODEM_PWRKEY, LOW);
 delay(5000);
}

String getPart(String data, int index) {
 int current = 0;
 int start = 0;

 for (int i = 0; i <= data.length(); i++) {
  if (data[i] == ',' || i == data.length()) {
   if (current == index) {
    String part = data.substring(start, i);
    part.trim();
    return part;
   }
   current++;
   start = i + 1;
  }
 }
 return "";
}


bool isLeapYear(int year) {
 return (year % 400 == 0) || ((year % 4 == 0) && (year % 100 != 0));
}

int daysInMonth(int year, int month) {
 const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
 if (month == 2 && isLeapYear(year)) return 29;
 if (month < 1 || month > 12) return 31;
 return days[month - 1];
}

void addOneDay(int &year, int &month, int &day) {
 day++;
 if (day > daysInMonth(year, month)) {
  day = 1;
  month++;
  if (month > 12) { month = 1; year++; }
 }
}

bool parseGnssLocalDateTime(String dateRaw, String utcRaw) {
 dateRaw.trim(); utcRaw.trim();
 if (dateRaw.length() < 6 || utcRaw.length() < 6) return false;

 int day = dateRaw.substring(0, 2).toInt();
 int month = dateRaw.substring(2, 4).toInt();
 int year2 = dateRaw.substring(4, 6).toInt();
 int year = 2000 + year2;
 int hour = utcRaw.substring(0, 2).toInt();
 int minute = utcRaw.substring(2, 4).toInt();
 int second = utcRaw.substring(4, 6).toInt();

 // GNSS ให้เวลา UTC; ประเทศไทย UTC+7
 hour += 7;
 if (hour >= 24) { hour -= 24; addOneDay(year, month, day); }

 char dateBuf[11];
 char timeBuf[9];
 snprintf(dateBuf, sizeof(dateBuf), "%04d-%02d-%02d", year, month, day);
 snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", hour, minute, second);
 latestLocalDate = String(dateBuf);
 latestLocalTime = String(timeBuf);
 latestSortKey = (unsigned long)(year % 100) * 100000000UL + (unsigned long)month * 1000000UL + (unsigned long)day * 10000UL + (unsigned long)hour * 100UL + (unsigned long)minute;
 return true;
}

double distanceMeters(double lat1, double lon1, double lat2, double lon2) {
 const double R = 6371000.0;
 double p1 = lat1 * PI / 180.0;
 double p2 = lat2 * PI / 180.0;
 double dp = (lat2 - lat1) * PI / 180.0;
 double dl = (lon2 - lon1) * PI / 180.0;

 double a = sin(dp / 2.0) * sin(dp / 2.0) +
       cos(p1) * cos(p2) * sin(dl / 2.0) * sin(dl / 2.0);
 double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
 return R * c;
}

void setupGNSS() {
 SerialMon.println("Setting GNSS...");
 sendAT("AT+CGNSSPWR=1", 3000);
 sendAT("AT+CGNSSPWR?", 3000);
 sendAT("AT+CGNSSMODE=15", 2000); // ถ้า ERROR ไม่เป็นไร
 SerialMon.println("GNSS started");
}

bool setHTTPURL(String path) {
 String url = String(FIREBASE_HOST) + path;
 String r = sendAT("AT+HTTPPARA=\"URL\",\"" + url + "\"", 3000);
 return r.indexOf("OK") >= 0;
}

bool setupHTTP() {
 SerialMon.println("Setting HTTP...");

 // ERROR ได้ถ้าไม่มี session เดิม ไม่เป็นไร
 sendAT("AT+HTTPTERM", 500);
 delay(200);

 String r1 = sendAT("AT+HTTPINIT", 3000);
 sendAT("AT+HTTPSSL=1", 2000);
 sendAT("AT+HTTPPARA=\"CID\",1", 3000);
 String r4 = sendAT("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 3000);

 if (r1.indexOf("OK") >= 0 && r4.indexOf("OK") >= 0) {
  SerialMon.println("HTTP ready");
  httpReady = true;
  return true;
 }

 SerialMon.println("HTTP setup failed");
 httpReady = false;
 return false;
}

bool readGPS() {
 String gps = sendAT("AT+CGNSSINFO", 1500, "OK");

 if (gps.indexOf("+CGNSSINFO:") < 0 || gps.indexOf(",,,,") >= 0) {
  SerialMon.println("GPS ยังจับสัญญาณไม่ได้");
  gpsFixed = false;
  return false;
 }

 int start = gps.indexOf(":") + 1;
 int end = gps.indexOf("OK");

 if (end < 0) {
  SerialMon.println("GPS response ไม่สมบูรณ์");
  return false;
 }

 String data = gps.substring(start, end);
 data.trim();

 int satellites = getPart(data, 1).toInt();

 double lat = 0;
 double lon = 0;
 double speedKmh = 0;
 String gnssDate = "";
 String gnssUtc = "";

 for (int i = 0; i < 20; i++) {
  String part = getPart(data, i);

  if (part == "N" || part == "S") {
   lat = getPart(data, i - 1).toDouble();
   if (part == "S") lat = -lat;

   lon = getPart(data, i + 1).toDouble();
   String lonDir = getPart(data, i + 2);
   if (lonDir == "W") lon = -lon;

   // รูปแบบ +CGNSSINFO หลัง N/S คือ:
   // lon, E/W, date, UTC, altitude, speed, course, ...
   // เดิมใช้ i+5 ซึ่งคือ altitude ทำให้ความเร็วมั่วตอนจอด
   // ค่าความเร็วที่ถูกต้องคือ i+6
   gnssDate = getPart(data, i + 3);
   gnssUtc = getPart(data, i + 4);
   double speedKnots = abs(getPart(data, i + 6).toDouble());
   speedKmh = speedKnots * KNOT_TO_KMH * SPEED_CALIBRATION;
   break;
  }
 }

 if (lat == 0 || lon == 0) {
  SerialMon.println("อ่าน Latitude / Longitude ไม่สำเร็จ");
  gpsFixed = false;
  return false;
 }

 parseGnssLocalDateTime(gnssDate, gnssUtc);

 latestLat = lat;
 latestLon = lon;
 latestSatellites = satellites;
 latestRawSpeed = speedKmh;

 unsigned long now = millis();
 double moveDistance = 0;
 double calcSpeedKmh = 0;

 if (hasAcceptedPoint) {
  moveDistance = distanceMeters(lastAcceptedLat, lastAcceptedLon, lat, lon);
  unsigned long dt = now - lastAcceptedTime;
  if (dt > 0) calcSpeedKmh = (moveDistance / (dt / 1000.0)) * 3.6;
 }

 // Logic ใหม่:
 // 1) ถ้า speed จาก GNSS เกิน 3 km/h ให้ถือว่าวิ่งทันที
 // 2) ถ้า speed จาก GNSS ยังไม่มา แต่ตำแหน่งขยับเกิน 6 m และ calcSpeed สมเหตุสมผล ให้ถือว่าวิ่ง
 // 3) ถ้าจอดและ GPS drift เล็ก ๆ ให้ speed = 0 ทันที ไม่ค้างค่ารอบก่อน
 bool movingByGpsSpeed = speedKmh >= MIN_DISPLAY_SPEED_KMH;
 bool movingByDistance = hasAcceptedPoint && moveDistance >= MIN_MOVE_DISTANCE_M && calcSpeedKmh >= MIN_DISPLAY_SPEED_KMH && calcSpeedKmh <= 180.0;

 if (movingByGpsSpeed) {
  latestSpeed = speedKmh;
 } else if (movingByDistance) {
  latestSpeed = calcSpeedKmh;
 } else {
  latestSpeed = 0;
 }

 // อัปเดตจุดล่าสุดทุกครั้งที่อ่าน GPS ได้ เพื่อไม่ให้ marker/ระยะทางหน่วงจาก threshold เดิม
 lastAcceptedLat = lat;
 lastAcceptedLon = lon;
 lastAcceptedTime = now;
 hasAcceptedPoint = true;

 gpsFixed = true;
 hasNewGPS = true;
 sequenceNo++;

 SerialMon.println("----- GPS UPDATED -----");
 SerialMon.print("Latitude: ");
 SerialMon.println(lat, 7);
 SerialMon.print("Longitude: ");
 SerialMon.println(lon, 7);
 SerialMon.print("Satellites: ");
 SerialMon.println(satellites);
 SerialMon.print("GNSS speed converted km/h: ");
 SerialMon.println(speedKmh, 2);
 SerialMon.print("Move distance m: ");
 SerialMon.println(moveDistance, 2);
 SerialMon.print("Calc speed km/h: ");
 SerialMon.println(calcSpeedKmh, 2);
 SerialMon.print("Filtered speed km/h: ");
 SerialMon.println(latestSpeed, 2);
 SerialMon.print("Sequence: ");
 SerialMon.println(sequenceNo);
 SerialMon.print("Local date/time: ");
 SerialMon.print(latestLocalDate);
 SerialMon.print(" ");
 SerialMon.println(latestLocalTime);

 return true;
}

bool httpPutJson(String path, String json) {
 if (!httpReady) {
  if (!setupHTTP()) return false;
 }

 if (!setHTTPURL(path)) {
  SerialMon.println("Set Firebase URL failed");
  httpReady = false;
  return false;
 }

 SerialMon.println("Sending to Firebase path: " + path);
 SerialMon.println(json);

 String dataCmd = "AT+HTTPDATA=" + String(json.length()) + ",10000";
 String dataRes = sendAT(dataCmd, 5000, "DOWNLOAD");

 if (dataRes.indexOf("DOWNLOAD") < 0) {
  SerialMon.println("HTTPDATA failed, reset HTTP next round");
  httpReady = false;
  return false;
 }

 SerialAT.print(json);
 delay(350);

 String actionRes = sendAT("AT+HTTPACTION=4", 12000, "+HTTPACTION"); // PUT

 if (actionRes.indexOf("+HTTPACTION: 4,200") >= 0) {
  SerialMon.println("Firebase sent OK");
  return true;
 }

 SerialMon.println("Firebase send failed");
 httpReady = false;
 return false;
}

String buildLocationJson() {
 String json = "{";
 json += "\"latitude\":" + String(latestLat, 7) + ",";
 json += "\"longitude\":" + String(latestLon, 7) + ",";
 json += "\"satellites\":" + String(latestSatellites) + ",";
 json += "\"speed\":" + String(latestSpeed, 2) + ",";
 json += "\"rawSpeed\":" + String(latestRawSpeed, 2) + ",";
 json += "\"status\":\"online\",";
 json += "\"sequence\":" + String(sequenceNo) + ",";
 json += "\"deviceMillis\":" + String(millis()) + ",";
 json += "\"localDate\":\"" + latestLocalDate + "\",";
 json += "\"localTime\":\"" + latestLocalTime + "\",";
 json += "\"sortKey\":" + String(latestSortKey) + ",";
 json += "\"savedAt\":" + String(millis());
 json += "}";
 return json;
}

bool sendToFirebase() {
 if (!gpsFixed || !hasNewGPS) {
  SerialMon.println("Skip Firebase: no new GPS");
  return false;
 }

 String json = buildLocationJson();
 bool latestOK = httpPutJson(String(FIREBASE_PATH), json);

 // บันทึก History แยกเป็นจุด ๆ
 // ใช้วัน+เวลา GNSS เป็น key เพื่อไม่ให้เขียนทับข้อมูลเดิมเมื่อบอร์ดรีสตาร์ต
 // ตัวอย่าง: /vehicle/history/h20260808_143015.json
 unsigned long now = millis();
 if (latestOK && now - lastHistoryUpload >= HISTORY_INTERVAL) {
  lastHistoryUpload = now;

  if (latestLocalDate.length() == 10 && latestLocalTime.length() >= 8) {
   String dateKey = latestLocalDate;
   String timeKey = latestLocalTime;
   dateKey.replace("-", "");
   timeKey.replace(":", "");

   String historyKey = "/h" + dateKey + "_" + timeKey + ".json";
   String historyPath = String(FIREBASE_HISTORY_BASE) + historyKey;

   SerialMon.println("History key: " + historyKey);
   httpPutJson(historyPath, json);
  } else {
   SerialMon.println("Skip History: GNSS date/time not ready");
  }
 }

 if (latestOK) {
  hasNewGPS = false;
  return true;
 }
 return false;
}

void setup() {
 SerialMon.begin(115200);
 delay(2000);

 SerialMon.println("LILYGO A7670E GPS Tracking");

 SerialAT.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
 delay(1000);

 powerOnModem();

 sendAT("AT", 1000);
 sendAT("ATE0", 1000);
 sendAT("AT+CPIN?", 3000);
 sendAT("AT+CSQ", 3000);

 SerialMon.println("Skip modem.restart for A7670E stability");

 SerialMon.println("Waiting for network...");
 if (!modem.waitForNetwork(60000L)) {
  SerialMon.println("เชื่อมต่อเครือข่ายไม่สำเร็จ");
  return;
 }

 SerialMon.println("Network connected");

 SerialMon.println("Connecting True 4G...");
 if (!modem.gprsConnect(apn, user, pass)) {
  SerialMon.println("เชื่อมต่ออินเทอร์เน็ตไม่สำเร็จ");
  return;
 }

 SerialMon.println("4G Internet connected");

 setupGNSS();
 setupHTTP();

 SerialMon.println("System ready");
}

void loop() {
 unsigned long now = millis();

 if (now - lastGPSRead >= GPS_INTERVAL) {
  lastGPSRead = now;
  readGPS();
 }

 if (now - lastUpload >= UPLOAD_INTERVAL) {
  lastUpload = now;
  sendToFirebase();
 }
}
