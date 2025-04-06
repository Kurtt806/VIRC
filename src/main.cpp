#include <WiFi.h>
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "SPIFFS.h"

#include "CFGUtils.h"  // chứa parseCfgFile + SectionMap
#include "LedDriver.h" // driver điều khiển LED

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

SectionMap vircCfg;
SectionMap wifiCfg;

LedDriver *led = nullptr; // Con trỏ đến LED driver

void blinkBluePixel(int pixel = 0, int times = 2, int delayMs = 25)
{
  if (!led) return;

  for (int i = 0; i < times; ++i)
  {
    led->setPixelColor(pixel, 0, 0, 255); // màu xanh dương
    led->show();
    delay(delayMs);
    led->setPixelColor(pixel, 0, 0, 0); // tắt
    led->show();
    delay(delayMs);
  }
}

void sendLogToClients(const String &msg)
{
  ws.textAll("[LOG]" + msg);
}

void initSystem()
{
  Serial.begin(115200);
  SPIFFS.begin();
  pinMode(8, OUTPUT);
}

void loadLedConfig()
{
  parseCfgFile("/virc.cfg", vircCfg);

  int pin = vircCfg["general"]["pin"].toInt();
  int count = vircCfg["general"]["led_count"].toInt();
  int brightness = vircCfg["general"].count("brightness") ? vircCfg["general"]["brightness"].toInt() : 255;

  led = new LedDriver(pin, count);
  led->setBrightness(brightness);

  for (const auto &sec : vircCfg)
  {
    if (!sec.first.startsWith("strip"))
      continue;
    const auto &map = sec.second;
    EffectConfig cfg;
    cfg.name = map.at("effect");
    cfg.region.start = map.at("ledstart").toInt();
    cfg.region.end = map.at("ledend").toInt();
    cfg.speed = map.count("speed") ? map.at("speed").toInt() : 20;

    String input = map.count("input") ? map.at("input") : "";
    if (input.startsWith("gpio"))
    {
      cfg.inputPin = digitalPinToPinNumber(input);
    }
    else if (input == "wifi")
    {
      cfg.inputWifi = true;
    }

    int r = 255, g = 255, b = 255;
    sscanf(map.at("color").c_str(), "%d,%d,%d", &r, &g, &b);
    cfg.r = r;
    cfg.g = g;
    cfg.b = b;

    led->addEffect(cfg);
  }

  led->begin();
}

void initLedDriver()
{
  int ledCount = vircCfg["general"]["led_count"].toInt();
  int pin = vircCfg["general"]["pin"].toInt();
  String effect = vircCfg["general"]["effect"];
  String colorStr = vircCfg["colors"]["color1"];
  int brightness = vircCfg["general"]["brightness"].toInt();
  int r = 0, g = 0, b = 0;
  sscanf(colorStr.c_str(), "%d,%d,%d", &r, &g, &b);

  led = new LedDriver(pin, ledCount);
  led->setBrightness(brightness > 0 ? brightness : 255); // fallback nếu chưa có
  led->begin();
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

void sendListToClients()
{
  // Gửi thông tin về các hiệu ứng đã đinh nghĩa
  String effects = "[EFFECT_LIST]";
  for (const auto &sec : vircCfg)
  {
    if (sec.first.startsWith("strip") && sec.second.count("effect"))
    {
      effects += sec.second.at("effect") + ",";
    }
  }
  if (effects.endsWith(","))
    effects.remove(effects.length() - 1); // bỏ dấu , cuối
  ws.textAll(effects);
}

void initWebSocket()
{
  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                AwsEventType type, void *arg, uint8_t *data, size_t len)
             {
    if (type == WS_EVT_DATA) {
      String msg = String((char*)data).substring(0, len);
      msg.trim();
      blinkBluePixel();

      // Xử lý lệnh điều khiển LED theo yêu cầu của client
      if (msg == "LED_ON") {
        if (led) led->setWifiTrigger(true);
        client->text("✅ LED ON");
        sendLogToClients("✅ LED trigger ON");

      }
      else if (msg == "LED_OFF") {
        if (led) led->setWifiTrigger(false);
        client->text("✅ LED OFF");
        sendLogToClients("✅ LED trigger OFF");
      }
    } });
  server.addHandler(&ws);
}

