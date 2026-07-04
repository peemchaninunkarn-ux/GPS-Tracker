// GPS Vehicle Tracking System V5 ENTERPRISE HISTORY
// Board: LilyGO T-A7670E / ESP32 + A7670E
// Network: True 4G
// Database: Firebase Realtime Database
//
// สิ่งที่แก้ใน V4 FINAL:
// - อ่าน GPS เร็วขึ้น
// - ส่ง Firebase เร็วขึ้นแต่ยังเสถียร
// - แก้ speed ติดลบ
// - ลด ERROR จาก HTTPTERM/HTTPINIT
// - ไม่เปิด/ปิด HTTP ซ้ำทุกครั้ง
// - เพิ่ม sequence และ deviceMillis ให้เว็บรู้ว่าข้อมูลใหม่จริง
// - ใช้ร่วมกับ index_v4_final.html
//
// หมายเหตุ:
// V5 Enterprise History ใช้ร่วมกับ index_v5_enterprise_history.html
// ส่งตำแหน่งล่าสุด + บันทึกประวัติลง Firebase history

#define TINY_GSM_MODEM_SIM7600

#include <TinyGsmClient.h>

#define SerialMon Serial
#define SerialAT  Serial1

#define MODEM_TX      26
#define MODEM_RX      25
#define MODEM_PWRKEY  4
#define MODEM_RESET   27
#define MODEM_BAUD    115200

const char apn[]  = "internet";
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

const unsigned long GPS_INTERVAL = 700;       // อ่าน GPS ทุก 0.7 วิ
const unsigned long UPLOAD_INTERVAL = 1200;   // ส่งตำแหน่งล่าสุดทุก 1.2 วิ
const unsigned long HISTORY_INTERVAL = 3000;  // บันทึกประวัติทุก 3 วิ เพื่อลดดีเลย์

const double MIN_DISPLAY_SPEED_KMH = 3.0;      // ต่ำกว่านี้ให้ถือว่าจอด
const double GPS_DRIFT_THRESHOLD_M = 15.0;     // พิกัดแกว่งไม่เกินนี้ให้ถือว่าจอด

double latestLat = 0;
double latestLon = 0;
double latestSpeed = 0;
double latestRawSpeed = 0;
int latestSatellites = 0;

double lastAcceptedLat = 0;
double lastAcceptedLon = 0;
bool hasAcceptedPoint = false;

bool gpsFixed = false;
bool httpReady = false;
bool hasNewGPS = false;

String sendAT(String cmd, uint32_t timeout = 3000) {
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
  String gps = sendAT("AT+CGNSSINFO", 1500);

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

  for (int i = 0; i < 20; i++) {
    String part = getPart(data, i);

    if (part == "N" || part == "S") {
      lat = getPart(data, i - 1).toDouble();
      if (part == "S") lat = -lat;

      lon = getPart(data, i + 1).toDouble();
      String lonDir = getPart(data, i + 2);
      if (lonDir == "W") lon = -lon;

      speedKmh = abs(getPart(data, i + 5).toDouble());
      break;
    }
  }

  if (lat == 0 || lon == 0) {
    SerialMon.println("อ่าน Latitude / Longitude ไม่สำเร็จ");
    gpsFixed = false;
    return false;
  }

  latestLat = lat;
  latestLon = lon;
  latestSatellites = satellites;
  latestRawSpeed = speedKmh;

  double moveDistance = 0;
  if (hasAcceptedPoint) {
    moveDistance = distanceMeters(lastAcceptedLat, lastAcceptedLon, lat, lon);
  }

  // กรองความเร็วตอนรถจอด / GPS drift
  // ถ้าพิกัดแกว่งไม่เกิน 15 เมตร หรือความเร็วต่ำกว่า 3 km/h ให้ส่งเป็น 0
  if (!hasAcceptedPoint || moveDistance <= GPS_DRIFT_THRESHOLD_M || speedKmh < MIN_DISPLAY_SPEED_KMH) {
    latestSpeed = 0;
  } else {
    latestSpeed = speedKmh;
    lastAcceptedLat = lat;
    lastAcceptedLon = lon;
  }

  if (!hasAcceptedPoint) {
    lastAcceptedLat = lat;
    lastAcceptedLon = lon;
    hasAcceptedPoint = true;
  }

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
  SerialMon.print("Raw speed km/h: ");
  SerialMon.println(speedKmh, 2);
  SerialMon.print("Filtered speed km/h: ");
  SerialMon.println(latestSpeed, 2);
  SerialMon.print("Sequence: ");
  SerialMon.println(sequenceNo);

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
  String dataRes = sendAT(dataCmd, 5000);

  if (dataRes.indexOf("DOWNLOAD") < 0) {
    SerialMon.println("HTTPDATA failed, reset HTTP next round");
    httpReady = false;
    return false;
  }

  SerialAT.print(json);
  delay(350);

  String actionRes = sendAT("AT+HTTPACTION=4", 12000); // PUT

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

  // บันทึก History แยกเป็นจุด ๆ โดยไม่เขียนทับตำแหน่งล่าสุด
  // Path ตัวอย่าง: /vehicle/history/p00000123.json
  unsigned long now = millis();
  if (latestOK && now - lastHistoryUpload >= HISTORY_INTERVAL) {
    lastHistoryUpload = now;
    char key[20];
    sprintf(key, "/p%010lu.json", sequenceNo);
    String historyPath = String(FIREBASE_HISTORY_BASE) + String(key);
    httpPutJson(historyPath, json);
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

  SerialMon.println("LILYGO A7670E GPS + Firebase V5 ENTERPRISE HISTORY");

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
