#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <VL53L1X.h>
#include <Adafruit_TCS34725.h>
#include <BH1750.h>
#include <Adafruit_SHT31.h>

/* ================= DEBUG ================= */
#define DEBUG 1

/* ================= PINLER ================= */
#define PIN_SDA_1    8
#define PIN_SCL_1    9
#define PIN_SDA_2    12
#define PIN_SCL_2    13
#define PIN_BUZZ     5
#define PIN_LAZER    6
#define PIN_RENK_LED 3

/* ================= SABITLER ================= */
#define MPU_ADDR              0x68
#define KASA_BOYU_MM          130
#define SENSOR_READ_INTERVAL  100
#define MPU_SCALE_FACTOR      16384.0
#define DISPLAY_WIDTH         128
#define DISPLAY_HEIGHT        64
#define BIP_SURESI            60
#define RENK_OKUMA_SURESI     10000
#define RENK_ORNEKLEME_SAYISI 20

/* ================= WIFI ================= */
const char* ssid     = "Akilli_Metre";
const char* password = "GuvenliSifre123!";

/* ================= MODLAR ================= */
enum Mod {
  MOD_MENU   = 0,
  MOD_MESAFE = 1,
  MOD_TERAZI = 2,
  MOD_RENK   = 3,
  MOD_ISIK   = 4,
  MOD_HAVA   = 5
};

enum RenkDurum {
  RENK_BEKLEME = 0,
  RENK_OKUYOR  = 1,
  RENK_HAZIR   = 2
};

/* ================= NESNELER ================= */
WebServer server(80);
Adafruit_SSD1306 display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, -1);
VL53L1X mesafeSensor;
BH1750 lightMeter;
Adafruit_SHT31 sht31;
Adafruit_TCS34725 tcs(TCS34725_INTEGRATIONTIME_154MS, TCS34725_GAIN_16X);

/* ================= DEGISKENLER ================= */
Mod      aktifMod = MOD_MENU;
unsigned long oncekiOkuma = 0;

float    mesafe = 0, lux = 0, sicaklik = 0, nem = 0;
float    egimX = 0, egimY = 0, egimX_prev = 0, egimY_prev = 0;
float    smoothFactor = 0.3;
uint16_t r = 0, g = 0, b = 0;
byte     gammaTable[256];

RenkDurum    renkDurum = RENK_BEKLEME;
unsigned long renkOkumaBaslangic = 0;
int          renkOrnekSayac = 0;
float        rSum = 0, gSum = 0, bSum = 0;

float mesafeMin = 999, mesafeMax = 0;
float sicaklikMin = 999, sicaklikMax = -999;

bool mesafeSensorOK = false, renkSensorOK = false;
bool isikSensorOK = false, havaSensorOK = false, mpuOK = false;

/* ================= YARDIMCI ================= */
void bip(int sure = BIP_SURESI) {
  digitalWrite(PIN_BUZZ, HIGH);
  delay(sure);
  digitalWrite(PIN_BUZZ, LOW);
}

void bipCift() {
  bip(80); delay(80); bip(80);
}

void debugLog(String msg) {
#if DEBUG
  Serial.println("[DBG] " + msg);
#endif
}

void mpuUyandir() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0);
  mpuOK = (Wire.endTransmission() == 0);
  debugLog(mpuOK ? "MPU6050 OK" : "MPU6050 HATA");
}

/* ================= SENSOR OKUMA ================= */
void okuMesafe() {
  if (!mesafeSensorOK) return;
  mesafe = (mesafeSensor.read() + KASA_BOYU_MM) / 10.0;
  if (mesafe < mesafeMin) mesafeMin = mesafe;
  if (mesafe > mesafeMax) mesafeMax = mesafe;
}

