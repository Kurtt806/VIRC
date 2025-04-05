// LedDriver.cpp
#include "LedDriver.h"

LedDriver::LedDriver(uint8_t pin, uint16_t count)
    : _pin(pin), _count(count), _brightness(255)
{
    _strip = new Adafruit_NeoPixel(_count, _pin, NEO_GRB + NEO_KHZ800);
}

void LedDriver::begin()
{
    _strip->begin();
    _strip->clear();
    _strip->show();

    // Test khởi động: sáng lần lượt RGB ở các pixel đầu tiên
    for (uint8_t i = 0; i < 3 && i < _count; i++)
    {
        _strip->setPixelColor(i, _strip->Color(i == 0 ? 255 : 0, i == 1 ? 255 : 0, i == 2 ? 255 : 0));
        _strip->show();
        delay(300);
    }
    _strip->clear();
    _strip->show();
}

void LedDriver::setBrightness(uint8_t brightness)
{
    _brightness = brightness;
}

void LedDriver::addEffect(const EffectConfig &cfg)
{
    if (cfg.inputPin >= 0)
    {
        pinMode(cfg.inputPin, INPUT);
    }
    _effects.push_back(cfg);
}

void LedDriver::setWifiTrigger(bool state)
{
    _wifiTriggered = state;
}

uint32_t LedDriver::scaleColor(uint8_t r, uint8_t g, uint8_t b)
{
    return _strip->Color((r * _brightness) / 255, (g * _brightness) / 255, (b * _brightness) / 255);
}

void LedDriver::loop()
{
    _strip->clear();
    for (auto &cfg : _effects)
    {
        if (cfg.inputPin >= 0 && digitalRead(cfg.inputPin) == LOW)
            continue;
        if (cfg.inputWifi && !_wifiTriggered)
            continue;

        if (cfg.name == "basic")
            renderBasic(cfg);
        else if (cfg.name == "xinhanRL")
            renderXRL(cfg);
        else if (cfg.name == "xinhanLR")
            renderXLR(cfg);
    }
    _strip->show();
    delay(20);
}

void LedDriver::renderBasic(const EffectConfig &cfg)
{
    for (uint16_t i = cfg.region.start; i <= cfg.region.end && i < _count; i++)
    {
        _strip->setPixelColor(i, scaleColor(cfg.r, cfg.g, cfg.b));
    }
}

void LedDriver::renderXRL(const EffectConfig &cfg)
{
    static uint16_t pos = 0;
    for (uint16_t i = cfg.region.start; i <= cfg.region.end; i++)
    {
        uint16_t len = cfg.region.end - cfg.region.start + 1;
        if (i == (cfg.region.end - pos % len))
        {
            _strip->setPixelColor(i, scaleColor(cfg.r, cfg.g, cfg.b));
        }
    }
    pos++;
    delay(cfg.speed);
}

void LedDriver::renderXLR(const EffectConfig &cfg)
{
    static uint16_t pos = 0;
    for (uint16_t i = cfg.region.start; i <= cfg.region.end; i++)
    {
        uint16_t len = cfg.region.end - cfg.region.start + 1;
        if (i == (cfg.region.start + pos % len))
        {
            _strip->setPixelColor(i, scaleColor(cfg.r, cfg.g, cfg.b));
        }
    }
    pos++;
    delay(cfg.speed);
}
