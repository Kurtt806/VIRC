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
        pinMode(cfg.inputPin, INPUT);
    _effects.push_back(cfg);
}

uint32_t LedDriver::scaleColor(uint8_t r, uint8_t g, uint8_t b)
{
    return _strip->Color((r * _brightness) / 255,
                         (g * _brightness) / 255,
                         (b * _brightness) / 255);
}

void LedDriver::loop()
{
    _strip->clear();

    for (auto &cfg : _effects)
    {
        if (!cfg.enabled)
            continue;
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
        else if (cfg.name == "flash")
            renderFlash(cfg);
        else if (cfg.name == "breathing")
            renderBreathing(cfg);
        else if (cfg.name == "random")
            renderRandom(cfg);
        else if (cfg.name == "twinkle")
            renderTwinkle(cfg);
        else if (cfg.name == "gradian")
            renderGradient(cfg);
        else if (cfg.name == "bolide")
            renderBolide(cfg);
    }

    renderOverlay();
    _strip->show();
    delay(20);
}

void LedDriver::renderBasic(const EffectConfig &cfg)
{
    for (uint16_t i = cfg.region.start; i <= cfg.region.end && i < _count; i++)
        _strip->setPixelColor(i, scaleColor(cfg.r, cfg.g, cfg.b));
}

void LedDriver::renderXRL(const EffectConfig &cfg)
{
    static uint16_t pos = 0;
    for (uint16_t i = cfg.region.start; i <= cfg.region.end; i++)
    {
        uint16_t len = cfg.region.end - cfg.region.start + 1;
        if (i == (cfg.region.end - pos % len))
            _strip->setPixelColor(i, scaleColor(cfg.r, cfg.g, cfg.b));
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
            _strip->setPixelColor(i, scaleColor(cfg.r, cfg.g, cfg.b));
    }
    pos++;
    delay(cfg.speed);
}

void LedDriver::setPixelColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    if (index < _count)
        _strip->setPixelColor(index, scaleColor(r, g, b));
}

bool LedDriver::toggleEffectByName(const String &name, bool enable)
{
    for (auto &cfg : _effects)
    {
        if (cfg.name.equalsIgnoreCase(name))
        {
            cfg.enabled = enable;
            return true;
        }
    }
    return false;
}

void LedDriver::show()
{
    _strip->show();
}

void LedDriver::clearEffects()
{
    _effects.clear();
}

void LedDriver::addOverlayBlink(uint16_t pixel, uint8_t r, uint8_t g, uint8_t b,
                                uint8_t times, uint8_t delayMs)
{
    OverlayEffect fx;
    fx.pixel = pixel;
    fx.r = r;
    fx.g = g;
    fx.b = b;
    fx.times = times * 2;
    fx.delayMs = delayMs;
    fx.count = 0;
    fx.isOn = false;
    fx.lastMillis = millis();
    fx.active = true;
    _overlays.push_back(fx);
}

// (Các hàm render khác giữ nguyên từ bản gốc)

bool LedDriver::applyEffectByName(const String &name)
{
    for (const auto &cfg : _effects)
    {
        if (cfg.name.equalsIgnoreCase(name))
        {
            clearEffects();
            addEffect(cfg);
            return true;
        }
    }
    return false;
}