void initFileServer()
{
  // Trả file mặc định
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  server.on("/effect_list", HTTP_GET, [](AsyncWebServerRequest *request)
            {
  String json = "[";
  for (const auto &sec : vircCfg)
  {
    if (sec.first.startsWith("strip") && sec.second.count("effect"))
    {
      json += "\"" + sec.second.at("effect") + "\",";
    }
  }
  if (json.endsWith(",")) json.remove(json.length() - 1); // xóa dấu , cuối
  json += "]";
  request->send(200, "application/json", json); });

  // Giả phản hồi có mạng Internet
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request)
            {
              request->send(204); // Android
            });

  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>"); });

  server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", "Microsoft NCSI"); });

  server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(200, "text/plain", "Internet Connect Test"); });

  server.on("/redirect", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->redirect("/"); });

  // Mặc định: không tìm thấy -> trả file 404
  server.onNotFound([](AsyncWebServerRequest *request)
                    { request->send(404, "text/plain", "Not Found"); });

  server.on(
      "/upload", HTTP_POST,
      [](AsyncWebServerRequest *request)
      {
        request->send(200, "text/plain", "OK");
        sendLogToClients("Upload hoàn tất");
      },
      [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data,
         size_t len, bool final)
      {
        static File f;
        static String targetPath;

        if (index == 0)
        {
          // Xác định tên file sẽ lưu (chỉ hỗ trợ virc.cfg hoặc wifi.cfg)
          String clientFile = request->getParam("configFile", true)->value();
          clientFile.toLowerCase();
          if (clientFile.endsWith("virc.cfg"))
          {
            targetPath = "/virc.cfg";
          }
          else if (clientFile.endsWith("wifi.cfg"))
          {
            targetPath = "/wifi.cfg";
          }
          else
          {
            targetPath = "";
          }

          Serial.println("[UPLOAD] Bắt đầu: " + filename + " => " + targetPath);
          if (targetPath != "")
          {
            if (SPIFFS.exists(targetPath))
              SPIFFS.remove(targetPath);
            f = SPIFFS.open(targetPath, FILE_WRITE);
          }
        }

        if (f && targetPath != "")
        {
          f.write(data, len);
        }

        if (final && targetPath != "")
        {
          f.close();
          Serial.println("[UPLOAD] Xong: " + targetPath);

          if (targetPath == "/virc.cfg")
          {
            loadLedConfig();
            sendListToClients();
          }
          else if (targetPath == "/wifi.cfg")
          {
            parseCfgFile("/wifi.cfg", wifiCfg);
            sendLogToClients("✅ Đã tải lại wifi.cfg");
          }
        }
      });
}




void setup()
{
  initSystem();
  setupWiFiAP();
  initLedDriver();
  initWebSocket();
  loadLedConfig();
  initFileServer();
  server.begin();
  sendListToClients();

  Serial.println("Nội dung debug");
  sendLogToClients("Nội dung debug");
}


void loop()
{
  static unsigned long lastSent = 0;
  if (led)
    led->loop();

  unsigned long now = millis();
  if (now - lastSent >= 1000) {
    lastSent = now;

    sendLogToClients(".");

    // Tạo JSON gửi về Web qua WebSocket
    String json = "{";
    json += "\"chip_id\":\"" + String((uint32_t)ESP.getEfuseMac(), HEX) + "\",";
    json += "\"mac\":\"" + WiFi.softAPmacAddress() + "\",";
    json += "\"flash_size\":" + String(ESP.getFlashChipSize() / 1024) + ",";
    json += "\"heap_free\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"cpu_freq\":" + String(ESP.getCpuFreqMHz()) + ",";
    json += "\"uptime\":\"" + String(millis() / 1000) + "s\"";
    json += "}";

    ws.textAll("[SYSINFO]" + json);
  }
}