void okuRenk() {
  if (!renkSensorOK) return;

  if (renkDurum == RENK_BEKLEME) {
    renkDurum = RENK_OKUYOR;
    renkOkumaBaslangic = millis();
    renkOrnekSayac = 0;
    rSum = gSum = bSum = 0;
    digitalWrite(PIN_RENK_LED, HIGH);  // LED'i yak
    tcs.setInterrupt(false);           // Sensör LED'i aç
    debugLog("Renk okumasi basladi");
  }
  else if (renkDurum == RENK_OKUYOR) {
    unsigned long gecen = millis() - renkOkumaBaslangic;

    if (renkOrnekSayac < RENK_ORNEKLEME_SAYISI && gecen < RENK_OKUMA_SURESI) {
      unsigned long adimSuresi = RENK_OKUMA_SURESI / RENK_ORNEKLEME_SAYISI;
      if (gecen >= (unsigned long)(renkOrnekSayac * adimSuresi)) {
        float red, green, blue;
        tcs.getRGB(&red, &green, &blue);
        rSum += red; gSum += green; bSum += blue;
        renkOrnekSayac++;
        debugLog("Ornek " + String(renkOrnekSayac) + "/10");
      }
    }

    if (gecen >= RENK_OKUMA_SURESI || renkOrnekSayac >= RENK_ORNEKLEME_SAYISI) {
      r = (uint16_t)(rSum / renkOrnekSayac);
      g = (uint16_t)(gSum / renkOrnekSayac);
      b = (uint16_t)(bSum / renkOrnekSayac);
      tcs.setInterrupt(true);           // Sensör LED'i kapat
      digitalWrite(PIN_RENK_LED, LOW);  // LED'i söndür
      renkDurum = RENK_HAZIR;
      debugLog("Renk OK - R:" + String(r) + " G:" + String(g) + " B:" + String(b));
      bipCift();
    }
  }
}

void okuIsik() {
  if (!isikSensorOK) return;
  lux = lightMeter.readLightLevel();
}

void okuHava() {
  if (!havaSensorOK) return;
  sicaklik = sht31.readTemperature();
  nem      = sht31.readHumidity();
  if (!isnan(sicaklik)) {
    if (sicaklik < sicaklikMin) sicaklikMin = sicaklik;
    if (sicaklik > sicaklikMax) sicaklikMax = sicaklik;
  }
}

void okuEgim() {
  if (!mpuOK) return;
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);
  if (Wire.available() >= 6) {
    int16_t ax = Wire.read()<<8 | Wire.read();
    int16_t ay = Wire.read()<<8 | Wire.read();
    int16_t az = Wire.read()<<8 | Wire.read();
    float axg = (float)ax / MPU_SCALE_FACTOR;
    float ayg = (float)ay / MPU_SCALE_FACTOR;
    float azg = (float)az / MPU_SCALE_FACTOR;
    float nx = atan2(ayg, sqrt(axg*axg + azg*azg)) * 180.0 / PI;
    float ny = atan2(-axg, sqrt(ayg*ayg + azg*azg)) * 180.0 / PI;
    egimX = egimX_prev + smoothFactor * (nx - egimX_prev);
    egimY = egimY_prev + smoothFactor * (ny - egimY_prev);
    egimX_prev = egimX; egimY_prev = egimY;
    egimX = constrain(egimX, -45, 45);
    egimY = constrain(egimY, -45, 45);
  }
}

/* ================= EKRAN ================= */
void ekranTemizle() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
}

void ekranGosterMenu() {
  ekranTemizle();
  display.println("AKILLI METRE");
  display.println("-------------");
  display.println(ssid);
  display.print("IP: "); display.println(WiFi.softAPIP());
  display.println();
  display.println("Web'den baglanin");
  display.display();
}

void ekranGosterMesafe() {
  ekranTemizle();
  display.println("MESAFE");
  display.setTextSize(2);
  display.setCursor(10, 22);
  display.print(mesafe, 1); display.print(" cm");
  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print("Min:"); display.print(mesafeMin, 0);
  display.print("  Max:"); display.print(mesafeMax, 0);
  display.display();
}

