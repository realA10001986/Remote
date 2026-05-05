#include "../../remote_global.h"

#ifdef HAVE_CRSF

#include <Arduino.h>
#include <Wire.h>

#include "elrs_crsf.h"
#include "crsf_settings.h"

namespace {

constexpr uint8_t CRSF_RX_PIN = 34;
constexpr uint8_t CRSF_TX_PIN = 2;
constexpr uint8_t CRSF_OE_PIN = 0;
constexpr uint8_t ADS1015_ADDR = 0x48;
constexpr uint8_t ADS_REG_CONVERT = 0x00;
constexpr uint8_t ADS_REG_CONFIG = 0x01;
constexpr int16_t ADS_LOG_DELTA_THRESHOLD = 20;
constexpr uint8_t ADS_FILTER_SHIFT = 2;

}

ELRSCrsfMode elrsMode;

ELRSCrsfMode::ELRSCrsfMode() : _serial(1)
{
}

bool ELRSCrsfMode::begin(
    uint16_t packetRateHz,
    uint8_t speedDisplayUnits,
    uint8_t telemetryRatio,
    uint8_t maxPower,
    uint8_t dynamicPower,
    const ELRSInputAxisProfile *axisProfiles,
    const ELRSGimbalRouting &inputRouting,
    ButtonPack *buttonPack,
    bool haveButtonPack,
    remDisplay *display,
    remLED *powerLed,
    remLED *levelMeter,
    remLED *stopLed,
    bool usePowerLed,
    bool useLevelMeter,
    bool powerLedOnFakePower,
    bool levelMeterOnFakePower,
    void (*fpOnWifiHandler)(bool))
{
    ELRSCrsfCoreConfig config;

    _buttonPack = buttonPack;
    _haveButtonPack = haveButtonPack;
    _display = display;
    _powerLed = powerLed;
    _levelMeter = levelMeter;
    _stopLed = stopLed;
    _usePowerLed = usePowerLed;
    _useLevelMeter = useLevelMeter;
    _powerLedOnFakePower = powerLedOnFakePower;
    _levelMeterOnFakePower = levelMeterOnFakePower;
    _haveAds = false;
    _oeActiveLow = true;
    _haveLoggedAxes = false;
    _haveFilteredAxes = false;
    for(int i = 0; i < ELRS_GIMBAL_AXIS_COUNT; i++) {
        _rawAxes[i] = 1024;
        _filteredAxes[i] = 1024;
        _lastLoggedAxes[i] = 1024;
    }

    _fpOnWifiHandler = fpOnWifiHandler;

    pinMode(FPOWER_IO_PIN, INPUT_PULLUP);
    pinMode(STOPS_IO_PIN, INPUT);
    pinMode(CALIBB_IO_PIN, INPUT_PULLUP);
    pinMode(BUTA_IO_PIN, INPUT_PULLUP);
    pinMode(BUTB_IO_PIN, INPUT_PULLUP);

    digitalWrite(CRSF_OE_PIN, _oeActiveLow ? HIGH : LOW);
    pinMode(CRSF_OE_PIN, OUTPUT);
    setDriverEnabled(false);

    _haveAds = initAds1015();

    config.haveButtonPack = _haveButtonPack;
    config.usePowerLed = _usePowerLed;
    config.useLevelMeter = _useLevelMeter;
    config.powerLedOnFakePower = _powerLedOnFakePower;
    config.levelMeterOnFakePower = _levelMeterOnFakePower;
    config.speedDisplayUnits = elrsSpeedUnitsOrDefault(speedDisplayUnits);
    config.telemetryRatio = elrsTelemetryRatioOrDefault(telemetryRatio);
    config.maxPower = elrsMaxPowerOrDefault(maxPower);
    config.dynamicPower = elrsDynamicPowerOrDefault(dynamicPower);
    for(int i = 0; i < ELRS_GIMBAL_AXIS_COUNT; i++) {
        if(axisProfiles) {
            config.axisProfiles[i] = elrsSanitizeInputAxisProfile(axisProfiles[i]);
        } else {
            config.axisProfiles[i] = elrsDefaultInputAxisProfile();
        }
    }
    config.inputRouting = elrsSanitizeGimbalRouting(inputRouting);
    config.transport.baudRate = elrsCrsfRecommendedBaudRate(packetRateHz);
    config.transport.invertLine = false;
    config.transport.packetRateHz = packetRateHz;
    config.transport.frameIntervalMs = (uint16_t)((1000UL + elrsPacketRateOrDefault(config.transport.packetRateHz) - 1) /
                                                  elrsPacketRateOrDefault(config.transport.packetRateHz));
    config.transport.telemetryTimeoutMs = 2000;
    config.transport.replyTimeoutMs = elrsCrsfModuleReplyTimeoutMs(packetRateHz);
    #ifdef REMOTE_DBG
    config.transport.debugEnabled = true;
    #else
    config.transport.debugEnabled = false;
    #endif
    #ifdef REMOTE_DBG_CRSF_RAW
    config.transport.rawFrameDebugEnabled = true;
    #else
    config.transport.rawFrameDebugEnabled = false;
    #endif
    config.transport.oeActiveLow = _oeActiveLow;

    return _core.begin(*this, config, millis(), micros());
}

