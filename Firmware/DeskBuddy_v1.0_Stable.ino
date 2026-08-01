/*
 ╔═══════════════════════════════════════════════════════════════╗
 ║              🌸  DESK BUDDY  —  ESP8266 Firmware v2.5        ║
 ║  128×64 SH1106 OLED · WiFi · OWM · MQTT · Notifications · Web ║
 ╚═══════════════════════════════════════════════════════════════╝

  FEATURES
  ────────
  1. Clock Screen (12h AM/PM with date & minute progress bar)
  2. Weather Screen (OpenWeatherMap with custom 7-seg & pixel icons)
  3. Mochi Face Screen (Physics eyes, saccades, blinking, 15 moods)
  4. Notification Screen (MQTT forwarded phone notifications)
  5. Web Config Portal (Hold BTN2 during boot to edit settings via browser)
  6. LED Indicators:
     - 🟢 Green LED (D4): WiFi + MQTT Status
     - 🟡 Yellow LED (D3): Unread Notifications
     - 🔴 Red LED (D6): Silent / DND Mode
  7. Piezo Buzzer (Intro jingle, screen change, emotion tones, alert sound)

  HARDWARE PINS
  ─────────────
  SDA: D2  |  SCL: D1  |  BTN1: D5  |  BTN2: D7
  STATUS_LED (Red): D6  |  NOTIFY_LED (Yellow): D3  |  EMOTION_LED (Green): D4
  BUZZER: D8
*/

