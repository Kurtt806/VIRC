// LedDriver.h
#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <vector>

struct EffectRegion
{
    uint16_t start = 0;
    uint16_t end = 0xFFFF;
};

struct EffectConfig
{
    String name;
    uint8_t size = 3;
    uint16_t speed = 20;
    EffectRegion region;
    uint8_t r = 255, g = 255, b = 255;
    int inputPin = -1;
    bool inputWifi = false;
};

class LedDriver
{
public:
    LedDriver(uint8_t pin, uint16_t count);

    void begin();
    void setBrightness(uint8_t brightness);
    void addEffect(const EffectConfig &cfg);
    void setWifiTrigger(bool state);

    void setPixelColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
    void show();

    void loop();

private:
    uint8_t _pin;
    uint16_t _count;
    uint8_t _brightness;
    bool _wifiTriggered = false;

    Adafruit_NeoPixel *_strip;
    std::vector<EffectConfig> _effects;

    uint32_t scaleColor(uint8_t r, uint8_t g, uint8_t b);
    void renderBasic(const EffectConfig &cfg);
    void renderXRL(const EffectConfig &cfg);
    void renderXLR(const EffectConfig &cfg);
};

#endif // LED_DRIVER_H