void ekranGosterTerazi() {
  ekranTemizle();
  display.println("SU TERAZISI");
  int cx = 64, cy = 40;
  display.drawLine(20, cy, 108, cy, WHITE);
  display.drawLine(cx, 20, cx, 60, WHITE);
  display.drawCircle(cx, cy, 30, WHITE);
  int dx = constrain(cx - (int)(egimX * 0.8), 22, 106);
  int dy = constrain(cy + (int)(egimY * 0.8), 22, 58);
  display.fillCircle(dx, dy, 4, WHITE);
  display.setCursor(0, 55);
  display.print("X:"); display.print(egimX, 1);
  display.print("  Y:"); display.print(egimY, 1);
  display.display();
}

String renkAdi() {
  if (r > g && r > b && r > 100) return "KIRMIZI";
  if (g > r && g > b && g > 100) return "YESIL";
  if (b > r && b > g && b > 100) return "MAVI";
  if (r > 200 && g > 200 && b > 200) return "BEYAZ";
  if (r < 50  && g < 50  && b < 50)  return "SIYAH";
  return "KARISIK";
}

void ekranGosterRenk() {
  ekranTemizle();
  if (renkDurum == RENK_BEKLEME) {
    display.println("RENK SENSORU");
    display.println();
    display.println("Nesneyi sensore");
    display.println("yaklastir, sonra");
    display.println("web'den Renk");
    display.println("tusuna bas.");
  }
  else if (renkDurum == RENK_OKUYOR) {
    display.println("OKUMA YAPILIYOR");
    unsigned long gecen = millis() - renkOkumaBaslangic;
    int prog = constrain((gecen * 100) / RENK_OKUMA_SURESI, 0, 100);
    display.print("%" ); display.println(prog);
    display.drawRect(14, 30, 104, 12, WHITE);
    display.fillRect(16, 32, (prog * 100) / 100, 8, WHITE);
    display.setCursor(0, 50);
    display.print("Ornek: "); display.print(renkOrnekSayac); display.print("/10");
  }
  else {
    display.println("RENK OKUNDU");
    display.println();
    display.print("R:"); display.print(r);
    display.print(" G:"); display.print(g);
    display.print(" B:"); display.println(b);
    display.println();
    display.println(renkAdi());
    display.setCursor(0, 56);
    display.print("Web'den TAMAM'a bas");
  }
  display.display();
}

void ekranGosterIsik() {
  ekranTemizle();
  display.println("ISIK");
  display.setTextSize(2);
  display.setCursor(10, 22);
  display.print((int)lux); display.print(" lx");
  display.setTextSize(1);
  display.setCursor(0, 55);
  if      (lux < 10)  display.print("KARANLIK");
  else if (lux < 100) display.print("LOSUM");
  else if (lux < 500) display.print("IC MEKAN");
  else                display.print("PARLAK");
  display.display();
}

void ekranGosterHava() {
  ekranTemizle();
  display.println("HAVA");
  display.setTextSize(2);
  display.setCursor(0, 18);
  display.print(sicaklik, 1); display.println(" C");
  display.print(nem, 0);      display.println(" %");
  display.setTextSize(1);
  display.setCursor(0, 55);
  display.print("Min:"); display.print(sicaklikMin,0);
  display.print(" Max:"); display.print(sicaklikMax,0);
  display.display();
}

