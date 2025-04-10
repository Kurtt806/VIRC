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
    bool enabled = true; // mới thêm
};


// --- Hiệu ứng chồng (overlay) ---
struct OverlayEffect
{
    uint16_t pixel;
    uint8_t r, g, b;
    uint8_t times; // tổng lần nháy
    uint8_t delayMs;
    uint8_t count; // đếm lần đã nháy
    bool isOn;
    unsigned long lastMillis;
    bool active;
};

class LedDriver
{
public:
    LedDriver(uint8_t pin, uint16_t count);

    void begin();
    void setBrightness(uint8_t brightness);
    void addEffect(const EffectConfig &cfg);
    bool applyEffectByName(const String &name);
    void setPixelColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b);
    void show();
    bool toggleEffectByName(const String &name, bool enable);
    void clearEffects();

    void loop();

    // --- API mở rộng ---
    void addOverlayBlink(uint16_t pixel, uint8_t r = 0, uint8_t g = 0, uint8_t b = 255,
                         uint8_t times = 2, uint8_t delayMs = 25);

private:
    uint8_t _pin;
    uint16_t _count;
    uint8_t _brightness;
    bool _wifiTriggered = false;

    Adafruit_NeoPixel *_strip;
    std::vector<EffectConfig> _effects;
    std::vector<OverlayEffect> _overlays;

    uint32_t scaleColor(uint8_t r, uint8_t g, uint8_t b);
    void renderBasic(const EffectConfig &cfg);
    void renderXRL(const EffectConfig &cfg);
    void renderXLR(const EffectConfig &cfg);
    void renderFlash(const EffectConfig &cfg);
    void renderBreathing(const EffectConfig &cfg);
    void renderRandom(const EffectConfig &cfg);
    void renderTwinkle(const EffectConfig &cfg);
    void renderGradient(const EffectConfig &cfg);
    void renderBolide(const EffectConfig &cfg);
    void renderOverlay();
};

#endif // LED_DRIVER_H
