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
    config.transport.baudRate = 400000;
    config.transport.invertLine = false;
    config.transport.packetRateHz = packetRateHz;
    config.transport.frameIntervalMs = (uint16_t)((1000UL + elrsPacketRateOrDefault(config.transport.packetRateHz) - 1) /
                                                  elrsPacketRateOrDefault(config.transport.packetRateHz));
    config.transport.telemetryTimeoutMs = 2000;
    config.transport.replyTimeoutMs = 20;
    #ifdef REMOTE_DBG
    config.transport.debugEnabled = true;
    #else
    config.transport.debugEnabled = false;
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

bool ELRSCrsfMode::initAds1015()
{
    Wire.beginTransmission(ADS1015_ADDR);
    return (Wire.endTransmission(true) == 0);
}

int16_t ELRSCrsfMode::readAdsChannel(uint8_t channel)
{
    static const uint8_t muxBits[ELRS_GIMBAL_AXIS_COUNT] = { 0x04, 0x05, 0x06, 0x07 };
    uint8_t cfg[3];
    uint8_t raw[2];
    int value = 0;

    if(channel >= ELRS_GIMBAL_AXIS_COUNT) {
        return 1024;
    }

    cfg[0] = ADS_REG_CONFIG;
    cfg[1] = 0x80 | (muxBits[channel] << 4) | 0x04 | 0x01;
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
    bool level = enabled ? !_oeActiveLow : _oeActiveLow;
    digitalWrite(CRSF_OE_PIN, level ? HIGH : LOW);
}

void ELRSCrsfMode::discardSerialInput()
{
    while(_serial.available()) {
        _serial.read();
    }
}

bool ELRSCrsfMode::sampleAxes(int16_t axes[ELRS_GIMBAL_AXIS_COUNT])
{
    if(!_haveAds) {
        return false;
    }

    for(int i = 0; i < ELRS_GIMBAL_AXIS_COUNT; i++) {
        _rawAxes[i] = readAdsChannel(i);
        axes[i] = _rawAxes[i];
    }

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
