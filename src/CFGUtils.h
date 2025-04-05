#ifndef DRIVERCONFIG_H
#define DRIVERCONFIG_H

#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <map>



// ==========================
// 🔧 Hàm tiện ích helper to map "gpioX" to pin number
// ==========================
int digitalPinToPinNumber(const String &name)
{
  if (name.startsWith("gpio"))
  {
    return name.substring(4).toInt();
  }
  return -1;
}

// ==========================
// 🔧 Hàm tiện ích đọc file .cfg
// ==========================
using SectionMap = std::map<String, std::map<String, String>>;

inline bool parseCfgFile(const char* path, SectionMap& outMap)
{
    File file = SPIFFS.open(path, "r");
    if (!file)
    {
        Serial.println("[parseCfgFile] ❌ Failed to open file: " + String(path));
        return false;
    }

    outMap.clear();
    String currentSection = "default";

    while (file.available())
    {
        String line = file.readStringUntil('\n');
        line.trim();

        if (line.isEmpty() || line.startsWith("#") || line.startsWith(";"))
            continue;

        if (line.startsWith("[") && line.endsWith("]"))
        {
            currentSection = line.substring(1, line.length() - 1);
            currentSection.trim();
            continue;
        }

        int sep = line.indexOf('=');
        if (sep > 0)
        {
            String key = line.substring(0, sep);
            String value = line.substring(sep + 1);
            key.trim(); value.trim();
            outMap[currentSection][key] = value;
        }
    }

    file.close();
    return true;
}

#endif // DRIVERCONFIG_H
