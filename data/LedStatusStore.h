#ifndef LED_STATUS_STORE_H
#define LED_STATUS_STORE_H

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <map>

class LedStatusStore {
public:
    static constexpr const char* STATUS_FILE = "/led_status.json";

    static bool saveStatus(const std::map<String, bool>& statusMap) {
        File f = SPIFFS.open(STATUS_FILE, FILE_WRITE);
        if (!f) return false;

        StaticJsonDocument<1024> doc;
        for (const auto& pair : statusMap) {
            doc[pair.first] = pair.second;
        }
        serializeJson(doc, f);
        f.close();
        return true;
    }

    static bool loadStatus(std::map<String, bool>& statusMap) {
        if (!SPIFFS.exists(STATUS_FILE)) return false;
        File f = SPIFFS.open(STATUS_FILE, FILE_READ);
        if (!f) return false;

        StaticJsonDocument<1024> doc;
        DeserializationError err = deserializeJson(doc, f);
        if (err) {
            f.close();
            return false;
        }

        statusMap.clear();
        for (JsonPair kv : doc.as<JsonObject>()) {
            statusMap[kv.key().c_str()] = kv.value();
        }
        f.close();
        return true;
    }
};

#endif