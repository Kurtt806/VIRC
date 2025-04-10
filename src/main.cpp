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

  // Kiểm tra khóa bắt buộc trong [general]
  if (!vircCfg.count("general") || !vircCfg["general"].count("pin") || !vircCfg["general"].count("led_count"))
  {
    Serial.println("[ERROR] Thiếu khóa [general]/pin hoặc led_count");
    return;
  }

  int pin = vircCfg["general"]["pin"].toInt();
  int count = vircCfg["general"]["led_count"].toInt();
  int brightness = vircCfg["general"].count("brightness") ? vircCfg["general"]["brightness"].toInt() : 255;

  if (led)
  {

    delete led;
    led = nullptr;
  }
  led = new LedDriver(pin, count);
  led->setBrightness(brightness);

  for (const auto &sec : vircCfg)
  {
    if (!sec.first.startsWith("strip"))
      continue;

    const auto &map = sec.second;
    if (!map.count("effect") || !map.count("ledstart") || !map.count("ledend"))
    {
      Serial.println("[WARN] Bỏ qua " + sec.first + " do thiếu effect/ledstart/ledend");
      continue;
    }

    EffectConfig cfg;
    cfg.name = map.at("effect");
    cfg.region.start = map.at("ledstart").toInt();
    cfg.region.end = map.at("ledend").toInt();
    cfg.speed = map.count("speed") ? map.at("speed").toInt() : 20;

    if (map.count("input"))
    {
      String input = map.at("input");
      if (input.startsWith("gpio"))
        cfg.inputPin = digitalPinToPinNumber(input);
      else if (input == "wifi")
        cfg.inputWifi = true;
    }

    // Xử lý color
    String colorStr = map.count("color") ? map.at("color") : "255,255,255";
    int r = 255, g = 255, b = 255;
    sscanf(colorStr.c_str(), "%d,%d,%d", &r, &g, &b);
    cfg.r = r;
    cfg.g = g;
    cfg.b = b;

    led->addEffect(cfg);
  }

  led->begin();
}

