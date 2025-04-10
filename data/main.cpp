#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "SPIFFS.h"

#include "CFGUtils.h"
#include "LedDriver.h"
#include "LedStatusStore.h" // NEW

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

SectionMap vircCfg;
SectionMap wifiCfg;

LedDriver *led = nullptr;
std::map<String, bool> effectStatus; // NEW

void sendLogToClients(const String &msg)
{
  ws.textAll("[LOG]" + msg);
}

void initSystem()
{
  Serial.begin(115200);
  SPIFFS.begin();
  pinMode(8, OUTPUT);
  digitalWrite(8, HIGH);
}

void loadLedConfig()
{
  if (!parseCfgFile("/virc.cfg", vircCfg))
  {
    Serial.println("[ERROR] Không đọc được virc.cfg");
    sendLogToClients("❌ Không đọc được virc.cfg");
    return;
  }

  if (!vircCfg.count("general") || !vircCfg["general"].count("pin") || !vircCfg["general"].count("led_count"))
  {
    Serial.println("[ERROR] Thiếu khóa [general]/pin hoặc led_count");
    return;
  }

  int pin = vircCfg["general"]["pin"].toInt();
  int count = vircCfg["general"]["led_count"].toInt();
  int brightness = vircCfg["general"].count("brightness") ? vircCfg["general"]["brightness"].toInt() : 255;

  if (led) {
    delete led;
    led = nullptr;
  }

  led = new LedDriver(pin, count);
  led->setBrightness(brightness);

  LedStatusStore::loadStatus(effectStatus); // NEW

  for (const auto &sec : vircCfg)
  {
    if (!sec.first.startsWith("strip"))
      continue;

    const auto &map = sec.second;
    if (!map.count("effect") || !map.count("ledstart") || !map.count("ledend"))
      continue;

    EffectConfig cfg;
    cfg.name = map.at("effect");
    cfg.region.start = map.at("ledstart").toInt();
    cfg.region.end = map.at("ledend").toInt();
    cfg.speed = map.count("speed") ? map.at("speed").toInt() : 20;

    if (map.count("input")) {
      String input = map.at("input");
      if (input.startsWith("gpio"))
        cfg.inputPin = digitalPinToPinNumber(input);
      else if (input == "wifi")
        cfg.inputWifi = true;
    }

    String colorStr = map.count("color") ? map.at("color") : "255,255,255";
    int r = 255, g = 255, b = 255;
    sscanf(colorStr.c_str(), "%d,%d,%d", &r, &g, &b);
    cfg.r = r;
    cfg.g = g;
    cfg.b = b;

    cfg.enabled = true;
    if (effectStatus.count(cfg.name)) // NEW
      cfg.enabled = effectStatus[cfg.name];

    led->addEffect(cfg);
  }

  led->begin();
}

void initWebSocket()
{
  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                AwsEventType type, void *arg, uint8_t *data, size_t len)
             {
    if (type == WS_EVT_DATA) {
      String msg = String((char *)data).substring(0, len);
      msg.trim();

      sendLogToClients("[HOST] " + msg);
      led->addOverlayBlink(0, 0, 0, 255, 1, 20);

      if (msg == "REFRESH_EFFECT_LIST") {
        sendListToClients();
      }
      else if (msg == "RESET_ESP") {
        client->text("🌀 Resetting...");
        delay(1000);
        ESP.restart();
      }
      else if (msg.startsWith("SET:")) {
        String effectName = msg.substring(4);
        if (led) {
          bool ok = led->applyEffectByName(effectName);
          if (ok) {
            client->text("✅ Đang chạy hiệu ứng: " + effectName);
          } else {
            client->text("❌ Không tìm thấy hiệu ứng: " + effectName);
          }
        }
      }
      else if (msg.startsWith("TOGGLE:")) {
        int sep1 = msg.indexOf(':');
        int sep2 = msg.indexOf(':', sep1 + 1);
        if (sep1 > 0 && sep2 > sep1) {
          String name = msg.substring(sep1 + 1, sep2);
          bool enable = msg.substring(sep2 + 1) == "1";
          if (led && led->toggleEffectByName(name, enable)) {
              effectStatus[name] = enable; // NEW
              LedStatusStore::saveStatus(effectStatus); // NEW
              sendLogToClients("🔁 Đã " + String(enable ? "bật" : "tắt") + " hiệu ứng: " + name);
          } else {
              sendLogToClients("⚠️ Không tìm thấy hiệu ứng: " + name);
          }
        }
      }
    }});
  server.addHandler(&ws);
}

void sendListToClients()
{
  String effects = "[EFFECT_LIST]";
  for (const auto &sec : vircCfg)
  {
    if (sec.first.startsWith("strip") && sec.second.count("effect"))
    {
      effects += sec.second.at("effect") + ",";
    }
  }
  if (effects.endsWith(","))
    effects.remove(effects.length() - 1);
  ws.textAll(effects);
}

void setupWiFiAP()
{
  if (parseCfgFile("/wifi.cfg", wifiCfg))
  {
    String ssid = wifiCfg["wifi"]["ssid"];
    String password = wifiCfg["wifi"]["password"];
    IPAddress ip, gw, subnet;
    ip.fromString(wifiCfg["wifi"]["ip"]);
    gw.fromString(wifiCfg["wifi"]["gateway"]);
    subnet.fromString(wifiCfg["wifi"]["subnet"]);
    WiFi.softAPConfig(ip, gw, subnet);
    WiFi.softAP(ssid.c_str(), password.c_str());
  }
}

void initFileServer()
{
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request)
  {
    request->send(200, "text/plain", "OK");
    sendLogToClients("✅ Upload hoàn tất");
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
  {
    static File f;
    if (index == 0) {
      if (SPIFFS.exists("/" + filename)) SPIFFS.remove("/" + filename);
      f = SPIFFS.open("/" + filename, FILE_WRITE);
    }
    if (f) f.write(data, len);
    if (final) {
      f.close();
      if (filename == "virc.cfg") {
        loadLedConfig();
        if (led) sendListToClients();
      } else if (filename == "wifi.cfg") {
        parseCfgFile("/wifi.cfg", wifiCfg);
      }
    }
  });
}

void setup()
{
  initSystem();
  setupWiFiAP();
  loadLedConfig();
  initWebSocket();
  initFileServer();
  server.begin();
  sendListToClients();
}

void loop()
{
  if (led)
    led->loop();
}