/* ================= WEB HTML ================= */
const char HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="tr">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Akilli Metre</title>
<style>
  :root {
    --bg: #0f0f13;
    --surface: #1a1a24;
    --border: #2a2a3a;
    --accent: #6c63ff;
    --accent2: #00d4aa;
    --text: #e8e8f0;
    --muted: #6b6b8a;
    --danger: #ff4757;
    --success: #2ed573;
    --warn: #ffa502;
  }
  * { margin:0; padding:0; box-sizing:border-box; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Segoe UI', system-ui, sans-serif;
    min-height: 100vh;
    padding: 20px;
  }
  .wrap { max-width: 420px; margin: 0 auto; }

  /* HEADER */
  header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 16px 0 24px;
    border-bottom: 1px solid var(--border);
    margin-bottom: 24px;
  }
  header h1 { font-size: 18px; font-weight: 600; letter-spacing: 0.5px; }
  .dot {
    width: 8px; height: 8px;
    border-radius: 50%;
    background: var(--success);
    box-shadow: 0 0 8px var(--success);
    animation: pulse 2s infinite;
  }
  @keyframes pulse { 0%,100%{opacity:1} 50%{opacity:0.4} }

  /* VALUE CARD */
  .vcard {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 16px;
    padding: 24px;
    margin-bottom: 16px;
  }
  .vcard-label {
    font-size: 11px;
    text-transform: uppercase;
    letter-spacing: 1.5px;
    color: var(--muted);
    margin-bottom: 12px;
  }
  .vcard-value {
    font-size: 36px;
    font-weight: 700;
    color: var(--text);
    line-height: 1.1;
    min-height: 44px;
  }
  .vcard-sub {
    font-size: 12px;
    color: var(--muted);
    margin-top: 10px;
  }

  /* PROGRESS BAR */
  .prog-wrap {
    background: var(--border);
    border-radius: 99px;
    height: 4px;
    margin-top: 14px;
    overflow: hidden;
    display: none;
  }
  .prog-fill {
    height: 100%;
    background: linear-gradient(90deg, var(--accent), var(--accent2));
    border-radius: 99px;
    transition: width 0.4s ease;
  }

  /* TAMAM BUTTON */
  #tamamWrap { display:none; margin-bottom:16px; }
  #tamamBtn {
    width: 100%;
    padding: 14px;
    background: var(--success);
    color: #000;
    font-size: 14px;
    font-weight: 700;
    letter-spacing: 0.5px;
    border: none;
    border-radius: 12px;
    cursor: pointer;
    transition: opacity 0.2s;
  }
  #tamamBtn:hover { opacity: 0.85; }

  /* GRID */
  .grid {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
  }
  .btn {
    background: var(--surface);
    border: 1px solid var(--border);
    color: var(--text);
    padding: 16px 12px;
    border-radius: 12px;
    font-size: 13px;
    font-weight: 600;
    cursor: pointer;
    transition: all 0.2s;
    text-align: center;
    letter-spacing: 0.3px;
  }
  .btn:hover {
    border-color: var(--accent);
    background: rgba(108,99,255,0.1);
  }
  .btn.active {
    border-color: var(--accent);
    background: rgba(108,99,255,0.15);
    color: #a89fff;
  }
  .btn-home {
    grid-column: 1 / -1;
    border-color: var(--border);
    color: var(--muted);
    font-weight: 500;
  }
  .btn-home:hover {
    border-color: var(--danger);
    background: rgba(255,71,87,0.08);
    color: var(--danger);
  }

  /* STATUS */
  .statusbar {
    font-size: 11px;
    color: var(--muted);
    text-align: center;
    margin-top: 20px;
    letter-spacing: 0.3px;
  }
</style>
</head>
<body>
<div class="wrap">

  <header>
    <h1>AKILLI METRE</h1>
    <div class="dot" id="dot"></div>
  </header>

  <div class="vcard">
    <div class="vcard-label" id="modLabel">HAZIR</div>
    <div class="vcard-value" id="val">---</div>
    <div class="prog-wrap" id="progWrap">
      <div class="prog-fill" id="progFill" style="width:0%"></div>
    </div>
    <div class="vcard-sub" id="statusSub">Baglaniyor...</div>
  </div>

  <div id="tamamWrap">
    <button id="tamamBtn" onclick="renkTamam()">TAMAM — Yeni Olcum</button>
  </div>

  <div class="grid">
    <button class="btn" id="b1" onclick="m(1)">Mesafe</button>
    <button class="btn" id="b2" onclick="m(2)">Terazi</button>
    <button class="btn" id="b3" onclick="m(3)">Renk</button>
    <button class="btn" id="b4" onclick="m(4)">Isik</button>
    <button class="btn" id="b5" onclick="m(5)">Hava</button>
    <button class="btn btn-home" onclick="m(0)">Ana Menu</button>
  </div>

  <div class="statusbar" id="statusBar">—</div>