void ELRSCrsfMode::loop(int battWarn)
{
    _core.loop(*this, millis(), micros(), battWarn);
}

bool ELRSCrsfMode::isCalibrating() const
{
    return _core.isCalibrating();
}

bool ELRSCrsfMode::fakePowerOn() const
{
    return _core.fakePowerOn();
}

ELRSCrsfStatus ELRSCrsfMode::getStatus() const
{
    return _core.getStatus();
}

bool ELRSCrsfMode::readCurrentRawAxes(int16_t axes[ELRS_GIMBAL_AXIS_COUNT])
{
    if(!axes) {
        return false;
    }

    if(!_haveAds) {
        _haveAds = initAds1015();
    }
    if(!_haveAds) {
        return false;
    }

    for(int i = 0; i < ELRS_GIMBAL_AXIS_COUNT; i++) {
        axes[i] = readAdsChannel(i);
    }

    return true;
}

bool ELRSCrsfMode::initAds1015()
{
    Wire.beginTransmission(ADS1015_ADDR);
    bool ok = (Wire.endTransmission(true) == 0);
    #ifdef REMOTE_DBG
    Serial.printf("ELRS/CRSF ADC: ADS1015 probe %s @0x%02X\n", ok ? "ok" : "failed", ADS1015_ADDR);
    #endif
    return ok;
}

int16_t ELRSCrsfMode::readAdsChannel(uint8_t channel)
{
    uint8_t cfg[3];
    uint8_t raw[2];
    int value = 0;

    if(channel >= ELRS_GIMBAL_AXIS_COUNT) {
        return 1024;
    }

    cfg[0] = ADS_REG_CONFIG;
    cfg[1] = elrsAds1015SingleEndedConfigHighByte(channel);
    cfg[2] = 0xE3;

    Wire.beginTransmission(ADS1015_ADDR);
    Wire.write(cfg[0]);
    Wire.write(cfg[1]);
    Wire.write(cfg[2]);
    if(Wire.endTransmission(true)) {
        return _rawAxes[channel];
    }

    delayMicroseconds(500);

    Wire.beginTransmission(ADS1015_ADDR);
    Wire.write(ADS_REG_CONVERT);
    if(Wire.endTransmission(false)) {
        return _rawAxes[channel];
    }

    if(Wire.requestFrom((uint8_t)ADS1015_ADDR, (uint8_t)2) != 2) {
        return _rawAxes[channel];
    }

    raw[0] = Wire.read();
    raw[1] = Wire.read();

    value = ((int16_t)((raw[0] << 8) | raw[1])) >> 4;
    if(value < 0) {
        value = 0;
    }

    return (int16_t)value;
}

void ELRSCrsfMode::logMessage(const char *message)
{
    Serial.println(message);
}

void ELRSCrsfMode::startSerial(uint32_t baud, bool invert)
{
    _serial.end();
    _serial.begin((unsigned long)baud, SERIAL_8N1, CRSF_RX_PIN, CRSF_TX_PIN, invert);
    setDriverEnabled(false);
    discardSerialInput();
}

void ELRSCrsfMode::stopSerial()
{
    _serial.end();
}

int ELRSCrsfMode::serialAvailable()
{
    return _serial.available();
}

int ELRSCrsfMode::serialRead()
{
    return _serial.read();
}

size_t ELRSCrsfMode::serialWrite(const uint8_t *data, size_t len)
{
    return _serial.write(data, len);
}

void ELRSCrsfMode::serialFlush()
{
    _serial.flush();
}

