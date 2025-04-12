#include "LedDriver.h"
std::vector<EffectState> _effectStates;
std::vector<EffectConfig> &LedDriver::getEffects()
{
    return _effects;
}

EffectState &LedDriver::getEffectState(const EffectConfig &cfg)
{
    for (auto &state : _effectStates)
    {
        if (state.effectName == cfg.name && state.regionStart == cfg.region.start && state.regionEnd == cfg.region.end)
        {
            return state;
        }
    }
    // Nếu chưa có trạng thái thì thêm mới
    EffectState newState;
    newState.effectName = cfg.name;
    newState.regionStart = cfg.region.start;
    newState.regionEnd = cfg.region.end;
    _effectStates.push_back(newState);
    return _effectStates.back();
}

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
            continue; // Bỏ qua nếu đang bị tắt
        if (cfg.inputPin >= 0 && digitalRead(cfg.inputPin) == LOW)
            continue;
        if (cfg.inputWifi && !_wifiTriggered)
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
        else if (cfg.name == "meteor")
            renderMeteor(cfg);
        else if (cfg.name == "wave")
            renderWave(cfg);
        else if (cfg.name == "rainbow")
            renderRainbow(cfg);
        else if (cfg.name == "theaterChase")
            renderTheaterChase(cfg);
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

//=======================================================================
//=======================================================================
//========================  RENDER EFX  =================================
//=======================================================================
//=======================================================================
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
    fx.times = times * 2; // on + off
    fx.delayMs = delayMs;
    fx.count = 0;
    fx.isOn = false;
    fx.lastMillis = millis();
    fx.active = true;
    _overlays.push_back(fx);
}

void LedDriver::renderFlash(const EffectConfig &cfg)
{
    static bool on = false;
    static unsigned long lastTime = 0;
    if (millis() - lastTime >= cfg.speed * 10)
    {
        on = !on;
        lastTime = millis();
    }
    for (uint16_t i = cfg.region.start; i <= cfg.region.end && i < _count; i++)
    {
        _strip->setPixelColor(i, on ? scaleColor(cfg.r, cfg.g, cfg.b) : 0);
    }
}

void LedDriver::renderBreathing(const EffectConfig &cfg)
{
    static float phase = 0;
    phase += 0.05;
    float brightness = (sin(phase) + 1.0) / 2.0;
    for (uint16_t i = cfg.region.start; i <= cfg.region.end && i < _count; i++)
    {
        _strip->setPixelColor(i, scaleColor(cfg.r * brightness, cfg.g * brightness, cfg.b * brightness));
    }
}

void LedDriver::renderRandom(const EffectConfig &cfg)
{
    for (uint16_t i = cfg.region.start; i <= cfg.region.end && i < _count; i++)
    {
        _strip->setPixelColor(i, random(2) ? scaleColor(cfg.r, cfg.g, cfg.b) : 0);
    }
    delay(cfg.speed * 2);
}

void LedDriver::renderTwinkle(const EffectConfig &cfg)
{
    for (uint16_t i = cfg.region.start; i <= cfg.region.end && i < _count; i++)
    {
        if (random(10) > 8)
        {
            _strip->setPixelColor(i, scaleColor(cfg.r, cfg.g, cfg.b));
        }
    }
    delay(cfg.speed * 2);
}

void LedDriver::renderGradient(const EffectConfig &cfg)
{
    uint16_t len = cfg.region.end - cfg.region.start + 1;
    for (uint16_t i = 0; i < len && cfg.region.start + i < _count; i++)
    {
        float t = (float)i / len;
        uint8_t r = cfg.r * t;
        uint8_t g = cfg.g * (1 - t);
        uint8_t b = cfg.b * t;
        _strip->setPixelColor(cfg.region.start + i, scaleColor(r, g, b));
    }
}