</div>

<script>
let curMode = 0;
const btns = [1,2,3,4,5];

function m(x) {
  fetch('/mod?m=' + x).then(() => {
    curMode = x;
    btns.forEach(i => document.getElementById('b'+i).classList.toggle('active', i===x));
  }).catch(() => setErr());
}

function renkTamam() {
  fetch('/renk_tamam').catch(() => setErr());
}

function setErr() {
  document.getElementById('dot').style.background = 'var(--danger)';
  document.getElementById('dot').style.boxShadow = '0 0 8px var(--danger)';
  document.getElementById('statusBar').innerText = 'Baglanti hatasi';
}

function updateDisplay() {
  fetch('/data')
    .then(r => r.json())
    .then(j => {
      // Dot yeşil
      const dot = document.getElementById('dot');
      dot.style.background = 'var(--success)';
      dot.style.boxShadow = '0 0 8px var(--success)';

      document.getElementById('modLabel').innerText = j.mod.toUpperCase();
      document.getElementById('val').innerText = j.val;

      // Progress bar & TAMAM butonu
      const prog = document.getElementById('progWrap');
      const fill = document.getElementById('progFill');
      const tamam = document.getElementById('tamamWrap');

      if (j.renkDurum === 1) {
        // Okuyor - ilerleme çubuğu
        const pct = Math.min(100, Math.round(j.prog || 0));
        prog.style.display = 'block';
        fill.style.width = pct + '%';
        tamam.style.display = 'none';
      } else if (j.renkDurum === 2) {
        // Hazir - TAMAM butonu
        prog.style.display = 'none';
        tamam.style.display = 'block';
      } else {
        prog.style.display = 'none';
        tamam.style.display = 'none';
      }

      const now = new Date();
      document.getElementById('statusBar').innerText =
        now.getHours().toString().padStart(2,'0') + ':' +
        now.getMinutes().toString().padStart(2,'0') + ':' +
        now.getSeconds().toString().padStart(2,'0');
    })
    .catch(() => setErr());
}

setInterval(updateDisplay, 800);
updateDisplay();
</script>
</body>
</html>
)rawliteral";

/* ================= WEB SUNUCU ================= */
void handleRoot() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/html", HTML);
}

void handleMod() {
  aktifMod = (Mod)server.arg("m").toInt();
  if (aktifMod == MOD_RENK) {
    renkDurum = RENK_BEKLEME;
    digitalWrite(PIN_RENK_LED, LOW);
    tcs.setInterrupt(true);
  }
  bip();
  server.send(200, "text/plain", "OK");
}

void handleData() {
  String mod = "Menu", val = "---";
  String extra = "";

  switch (aktifMod) {
    case MOD_MENU:
      mod = "Hazir"; val = "Mod seciniz"; break;
    case MOD_MESAFE:
      mod = "Mesafe";
      val = String(mesafe, 1) + " cm"; break;
    case MOD_TERAZI:
      mod = "Su Terazisi";
      val = "X: " + String(egimX, 1) + "  Y: " + String(egimY, 1); break;
    case MOD_RENK:
      mod = "Renk Sensoru";
      if (renkDurum == RENK_BEKLEME) {
        val = "Renk tusuna basin";
      } else if (renkDurum == RENK_OKUYOR) {
        int prog = constrain((int)((millis() - renkOkumaBaslangic) * 100 / RENK_OKUMA_SURESI), 0, 100);
        val = "Okuyor... %" + String(prog);
        extra += ",\"prog\":" + String(prog);
      } else {
        val = "R:" + String(r) + "  G:" + String(g) + "  B:" + String(b);
      }
      extra += ",\"renkDurum\":" + String(renkDurum);
      break;
    case MOD_ISIK:
      mod = "Isik"; val = String((int)lux) + " lux"; break;
    case MOD_HAVA:
      mod = "Hava";
      val = String(sicaklik, 1) + " C   " + String(nem, 0) + " %"; break;
  }

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json",
    "{\"mod\":\"" + mod + "\",\"val\":\"" + val + "\"" + extra + "}");
}