void ELRSCrsfMode::setDriverEnabled(bool enabled)
{
    if(!enabled) {
        delayMicroseconds(elrsCrsfDriverDisableHoldUs());
    }

    bool level = enabled ? !_oeActiveLow : _oeActiveLow;
    digitalWrite(CRSF_OE_PIN, level ? HIGH : LOW);
    delayMicroseconds(enabled ? elrsCrsfDriverEnableSetupUs() : elrsCrsfDriverReleaseGuardUs());
}

void ELRSCrsfMode::discardSerialInput()
{
    while(_serial.available()) {
        _serial.read();
    }
}

unsigned long ELRSCrsfMode::microsNow()
{
    return micros();
}

bool ELRSCrsfMode::sampleAxes(int16_t axes[ELRS_GIMBAL_AXIS_COUNT])
{
    int16_t sample;

    if(!_haveAds) {
        return false;
    }

    for(int i = 0; i < ELRS_GIMBAL_AXIS_COUNT; i++) {
        sample = readAdsChannel(i);
        if(!_haveFilteredAxes) {
            _filteredAxes[i] = sample;
        } else {
            _filteredAxes[i] = elrsIirFilterStep(_filteredAxes[i], sample, ADS_FILTER_SHIFT);
        }
        _rawAxes[i] = _filteredAxes[i];
        axes[i] = _filteredAxes[i];
    }
    _haveFilteredAxes = true;

    #ifdef REMOTE_DBG
    if(!_haveLoggedAxes || elrsAxesChanged(_rawAxes, _lastLoggedAxes, ELRS_GIMBAL_AXIS_COUNT, ADS_LOG_DELTA_THRESHOLD)) {
        memcpy(_lastLoggedAxes, _rawAxes, sizeof(_lastLoggedAxes));
        _haveLoggedAxes = true;
        Serial.printf("ELRS/CRSF ADC raw: A0=%d A1=%d A2=%d A3=%d\n",
                      _rawAxes[0], _rawAxes[1], _rawAxes[2], _rawAxes[3]);
    }
    #endif

    return true;
}

bool ELRSCrsfMode::readFakePowerSwitch()
{
    return (digitalRead(FPOWER_IO_PIN) == LOW);
}

bool ELRSCrsfMode::readStopSwitch()
{
    return (digitalRead(STOPS_IO_PIN) == HIGH);
}

bool ELRSCrsfMode::readButtonA()
{
    return (digitalRead(BUTA_IO_PIN) == LOW);
}

bool ELRSCrsfMode::readButtonB()
{
    return (digitalRead(BUTB_IO_PIN) == LOW);
}

bool ELRSCrsfMode::readCalibrationButton()
{
    return (digitalRead(CALIBB_IO_PIN) == LOW);
}

bool ELRSCrsfMode::samplePackStates(uint8_t &states)
{
    if(!_haveButtonPack || !_buttonPack) {
        states = 0;
        return false;
    }

    return _buttonPack->sampleStates(states);
}

void ELRSCrsfMode::displayOn()
{
    if(_display) {
        _display->on();
    }
}

void ELRSCrsfMode::displaySetText(const char *text)
{
    if(_display) {
        _display->setText(text);
    }
}

void ELRSCrsfMode::displaySetSpeed(int speed)
{
    if(_display) {
        _display->setSpeed(speed);
    }
}

void ELRSCrsfMode::displayShow()
{
    if(_display) {
        _display->show();
    }
}

void ELRSCrsfMode::setPowerLed(bool state)
{
    if(_powerLed) {
        _powerLed->setState(state);
    }
}

bool ELRSCrsfMode::getPowerLed() const
{
    return _powerLed ? _powerLed->getState() : false;
}

void ELRSCrsfMode::setLevelMeter(bool state)
{
    if(_levelMeter) {
        _levelMeter->setState(state);
    }
}

bool ELRSCrsfMode::getLevelMeter() const
{
    return _levelMeter ? _levelMeter->getState() : false;
}

void ELRSCrsfMode::setStopLed(bool state)
{
    if(_stopLed) {
        _stopLed->setState(state);
    }
}

void ELRSCrsfMode::loadCalibration(ELRSAxisCalibrationData *cal, int count)
{
    loadELRSCalibration(cal, count);
}

void ELRSCrsfMode::saveCalibration(const ELRSAxisCalibrationData *cal, int count)
{
    saveELRSCalibration(cal, count);
}

#endif