void initLedDriver()
{
  if (!vircCfg.count("general") || !vircCfg["general"].count("led_count") || !vircCfg["general"].count("pin"))
  {
    Serial.println("[ERROR] Thiếu cấu hình [general]");
    return;
  }

  int ledCount = vircCfg["general"]["led_count"].toInt();
  int pin = vircCfg["general"]["pin"].toInt();
  String effect = vircCfg["general"].count("effect") ? vircCfg["general"]["effect"] : "basic";
  String colorStr = vircCfg.count("colors") && vircCfg["colors"].count("color1") ? vircCfg["colors"]["color1"] : "255,255,255";
  int brightness = vircCfg["general"].count("brightness") ? vircCfg["general"]["brightness"].toInt() : 255;

  int r = 0, g = 0, b = 0;
  sscanf(colorStr.c_str(), "%d,%d,%d", &r, &g, &b);

  if (led)
  {
    delete led;
    led = nullptr;
  }

  led = new LedDriver(pin, ledCount);
  led->setBrightness(brightness);
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
      String msg = String((char *)data).substring(0, len);
      msg.trim();

      sendLogToClients("[HOST] " + msg);
      led->addOverlayBlink(0, 0, 0, 255, 1, 20); // chớp xanh pixel 0 ba lần

      if (msg == "REFRESH_EFFECT_LIST") {
        sendLogToClients("📤 Đã gửi lại danh sách hiệu ứng sau khi upload virc.cfg");
        sendListToClients();
      }
      else if (msg == "RESET_ESP") {
        sendLogToClients("🌀 ESP32 sẽ reset sau 1 giây...");
        client->text("🌀 Resetting...");
        delay(1000);
        ESP.restart();
      }      
      else if (msg.startsWith("SET:")) {
        String effectName = msg.substring(4);
        sendLogToClients("🎛️ Yêu cầu chạy hiệu ứng: " + effectName);

        if (led) {
          bool ok = led->applyEffectByName(effectName); // hàm này bạn cần hiện thực
          if (ok) {
            client->text("✅ Đang chạy hiệu ứng: " + effectName);
            sendLogToClients("✅ Đang chạy hiệu ứng: " + effectName);
          } else {
            client->text("❌ Không tìm thấy hiệu ứng: " + effectName);
            sendLogToClients("❌ Không tìm thấy hiệu ứng: " + effectName);
          }
        }
      }
      else if (msg.startsWith("SET_BRIGHTNESS:")) {
        int b = msg.substring(strlen("SET_BRIGHTNESS:")).toInt();
        if (b >= 0 && b <= 255 && led) {
          led->setBrightness(b);
          sendLogToClients("✨ Đã đổi độ sáng thành: " + String(b));
          client->text("✅ Brightness set to " + String(b));
        } else {
          client->text("❌ Brightness không hợp lệ");
        }
      }
      
      else if (msg.startsWith("TOGGLE:")) 
      {
        int sep1 = msg.indexOf(':');
        int sep2 = msg.indexOf(':', sep1 + 1);
        if (sep1 > 0 && sep2 > sep1) 
        {
          String name = msg.substring(sep1 + 1, sep2);
          bool enable = msg.substring(sep2 + 1) == "1";
          if (led && led->toggleEffectByName(name, enable)) {
              sendLogToClients("🔁 Đã " + String(enable ? "bật" : "tắt") + " hiệu ứng: " + name);
              // TODO: Ghi vào file JSON toggle trạng thái
          } 
          else 
          {
              sendLogToClients("⚠️ Không tìm thấy hiệu ứng: " + name);
          }
        }
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
              sendLogToClients("Send effect_list");
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

  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request)
            {
                        request->send(200, "text/plain", "OK");
                        sendLogToClients("✅ Upload hoàn tất"); }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
            {
                        static File f;
                    
                        if (index == 0) {
                          sendLogToClients("📥 Bắt đầu nhận file: " + filename);
                          
                          if (SPIFFS.exists("/" + filename)) SPIFFS.remove("/" + filename);
                          f = SPIFFS.open("/" + filename, FILE_WRITE);
                        }
                    
                        if (f) {
                          f.write(data, len);
                        }
                    
                        if (final) {
                          f.close();
                          sendLogToClients("✅ Đã lưu file thành công: " + filename);
                    
                          // Tùy theo tên file, nạp lại cấu hình nếu cần
                          if (filename == "virc.cfg") {
                            sendLogToClients("⏳ Nạp lại virc.cfg...");
                            delay(50);
                            loadLedConfig();
                            if (led) sendListToClients();
                            sendLogToClients("✅ virc.cfg đã nạp");
                          }
                    
                          else if (filename == "wifi.cfg") {
                            sendLogToClients("⏳ Nạp lại wifi.cfg...");
                            parseCfgFile("/wifi.cfg", wifiCfg);
                            sendLogToClients("✅ wifi.cfg đã nạp");
                          }
                        } });
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
  if (now - lastSent >= 1000)
  {
    lastSent = now;

    // Tạo JSON hệ thống
    String json = "{";
    json += "\"chip_id\":\"" + String((uint32_t)ESP.getEfuseMac(), HEX) + "\",";
    json += "\"mac\":\"" + WiFi.softAPmacAddress() + "\",";
    json += "\"flash_size\":" + String(ESP.getFlashChipSize() / 1024) + ",";
    json += "\"heap_free\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"heap_total\":" + String(ESP.getHeapSize()) + ",";
    json += "\"flash_size\":" + String(ESP.getFlashChipSize() / 1024) + ",";
    json += "\"max_alloc\":" + String(ESP.getMaxAllocHeap()) + ",";
    json += "\"cpu_freq\":" + String(ESP.getCpuFreqMHz()) + ",";
    json += "\"uptime\":\"" + String(millis() / 1000) + "s\"";

#ifdef BOARD_HAS_PSRAM
    json += ",\"psram_total\":" + String(ESP.getPsramSize());
    json += ",\"psram_free\":" + String(ESP.getFreePsram());
#endif

    json += "}";

    // Gửi qua WebSocket
    ws.textAll("[SYSINFO]" + json);
  }
}