/* ================= SETUP ================= */
void setup() {
#if DEBUG
  Serial.begin(115200);
  delay(100);
  debugLog("Baslatiliyor...");
#endif

  pinMode(PIN_BUZZ,     OUTPUT);
  pinMode(PIN_LAZER,    OUTPUT);
  pinMode(PIN_RENK_LED, OUTPUT);
  digitalWrite(PIN_LAZER,    LOW);
  digitalWrite(PIN_RENK_LED, LOW);
  analogReadResolution(12);

  Wire.begin(PIN_SDA_1, PIN_SCL_1);
  Wire1.begin(PIN_SDA_2, PIN_SCL_2);

  if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    ekranTemizle();
    display.println("Baslatiliyor...");
    display.display();
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  WiFi.softAP(ssid, password, 1);

  if (lightMeter.begin())  isikSensorOK = true;
  if (sht31.begin(0x44))   havaSensorOK = true;
  mpuUyandir();

  mesafeSensor.setTimeout(500);
  if (mesafeSensor.init()) {
    mesafeSensorOK = true;
    mesafeSensor.setDistanceMode(VL53L1X::Long);
    mesafeSensor.setMeasurementTimingBudget(100000);
    mesafeSensor.startContinuous(100);
  }

  if (tcs.begin(0x29, &Wire1)) {
    renkSensorOK = true;
    tcs.setGain(TCS34725_GAIN_16X);
    tcs.setIntegrationTime(TCS34725_INTEGRATIONTIME_154MS);
    tcs.setInterrupt(true);
  }

  for (int i = 0; i < 256; i++) {
    float x = pow(i / 255.0, 2.5) * 255.0;
    gammaTable[i] = (byte)x;
  }

  server.on("/",          handleRoot);
  server.on("/mod",       handleMod);
  server.on("/data",      handleData);
  server.on("/renk_tamam", [](){
    renkDurum = RENK_BEKLEME;
    digitalWrite(PIN_RENK_LED, LOW);
    server.send(200, "text/plain", "OK");
  });
  server.begin();

  bip(80); delay(100); bip(80);
  debugLog("Hazir. IP: " + WiFi.softAPIP().toString());
}

/* ================= LOOP ================= */
void loop() {
  server.handleClient();

  digitalWrite(PIN_LAZER, aktifMod == MOD_MESAFE ? HIGH : LOW);

  unsigned long now = millis();

  if (aktifMod == MOD_TERAZI) {
    if (now - oncekiOkuma > 50) {
      oncekiOkuma = now;
      okuEgim();
    }
  }
  else if (now - oncekiOkuma > SENSOR_READ_INTERVAL) {
    oncekiOkuma = now;
    switch (aktifMod) {
      case MOD_MESAFE: okuMesafe(); break;
      case MOD_RENK:   okuRenk();   break;
      case MOD_ISIK:   okuIsik();   break;
      case MOD_HAVA:   okuHava();   break;
      default: break;
    }
  }

  switch (aktifMod) {
    case MOD_MENU:   ekranGosterMenu();   break;
    case MOD_MESAFE: ekranGosterMesafe(); break;
    case MOD_TERAZI: ekranGosterTerazi(); break;
    case MOD_RENK:   ekranGosterRenk();  break;
    case MOD_ISIK:   ekranGosterIsik();  break;
    case MOD_HAVA:   ekranGosterHava();  break;
  }
}