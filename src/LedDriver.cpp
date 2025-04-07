#include "LedDriver.h"

LedDriver::LedDriver(uint8_t pin, uint16_t count)
    : _pin(pin), _count(count), _brightness(255) {
    _strip = new Adafruit_NeoPixel(_count, _pin, NEO_GRB + NEO_KHZ800);
}

void LedDriver::begin() {
    _strip->begin();
    _strip->clear();
    _strip->show();

    for (uint8_t i = 0; i < 3 && i < _count; i++) {
        _strip->setPixelColor(i, _strip->Color(i == 0 ? 255 : 0, i == 1 ? 255 : 0, i == 2 ? 255 : 0));
        _strip->show();
        delay(300);
    }
    _strip->clear();
    _strip->show();
}

void LedDriver::setBrightness(uint8_t brightness) {
    _brightness = brightness;
}

void LedDriver::addEffect(const EffectConfig &cfg) {
    if (cfg.inputPin >= 0)
        pinMode(cfg.inputPin, INPUT);
    _effects.push_back(cfg);
}

void LedDriver::setWifiTrigger(bool state) {
    _wifiTriggered = state;
}

uint32_t LedDriver::scaleColor(uint8_t r, uint8_t g, uint8_t b) {
    return _strip->Color((r * _brightness) / 255,
                         (g * _brightness) / 255,
                         (b * _brightness) / 255);
}

void LedDriver::loop() {
    _strip->clear();

    for (auto &cfg : _effects) {
        if (cfg.inputPin >= 0 && digitalRead(cfg.inputPin) == LOW) continue;
        if (cfg.inputWifi && !_wifiTriggered) continue;

        if (cfg.name == "basic") renderBasic(cfg);
        else if (cfg.name == "xinhanRL") renderXRL(cfg);
        else if (cfg.name == "xinhanLR") renderXLR(cfg);
    }

    renderOverlay();
    _strip->show();
    delay(20);
}

void LedDriver::renderBasic(const EffectConfig &cfg) {
    for (uint16_t i = cfg.region.start; i <= cfg.region.end && i < _count; i++)
        _strip->setPixelColor(i, scaleColor(cfg.r, cfg.g, cfg.b));
}

void LedDriver::renderXRL(const EffectConfig &cfg) {
    static uint16_t pos = 0;
    for (uint16_t i = cfg.region.start; i <= cfg.region.end; i++) {
        uint16_t len = cfg.region.end - cfg.region.start + 1;
        if (i == (cfg.region.end - pos % len))
            _strip->setPixelColor(i, scaleColor(cfg.r, cfg.g, cfg.b));
    }
    pos++;
    delay(cfg.speed);
}

void LedDriver::renderXLR(const EffectConfig &cfg) {
    static uint16_t pos = 0;
    for (uint16_t i = cfg.region.start; i <= cfg.region.end; i++) {
        uint16_t len = cfg.region.end - cfg.region.start + 1;
        if (i == (cfg.region.start + pos % len))
            _strip->setPixelColor(i, scaleColor(cfg.r, cfg.g, cfg.b));
    }
    pos++;
    delay(cfg.speed);
}

void LedDriver::setPixelColor(uint16_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < _count)
        _strip->setPixelColor(index, scaleColor(r, g, b));
}

void LedDriver::show() {
    _strip->show();
}

void LedDriver::clearEffects() {
    _effects.clear();
}

void LedDriver::addOverlayBlink(uint16_t pixel, uint8_t r, uint8_t g, uint8_t b,
                                uint8_t times, uint8_t delayMs) {
    OverlayEffect fx;
    fx.pixel = pixel;
    fx.r = r;
    fx.g = g;
    fx.b = b;
    fx.times = times * 2; // on + off
    fx.delayMs = delayMs;
    fx.count = 0;
    fx.isOn = false;
    fx.lastMillis = millis();
    fx.active = true;
    _overlays.push_back(fx);
}

void LedDriver::renderOverlay() {
    unsigned long now = millis();
    for (auto &fx : _overlays) {
        if (!fx.active) continue;
        if (now - fx.lastMillis >= fx.delayMs) {
            fx.lastMillis = now;
            if (fx.isOn)
                _strip->setPixelColor(fx.pixel, 0, 0, 0);
            else
                _strip->setPixelColor(fx.pixel, scaleColor(fx.r, fx.g, fx.b));
            fx.isOn = !fx.isOn;
            fx.count++;
            if (fx.count >= fx.times)
                fx.active = false;
        }
    }
    // Xóa hiệu ứng đã kết thúc
    _overlays.erase(std::remove_if(_overlays.begin(), _overlays.end(),
                                   [](const OverlayEffect &fx) { return !fx.active; }),
                    _overlays.end());
}