// ═══════════════════════════════════════════════════════════════
//  ① INCLUDES
// ═══════════════════════════════════════════════════════════════
#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>
#include <time.h>
#include <math.h>
#include <WiFiManager.h>
#include <pgmspace.h>
//==================================================================
//HEADER
//==================================================================
const char WEB_HEADER[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name='viewport' content='width=device-width, initial-scale=1'>
<title>Desk Buddy Config</title>
<style>
body{font-family:sans-serif;background:#121212;color:#eee;padding:20px;max-width:500px;margin:0 auto;}
h2{color:#4CAF50;text-align:center;}
label{display:block;margin-top:12px;font-size:14px;color:#aaa;}
input[type=text],input[type=password],input[type=number]{width:100%;padding:10px;margin-top:4px;box-sizing:border-box;border-radius:4px;border:1px solid #333;background:#222;color:#fff;}
input[type=checkbox]{margin-top:12px;}
input[type=submit],.btn{margin-top:16px;width:100%;padding:12px;background:#4CAF50;border:none;color:white;font-weight:bold;border-radius:4px;cursor:pointer;}
input[type=submit]:hover,.btn:hover{background:#45a049;}
.btn-danger{background:#f44336;}
.btn-danger:hover{background:#d32f2f;}
</style>
</head>
<body>
<h2>🌸 Desk Buddy Config</h2>
<form action='/save' method='POST'>
)rawliteral";
//================================================================
//FOOTER
//================================================================
const char WEB_FOOTER[] PROGMEM = R"rawliteral(
<input type='submit' value='Save & Restart'>
</form>

<form action='/resetwifi' method='POST'>
<input type='submit' class='btn btn-danger' value='Reset WiFi Network'>
</form>

</body>
</html>
)rawliteral";

// ═══════════════════════════════════════════════════════════════
//  ② HARDWARE DEFINITIONS
// ═══════════════════════════════════════════════════════════════
#define SDA_PIN D2
#define SCL_PIN D1

#define BTN1 D5
#define BTN2 D7

#define STATUS_LED  D6  // Red (Silent / DND)
#define NOTIFY_LED  D3  // Yellow (Notifications)
#define EMOTION_LED D4  // Green (WiFi / MQTT status)

#define BUZZER_PIN D8

// OLED Display Object (SH1106 I2C 128x64)
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);

// ═══════════════════════════════════════════════════════════════
//  ③ CONFIGURATION STRUCTURE
// ═══════════════════════════════════════════════════════════════
struct Config {
  // ---------- OpenWeather ----------
  char owm_key[40];
  char owm_city[32];
  char owm_units[10];

  // ---------- MQTT ----------
  char mqtt_broker[80];
  int  mqtt_port;
  char mqtt_user[32];
  char mqtt_pass[32];
  char mqtt_topic[40];

  // ---------- HTTP Notification ----------
  bool http_enabled;
  int  http_port;
  char http_endpoint[20];

  // ---------- Device ----------
  bool silent;
};

Config config = {
  // ---------- OpenWeather ----------
  "5764755d3ff40eb01e9f156f02d1dc09",
  "Bengaluru",
  "metric",

  // ---------- MQTT ----------
  "adb3db2394804d66ac242acd9d202d3b.s1.eu.hivemq.cloud",
  8883,
  "",
  "",
  "deskbuddy/notify",

  // ---------- HTTP ----------
  true,          // Enable HTTP notifications
  80,            // HTTP Server Port
  "/notify",     // Endpoint

  // ---------- Device ----------
  false          // Silent Mode
};

const char* POSIX_TZ = "IST-5:30";
const char* NTP_SRV  = "pool.ntp.org";
// ═══════════════════════════════════════════════════════════════
//  ④ NETWORK & SERVICES
// ═══════════════════════════════════════════════════════════════
ESP8266WebServer* server;
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

bool isConfigPortalMode = false;
unsigned long mqttConnectAttemptTime = 0;
const uint16_t MQTT_CONNECT_TIMEOUT = 5000;
//================================================================
// FUNCTION PROTOTYPES
//================================================================

void handleWebRoot();
void handleWebSave();
void handleResetWiFi();
void handleNotify();
void startConfigPortal();
void pushNotification(const char* app, const char* title, const char* body);

// ═══════════════════════════════════════════════════════════════
//  ⑤ TUNABLES & STATES
// ═══════════════════════════════════════════════════════════════
#define DEBOUNCE_MS      50
#define WEATHER_TTL  600000UL // 10 minutes
#define FRAME_MS         50
#define BRIGHTNESS      220
#define CYCLE_HOLD_MS  2000UL // hold 2s to toggle silent mode

enum ScreenMode : uint8_t { MODE_CLOCK, MODE_WEATHER, MODE_MOCHI, MODE_NOTIFY, MODE_COUNT };
ScreenMode currentMode = MODE_CLOCK;

// Buttons
bool lastBtn1 = HIGH;
bool lastBtn2 = HIGH;
unsigned long btn2PressStart = 0;

// LED & Status Timers
unsigned long previousBlink = 0;
bool greenLedState = false;

// Weather State
struct WX {
  char  city[24];
  char  desc[32];
  char  icon[4];
  int   tempC, feelsC, humid;
  float windMs;
  bool  valid;
} wx = {};
uint32_t wxLastFetch   = 0;
uint32_t minuteStartMs = 0;

// Moods
#define MOOD_DEFAULT     0
#define MOOD_HAPPY       1
#define MOOD_LOVE        2
#define MOOD_STAR        3
#define MOOD_WINK        4
#define MOOD_DIZZY       5
#define MOOD_ANGRY       6
#define MOOD_SAD         7
#define MOOD_SLEEPY      8
#define MOOD_SURPRISED   9
#define MOOD_SMUG       10
#define MOOD_NERVOUS    11
#define MOOD_CAT        12
#define MOOD_SLEEPING   13
#define MOOD_CUTE       14
#define MOOD_COUNT      15

int currentMood = MOOD_DEFAULT;

// Notifications Buffer
#define MAX_NOTIFS 8
struct Notification {
  char app[16];
  char title[24];
  char body[64];
  uint32_t timestamp;
};
Notification notifBuf[MAX_NOTIFS];
int notifHead = 0;
int notifCount = 0;
int unreadNotifs = 0;
int selectedNotifIdx = 0;

// Popup Overlay
bool popupActive = false;
uint32_t popupStartMs = 0;
char popupTitle[24];
char popupBody[64];

// Physics Eyes
struct Eye {
  float x, y, w, h;
  float targetX, targetY, targetW, targetH;
  float pupilX, pupilY, targetPupilX, targetPupilY;
  bool  blinking;
  unsigned long lastBlink, nextBlinkTime;
  void init(float _x, float _y, float _w, float _h){
    x=_x; y=_y; w=_w; h=_h;
    targetX=_x; targetY=_y; targetW=_w; targetH=_h;
    pupilX=0; pupilY=0; targetPupilX=0; targetPupilY=0;
    blinking=false; lastBlink=0; nextBlinkTime=2000;
  }
};
Eye leftEye, rightEye;
unsigned long lastSaccade     = 0;
unsigned long saccadeInterval = 800;
float breathVal = 0;

// Sparkles
struct Sparkle { int8_t x, y; uint8_t speed; bool alive; };
Sparkle sparkles[6];

// ═══════════════════════════════════════════════════════════════
//  ⑥ BUZZER SYSTEM
// ═══════════════════════════════════════════════════════════════
void pTone(uint16_t freq, uint16_t durMs, uint16_t gapMs){
  if(freq==0) noTone(BUZZER_PIN);
  else        tone(BUZZER_PIN, freq, durMs);
  delay(durMs);
  noTone(BUZZER_PIN);
  if(gapMs) delay(gapMs);
}

void buzzScreenChange(){
  pTone(784,  30, 10);
  pTone(1047, 50,  0);
}

void buzzIntro(){
  pTone(659,  80, 20); pTone(784,  80, 20); pTone(880, 160, 40);
  pTone(659,  80, 20); pTone(784,  80, 20); pTone(988, 160, 40);
  pTone(659,  80, 20); pTone(784,  80, 20); pTone(880,  80, 20);
  pTone(784,  80, 20); pTone(659, 120, 20); pTone(587, 120, 20);
  pTone(523, 240,  0); delay(80);
  pTone(523,  60, 15); pTone(587,  60, 15); pTone(659,  60, 15);
  pTone(784, 100, 15); pTone(880, 100, 15); pTone(1047,300,  0);
}

void buzzNotification(){
  if(config.silent) return;
  pTone(880, 60, 15);
  pTone(1175, 100, 0);
}

void buzzEmotion(int mood){
  if(config.silent) return;
  switch(mood){
    case MOOD_DEFAULT:   pTone(523,60,15); pTone(659,60,15); pTone(784,120,0); break;
    case MOOD_HAPPY:     pTone(523,60,10); pTone(659,60,10); pTone(784,60,10); pTone(1047,120,15); pTone(784,50,10); pTone(1047,150,0); break;
    case MOOD_LOVE:      pTone(659,120,20); pTone(784,120,20); pTone(880,200,40); pTone(784,80,15); pTone(659,80,15); pTone(587,200,0); break;
    case MOOD_STAR:      pTone(1047,40,8); pTone(988,40,8); pTone(880,40,8); pTone(784,40,8); pTone(740,40,8); pTone(784,40,8); pTone(880,40,8); pTone(1047,160,0); break;
    case MOOD_WINK:      pTone(784,80,15); pTone(988,40,10); pTone(784,40,10); pTone(988,120,0); break;
    case MOOD_DIZZY:     pTone(523,50,8); pTone(554,50,8); pTone(587,50,8); pTone(622,50,8); pTone(587,50,8); pTone(554,50,8); pTone(523,100,0); break;
    case MOOD_ANGRY:     pTone(247,80,15); pTone(247,80,15); pTone(247,80,15); pTone(247,200,0); break;
    case MOOD_SAD:       pTone(440,180,30); pTone(392,180,30); pTone(349,180,30); pTone(330,180,30); pTone(294,300,0); break;
    case MOOD_SLEEPY:    pTone(784,200,40); pTone(659,200,40); pTone(523,200,40); pTone(587,150,30); pTone(523,400,0); break;
    case MOOD_SURPRISED: pTone(523,30,5); pTone(659,30,5); pTone(784,30,5); pTone(1047,30,5); pTone(1319,200,0); break;
    case MOOD_SMUG:      pTone(988,120,20); pTone(880,120,20); pTone(784,120,20); pTone(659,300,0); break;
    case MOOD_NERVOUS:   pTone(659,30,10); pTone(698,25,8); pTone(659,30,10); pTone(622,25,8); pTone(659,30,10); pTone(698,25,8); pTone(784,120,0); break;
    case MOOD_CAT:       pTone(392,80,20); pTone(440,80,20); pTone(392,80,20); pTone(440,160,15); pTone(392,60,10); pTone(440,200,0); break;
    case MOOD_SLEEPING:  pTone(523,300,80); pTone(494,300,80); pTone(440,400,0); break;
    case MOOD_CUTE:      pTone(1047,60,10); pTone(1175,60,10); pTone(1319,60,10); pTone(1047,40,8); pTone(1319,40,8); pTone(1568,40,8); pTone(1047,30,6); pTone(1319,30,6); pTone(1568,150,10); pTone(1319,60,8); pTone(1047,200,0); break;
  }
}

// ═══════════════════════════════════════════════════════════════
//  ⑦ CONFIG STORAGE (LittleFS)
// ═══════════════════════════════════════════════════════════════
void loadConfig() {
  if (!LittleFS.begin()) {
    Serial.println(F("LittleFS Mount Failed. Formatting..."));
    LittleFS.format();
    LittleFS.begin();
    return;
  }

  if (!LittleFS.exists("/config.json")) {
    Serial.println(F("No config file found. Using defaults."));
    return;
  }

  File file = LittleFS.open("/config.json", "r");
  if (!file) return;

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.println(F("Failed to parse config file"));
    return;
  }

  // ---------- OpenWeather ----------
  strlcpy(config.owm_key,     doc["owm_key"]     | config.owm_key,     sizeof(config.owm_key));
  strlcpy(config.owm_city,    doc["owm_city"]    | config.owm_city,    sizeof(config.owm_city));
  strlcpy(config.owm_units,   doc["owm_units"]   | config.owm_units,   sizeof(config.owm_units));

  // ---------- MQTT ----------
  strlcpy(config.mqtt_broker, doc["mqtt_broker"] | config.mqtt_broker, sizeof(config.mqtt_broker));
  config.mqtt_port = doc["mqtt_port"] | config.mqtt_port;
  strlcpy(config.mqtt_user,   doc["mqtt_user"]   | config.mqtt_user,   sizeof(config.mqtt_user));
  strlcpy(config.mqtt_pass,   doc["mqtt_pass"]   | config.mqtt_pass,   sizeof(config.mqtt_pass));
  strlcpy(config.mqtt_topic,  doc["mqtt_topic"]  | config.mqtt_topic,  sizeof(config.mqtt_topic));

  // ---------- HTTP ----------
  config.http_enabled = doc["http_enabled"] | config.http_enabled;
  config.http_port    = doc["http_port"] | config.http_port;
  strlcpy(config.http_endpoint,
          doc["http_endpoint"] | config.http_endpoint,
          sizeof(config.http_endpoint));

  // ---------- Device ----------
  config.silent = doc["silent"] | config.silent;

  Serial.println(F("Config loaded successfully."));
}
//================================================================
//  SAVE CONFIG
//================================================================
void saveConfig() {
  StaticJsonDocument<512> doc;

  // ---------- OpenWeather ----------
  doc["owm_key"] = config.owm_key;
  doc["owm_city"] = config.owm_city;
  doc["owm_units"] = config.owm_units;

  // ---------- MQTT ----------
  doc["mqtt_broker"] = config.mqtt_broker;
  doc["mqtt_port"] = config.mqtt_port;
  doc["mqtt_user"] = config.mqtt_user;
  doc["mqtt_pass"] = config.mqtt_pass;
  doc["mqtt_topic"] = config.mqtt_topic;

  // ---------- HTTP ----------
  doc["http_enabled"] = config.http_enabled;
  doc["http_port"] = config.http_port;
  doc["http_endpoint"] = config.http_endpoint;

  // ---------- Device ----------
  doc["silent"] = config.silent;

  File file = LittleFS.open("/config.json", "w");
  if (!file) {
    Serial.println(F("Failed to open config file for writing"));
    return;
  }

  serializeJson(doc, file);
  file.close();

  Serial.println(F("Config saved successfully."));
}
// ═══════════════════════════════════════════════════════════════
//  ⑧ WEB CONFIG PORTAL
// ═══════════════════════════════════════════════════════════════
void handleWebRoot() {

  String html;
  html.reserve(2500);

  // Load the static header from PROGMEM
  html += FPSTR(WEB_HEADER);

  // Dynamic fields
  html += F("<label>Weather City:</label><input type='text' name='owm_city' value='");
  html += config.owm_city;
  html += F("'>");

  html += F("<label>OpenWeatherMap API Key:</label><input type='text' name='owm_key' value='");
  html += config.owm_key;
  html += F("'>");

  html += F("<label>MQTT Broker:</label><input type='text' name='mqtt_broker' value='");
  html += config.mqtt_broker;
  html += F("'>");

  html += F("<label>MQTT Port:</label><input type='number' name='mqtt_port' value='");
  html += config.mqtt_port;
  html += F("'>");

  html += F("<label>MQTT Username:</label><input type='text' name='mqtt_user' value='");
  html += config.mqtt_user;
  html += F("'>");

  html += F("<label>MQTT Password:</label><input type='password' name='mqtt_pass' value='");
  html += config.mqtt_pass;
  html += F("'>");

  html += F("<label>MQTT Topic:</label><input type='text' name='mqtt_topic' value='");
  html += config.mqtt_topic;
  html += F("'>");

  html += F("<label>HTTP Notify Port:</label><input type='number' name='http_port' value='");
  html += config.http_port;
  html += F("'>");

  html += F("<label>HTTP Notify Endpoint:</label><input type='text' name='http_endpoint' value='");
  html += config.http_endpoint;
  html += F("'>");

  html += F("<label><input type='checkbox' name='http_enabled' ");

  if (config.http_enabled)
    html += F("checked");

  html += F("> Enable HTTP Notifications</label><br>");

  html += F("<label><input type='checkbox' name='silent' ");

  if (config.silent)
    html += F("checked");

  html += F("> Silent / DND Mode</label><br>");

  // Load the static footer from PROGMEM
  html += FPSTR(WEB_FOOTER);

  server->send(200, "text/html", html);
}
//================================================================
//
//================================================================
void handleResetWiFi() {
  WiFiManager wm;
  wm.resetSettings();

  String html;
  html.reserve(256);

  html += F("<html><body style='background:#121212;color:#eee;font-family:sans-serif;text-align:center;padding-top:50px;'>");
  html += F("<h2>🔄 WiFi Reset Done!</h2><p>Restarting into WiFi Setup mode...</p></body></html>");

  server->send(200, "text/html", html);

  delay(2000);

  if (server) {
    server->stop();
    delete server;
    server = nullptr;
  }

  ESP.restart();
}

//================================================================
//  HTTP NOTIFICATION ENDPOINT  (e.g. /notify?title=Mother&msg=Hello)
//================================================================
void handleNotify() {
  if (!config.http_enabled) {
    server->send(403, F("text/plain"), F("HTTP notifications disabled"));
    return;
  }

  char title[24];
  char body[64];
  strlcpy(title, server->hasArg("title") ? server->arg("title").c_str() : "Notification", sizeof(title));
  strlcpy(body,  server->hasArg("msg")   ? server->arg("msg").c_str()   : "",             sizeof(body));

  pushNotification("HTTP", title, body);

  server->send(200, F("text/plain"), F("OK"));
}

void handleWebSave() {

  if (server->hasArg("owm_city"))
    strlcpy(config.owm_city, server->arg("owm_city").c_str(), sizeof(config.owm_city));

  if (server->hasArg("owm_key"))
    strlcpy(config.owm_key, server->arg("owm_key").c_str(), sizeof(config.owm_key));

  if (server->hasArg("mqtt_broker"))
    strlcpy(config.mqtt_broker, server->arg("mqtt_broker").c_str(), sizeof(config.mqtt_broker));

  if (server->hasArg("mqtt_port"))
    config.mqtt_port = server->arg("mqtt_port").toInt();

  if (server->hasArg("mqtt_user"))
    strlcpy(config.mqtt_user, server->arg("mqtt_user").c_str(), sizeof(config.mqtt_user));

  if (server->hasArg("mqtt_pass"))
    strlcpy(config.mqtt_pass, server->arg("mqtt_pass").c_str(), sizeof(config.mqtt_pass));

  if (server->hasArg("mqtt_topic"))
    strlcpy(config.mqtt_topic, server->arg("mqtt_topic").c_str(), sizeof(config.mqtt_topic));

  if (server->hasArg("http_port"))
    config.http_port = server->arg("http_port").toInt();

  if (server->hasArg("http_endpoint")) {
    String ep = server->arg("http_endpoint");
    if (ep.length() == 0 || ep[0] != '/') ep = "/" + ep;
    strlcpy(config.http_endpoint, ep.c_str(), sizeof(config.http_endpoint));
  }

  config.http_enabled = server->hasArg("http_enabled");
  config.silent = server->hasArg("silent");

  saveConfig();

  String html;
  html.reserve(256);

  html += F("<html><body style='background:#121212;color:#eee;font-family:sans-serif;text-align:center;padding-top:50px;'>");
  html += F("<h2>✅ Settings Saved!</h2><p>Desk Buddy is restarting...</p></body></html>");

  server->send(200, "text/html", html);

  delay(2000);

  if (server) {
    server->stop();
    delete server;
    server = nullptr;
  }

  ESP.restart();
}

void startConfigPortal() {

  isConfigPortalMode = true;

  WiFi.mode(WIFI_AP_STA);

  WiFi.softAP("DeskBuddy_Config");

  IPAddress apIP = WiFi.softAPIP();

  Serial.print(F("Config Portal IP: "));
  Serial.println(apIP);

  server = new ESP8266WebServer(80);

  server->on("/", handleWebRoot);
  server->on("/save", HTTP_POST, handleWebSave);
  server->on("/resetwifi", HTTP_POST, handleResetWiFi);
  server->on(config.http_endpoint, HTTP_GET, handleNotify);

  server->begin();

  display.clearBuffer();

  display.setFont(u8g2_font_7x14B_tf);
  display.drawStr(10, 18, "CONFIG MODE");

  display.setFont(u8g2_font_5x8_tf);
  display.drawStr(0, 34, "Connect WiFi:");
  display.drawStr(0, 44, "DeskBuddy_Config");
  display.drawStr(0, 58, "192.168.4.1");

  display.sendBuffer();
}
// ═══════════════════════════════════════════════════════════════
//  ⑨ MQTT & NOTIFICATIONS
// ═══════════════════════════════════════════════════════════════
// Shared push path for both MQTT and HTTP notification sources.
void pushNotification(const char* app, const char* title, const char* body) {
  Notification n;
  n.timestamp = millis();
  strlcpy(n.app,   app,   sizeof(n.app));
  strlcpy(n.title, title, sizeof(n.title));
  strlcpy(n.body,  body,  sizeof(n.body));

  // Push to circular buffer
  notifBuf[notifHead] = n;
  notifHead = (notifHead + 1) % MAX_NOTIFS;
  if (notifCount < MAX_NOTIFS) notifCount++;

  unreadNotifs++;

  // Setup Popup
  strlcpy(popupTitle, n.title, sizeof(popupTitle));
  strlcpy(popupBody,  n.body,  sizeof(popupBody));
  popupActive = true;
  popupStartMs = millis();

  buzzNotification();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char msg[256];
  if (length >= sizeof(msg)) length = sizeof(msg) - 1;
  memcpy(msg, payload, length);
  msg[length] = '\0';

  Serial.print(F("MQTT Received: "));
  Serial.println(msg);

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, msg);

  char app[16], title[24], body[64];
  if (!err) {
    strlcpy(app,   doc["app"]   | "Phone",        sizeof(app));
    strlcpy(title, doc["title"] | "Notification", sizeof(title));
    strlcpy(body,  doc["body"]  | msg,            sizeof(body));
  } else {
    strlcpy(app,   "Message",      sizeof(app));
    strlcpy(title, "Notification", sizeof(title));
    strlcpy(body,  msg,            sizeof(body));
  }

  pushNotification(app, title, body);
}

void mqttReconnect() {
  static unsigned long lastAttempt = 0;

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (mqttClient.connected()) {
    return;
  }

  // Prevent rapid reconnect attempts
  if (millis() - lastAttempt < 5000) {
    return;
  }
  lastAttempt = millis();

  // Check credentials
  if (strlen(config.mqtt_user) == 0 || strlen(config.mqtt_pass) == 0) {
    return;
  }

  Serial.println();
  Serial.println(F("========== MQTT =========="));
  Serial.print(F("Broker   : "));
  Serial.println(config.mqtt_broker);
  Serial.print(F("Port     : "));
  Serial.println(config.mqtt_port);
  Serial.print(F("User     : "));
  Serial.println(config.mqtt_user);
  Serial.print(F("Topic    : "));
  Serial.println(config.mqtt_topic);

  // Configure TLS
  espClient.setInsecure();
  espClient.setTimeout(10000);

  String clientId = "DeskBuddy-" + String(ESP.getChipId(), HEX);

  Serial.print(F("Connecting to MQTT... "));

  if (mqttClient.connect(
        clientId.c_str(),
        config.mqtt_user,
        config.mqtt_pass)) {

    Serial.println(F("MQTT CONNECTION SUCCESS"));

    if (mqttClient.subscribe(config.mqtt_topic)) {
      Serial.print(F("Subscribed to: "));
      Serial.println(config.mqtt_topic);
    } else {
      Serial.println(F("Subscription FAILED"));
    }

  } else {
    Serial.print(F("FAILED"));
    Serial.print(F("  rc="));
    Serial.println(mqttClient.state());

    switch (mqttClient.state()) {
      case MQTT_CONNECTION_TIMEOUT:
        Serial.println(F("Reason: Connection timeout"));
        break;
      case MQTT_CONNECTION_LOST:
        Serial.println(F("Reason: Connection lost"));
        break;
      case MQTT_CONNECT_FAILED:
        Serial.println(F("Reason: Network connect failed"));
        break;
      case MQTT_DISCONNECTED:
        Serial.println(F("Reason: Disconnected"));
        break;
      case MQTT_CONNECT_BAD_PROTOCOL:
        Serial.println(F("Reason: Bad protocol"));
        break;
      case MQTT_CONNECT_BAD_CLIENT_ID:
        Serial.println(F("Reason: Bad client ID"));
        break;
      case MQTT_CONNECT_UNAVAILABLE:
        Serial.println(F("Reason: Server unavailable"));
        break;
      case MQTT_CONNECT_BAD_CREDENTIALS:
        Serial.println(F("Reason: Bad username/password"));
        break;
      case MQTT_CONNECT_UNAUTHORIZED:
        Serial.println(F("Reason: Unauthorized"));
        break;
      default:
        Serial.println(F("Reason: Unknown"));
        break;
    }

    espClient.stop();
  }

  Serial.println("==========================");
}

// ═══════════════════════════════════════════════════════════════
//  ⑩ DRAWING HELPERS & EYE PHYSICS
// ═══════════════════════════════════════════════════════════════
static inline float spf(float cur, float tgt, float k){
  return cur + (tgt - cur) * k;
}

void roundedBox(int ex, int ey, int ew, int eh, uint8_t v=1, int r=8){
  r = min(r, min(ew/4, eh/4));
  display.setDrawColor(v);
  display.drawBox(ex+r, ey, ew-2*r, eh);
  display.drawBox(ex, ey+r, ew, eh-2*r);
  display.drawDisc(ex+r,       ey+r,       r, U8G2_DRAW_ALL);
  display.drawDisc(ex+ew-1-r,  ey+r,       r, U8G2_DRAW_ALL);
  display.drawDisc(ex+r,       ey+eh-1-r,  r, U8G2_DRAW_ALL);
  display.drawDisc(ex+ew-1-r,  ey+eh-1-r,  r, U8G2_DRAW_ALL);
  display.setDrawColor(1);
}

void arcBottom(int cx, int cy, int rx, int ry, int thick=5){
  for(int t=0; t<thick; t++){
    int rx2=max(1,rx-t), ry2=max(1,ry-t);
    int px0=-1, py0=-1;
    for(int ad=0; ad<=180; ad+=2){
      float a = ad * 0.01745f;
      int x = cx+(int)(rx2*cosf(a)), y = cy+(int)(ry2*sinf(a));
      if(px0>=0) display.drawLine(px0,py0,x,y);
      px0=x; py0=y;
    }
  }
}

void sparkle(int cx, int cy, int s){
  for(int i=-s; i<=s; i++){
    display.drawPixel(cx+i, cy);
    display.drawPixel(cx, cy+i);
  }
  for(int d=1; d<s; d++){
    display.drawPixel(cx+d, cy-d); display.drawPixel(cx-d, cy-d);
    display.drawPixel(cx+d, cy+d); display.drawPixel(cx-d, cy+d);
  }
}

void pixHeart(int cx, int cy){
  const int8_t pts[][2]={{0,-1},{1,-1},{-1,-1},{2,0},{-2,0},
                         {2,1},{-2,1},{1,1},{-1,1},{0,2}};
  for(auto& p : pts) display.drawPixel(cx+p[0], cy+p[1]);
}

void drawPupil(int cx, int cy, int gx, int gy, int clampR=8){
  float mag = sqrtf((float)(gx*gx + gy*gy));
  if(mag > clampR){ gx=(int)(gx*clampR/mag); gy=(int)(gy*clampR/mag); }
  int px = cx+gx, py = cy+gy;
  display.setDrawColor(0); display.drawDisc(px, py, 7, U8G2_DRAW_ALL);
  display.setDrawColor(1); display.drawDisc(px, py, 5, U8G2_DRAW_ALL);
  display.drawDisc(px-3, py-3, 2, U8G2_DRAW_ALL);
  display.setDrawColor(0); display.drawDisc(px-3, py-3, 1, U8G2_DRAW_ALL);
  display.setDrawColor(1);
}

// Eye Drawers
void eyeDefault(int ex, int ey, int ew, int eh, int gx, int gy){ roundedBox(ex, ey, ew, eh, 1, 10); drawPupil(ex+ew/2, ey+eh/2, gx, gy, 10); }
void eyeHappy(int cx, int cy, int gx, int gy){ int br = (int)breathVal; arcBottom(cx, cy+br, 26, 18, 6); display.drawDisc(cx+gx/2-8, cy-8+gy/3, 3, U8G2_DRAW_ALL); display.setDrawColor(0); display.drawDisc(cx+gx/2-8, cy-8+gy/3, 1, U8G2_DRAW_ALL); display.setDrawColor(1); }
void eyeLove(int cx, int cy, int gx, int gy){ roundedBox(cx-24, cy-22, 48, 44, 1, 8); roundedBox(cx-22, cy-20, 44, 40, 0, 7); pixHeart(cx-5+gx/2, cy-2+gy/2); pixHeart(cx+6+gx/2, cy-2+gy/2); sparkle(cx+18, cy-18, 2); }
void eyeStar(int cx, int cy, int gx, int gy){ roundedBox(cx-22, cy-20, 44, 40, 1, 7); display.setDrawColor(0); display.drawDisc(cx+gx/2, cy+gy/2, 12, U8G2_DRAW_ALL); display.setDrawColor(1); sparkle(cx+gx/2, cy+gy/2, 9); display.drawDisc(cx-8+gx/3, cy-10+gy/3, 2, U8G2_DRAW_ALL); }
void eyeWinkClosed(int cx, int cy){ int br = (int)breathVal; display.setDrawColor(1); for(int r=0; r<3; r++) display.drawHLine(cx-20, cy+br+r, 40); display.drawDisc(cx+8, cy-6+br, 2, U8G2_DRAW_ALL); }
void eyeWinkOpen(int cx, int cy, int gx, int gy){ roundedBox(cx-22, cy-20, 44, 40, 1, 8); drawPupil(cx, cy, gx, gy, 8); }
void eyeDizzy(int cx, int cy, int gx, int gy){ roundedBox(cx-22, cy-20, 44, 40, 1, 7); int ox=gx/3, oy=gy/3; display.setDrawColor(1); for(int d=0; d<3; d++){ display.drawLine(cx-8+ox+d, cy-8+oy, cx+8+ox+d, cy+8+oy); display.drawLine(cx+8+ox+d, cy-8+oy, cx-8+ox+d, cy+8+oy); } }
void eyeAngry(int cx, int cy, int gx, int gy, bool isLeft){ int br = (int)breathVal; roundedBox(cx-22, cy-18+br, 44, 38, 1, 5); display.setDrawColor(0); if(!isLeft) for(int i=0;i<16;i++) display.drawHLine(cx-22, cy-18+br+i, (int)(44*(16-i)/16.0f)); else for(int i=0;i<16;i++) display.drawHLine(cx-22+(int)(44*i/16.0f), cy-18+br+i, 44-(int)(44*i/16.0f)); display.setDrawColor(1); drawPupil(cx, cy+3+br, gx, gy, 6); }
void eyeSad(int cx, int cy, int gx, int gy, bool isLeft){ int br = (int)breathVal; roundedBox(cx-20, cy-12+br, 40, 42, 1, 7); display.setDrawColor(0); if(!isLeft) for(int i=0;i<14;i++) display.drawHLine(cx-20, cy-12+br+i, (int)(40*(14-i)/14.0f)); else for(int i=0;i<14;i++) display.drawHLine(cx-20+(int)(40*i/14.0f), cy-12+br+i, 40-(int)(40*i/14.0f)); display.setDrawColor(1); drawPupil(cx, cy+8+br, gx, gy, 7); }
void eyeSleepy(int cx, int cy, int gx){ int br = (int)breathVal; roundedBox(cx-22, cy-8+br, 44, 32, 1, 6); display.setDrawColor(0); display.drawBox(cx-22, cy-8+br, 44, 22); display.setDrawColor(1); display.drawDisc(cx+gx/3, cy+14+br, 4, U8G2_DRAW_ALL); display.setDrawColor(0); display.drawDisc(cx+gx/3, cy+14+br, 2, U8G2_DRAW_ALL); display.setDrawColor(1); }
void eyeSurprised(int cx, int cy, int gx, int gy){ display.setDrawColor(1); display.drawDisc(cx, cy, 22, U8G2_DRAW_ALL); display.setDrawColor(0); display.drawDisc(cx, cy, 20, U8G2_DRAW_ALL); display.setDrawColor(1); display.drawDisc(cx, cy, 20, U8G2_DRAW_ALL); drawPupil(cx, cy, gx, gy, 12); for(int r=0; r<2; r++) display.drawHLine(cx-12, cy-24+r, 24); }
void eyeSmug(int cx, int cy, int gx, int gy){ int br = (int)breathVal; roundedBox(cx-22, cy-14+br, 44, 32, 1, 6); display.setDrawColor(0); display.drawBox(cx-22, cy-14+br, 44, 18); display.setDrawColor(1); drawPupil(cx, cy+8+br, gx, gy, 5); }
void eyeNervous(int cx, int cy, int gx, int gy){ int shake = (int)(sinf(millis()/80.0f)*3); roundedBox(cx-20, cy-18+shake, 40, 38, 1, 6); drawPupil(cx+shake, cy+2, gx, gy, 7); for(int i=0; i<3; i++) display.drawVLine(cx-15+i*4, cy-12+shake, 8); }
void eyeCat(int cx, int cy, int gx, int gy){ display.setDrawColor(1); display.drawDisc(cx, cy, 20, U8G2_DRAW_ALL); display.setDrawColor(0); display.drawDisc(cx, cy, 18, U8G2_DRAW_ALL); display.setDrawColor(1); display.drawDisc(cx, cy, 18, U8G2_DRAW_ALL); int ox = gx/2; for(int dy=-13; dy<=13; dy++){ int hw = (int)(4*sqrtf(fmaxf(0.0f, 1.0f-(float)(dy*dy)/(13.0f*13.0f)))); if(hw>0){ display.setDrawColor(0); display.drawHLine(cx-hw+ox, cy+dy, hw*2+1); if(hw>1){ display.setDrawColor(1); display.drawHLine(cx-hw+1+ox, cy+dy, hw*2-1); display.setDrawColor(0); } } } display.setDrawColor(1); display.drawDisc(cx-6, cy-8, 2, U8G2_DRAW_ALL); }
void eyeCute(int cx, int cy, int gx, int gy){ display.setDrawColor(1); display.drawDisc(cx, cy, 20, U8G2_DRAW_ALL); display.setDrawColor(0); display.drawDisc(cx, cy, 17, U8G2_DRAW_ALL); display.setDrawColor(1); display.drawDisc(cx, cy, 17, U8G2_DRAW_ALL); float mag = sqrtf((float)(gx*gx + gy*gy)); if(mag > 9){ gx=(int)(gx*9/mag); gy=(int)(gy*9/mag); } display.setDrawColor(0); display.drawDisc(cx+gx, cy+gy, 9, U8G2_DRAW_ALL); display.setDrawColor(1); display.drawDisc(cx+gx, cy+gy, 7, U8G2_DRAW_ALL); display.drawDisc(cx+gx-4, cy+gy-4, 2, U8G2_DRAW_ALL); display.drawDisc(cx+gx+2, cy+gy-3, 1, U8G2_DRAW_ALL); sparkle(cx+16, cy-16, 2); for(int d=0; d<3; d++){ display.drawPixel(cx-18+d*4, cy+14); display.drawPixel(cx-18+d*4, cy+16); } }

void drawMouth(int mood){
  int cx = 64; int my = 56; display.setDrawColor(1);
  switch(mood){
    case MOOD_DEFAULT: for(int r=0; r<2; r++) display.drawHLine(50, my+2+r, 28); break;
    case MOOD_HAPPY: case MOOD_LOVE: case MOOD_WINK: case MOOD_CUTE:
    { int w2 = 24, d2 = 10; for(int r=0; r<3; r++){ display.drawLine(cx-w2, my+r, cx-6, my+d2+r); display.drawHLine(cx-6, my+d2+r, 12); display.drawLine(cx+6, my+d2+r, cx+w2, my+r); } break; }
    case MOOD_ANGRY: for(int r=0; r<3; r++) display.drawHLine(44, my+r, 40); display.setDrawColor(0); display.drawBox(45, my+1, 38, 2); display.setDrawColor(1); break;
    case MOOD_SLEEPING: for(int r=0; r<2; r++) display.drawHLine(54, my+2+r, 20); break;
    case MOOD_SURPRISED: display.drawDisc(cx, my+4, 6, U8G2_DRAW_ALL); display.setDrawColor(0); display.drawDisc(cx, my+4, 4, U8G2_DRAW_ALL); display.setDrawColor(1); break;
  }
}

void drawMoodParticles(){
  uint32_t now = millis();
  switch(currentMood){
    case MOOD_SLEEPING: { uint8_t xo = (now/300)%28; display.setFont(u8g2_font_4x6_tf); display.drawStr(76+xo,12,"z"); display.drawStr(84+xo,6,"z"); break; }
    case MOOD_HAPPY: case MOOD_SURPRISED: for(uint8_t i=0;i<6;i++) if(sparkles[i].alive){ display.drawPixel(sparkles[i].x,(uint8_t)sparkles[i].y); display.drawPixel(sparkles[i].x+1,(uint8_t)sparkles[i].y); } break;
    case MOOD_LOVE: for(int h=0;h<3;h++){ int8_t hy = (int8_t)(5 + sinf(now/600.0f + h*1.2f)*5); pixHeart(20+h*44,hy); } break;
    case MOOD_ANGRY: for(int r=0;r<2;r++){ display.drawLine(56+r,4,60+r,10); display.drawLine(60+r,10,64+r,6); } break;
    case MOOD_CUTE: for(int h=0;h<3;h++){ int8_t hy = (int8_t)(8 + sinf(now/500.0f + h*1.0f)*4); pixHeart(12+h*52,hy); } break;
  }
}

void updatePhysics(){
  unsigned long now = millis();
  breathVal = sinf(now / 900.0f) * 2.0f;
  if(now > leftEye.nextBlinkTime){ leftEye.blinking = rightEye.blinking = true; leftEye.lastBlink = now; leftEye.nextBlinkTime = now + random(1800, 5000); }
  if(leftEye.blinking){ leftEye.targetH = rightEye.targetH = 2; if(now - leftEye.lastBlink > 130) leftEye.blinking = rightEye.blinking = false; }
  if(!leftEye.blinking && now - lastSaccade > saccadeInterval){ lastSaccade = now; saccadeInterval = random(500, 2000); const float GX[] = {0,8,-8,0,0,6,-6,6,-6}; const float GY[] = {0,0, 0,-6,6,-5,-5,5,5}; int dir = random(0, 9); leftEye.targetPupilX = rightEye.targetPupilX = GX[dir]; leftEye.targetPupilY = rightEye.targetPupilY = GY[dir]; }
  float pk = 0.12f; leftEye.pupilX += (leftEye.targetPupilX - leftEye.pupilX)*pk; leftEye.pupilY += (leftEye.targetPupilY - leftEye.pupilY)*pk; rightEye.pupilX += (rightEye.targetPupilX - rightEye.pupilX)*pk; rightEye.pupilY += (rightEye.targetPupilY - rightEye.pupilY)*pk;
  float ek = 0.14f; leftEye.y = spf(leftEye.y, leftEye.targetY, ek); leftEye.h = spf(leftEye.h, leftEye.targetH, ek); rightEye.y = spf(rightEye.y, rightEye.targetY, ek); rightEye.h = spf(rightEye.h, rightEye.targetH, ek);
  if(!leftEye.blinking){ leftEye.targetH = rightEye.targetH = 46; leftEye.targetY = 3 + breathVal; rightEye.targetY = 3 + breathVal; }
}

// ═══════════════════════════════════════════════════════════════
//  ⑪ SCREENS
// ═══════════════════════════════════════════════════════════════

// Helper function for centered text
void centerStr(const char* s, uint8_t y){
  display.drawStr(64 - display.getStrWidth(s)/2, y, s);
}

// Mochi Face Screen
void drawMochi() {
  updatePhysics();
  drawMoodParticles();
  int lcx = (int)(leftEye.x  + leftEye.w  / 2); int lcy = (int)(leftEye.y  + leftEye.h  / 2);
  int rcx = (int)(rightEye.x + rightEye.w / 2); int rcy = (int)(rightEye.y + rightEye.h / 2);
  int gx = (int)leftEye.pupilX; int gy = (int)leftEye.pupilY;

  switch(currentMood) {
    case MOOD_HAPPY:     eyeHappy(lcx, lcy, gx, gy); eyeHappy(rcx, rcy, gx, gy); break;
    case MOOD_LOVE:      eyeLove(lcx, lcy, gx, gy); eyeLove(rcx, rcy, gx, gy); break;
    case MOOD_STAR:      eyeStar(lcx, lcy, gx, gy); eyeStar(rcx, rcy, gx, gy); break;
    case MOOD_WINK:      eyeWinkClosed(lcx, lcy); eyeWinkOpen(rcx, rcy, gx, gy); break;
    case MOOD_DIZZY:     eyeDizzy(lcx, lcy, gx, gy); eyeDizzy(rcx, rcy, gx, gy); break;
    case MOOD_ANGRY:     eyeAngry(lcx, lcy, gx, gy, true); eyeAngry(rcx, rcy, gx, gy, false); break;
    case MOOD_SAD:       eyeSad(lcx, lcy, gx, gy, true); eyeSad(rcx, rcy, gx, gy, false); break;
    case MOOD_SLEEPY:    eyeSleepy(lcx, lcy, gx); eyeSleepy(rcx, rcy, gx); break;
    case MOOD_SURPRISED: eyeSurprised(lcx, lcy, gx, gy); eyeSurprised(rcx, rcy, gx, gy); break;
    case MOOD_SMUG:      eyeSmug(lcx, lcy, gx, gy); eyeSmug(rcx, rcy, gx, gy); break;
    case MOOD_NERVOUS:   eyeNervous(lcx, lcy, gx, gy); eyeNervous(rcx, rcy, gx, gy); break;
    case MOOD_CAT:       eyeCat(lcx, lcy, gx, gy); eyeCat(rcx, rcy, gx, gy); break;
    case MOOD_CUTE:      eyeCute(lcx, lcy, gx, gy); eyeCute(rcx, rcy, gx, gy); break;
    case MOOD_SLEEPING:  eyeWinkClosed(lcx, lcy); eyeWinkClosed(rcx, rcy); break;
    default:             eyeDefault((int)leftEye.x, (int)leftEye.y, (int)leftEye.w, (int)leftEye.h, gx, gy);
                         eyeDefault((int)rightEye.x, (int)rightEye.y, (int)rightEye.w, (int)rightEye.h, gx, gy); break;
  }
  drawMouth(currentMood);
}

// Clock Screen
void drawClock(){
  time_t now = time(nullptr);
  struct tm t;
  localtime_r(&now, &t);

  display.setFont(u8g2_font_6x10_tf);
  char date[20];
  strftime(date, sizeof(date), "%a, %d %b %Y", &t);
  centerStr(date, 10);

  for(uint8_t x=0; x<128; x+=4) display.drawPixel(x, 13);

  display.setFont(u8g2_font_logisoso32_tf);
  char hhmm[6];
  uint8_t hr = t.tm_hour % 12; if(hr==0) hr=12;
  snprintf(hhmm, sizeof(hhmm), "%02d:%02d", hr, t.tm_min);
  uint8_t tw = display.getStrWidth(hhmm);
  display.drawStr(64 - tw/2 - 10, 50, hhmm);

  display.setFont(u8g2_font_6x10_tf);
  display.drawStr(64 + tw/2 - 8, 40, t.tm_hour<12 ? "AM" : "PM");

  if(t.tm_sec == 0) minuteStartMs = millis();
  if(minuteStartMs == 0) minuteStartMs = millis() - (uint32_t)t.tm_sec * 1000;
  uint8_t barW = (uint8_t)((millis()-minuteStartMs) * 126UL / 60000UL);
  if(barW > 126) barW = 126;
  display.drawHLine(1, 62, barW);
}

// Weather Screen
void wxSeg(char ch, int ox, int oy, uint8_t sw=10, uint8_t sh=8, uint8_t t=2){
  const bool S[10][7]={{1,1,1,1,1,1,0},{0,1,1,0,0,0,0},{1,1,0,1,1,0,1},{1,1,1,1,0,0,1},{0,1,1,0,0,1,1},{1,0,1,1,0,1,1},{1,0,1,1,1,1,1},{1,1,1,0,0,0,0},{1,1,1,1,1,1,1},{1,1,1,1,0,1,1}};
  if(ch<'0'||ch>'9') return;
  const bool* s=S[ch-'0'];
  if(s[0]) display.drawBox(ox+t,oy,sw-2*t,t); if(s[1]) display.drawBox(ox+sw-t,oy+t,t,sh-t); if(s[2]) display.drawBox(ox+sw-t,oy+sh+1,t,sh-t); if(s[3]) display.drawBox(ox+t,oy+2*sh-t,sw-2*t,t); if(s[4]) display.drawBox(ox,oy+sh+1,t,sh-t); if(s[5]) display.drawBox(ox,oy+t,t,sh-t); if(s[6]) display.drawBox(ox+t,oy+sh-1,sw-2*t,t);
}
void wxBigNum(const char* s, int ox, int oy){ int cx = ox; for(;*s;s++){ wxSeg(*s,cx,oy); cx+=12; } }
void wxIconSun(int cx, int cy){ display.drawDisc(cx,cy,7,U8G2_DRAW_ALL); display.setDrawColor(0); display.drawDisc(cx,cy,4,U8G2_DRAW_ALL); display.setDrawColor(1); for(int a=0;a<8;a++){ float ang=a*3.14159f/4; display.drawLine(cx+(int)(9*cosf(ang)),cy+(int)(9*sinf(ang)),cx+(int)(12*cosf(ang)),cy+(int)(12*sinf(ang))); } }
void wxIconCloud(int cx, int cy){ display.drawDisc(cx-4,cy+2,4,U8G2_DRAW_ALL); display.drawDisc(cx+3,cy+2,3,U8G2_DRAW_ALL); display.drawDisc(cx,cy-1,4,U8G2_DRAW_ALL); display.drawBox(cx-7,cy+3,14,3); }
void wxIconRain(int cx, int cy){ wxIconCloud(cx,cy-3); for(int i=0;i<3;i++) display.drawLine(cx-4+i*4,cy+4,cx-6+i*4,cy+9); }
void wxIconThunder(int cx, int cy){ wxIconCloud(cx,cy-3); display.drawLine(cx+2,cy+4,cx-1,cy+8); display.drawLine(cx-1,cy+8,cx+1,cy+8); display.drawLine(cx+1,cy+8,cx-2,cy+11); }
void wxIconSnow(int cx, int cy){ wxIconCloud(cx,cy-3); for(int i=0;i<3;i++){ int sx=cx-4+i*4; display.drawPixel(sx,cy+5); display.drawPixel(sx-1,cy+6); display.drawPixel(sx+1,cy+6); display.drawPixel(sx,cy+7); } }
void wxIconMist(int cx, int cy){ for(int i=0;i<4;i++){ uint8_t w=10+(i%2)*4, x=cx-w/2; display.drawHLine(x,cy-3+i*3,w); } }

void drawWeather(){
  if(!wx.valid){
    display.setFont(u8g2_font_6x10_tf);
    centerStr("Fetching Weather...", 32);
    return;
  }
  #define CSTR(str,zcx,y) display.drawStr((zcx)-display.getStrWidth(str)/2,(y),(str))
  display.setFont(u8g2_font_4x6_tf);
  display.drawHLine(0, 0, 128); display.drawHLine(0, 9, 128); display.drawHLine(0, 37, 128); display.drawHLine(0, 63, 128);
  display.drawVLine(42, 9, 28); display.drawVLine(85, 9, 28); display.drawVLine(63, 37, 26);

  char city[16]; strncpy(city, wx.city, 15); city[15]='\0';
  for(uint8_t i=0; city[i]; i++) city[i]=toupper((unsigned char)city[i]);
  uint8_t cw=display.getStrWidth(city), hx=64-cw/2;
  display.drawStr(hx,7,city);
  if(hx>4) display.drawHLine(2,4,hx-4); if(hx+cw+3<126) display.drawHLine(hx+cw+3,4,126-(hx+cw+3));

  int iconCode=atoi(wx.icon);
  if      (iconCode<=1)  wxIconSun(21,23);
  else if (iconCode<=2)  { wxIconSun(16,19); wxIconCloud(22,24); }
  else if (iconCode<=4)  wxIconCloud(21,23);
  else if (iconCode<=10) wxIconRain(21,22);
  else if (iconCode==11) wxIconThunder(21,22);
  else if (iconCode==13) wxIconSnow(21,22);
  else                   wxIconMist(21,23);

  char tmp[5]; snprintf(tmp,sizeof(tmp),"%d",wx.tempC);
  uint8_t digits=strlen(tmp), tempW=digits*12-2;
  uint8_t groupW=tempW+3+9, tempX=63-groupW/2;
  wxBigNum(tmp,tempX,12);
  uint8_t ax=tempX+tempW+3;
  display.drawDisc(ax+1,12,2,U8G2_DRAW_ALL); display.setDrawColor(0); display.drawDisc(ax+1,12,1,U8G2_DRAW_ALL); display.setDrawColor(1);
  display.drawHLine(ax+4,12,4); display.drawHLine(ax+4,16,4); display.drawVLine(ax+4,12,5);

  char fl[9]; snprintf(fl,sizeof(fl),"FL %dC",wx.feelsC); CSTR(fl,106,17); display.drawHLine(87,20,38);
  char desc[10]; strncpy(desc,wx.desc,9); desc[9]='\0'; desc[0]=toupper((unsigned char)desc[0]); for(uint8_t i=1;desc[i];i++) desc[i]=tolower((unsigned char)desc[i]); CSTR(desc,106,30);

  char humline[12]; snprintf(humline,sizeof(humline),"HUM %d%%",wx.humid); CSTR(humline,31,45);
  uint8_t hbw=(uint8_t)((uint16_t)wx.humid*57U/100U); display.drawHLine(2,49,59); display.drawHLine(2,52,59); display.drawVLine(2,49,4); display.drawVLine(60,49,4); display.drawBox(3,50,hbw,2);

  char wd[9]; snprintf(wd,sizeof(wd),"%dm/s",(int)wx.windMs); CSTR("WIND",95,45); CSTR(wd,95,57);
  #undef CSTR
}

// Notification Screen
void drawNotificationScreen() {
  unreadNotifs = 0; // Clear unread count on view

  display.setFont(u8g2_font_6x10_tf);
  display.drawStr(0, 10, "NOTIFICATIONS");
  display.drawHLine(0, 12, 128);

  if (notifCount == 0) {
    display.setFont(u8g2_font_5x8_tf);
    centerStr("No notifications yet", 36);
    return;
  }

  // Draw current selected notification
  int idx = (notifHead - 1 - selectedNotifIdx + MAX_NOTIFS) % MAX_NOTIFS;
  Notification& n = notifBuf[idx];

  display.setFont(u8g2_font_6x10_tf);
  display.drawStr(0, 24, n.app);
  display.setFont(u8g2_font_5x8_tf);
  display.drawStr(0, 36, n.title);

  display.drawHLine(0, 40, 128);

  display.drawStr(0, 52, n.body);

  // Counter at bottom right
  char countStr[10];
  snprintf(countStr, sizeof(countStr), "%d/%d", selectedNotifIdx + 1, notifCount);
  display.drawStr(100, 62, countStr);
}

// Popup Overlay (bottom banner)
void drawPopupOverlay() {
  if (!popupActive) return;
  if (millis() - popupStartMs > 4000) {
    popupActive = false;
    return;
  }

  display.setDrawColor(0);
  display.drawBox(0, 42, 128, 22);
  display.setDrawColor(1);
  display.drawFrame(0, 42, 128, 22);

  display.setFont(u8g2_font_4x6_tf);
  display.drawStr(4, 50, popupTitle);
  display.drawStr(4, 60, popupBody);
}

// ═══════════════════════════════════════════════════════════════
//  ⑫ WEATHER FETCH
// ═══════════════════════════════════════════════════════════════
void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClient client;
  HTTPClient http;

  String url = "http://api.openweathermap.org/data/2.5/weather?q=";
  url += config.owm_city;
  url += "&appid=";
  url += config.owm_key;
  url += "&units=";
  url += config.owm_units;

  http.begin(client, url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    StaticJsonDocument<512> doc;
    if (!deserializeJson(doc, http.getStream())) {
      strncpy(wx.city, doc["name"] | config.owm_city, 23);
      strncpy(wx.desc, doc["weather"][0]["description"] | "", 31);
      strncpy(wx.icon, doc["weather"][0]["icon"] | "01d", 3);
      wx.icon[3] = '\0';
      for (uint8_t i = 0; wx.icon[i]; i++) {
        if (wx.icon[i] == 'd' || wx.icon[i] == 'n') { wx.icon[i] = '\0'; break; }
      }
      wx.tempC  = (int)round((float)doc["main"]["temp"]);
      wx.feelsC = (int)round((float)doc["main"]["feels_like"]);
      wx.humid  = doc["main"]["humidity"];
      wx.windMs = doc["wind"]["speed"];
      wx.valid  = true;
    }
  }
  http.end();
  wxLastFetch = millis();
}

// ═══════════════════════════════════════════════════════════════
//  ⑬ BUTTON HANDLING
// ═══════════════════════════════════════════════════════════════
void handleButtons() {
  bool btn1 = digitalRead(BTN1);
  bool btn2 = digitalRead(BTN2);

  // BTN1: Mood Cycle or Notification Scroll
  if (lastBtn1 == HIGH && btn1 == LOW) {
    Serial.println(F(">>> BTN1 Pressed!"));
    if (currentMode == MODE_NOTIFY && notifCount > 0) {
      selectedNotifIdx = (selectedNotifIdx + 1) % notifCount;
      buzzScreenChange();
    } else {
      currentMood = (currentMood + 1) % MOOD_COUNT;
      buzzEmotion(currentMood);
    }
    delay(150);
  }

  // BTN2 Press Start
  if (lastBtn2 == HIGH && btn2 == LOW) {
    btn2PressStart = millis();
  }

  // BTN2 Release
  if (lastBtn2 == LOW && btn2 == HIGH) {
    unsigned long pressDuration = millis() - btn2PressStart;
    Serial.print(F(">>> BTN2 Released after "));
    Serial.print(pressDuration);
    Serial.println(F(" ms"));

    if (pressDuration < CYCLE_HOLD_MS) { // Short Press: Cycle Screens
      currentMode = (ScreenMode)((currentMode + 1) % MODE_COUNT);
      Serial.print(F(">>> Switched Screen to: "));
      Serial.println(currentMode);
      buzzScreenChange();
    } else { // Long Press (>2s): Toggle Silent Mode
      config.silent = !config.silent;
      saveConfig();
      buzzNotification();
    }
    delay(150);
  }

  lastBtn1 = btn1;
  lastBtn2 = btn2;
}

// ═══════════════════════════════════════════════════════════════
//  ⑭ LED INDICATORS LOGIC
// ═══════════════════════════════════════════════════════════════
void updateLEDs() {
  // 🔴 Red LED (Status): Solid ON if Silent Mode active
  digitalWrite(STATUS_LED, config.silent ? HIGH : LOW);

  // 🟡 Yellow LED (Notify): ON if unread notifications exist
  digitalWrite(NOTIFY_LED, unreadNotifs > 0 ? HIGH : LOW);

  // 🟢 Green LED (WiFi/MQTT):
  // Solid ON = WiFi & MQTT connected
  // Blink slow = WiFi connected, MQTT connecting
  // OFF = No WiFi
  if (WiFi.status() == WL_CONNECTED) {
    if (mqttClient.connected()) {
      digitalWrite(EMOTION_LED, HIGH);
    } else {
      if (millis() - previousBlink > 500) {
        previousBlink = millis();
        greenLedState = !greenLedState;
        digitalWrite(EMOTION_LED, greenLedState ? HIGH : LOW);
      }
    }
  } else {
    digitalWrite(EMOTION_LED, LOW);
  }
}

// ═══════════════════════════════════════════════════════════════
//  ⑮ SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(BTN1, INPUT_PULLUP);
  pinMode(BTN2, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  pinMode(NOTIFY_LED, OUTPUT);
  pinMode(EMOTION_LED, OUTPUT);

  randomSeed(analogRead(0));

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  display.begin();
  display.setContrast(BRIGHTNESS);

  leftEye.init(3, 3, 56, 46);
  rightEye.init(69, 3, 56, 46);

  loadConfig();

  // Config mode check
  if (digitalRead(BTN2) == LOW) {
    delay(500); // debounce
    if (digitalRead(BTN2) == LOW) {
      startConfigPortal();
      return;
    }
  }

  // Normal boot - WiFi connection
  WiFiManager wm;
  if (!wm.autoConnect("DeskBuddy_Setup")) {
    Serial.println(F("WiFi Failed!"));
    ESP.restart();
  }

  Serial.println(F("WiFi Connected!"));
  Serial.print(F("IP: "));
  Serial.println(WiFi.localIP());

  configTime(19800, 0, NTP_SRV);

  espClient.setInsecure();
  espClient.setTimeout(MQTT_CONNECT_TIMEOUT);

  mqttClient.setServer(config.mqtt_broker, config.mqtt_port);
  mqttClient.setCallback(mqttCallback);

  // HTTP notification server (coexists with MQTT notifications)
  if (config.http_enabled) {
    server = new ESP8266WebServer(config.http_port);
    server->on(config.http_endpoint, HTTP_GET, handleNotify);
    server->begin();
    Serial.print(F("HTTP notify endpoint: http://"));
    Serial.print(WiFi.localIP());
    Serial.print(F(":"));
    Serial.print(config.http_port);
    Serial.println(config.http_endpoint);
  }

  fetchWeather();
  buzzIntro();
}

// ═══════════════════════════════════════════════════════════════
//  ⑯ MAIN LOOP
// ═══════════════════════════════════════════════════════════════
void loop() {
  if (isConfigPortalMode) {
    if (server) server->handleClient();
    return;
  }

  if (server) server->handleClient();

  handleButtons();
  updateLEDs();

  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      mqttReconnect();
    } else {
      mqttClient.loop();
    }

    if (millis() - wxLastFetch > WEATHER_TTL) {
      fetchWeather();
    }
  }

  display.clearBuffer();
  display.setDrawColor(1);

  switch (currentMode) {
    case MODE_CLOCK:   drawClock(); break;
    case MODE_WEATHER: drawWeather(); break;
    case MODE_MOCHI:   drawMochi(); break;
    case MODE_NOTIFY:  drawNotificationScreen(); break;
    default: break;
  }

  drawPopupOverlay();
  display.sendBuffer();

  delay(FRAME_MS);
}