void LedDriver::renderBolide(const EffectConfig &cfg)
{
    static uint16_t pos = 0;
    static unsigned long lastTime = 0;
    if (millis() - lastTime < cfg.speed * 2)
        return;
    lastTime = millis();
    pos = (pos + 1) % (_count - 1);
    for (uint16_t i = cfg.region.start; i <= cfg.region.end && i < _count; i++)
    {
        _strip->setPixelColor(i, 0); // clear
    }
    if (cfg.region.start + pos < _count)
        _strip->setPixelColor(cfg.region.start + pos, scaleColor(cfg.r, cfg.g, cfg.b));
}

void LedDriver::renderMeteor(const EffectConfig &cfg)
{
    static int pos = 0;
    static unsigned long lastTime = 0;
    if (millis() - lastTime < cfg.speed)
        return;
    lastTime = millis();

    for (uint16_t i = cfg.region.start; i <= cfg.region.end; i++)
    {
        _strip->setPixelColor(i, 0);
    }

    for (int i = 0; i < cfg.size; i++)
    {
        if ((pos - i) >= cfg.region.start && (pos - i) <= cfg.region.end)
        {
            uint8_t brightness = 255 - (255 / cfg.size) * i;
            _strip->setPixelColor(pos - i, scaleColor(cfg.r * brightness / 255, cfg.g * brightness / 255, cfg.b * brightness / 255));
        }
    }

    pos--;
    if (pos < cfg.region.start)
        pos = cfg.region.end;
}

void LedDriver::renderWave(const EffectConfig &cfg)
{
    static uint16_t offset = 0;
    for (uint16_t i = cfg.region.start; i <= cfg.region.end; i++)
    {
        uint8_t brightness = (sin((i + offset) * 0.3) + 1) * 127;
        _strip->setPixelColor(i, scaleColor(cfg.r * brightness / 255, cfg.g * brightness / 255, cfg.b * brightness / 255));
    }
    offset++;
    delay(cfg.speed);
}

uint32_t LedDriver::Wheel(byte WheelPos)
{
    WheelPos = 255 - WheelPos;
    if (WheelPos < 85)
    {
        return scaleColor(255 - WheelPos * 3, 0, WheelPos * 3);
    }
    if (WheelPos < 170)
    {
        WheelPos -= 85;
        return scaleColor(0, WheelPos * 3, 255 - WheelPos * 3);
    }
    WheelPos -= 170;
    return scaleColor(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void LedDriver::renderRainbow(const EffectConfig &cfg)
{
    static uint16_t offset = 0;
    for (uint16_t i = cfg.region.start; i <= cfg.region.end; i++)
    {
        _strip->setPixelColor(i, Wheel((i + offset) & 255));
    }
    offset++;
    delay(cfg.speed);
}
void LedDriver::renderTheaterChase(const EffectConfig &cfg)
{
    static int j = 0;
    static unsigned long lastTime = 0;
    if (millis() - lastTime < cfg.speed)
        return;
    lastTime = millis();
    for (uint16_t i = cfg.region.start; i <= cfg.region.end; i++)
    {
        if ((i + j) % 3 == 0)
        {
            _strip->setPixelColor(i, scaleColor(cfg.r, cfg.g, cfg.b));
        }
        else
        {
            _strip->setPixelColor(i, 0);
        }
    }
    j = (j + 1) % 3;
}

void LedDriver::renderOverlay()
{
    unsigned long now = millis();
    for (auto &fx : _overlays)
    {
        if (!fx.active)
            continue;
        if (now - fx.lastMillis >= fx.delayMs)
        {
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
                                   [](const OverlayEffect &fx)
                                   { return !fx.active; }),
                    _overlays.end());
}

bool LedDriver::applyEffectByName(const String &name)
{
    for (const auto &cfg : _effects)
    {
        if (cfg.name.equalsIgnoreCase(name))
        {
            clearEffects(); // Xóa hết hiệu ứng cũ
            addEffect(cfg); // Thêm lại hiệu ứng được chọn
            return true;
        }
    }
    return false;
}
