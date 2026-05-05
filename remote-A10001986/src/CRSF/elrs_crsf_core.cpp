#include "../../remote_global.h"

#ifdef HAVE_CRSF

#include "elrs_crsf_core.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace {

constexpr uint16_t CRSF_CHANNEL_MIN = 172;
constexpr uint16_t CRSF_CHANNEL_MID = 992;
constexpr uint16_t CRSF_CHANNEL_MAX = 1811;

constexpr uint8_t CRSF_FRAME_GPS = 0x02;
constexpr uint8_t CRSF_FRAME_BATTERY = 0x08;
constexpr uint8_t CRSF_FRAME_AIRSPEED = 0x0A;
constexpr uint8_t CRSF_FRAME_LINK_STATS = 0x14;
constexpr uint8_t CRSF_FRAME_DEVICE_PING = 0x28;
constexpr uint8_t CRSF_FRAME_DEVICE_INFO = 0x29;
constexpr uint8_t CRSF_FRAME_PARAMETER_SETTINGS_ENTRY = 0x2B;
constexpr uint8_t CRSF_FRAME_PARAMETER_READ = 0x2C;
constexpr uint8_t CRSF_FRAME_PARAMETER_WRITE = 0x2D;
constexpr uint8_t CRSF_SYNC_BYTE = 0xC8;
constexpr uint8_t CRSF_EXT_ADDR_BROADCAST = 0x00;
constexpr uint8_t CRSF_EXT_ADDR_RADIO_TRANSMITTER = 0xEA;
constexpr uint8_t CRSF_EXT_ADDR_HANDSET = 0xEF;
constexpr uint8_t CRSF_EXT_ADDR_CRSF_TRANSMITTER = 0xEE;
constexpr uint8_t CRSF_PARAM_TEXT_SELECTION = 0x09;

constexpr unsigned long CRSF_DISPLAY_INTERVAL = 200;
constexpr unsigned long CRSF_INPUT_STALE_MS = 100;
constexpr unsigned long CRSF_COMM_OVERLAY_MS = 1000;
constexpr unsigned long CRSF_COMM_NSY_OVERLAY_MS = 1500;
constexpr unsigned long CRSF_MODULE_CONFIG_START_DELAY_MS = 1000;
constexpr unsigned long CRSF_MODULE_CONFIG_REPLY_TIMEOUT_MS = 500;
constexpr unsigned long CRSF_MODULE_CONFIG_WRITE_DELAY_MS = 300;
constexpr unsigned long CRSF_MODULE_CONFIG_PROBE_RETRY_DELAY_MS = 1000;
constexpr unsigned long CRSF_MODULE_CONFIG_RETRY_DELAY_MS = 10000;
constexpr uint8_t CRSF_MODULE_CONFIG_MAX_PROBE_RETRIES = 2;
constexpr uint8_t CRSF_MODULE_CONFIG_MAX_PARAMETER_RETRIES = 2;
constexpr unsigned long BATTERY_BANNER_INTERVAL = 30000;
constexpr unsigned long BATTERY_BANNER_DURATION = 1000;
constexpr size_t CRSF_DEVICE_INFO_TRAILER_LEN = 14;

constexpr uint8_t AXIS_AILERON = 0;
constexpr uint8_t AXIS_ELEVATOR = 1;
constexpr uint8_t AXIS_RUDDER = 2;
constexpr uint8_t AXIS_THROTTLE = 3;

}

ELRSCrsfCore::ELRSCrsfCore()
{
    const ELRSInputAxisProfile defaultProfile = elrsDefaultInputAxisProfile();

    for(int i = 0; i < 16; i++) {
        _channels[i] = CRSF_CHANNEL_MID;
    }

    for(int i = 0; i < ELRS_GIMBAL_AXIS_COUNT; i++) {
        _rawAxes[i] = 1024;
        _axisCal[i].minimum = 0;
        _axisCal[i].center = 1024;
        _axisCal[i].maximum = 2047;
        _axisProfiles[i] = defaultProfile;
    }
    _inputRouting = elrsDefaultGimbalRouting();

    memset(_overlayText, 0, sizeof(_overlayText));
    memset(_commOverlayText, 0, sizeof(_commOverlayText));
}

bool ELRSCrsfCore::begin(ELRSCrsfHost &host, const ELRSCrsfCoreConfig &config, unsigned long now)
{
    return begin(host, config, now, now * 1000UL);
}

bool ELRSCrsfCore::begin(ELRSCrsfHost &host, const ELRSCrsfCoreConfig &config, unsigned long now, unsigned long nowUs)
{
    _config = config;
    _logHost = &host;
    _config.transport.packetRateHz = elrsPacketRateOrDefault(_config.transport.packetRateHz);
    _config.speedDisplayUnits = elrsSpeedUnitsOrDefault(_config.speedDisplayUnits);
    _config.telemetryRatio = elrsTelemetryRatioOrDefault(_config.telemetryRatio);
    _config.maxPower = elrsMaxPowerOrDefault(_config.maxPower);
    _config.dynamicPower = elrsDynamicPowerOrDefault(_config.dynamicPower);
    for(int i = 0; i < ELRS_GIMBAL_AXIS_COUNT; i++) {
        _axisProfiles[i] = elrsSanitizeInputAxisProfile(_config.axisProfiles[i]);
        _config.axisProfiles[i] = _axisProfiles[i];
    }
    _inputRouting = elrsSanitizeGimbalRouting(_config.inputRouting);
    _config.inputRouting = _inputRouting;
    _transport = ELRSCrsfTransport();
    _transport.setSink(this);

    _startedAt = now;
    _selfTestUntil = 0;
    _lastAxisAttemptAt = 0;
    _lastGoodAxesAt = 0;
    _lastGoodPackAt = 0;
    _lastDisplay = 0;
    _lastTelemetry = 0;
    _lastLinkStats = 0;
    _lastGpsSpeed = 0;
    _lastAirspeed = 0;
    _batteryBlinkAt = 0;
    _batteryBannerAt = 0;
    _overlayUntil = 0;
    _commOverlayUntil = 0;
    _linkQuality = 0;
    _remoteBattery = 0;
    _remoteBatteryVoltage = 0.0f;
    _faultFlags = ELRS_FAULT_NONE;
    _gpsSpeed10 = 0;
    _airspeed10 = 0;
    _activeSpeedSource = SPEED_SOURCE_NONE;
    _haveAds = false;
    _fakePowerOn = false;
    _selfTestActive = false;
    _hasValidPackState = false;
    _lastPackStates = 0;
    _adcFaultActive = false;
    _buttonPackFaultActive = false;
    _lastCommCode = ELRS_COMM_NONE;
    _lastReplyActive = false;
    _calibRaw = false;
    _calibPressed = false;
    _calibLongSent = false;
    _calibDebounceAt = 0;
    _calibPressedAt = 0;
    _calStage = CAL_IDLE;
    memset(_overlayText, 0, sizeof(_overlayText));
    memset(_commOverlayText, 0, sizeof(_commOverlayText));
    resetModuleConfigSession();
    _moduleConfigPending = true;
    _moduleConfigState = MODULECFG_WAIT_START;
    _moduleConfigNextAt = now + CRSF_MODULE_CONFIG_START_DELAY_MS;
    _moduleConfigDeadlineAt = 0;

    host.loadCalibration(_axisCal, ELRS_GIMBAL_AXIS_COUNT);

    log(host, "ELRS/CRSF: basic external module mode");
    log(host, "ELRS/CRSF: full ELRS Lua/config menu unsupported");
    log(host, "ELRS/CRSF: TX=GPIO2 RX=GPIO34 OE=GPIO0");
    logf(host, "ELRS/CRSF: packet rate %uHz (module must match externally)", (unsigned)_config.transport.packetRateHz);
    logf(host, "ELRS/CRSF: desired Telem Ratio=%s Max Power=%umW Dynamic=%s",
         elrsTelemetryRatioLabel(_config.telemetryRatio),
         (unsigned)elrsMaxPowerMilliwatts(_config.maxPower),
         elrsDynamicPowerLabel(_config.dynamicPower));
    logf(host,
         "ELRS/CRSF: gimbals Aileron CH%u Elevator CH%u Throttle CH%u Rudder CH%u; fixed inputs fill unclaimed channels",
         (unsigned)_inputRouting.aileronChannel,
         (unsigned)_inputRouting.elevatorChannel,
         (unsigned)_inputRouting.throttleChannel,
         (unsigned)_inputRouting.rudderChannel);
    log(host, "ELRS/CRSF: display GPS, airspeed, then LQ");

    sampleAxes(host, now, true);

    for(int i = 0; i < 16; i++) {
        _channels[i] = CRSF_CHANNEL_MID;
    }

    _fakePowerOn = host.readFakePowerSwitch();
    applyIdleOutputs(host, _fakePowerOn);
    host.setStopLed(false);

    updateChannels(now, _fakePowerOn, host.readStopSwitch(), host.readButtonA(), host.readButtonB(), samplePackStates(host, now));

    _transport.begin(host, _config.transport, now, nowUs);

    showOverlay("ELR", now, 1000);
    if(!_haveAds) {
        _faultFlags |= ELRS_FAULT_ADC_MISSING;
        _adcFaultActive = true;
        updateChannels(now, _fakePowerOn, host.readStopSwitch(), host.readButtonA(), host.readButtonB(), _lastPackStates);
        log(host, "ELRS/CRSF: ADC missing");
        showOverlay("ADC", now, 1000);
    }
    updateInputFaults(host, now);

    return true;
}

void ELRSCrsfCore::loop(ELRSCrsfHost &host, unsigned long now, int battWarn)
{
    loop(host, now, now * 1000UL, battWarn);
}

void ELRSCrsfCore::loop(ELRSCrsfHost &host, unsigned long now, unsigned long nowUs, int battWarn)
{
    bool fakePower = host.readFakePowerSwitch();
    bool stopOn = host.readStopSwitch();
    bool buttonAOn = host.readButtonA();
    bool buttonBOn = host.readButtonB();
    uint8_t packStates = samplePackStates(host, now);

    _fakePowerOn = fakePower;
    host.setStopLed(stopOn);

    sampleAxes(host, now);
    updateInputFaults(host, now);

    if(_selfTestActive && _selfTestUntil && now >= _selfTestUntil) {
        stopSelfTest();
        log(host, "ELRS/CRSF: self-test stopped");
    }

    if(!_selfTestActive) {
        updateCalibrationButton(host, now, battWarn);
    }

    updateChannels(now, fakePower, stopOn, buttonAOn, buttonBOn, packStates);
#ifdef REMOTE_DBG
    static unsigned long lastMapLogAt = 0;
    if(now - lastMapLogAt >= 200) {
        lastMapLogAt = now;
        logf(host,
             "ELRS/CRSF map: raw=[%d,%d,%d,%d] ch1=%u ch2=%u ch3=%u ch4=%u faults=0x%02X",
             _rawAxes[0], _rawAxes[1], _rawAxes[2], _rawAxes[3],
             (unsigned)_channels[0], (unsigned)_channels[1],
             (unsigned)_channels[2], (unsigned)_channels[3],
             (unsigned)_faultFlags);
    }
#endif
    _transport.setChannels(_channels);
    _transport.loop(host, now, nowUs);

    // Module config runs after the transport loop; service frames queued here
    // are transmitted on the next scheduler pass.
    if(_moduleConfigPending || _moduleConfigState == MODULECFG_BACKOFF) {
        updateModuleConfig(host, now);
    }

    updateBatteryWarning(host, now, battWarn, fakePower);
    updateBenchState(host, now);
    updateDisplay(host, now, battWarn);
}

void ELRSCrsfCore::startSelfTest(unsigned long now, unsigned long durationMs)
{
    _selfTestActive = true;
    _selfTestUntil = now + durationMs;
}

void ELRSCrsfCore::stopSelfTest()
{
    _selfTestActive = false;
    _selfTestUntil = 0;
}

bool ELRSCrsfCore::selfTestActive() const
{
    return _selfTestActive;
}

bool ELRSCrsfCore::isCalibrating() const
{
    return (_calStage != CAL_IDLE);
}

bool ELRSCrsfCore::fakePowerOn() const
{
    return _fakePowerOn;
}

uint32_t ELRSCrsfCore::baudRate() const
{
    return _transport.status().baudRate;
}

uint16_t ELRSCrsfCore::channelAt(uint8_t index) const
{
    return (index < 16) ? _channels[index] : 0;
}

uint8_t ELRSCrsfCore::linkQuality() const
{
    return _linkQuality;
}

uint8_t ELRSCrsfCore::remoteBatteryPercent() const
{
    return _remoteBattery;
}

float ELRSCrsfCore::remoteBatteryVoltage() const
{
    return _remoteBatteryVoltage;
}

uint16_t ELRSCrsfCore::gpsSpeed10() const
{
    return _gpsSpeed10;
}

uint16_t ELRSCrsfCore::airspeed10() const
{
    return _airspeed10;
}

bool ELRSCrsfCore::telemetryActive() const
{
    return _transport.status().telemetryActive;
}

bool ELRSCrsfCore::replyActive() const
{
    return _transport.status().replyActive;
}

bool ELRSCrsfCore::synced() const
{
    return _transport.status().synced;
}

ELRSCrsfCore::SpeedSource ELRSCrsfCore::activeSpeedSource() const
{
    return _activeSpeedSource;
}

ELRSCrsfStatus ELRSCrsfCore::getStatus() const
{
    const ELRSCrsfTransportStatus &transportStatus = _transport.status();
    ELRSCrsfStatus status;

    status.baudRate = transportStatus.baudRate;
    status.packetRateHz = transportStatus.packetRateHz;
    status.telemetryActive = transportStatus.telemetryActive;
    status.replyActive = transportStatus.replyActive;
    status.synced = transportStatus.synced;
    status.activeSpeedSource = (uint8_t)_activeSpeedSource;
    status.linkQuality = _linkQuality;
    status.remoteBatteryPercent = _remoteBattery;
    status.remoteBatteryVoltage = _remoteBatteryVoltage;
    status.faultFlags = _faultFlags;
    status.commCode = transportStatus.commCode;
    status.everReplied = transportStatus.everReplied;
    status.everSynced = transportStatus.everSynced;
    status.invertLine = transportStatus.invertLine;
    status.debugEnabled = transportStatus.debugEnabled;
    status.rawFrameDebugEnabled = transportStatus.rawFrameDebugEnabled;
    status.lastTxAt = transportStatus.lastTxAt;
    status.lastReplyAt = transportStatus.lastReplyAt;
    status.lastRxAt = transportStatus.lastRxAt;
    status.lastReplyTimeoutAt = transportStatus.lastReplyTimeoutAt;
    status.lastRawFrameSyncByte = transportStatus.lastRawFrame.syncByte;
    status.lastRawFrameType = transportStatus.lastRawFrame.type;
    status.lastRawFrameLength = transportStatus.lastRawFrame.length;
    status.lastRawFrameCrcValid = transportStatus.lastRawFrame.crcValid;
    status.fakePowerOn = _fakePowerOn;
    status.calibrating = (_calStage != CAL_IDLE);
    status.selfTestActive = _selfTestActive;

    return status;
}

bool ELRSCrsfCore::sampleAxes(ELRSCrsfHost &host, unsigned long now, bool force)
{
    int16_t axes[ELRS_GIMBAL_AXIS_COUNT];
    unsigned long sampleIntervalMs = axisSampleIntervalMs(_config.transport.packetRateHz);

    if(!force && (now - _lastAxisAttemptAt < sampleIntervalMs)) {
        return _haveAds;
    }

    _lastAxisAttemptAt = now;
    if(!host.sampleAxes(axes)) {
        return false;
    }

    for(int i = 0; i < ELRS_GIMBAL_AXIS_COUNT; i++) {
        _rawAxes[i] = axes[i];
    }
    _haveAds = true;
    _lastGoodAxesAt = now;

    return true;
}

uint8_t ELRSCrsfCore::samplePackStates(ELRSCrsfHost &host, unsigned long now)
{
    uint8_t packStates = _hasValidPackState ? _lastPackStates : 0;
    uint8_t sampledStates = 0;

    if(!_config.haveButtonPack) {
        return 0;
    }

    if(host.samplePackStates(sampledStates)) {
        _lastPackStates = sampledStates;
        _lastGoodPackAt = now;
        _hasValidPackState = true;
        packStates = sampledStates;
    }

    return packStates;
}

bool ELRSCrsfCore::onCrsfFrame(uint8_t syncByte, uint8_t type, const uint8_t *payload, size_t payloadLen, unsigned long now)
{
    (void)syncByte;
    bool supportedTelemetry = false;

    switch(type) {
    case CRSF_FRAME_LINK_STATS:
        if(payloadLen >= 10) {
            _linkQuality = payload[2];
            _lastLinkStats = now;
            supportedTelemetry = true;
        }
        break;
    case CRSF_FRAME_BATTERY:
        if(payloadLen >= 8) {
            _remoteBatteryVoltage = (float)readBE16(payload) * 0.00001f;
            _remoteBattery = payload[7];
            supportedTelemetry = true;
        }
        break;
    case CRSF_FRAME_GPS:
        if(payloadLen >= 15) {
            _gpsSpeed10 = readBE16(payload + 8) / 10;
            _lastGpsSpeed = now;
            supportedTelemetry = true;
        }
        break;
    case CRSF_FRAME_AIRSPEED:
        if(payloadLen >= 2) {
            _airspeed10 = readBE16(payload);
            _lastAirspeed = now;
            supportedTelemetry = true;
        }
        break;
    case CRSF_FRAME_DEVICE_INFO:
        handleDeviceInfo(payload, payloadLen, now);
        break;
    case CRSF_FRAME_PARAMETER_SETTINGS_ENTRY:
        handleParameterSettingsEntry(payload, payloadLen, now);
        break;
    default:
        break;
    }

    if(supportedTelemetry) {
        _lastTelemetry = now;
    }

    return supportedTelemetry;
}

void ELRSCrsfCore::updateCalibrationButton(ELRSCrsfHost &host, unsigned long now, int battWarn)
{
    bool raw = host.readCalibrationButton();

    if(raw != _calibRaw) {
        _calibRaw = raw;
        _calibDebounceAt = now;
    }

    if(now - _calibDebounceAt < 50) {
        return;
    }

    if(raw != _calibPressed) {
        _calibPressed = raw;
        if(raw) {
            _calibPressedAt = now;
            _calibLongSent = false;
        } else if(!_calibLongSent) {
            handleCalibrationShort(host, now, battWarn);
        }
    }

    if(_calibPressed && !_calibLongSent && (now - _calibPressedAt >= 2000)) {
        _calibLongSent = true;
        handleCalibrationLong(host, now, battWarn);
    }
}

void ELRSCrsfCore::handleCalibrationShort(ELRSCrsfHost &host, unsigned long now, int battWarn)
{
    if(_fakePowerOn || _selfTestActive) {
        return;
    }

    if(battWarn) {
        showOverlay("BAT", now, BATTERY_BANNER_DURATION);
        return;
    }

    if(_calStage == CAL_IDLE) {
        return;
    }

    sampleAxes(host, now, true);

    switch(_calStage) {
    case CAL_CENTER:
        for(int i = 0; i < ELRS_GIMBAL_AXIS_COUNT; i++) {
            _axisCal[i].center = _rawAxes[i];
        }
        _calStage = CAL_TLOW;
        break;
    case CAL_TLOW:
        _axisCal[AXIS_THROTTLE].minimum = _rawAxes[AXIS_THROTTLE];
        _calStage = CAL_THIGH;
        break;
    case CAL_THIGH:
        _axisCal[AXIS_THROTTLE].maximum = _rawAxes[AXIS_THROTTLE];
        _calStage = CAL_YLOW;
        break;
    case CAL_YLOW:
        _axisCal[AXIS_RUDDER].minimum = _rawAxes[AXIS_RUDDER];
        _calStage = CAL_YHIGH;
        break;
    case CAL_YHIGH:
        _axisCal[AXIS_RUDDER].maximum = _rawAxes[AXIS_RUDDER];
        _calStage = CAL_PLOW;
        break;
    case CAL_PLOW:
        _axisCal[AXIS_ELEVATOR].minimum = _rawAxes[AXIS_ELEVATOR];
        _calStage = CAL_PHIGH;
        break;
    case CAL_PHIGH:
        _axisCal[AXIS_ELEVATOR].maximum = _rawAxes[AXIS_ELEVATOR];
        _calStage = CAL_RLOW;
        break;
    case CAL_RLOW:
        _axisCal[AXIS_AILERON].minimum = _rawAxes[AXIS_AILERON];
        _calStage = CAL_RHIGH;
        break;
    case CAL_RHIGH:
        _axisCal[AXIS_AILERON].maximum = _rawAxes[AXIS_AILERON];
        _calStage = CAL_IDLE;
        for(int i = 0; i < ELRS_GIMBAL_AXIS_COUNT; i++) {
            _axisProfiles[i].minimum = _axisCal[i].minimum;
            _axisProfiles[i].center = _axisCal[i].center;
            _axisProfiles[i].maximum = _axisCal[i].maximum;
        }
        host.saveCalibration(_axisCal, ELRS_GIMBAL_AXIS_COUNT);
        showOverlay("CAL", now, 1000);
        break;
    default:
        _calStage = CAL_IDLE;
        break;
    }
}

void ELRSCrsfCore::handleCalibrationLong(ELRSCrsfHost &host, unsigned long now, int battWarn)
{
    if(_fakePowerOn || _selfTestActive) {
        return;
    }

    if(battWarn) {
        showOverlay("BAT", now, BATTERY_BANNER_DURATION);
        return;
    }

    if(_calStage == CAL_IDLE) {
        sampleAxes(host, now, true);
        _calStage = CAL_CENTER;
    } else {
        _calStage = CAL_IDLE;
        showOverlay("CAN", now, 1000);
    }
}

const char *ELRSCrsfCore::getCalibrationPrompt() const
{
    switch(_calStage) {
    case CAL_CENTER: return "CEN";
    case CAL_TLOW:   return "TLO";
    case CAL_THIGH:  return "THI";
    case CAL_YLOW:   return "YLO";
    case CAL_YHIGH:  return "YHI";
    case CAL_PLOW:   return "PLO";
    case CAL_PHIGH:  return "PHI";
    case CAL_RLOW:   return "RLO";
    case CAL_RHIGH:  return "RHI";
    default:         return "";
    }
}

void ELRSCrsfCore::updateChannels(unsigned long now, bool fakePowerOn, bool stopOn, bool buttonAOn, bool buttonBOn, uint8_t packStates)
{
    resetChannels(CRSF_CHANNEL_MIN);
    if(_selfTestActive) {
        writeGimbalChannels(true);
        writeFixedChannelIfUnclaimed(5, CRSF_CHANNEL_MAX);
        return;
    }

    if(adcFaultActive(now)) {
        writeGimbalChannels(true);
    } else {
        writeGimbalChannels(false);
    }

    writeFixedChannelIfUnclaimed(5, stopOn ? CRSF_CHANNEL_MAX : CRSF_CHANNEL_MIN);
    writeFixedChannelIfUnclaimed(6, fakePowerOn ? CRSF_CHANNEL_MAX : CRSF_CHANNEL_MIN);
    writeFixedChannelIfUnclaimed(7, buttonAOn ? CRSF_CHANNEL_MAX : CRSF_CHANNEL_MIN);
    writeFixedChannelIfUnclaimed(8, buttonBOn ? CRSF_CHANNEL_MAX : CRSF_CHANNEL_MIN);

    for(int i = 0; i < 8; i++) {
        writeFixedChannelIfUnclaimed((uint8_t)(9 + i), (packStates & (1 << i)) ? CRSF_CHANNEL_MAX : CRSF_CHANNEL_MIN);
    }
}

void ELRSCrsfCore::resetChannels(uint16_t defaultTicks)
{
    for(int i = 0; i < 16; i++) {
        _channels[i] = defaultTicks;
    }
}

void ELRSCrsfCore::writeGimbalChannels(bool safeOutputs)
{
    writeGimbalChannel(_inputRouting.aileronChannel, safeOutputs ? safeAxisTicks(AXIS_AILERON) : axisToTicks(AXIS_AILERON));
    writeGimbalChannel(_inputRouting.elevatorChannel, safeOutputs ? safeAxisTicks(AXIS_ELEVATOR) : axisToTicks(AXIS_ELEVATOR));
    writeGimbalChannel(_inputRouting.throttleChannel, safeOutputs ? safeAxisTicks(AXIS_THROTTLE) : axisToTicks(AXIS_THROTTLE));
    writeGimbalChannel(_inputRouting.rudderChannel, safeOutputs ? safeAxisTicks(AXIS_RUDDER) : axisToTicks(AXIS_RUDDER));
}

void ELRSCrsfCore::writeGimbalChannel(uint8_t channel, uint16_t ticks)
{
    if(channel >= 1 && channel <= 16) {
        _channels[channel - 1] = ticks;
    }
}

void ELRSCrsfCore::writeFixedChannelIfUnclaimed(uint8_t channel, uint16_t ticks)
{
    if(channel >= 1 && channel <= 16 && !channelClaimedByGimbal(channel)) {
        _channels[channel - 1] = ticks;
    }
}

bool ELRSCrsfCore::channelClaimedByGimbal(uint8_t channel) const
{
    return _inputRouting.aileronChannel == channel ||
           _inputRouting.elevatorChannel == channel ||
           _inputRouting.throttleChannel == channel ||
           _inputRouting.rudderChannel == channel;
}

uint16_t ELRSCrsfCore::safeAxisTicks(uint8_t axis) const
{
    return (axis == AXIS_THROTTLE) ? CRSF_CHANNEL_MIN : CRSF_CHANNEL_MID;
}

uint16_t ELRSCrsfCore::axisToTicks(uint8_t axis) const
{
    const ELRSInputAxisProfile &profile = _axisProfiles[axis];

    if(axis >= ELRS_GIMBAL_AXIS_COUNT) {
        return CRSF_CHANNEL_MID;
    }

    return elrsInputUsToCrsfTicks(elrsInputModelAxisToUs(profile, _rawAxes[axis]));
}

void ELRSCrsfCore::applyIdleOutputs(ELRSCrsfHost &host, bool fakePowerOn)
{
    if(_config.usePowerLed) {
        host.setPowerLed(_config.powerLedOnFakePower ? fakePowerOn : !fakePowerOn);
    }

    if(_config.useLevelMeter) {
        host.setLevelMeter(_config.levelMeterOnFakePower ? fakePowerOn : !fakePowerOn);
    }
}

void ELRSCrsfCore::updateBatteryWarning(ELRSCrsfHost &host, unsigned long now, int battWarn, bool fakePowerOn)
{
    if(!battWarn) {
        applyIdleOutputs(host, fakePowerOn);
        return;
    }

    if(_config.useLevelMeter) {
        host.setLevelMeter(false);
    }

    if(_config.usePowerLed) {
        if(now - _batteryBlinkAt >= 1000) {
            _batteryBlinkAt = now;
            host.setPowerLed(!host.getPowerLed());
        }
        return;
    }

    if(now - _batteryBannerAt >= BATTERY_BANNER_INTERVAL) {
        _batteryBannerAt = now;
        showOverlay("BAT", now, BATTERY_BANNER_DURATION);
    }
}

void ELRSCrsfCore::updateInputFaults(ELRSCrsfHost &host, unsigned long now)
{
    bool adcMissing = !_haveAds && (_lastAxisAttemptAt >= _startedAt);
    bool adcStale = _haveAds && (now - _lastGoodAxesAt > CRSF_INPUT_STALE_MS);
    bool adcFault = adcMissing || adcStale;
    bool buttonPackStale = false;

    if(adcMissing) {
        _faultFlags = (uint8_t)((_faultFlags | ELRS_FAULT_ADC_MISSING) & ~ELRS_FAULT_ADC_STALE);
    } else if(adcStale) {
        _faultFlags = (uint8_t)((_faultFlags | ELRS_FAULT_ADC_STALE) & ~ELRS_FAULT_ADC_MISSING);
    } else {
        _faultFlags = (uint8_t)(_faultFlags & ~(ELRS_FAULT_ADC_MISSING | ELRS_FAULT_ADC_STALE));
    }

    if(adcFault != _adcFaultActive) {
        _adcFaultActive = adcFault;
        if(adcFault) {
            log(host, adcMissing ? "ELRS/CRSF: ADC missing" : "ELRS/CRSF: ADC stale");
            showOverlay("ADC", now, 1000);
        } else {
            log(host, "ELRS/CRSF: ADC recovered");
        }
    }

    if(_config.haveButtonPack) {
        buttonPackStale = _hasValidPackState ? (now - _lastGoodPackAt > CRSF_INPUT_STALE_MS)
                                             : (now - _startedAt >= CRSF_INPUT_STALE_MS);
    }

    if(buttonPackStale) {
        _faultFlags |= ELRS_FAULT_BUTTONPACK_STALE;
    } else {
        _faultFlags = (uint8_t)(_faultFlags & ~ELRS_FAULT_BUTTONPACK_STALE);
    }

    if(buttonPackStale != _buttonPackFaultActive) {
        _buttonPackFaultActive = buttonPackStale;
        if(buttonPackStale) {
            log(host, "ELRS/CRSF: button pack stale");
            showOverlay("BPK", now, 1000);
        } else {
            log(host, "ELRS/CRSF: button pack recovered");
        }
    }
}

void ELRSCrsfCore::updateBenchState(ELRSCrsfHost &host, unsigned long now)
{
    const ELRSCrsfTransportStatus &transportStatus = _transport.status();
    SpeedSource speedSource = SPEED_SOURCE_NONE;
    uint8_t commCode = transportStatus.commCode;

    getDisplaySpeed10(now, &speedSource);

    if(commCode != _lastCommCode) {
        if(commCode != ELRS_COMM_NONE) {
            const char *text = commCodeText(commCode);
            unsigned long durationMs = (commCode == ELRS_COMM_NRY) ? CRSF_COMM_NSY_OVERLAY_MS : CRSF_COMM_OVERLAY_MS;
            showCommOverlay(text, now, durationMs);
            logf(host, "ELRS/CRSF: communication state %s", text);
        }
        _lastCommCode = commCode;
    }

    if(speedSource != _activeSpeedSource) {
        _activeSpeedSource = speedSource;
        if(speedSource == SPEED_SOURCE_NONE) {
            if(_lastLinkStats && (now - _lastLinkStats < _config.transport.telemetryTimeoutMs)) {
                log(host, "ELRS/CRSF: no speed telemetry, displaying LQ");
            }
        } else {
            logf(host, "ELRS/CRSF: displaying %s", speedSourceName(speedSource));
        }
    }
}

void ELRSCrsfCore::updateDisplay(ELRSCrsfHost &host, unsigned long now, int battWarn)
{
    char buf[8];
    SpeedSource speedSource = SPEED_SOURCE_NONE;
    uint16_t speed10 = 0;

    if(_overlayUntil > now) {
        host.displayOn();
        host.displaySetText(_overlayText);
        host.displayShow();
        return;
    }

    if(_calStage != CAL_IDLE) {
        host.displayOn();
        host.displaySetText(getCalibrationPrompt());
        host.displayShow();
        return;
    }

    if(battWarn && !_config.usePowerLed && now - _batteryBannerAt < BATTERY_BANNER_DURATION) {
        host.displayOn();
        host.displaySetText("BAT");
        host.displayShow();
        return;
    }

    if(_commOverlayUntil > now) {
        host.displayOn();
        host.displaySetText(_commOverlayText);
        host.displayShow();
        return;
    }

    if(now - _lastDisplay < CRSF_DISPLAY_INTERVAL) {
        return;
    }
    _lastDisplay = now;

    host.displayOn();
    speed10 = getDisplaySpeed10(now, &speedSource);
    if(speedSource != SPEED_SOURCE_NONE) {
        host.displaySetSpeed((int)getDisplaySpeed10ForUnits(speed10));
    } else {
        snprintf(buf, sizeof(buf), "%3d", (_lastLinkStats && (now - _lastLinkStats < _config.transport.telemetryTimeoutMs)) ? _linkQuality : 0);
        host.displaySetText(buf);
    }
    host.displayShow();
}

bool ELRSCrsfCore::hasRecentTelemetry(unsigned long now) const
{
    (void)now;
    return _transport.status().telemetryActive;
}

bool ELRSCrsfCore::adcFaultActive(unsigned long now) const
{
    if(_faultFlags & (ELRS_FAULT_ADC_MISSING | ELRS_FAULT_ADC_STALE)) {
        return true;
    }

    if(!_haveAds) {
        return true;
    }

    return (now - _lastGoodAxesAt > CRSF_INPUT_STALE_MS);
}

bool ELRSCrsfCore::buttonPackFaultActive(unsigned long now) const
{
    if(_faultFlags & ELRS_FAULT_BUTTONPACK_STALE) {
        return true;
    }

    if(!_config.haveButtonPack) {
        return false;
    }

    if(!_hasValidPackState) {
        return (now - _startedAt >= CRSF_INPUT_STALE_MS);
    }

    return (now - _lastGoodPackAt > CRSF_INPUT_STALE_MS);
}

uint16_t ELRSCrsfCore::getDisplaySpeed10(unsigned long now, SpeedSource *source) const
{
    SpeedSource activeSource = SPEED_SOURCE_NONE;
    uint16_t speed10 = 0;

    if(_lastGpsSpeed && (now - _lastGpsSpeed < _config.transport.telemetryTimeoutMs)) {
        activeSource = SPEED_SOURCE_GPS;
        speed10 = _gpsSpeed10;
    } else if(_lastAirspeed && (now - _lastAirspeed < _config.transport.telemetryTimeoutMs)) {
        activeSource = SPEED_SOURCE_AIRSPEED;
        speed10 = _airspeed10;
    }

    if(source) {
        *source = activeSource;
    }

    return speed10;
}

uint16_t ELRSCrsfCore::getDisplaySpeed10ForUnits(uint16_t speed10) const
{
    if(_config.speedDisplayUnits == ELRS_SPEED_UNITS_MPH) {
        return (uint16_t)(((uint32_t)speed10 * 62137UL + 50000UL) / 100000UL);
    }

    return speed10;
}

void ELRSCrsfCore::resetModuleParameters()
{
    _moduleFieldCount = 0;
    _moduleFieldIndex = 0;
    _moduleTargetIndex = 0;
    _moduleWriteFieldId = 0;
    _moduleProbeRetryCount = 0;
    _moduleParameterRetryCount = 0;
    _moduleChunkActive = false;
    _moduleChunkFieldId = 0;
    _moduleChunkNextIndex = 0;
    _moduleChunkLen = 0;
    memset(_moduleChunkData, 0, sizeof(_moduleChunkData));
    _moduleTelemetryRatio = ModuleParameterInfo();
    _moduleMaxPower = ModuleParameterInfo();
    _moduleDynamicPower = ModuleParameterInfo();
    memset(_moduleTelemetryRatio.options, 0, sizeof(_moduleTelemetryRatio.options));
    memset(_moduleMaxPower.options, 0, sizeof(_moduleMaxPower.options));
    memset(_moduleDynamicPower.options, 0, sizeof(_moduleDynamicPower.options));
}

void ELRSCrsfCore::resetModuleConfigSession()
{
    _moduleConfigState = MODULECFG_IDLE;
    _moduleConfigNextAt = 0;
    _moduleConfigDeadlineAt = 0;
    resetModuleParameters();
}

bool ELRSCrsfCore::buildExtendedFrame(uint8_t type, uint8_t destAddr, uint8_t origAddr, const uint8_t *payload, size_t payloadLen, uint8_t *frame, size_t frameSize) const
{
    if(!frame || frameSize < payloadLen + 6 || payloadLen + 4 > 62) {
        return false;
    }

    frame[0] = CRSF_SYNC_BYTE;
    frame[1] = (uint8_t)(payloadLen + 4);
    frame[2] = type;
    frame[3] = destAddr;
    frame[4] = origAddr;
    if(payloadLen) {
        memcpy(frame + 5, payload, payloadLen);
    }
    frame[5 + payloadLen] = crc8D5(frame + 2, payloadLen + 3);

    return true;
}

bool ELRSCrsfCore::queueModulePing()
{
    uint8_t frame[6];

    if(!buildExtendedFrame(CRSF_FRAME_DEVICE_PING, CRSF_EXT_ADDR_BROADCAST, CRSF_EXT_ADDR_RADIO_TRANSMITTER, NULL, 0, frame, sizeof(frame))) {
        return false;
    }

    return _transport.queueServiceFrame(frame, sizeof(frame));
}

bool ELRSCrsfCore::queueParameterRead(uint8_t fieldId, uint8_t chunkIndex)
{
    uint8_t frame[8];
    uint8_t payload[2] = { fieldId, chunkIndex };

    if(!buildExtendedFrame(CRSF_FRAME_PARAMETER_READ, CRSF_EXT_ADDR_CRSF_TRANSMITTER, CRSF_EXT_ADDR_HANDSET, payload, sizeof(payload), frame, sizeof(frame))) {
        return false;
    }

    return _transport.queueServiceFrame(frame, sizeof(frame));
}

bool ELRSCrsfCore::queueParameterWrite(uint8_t fieldId, uint8_t value)
{
    uint8_t frame[8];
    uint8_t payload[2] = { fieldId, value };

    if(!buildExtendedFrame(CRSF_FRAME_PARAMETER_WRITE, CRSF_EXT_ADDR_CRSF_TRANSMITTER, CRSF_EXT_ADDR_HANDSET, payload, sizeof(payload), frame, sizeof(frame))) {
        return false;
    }

    return _transport.queueServiceFrame(frame, sizeof(frame));
}

void ELRSCrsfCore::startModuleConfigSession(unsigned long now)
{
    resetModuleParameters();
    _moduleConfigPending = true;
    _moduleConfigState = MODULECFG_WAIT_START;
    _moduleConfigNextAt = now + CRSF_MODULE_CONFIG_START_DELAY_MS;
    _moduleConfigDeadlineAt = 0;
}

void ELRSCrsfCore::setModuleConfigBackoff(unsigned long now, unsigned long delayMs)
{
    _moduleChunkActive = false;
    _moduleChunkLen = 0;
    _moduleConfigState = MODULECFG_BACKOFF;
    _moduleConfigNextAt = now + delayMs;
    _moduleConfigDeadlineAt = 0;
}

void ELRSCrsfCore::noteModuleConfigResponse()
{
    if(_moduleConfigState == MODULECFG_WAIT_DEVICE_INFO || _moduleConfigState == MODULECFG_WAIT_PARAMETER) {
        _moduleConfigDeadlineAt = 0;
    }
}

void ELRSCrsfCore::handleDeviceInfo(const uint8_t *payload, size_t payloadLen, unsigned long now)
{
    const uint8_t *name;
    size_t nameLen;
    const uint8_t *info;

    if(!payload || payloadLen < 2 + 1 + CRSF_DEVICE_INFO_TRAILER_LEN) {
        return;
    }
    if(payload[1] != CRSF_EXT_ADDR_CRSF_TRANSMITTER) {
        return;
    }

    name = payload + 2;
    nameLen = strnlen((const char *)name, payloadLen - 2);
    if(nameLen >= payloadLen - 2) {
        return;
    }

    info = name + nameLen + 1;
    if((size_t)(info - payload) + CRSF_DEVICE_INFO_TRAILER_LEN > payloadLen) {
        return;
    }

    noteModuleConfigResponse();
    resetModuleParameters();
    _moduleProbeRetryCount = 0;
    _moduleFieldCount = info[12];
    _moduleFieldIndex = 1;
    _moduleTargetIndex = 0;
    debugLogf("ELRS/CRSF resp: device=\"%.*s\" fields=%u",
              (int)nameLen,
              (const char *)name,
              (unsigned)_moduleFieldCount);

    if(!_moduleFieldCount) {
        _moduleConfigPending = false;
        _moduleConfigState = MODULECFG_DONE;
        _moduleConfigNextAt = now;
        return;
    }

    _moduleConfigState = MODULECFG_READ_PARAMETER;
    _moduleConfigNextAt = now;
}

void ELRSCrsfCore::handleParameterSettingsEntry(const uint8_t *payload, size_t payloadLen, unsigned long now)
{
    const uint8_t *chunkData;
    size_t chunkLen;
    uint8_t fieldId;
    uint8_t chunksRemain;

    if(!payload || payloadLen < 4) {
        return;
    }
    if(payload[1] != CRSF_EXT_ADDR_CRSF_TRANSMITTER) {
        return;
    }

    fieldId = payload[2];
    chunksRemain = payload[3];
    chunkData = payload + 4;
    chunkLen = payloadLen - 4;
    if(!fieldId || !chunkLen) {
        return;
    }

    noteModuleConfigResponse();
    _moduleParameterRetryCount = 0;

    if(!_moduleChunkActive || _moduleChunkFieldId != fieldId) {
        _moduleChunkActive = true;
        _moduleChunkFieldId = fieldId;
        _moduleChunkNextIndex = 1;
        _moduleChunkLen = 0;
    }

    if(_moduleChunkLen + chunkLen > sizeof(_moduleChunkData)) {
        _moduleChunkActive = false;
        _moduleChunkLen = 0;
        return;
    }

    memcpy(_moduleChunkData + _moduleChunkLen, chunkData, chunkLen);
    _moduleChunkLen += chunkLen;

    if(chunksRemain) {
        if(_moduleConfigState == MODULECFG_WAIT_PARAMETER &&
           fieldId == _moduleFieldIndex &&
           !_transport.hasPendingServiceFrame() &&
           queueParameterRead(fieldId, _moduleChunkNextIndex)) {
            _moduleChunkNextIndex++;
            _moduleConfigDeadlineAt = now + CRSF_MODULE_CONFIG_REPLY_TIMEOUT_MS;
        }
    } else {
        finishParameterChunk(fieldId, _moduleChunkData, _moduleChunkLen, now);
        _moduleChunkActive = false;
        _moduleChunkNextIndex = 0;
        _moduleChunkLen = 0;
    }
}

void ELRSCrsfCore::finishParameterChunk(uint8_t fieldId, const uint8_t *data, size_t len, unsigned long now)
{
    const char *name;
    const char *options;
    const char *currentOption = NULL;
    size_t nameLen;
    size_t optionsLen;
    size_t currentOptionLen = 0;
    uint8_t type;
    uint8_t currentValue;

    if(!data || len < 4) {
        return;
    }

    type = data[1] & 0x3f;
    name = (const char *)(data + 2);
    nameLen = strnlen(name, len - 2);
    if(nameLen >= len - 2) {
        return;
    }

    if(type == CRSF_PARAM_TEXT_SELECTION) {
        options = name + nameLen + 1;
        if((size_t)(options - (const char *)data) >= len) {
            return;
        }
        optionsLen = strnlen(options, len - (size_t)(options - (const char *)data));
        if((size_t)(options - (const char *)data) + optionsLen + 5 > len) {
            return;
        }
        currentValue = (uint8_t)options[optionsLen + 1];
        if(!readOptionAt(options, currentValue, &currentOption, &currentOptionLen)) {
            currentOption = NULL;
            currentOptionLen = 0;
        }
        debugLogf("ELRS/CRSF resp: field=%u name=\"%s\" current=\"%.*s\"",
                  (unsigned)fieldId,
                  name,
                  (int)currentOptionLen,
                  currentOption ? currentOption : "");
        applyDiscoveredParameter(fieldId, name, type, options, currentValue);
    } else {
        debugLogf("ELRS/CRSF resp: field=%u name=\"%s\" type=%u len=%u",
                  (unsigned)fieldId,
                  name,
                  (unsigned)type,
                  (unsigned)len);
    }

    if(_moduleConfigState == MODULECFG_WAIT_PARAMETER && fieldId == _moduleFieldIndex) {
        _moduleFieldIndex++;
        _moduleParameterRetryCount = 0;
        if(haveAllModuleTargets()) {
            _moduleConfigState = MODULECFG_APPLY_SETTING;
        } else {
            _moduleConfigState = (_moduleFieldIndex <= _moduleFieldCount) ? MODULECFG_READ_PARAMETER : MODULECFG_APPLY_SETTING;
        }
        _moduleConfigNextAt = now;
        _moduleConfigDeadlineAt = 0;
    }
}

void ELRSCrsfCore::applyDiscoveredParameter(uint8_t fieldId, const char *name, uint8_t type, const char *options, uint8_t currentValue)
{
    ModuleParameterInfo *target = NULL;

    if(type != CRSF_PARAM_TEXT_SELECTION || !name || !options) {
        return;
    }

    if(!strcmp(name, "Telem Ratio")) {
        target = &_moduleTelemetryRatio;
    } else if(!strcmp(name, "Max Power")) {
        target = &_moduleMaxPower;
    } else if(!strcmp(name, "Dynamic")) {
        target = &_moduleDynamicPower;
    }

    if(!target) {
        return;
    }

    target->found = true;
    target->fieldId = fieldId;
    target->currentValue = currentValue;
    strncpy(target->options, options, sizeof(target->options) - 1);
    target->options[sizeof(target->options) - 1] = 0;
}

bool ELRSCrsfCore::moduleSettingValueForTarget(uint8_t targetIndex, const char *options, uint8_t *value) const
{
    const char *optionStart = NULL;
    size_t optionLen = 0;

    if(!options || !value) {
        return false;
    }

    for(uint8_t i = 0; readOptionAt(options, i, &optionStart, &optionLen); i++) {
        switch(targetIndex) {
        case 0:
            if(optionEquals(optionStart, optionLen, elrsTelemetryRatioLabel(_config.telemetryRatio))) {
                *value = i;
                return true;
            }
            break;
        case 1: {
            uint16_t moduleMilliwatts = 0;

            if(parseOptionTerminalMilliwatts(optionStart, optionLen, moduleMilliwatts) &&
               moduleMilliwatts == elrsMaxPowerMilliwatts(_config.maxPower)) {
                *value = i;
                return true;
            }
            break;
        }
        case 2:
            if(optionEquals(optionStart, optionLen, elrsDynamicPowerLabel(_config.dynamicPower))) {
                *value = i;
                return true;
            }
            break;
        default:
            return false;
        }
    }

    return false;
}

bool ELRSCrsfCore::haveAllModuleTargets() const
{
    return _moduleTelemetryRatio.found &&
           _moduleMaxPower.found &&
           _moduleDynamicPower.found;
}

void ELRSCrsfCore::updateModuleConfig(ELRSCrsfHost &host, unsigned long now)
{
    const bool replyActive = _transport.status().replyActive;

    if(replyActive != _lastReplyActive) {
        if(!replyActive) {
            _moduleConfigPending = true;
        } else if(_moduleConfigPending &&
                  (_moduleConfigState == MODULECFG_DONE || _moduleConfigState == MODULECFG_BACKOFF || _moduleConfigState == MODULECFG_IDLE)) {
            startModuleConfigSession(now);
            _moduleConfigNextAt = now + 200;
        }
        _lastReplyActive = replyActive;
    }

    if(!_moduleConfigPending && _moduleConfigState != MODULECFG_BACKOFF) {
        return;
    }

    switch(_moduleConfigState) {
    case MODULECFG_WAIT_START:
        if(now < _moduleConfigNextAt || _transport.hasPendingServiceFrame()) {
            return;
        }
        if(queueModulePing()) {
            log(host, "ELRS/CRSF: probing module settings");
            _moduleConfigState = MODULECFG_WAIT_DEVICE_INFO;
            _moduleConfigDeadlineAt = now + CRSF_MODULE_CONFIG_REPLY_TIMEOUT_MS;
        }
        break;

    case MODULECFG_WAIT_DEVICE_INFO:
        if(_moduleConfigDeadlineAt && now >= _moduleConfigDeadlineAt) {
            if(_moduleProbeRetryCount < CRSF_MODULE_CONFIG_MAX_PROBE_RETRIES) {
                _moduleProbeRetryCount++;
                _moduleConfigState = MODULECFG_WAIT_START;
                _moduleConfigNextAt = now + CRSF_MODULE_CONFIG_PROBE_RETRY_DELAY_MS;
                _moduleConfigDeadlineAt = 0;
            } else {
                log(host, "ELRS/CRSF: module settings probe timed out");
                setModuleConfigBackoff(now, CRSF_MODULE_CONFIG_RETRY_DELAY_MS);
            }
        }
        break;

    case MODULECFG_READ_PARAMETER:
        if(!_moduleFieldCount || _moduleFieldIndex > _moduleFieldCount) {
            _moduleConfigState = MODULECFG_APPLY_SETTING;
            _moduleConfigNextAt = now;
            return;
        }
        if(_transport.hasPendingServiceFrame()) {
            return;
        }
        if(queueParameterRead(_moduleFieldIndex, 0)) {
            _moduleChunkNextIndex = 1;
            _moduleParameterRetryCount = 0;
            _moduleConfigState = MODULECFG_WAIT_PARAMETER;
            _moduleConfigDeadlineAt = now + CRSF_MODULE_CONFIG_REPLY_TIMEOUT_MS;
        }
        break;

    case MODULECFG_WAIT_PARAMETER:
        if(_moduleConfigDeadlineAt && now >= _moduleConfigDeadlineAt) {
            const uint8_t retryChunkIndex = (_moduleChunkActive &&
                                             _moduleChunkFieldId == _moduleFieldIndex &&
                                             _moduleChunkNextIndex) ? (uint8_t)(_moduleChunkNextIndex - 1) : 0;

            if(_moduleParameterRetryCount < CRSF_MODULE_CONFIG_MAX_PARAMETER_RETRIES &&
               !_transport.hasPendingServiceFrame() &&
               queueParameterRead(_moduleFieldIndex, retryChunkIndex)) {
                _moduleParameterRetryCount++;
                _moduleConfigDeadlineAt = now + CRSF_MODULE_CONFIG_REPLY_TIMEOUT_MS;
            } else {
                logf(host, "ELRS/CRSF: parameter scan timed out at field %u", (unsigned)_moduleFieldIndex);
                setModuleConfigBackoff(now, CRSF_MODULE_CONFIG_RETRY_DELAY_MS);
            }
        }
        break;

    case MODULECFG_APPLY_SETTING: {
        ModuleParameterInfo *targetInfo = NULL;
        uint8_t desiredValue = 0;
        const char *targetName = moduleConfigTargetName(_moduleTargetIndex);

        if(_moduleTargetIndex >= 3) {
            log(host, "ELRS/CRSF: module settings apply complete");
            _moduleConfigPending = false;
            _moduleConfigState = MODULECFG_DONE;
            return;
        }

        switch(_moduleTargetIndex) {
        case 0: targetInfo = &_moduleTelemetryRatio; break;
        case 1: targetInfo = &_moduleMaxPower; break;
        default: targetInfo = &_moduleDynamicPower; break;
        }

        if(!targetInfo->found) {
            logf(host, "ELRS/CRSF: setting '%s' not found", targetName);
            _moduleTargetIndex++;
            return;
        }

        if(!moduleSettingValueForTarget(_moduleTargetIndex, targetInfo->options, &desiredValue)) {
            logf(host, "ELRS/CRSF: setting '%s' unsupported", targetName);
            _moduleTargetIndex++;
            return;
        }

        if(targetInfo->currentValue == desiredValue) {
            _moduleTargetIndex++;
            return;
        }

        if(_transport.hasPendingServiceFrame()) {
            return;
        }

        if(queueParameterWrite(targetInfo->fieldId, desiredValue)) {
            logf(host, "ELRS/CRSF: applying module setting '%s'", targetName);
            _moduleWriteFieldId = targetInfo->fieldId;
            _moduleConfigState = MODULECFG_WAIT_WRITE;
            _moduleConfigDeadlineAt = now + CRSF_MODULE_CONFIG_WRITE_DELAY_MS;
        }
        break;
    }

    case MODULECFG_WAIT_WRITE:
        if(now >= _moduleConfigDeadlineAt) {
            _moduleTargetIndex++;
            _moduleConfigState = MODULECFG_APPLY_SETTING;
            _moduleConfigNextAt = now;
        }
        break;

    case MODULECFG_BACKOFF:
        if(now >= _moduleConfigNextAt) {
            _moduleConfigState = MODULECFG_WAIT_START;
        }
        break;

    case MODULECFG_DONE:
    case MODULECFG_IDLE:
    default:
        break;
    }
}

void ELRSCrsfCore::showOverlay(const char *text, unsigned long now, unsigned long durationMs)
{
    if(!text) {
        return;
    }

    memset(_overlayText, 0, sizeof(_overlayText));
    strncpy(_overlayText, text, sizeof(_overlayText) - 1);
    _overlayUntil = now + durationMs;
}

void ELRSCrsfCore::showCommOverlay(const char *text, unsigned long now, unsigned long durationMs)
{
    if(!text) {
        return;
    }

    memset(_commOverlayText, 0, sizeof(_commOverlayText));
    strncpy(_commOverlayText, text, sizeof(_commOverlayText) - 1);
    _commOverlayUntil = now + durationMs;
}

void ELRSCrsfCore::log(ELRSCrsfHost &host, const char *message) const
{
    if(message && *message) {
        host.logMessage(message);
    }
}

void ELRSCrsfCore::logf(ELRSCrsfHost &host, const char *fmt, ...) const
{
    char buffer[192];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    host.logMessage(buffer);
}

void ELRSCrsfCore::debugLogf(const char *fmt, ...) const
{
#ifdef REMOTE_DBG
    char buffer[192];
    va_list args;

    if(!_logHost || !fmt || !*fmt) {
        return;
    }

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    _logHost->logMessage(buffer);
#else
    (void)fmt;
#endif
}

uint16_t ELRSCrsfCore::readBE16(const uint8_t *data)
{
    return ((uint16_t)data[0] << 8) | data[1];
}

size_t ELRSCrsfCore::copyToken(char *out, size_t outSize, const char *start, size_t len)
{
    size_t copyLen;

    if(!out || !outSize) {
        return 0;
    }

    copyLen = (len < outSize - 1) ? len : (outSize - 1);
    if(copyLen && start) {
        memcpy(out, start, copyLen);
    }
    out[copyLen] = 0;

    return copyLen;
}

bool ELRSCrsfCore::optionEquals(const char *start, size_t len, const char *text)
{
    return (text && strlen(text) == len && !strncmp(start, text, len));
}

bool ELRSCrsfCore::parseOptionTerminalMilliwatts(const char *start, size_t len, uint16_t &value)
{
    long parsed = 0;
    bool haveDigit = false;

    if(!start || !len) {
        return false;
    }

    while(len && (start[len - 1] < '0' || start[len - 1] > '9')) {
        len--;
    }
    while(len && start[len - 1] >= '0' && start[len - 1] <= '9') {
        len--;
    }

    for(size_t i = len; start[i] >= '0' && start[i] <= '9'; i++) {
        parsed = (parsed * 10) + (start[i] - '0');
        haveDigit = true;
    }

    if(!haveDigit || parsed < 0 || parsed > 65535) {
        return false;
    }

    value = (uint16_t)parsed;
    return true;
}

bool ELRSCrsfCore::readOptionAt(const char *options, uint8_t index, const char **start, size_t *len)
{
    const char *cursor;
    uint8_t current = 0;

    if(!options || !start || !len) {
        return false;
    }

    cursor = options;
    *start = NULL;
    *len = 0;

    while(1) {
        const char *tokenStart = cursor;

        while(*cursor && *cursor != ';') {
            cursor++;
        }

        if(current == index) {
            *start = tokenStart;
            *len = (size_t)(cursor - tokenStart);
            return true;
        }

        if(!*cursor) {
            break;
        }

        cursor++;
        current++;
    }

    return false;
}

uint8_t ELRSCrsfCore::crc8D5(const uint8_t *data, size_t len)
{
    return ELRSCrsfTransport::crc8D5(data, len);
}

size_t ELRSCrsfCore::packRcChannelsFrame(const uint16_t channels[16], uint8_t *frame, size_t frameSize)
{
    return ELRSCrsfTransport::packRcChannelsFrame(channels, frame, frameSize);
}

const char *ELRSCrsfCore::speedSourceName(SpeedSource source)
{
    switch(source) {
    case SPEED_SOURCE_GPS:
        return "GPS";
    case SPEED_SOURCE_AIRSPEED:
        return "airspeed";
    default:
        return "none";
    }
}

const char *ELRSCrsfCore::commCodeText(uint8_t code)
{
    switch(code) {
    case ELRS_COMM_NRY:
        return "NRY";
    case ELRS_COMM_RLS:
        return "RLS";
    case ELRS_COMM_CRC:
        return "CRC";
    case ELRS_COMM_FRM:
        return "FRM";
    default:
        return "";
    }
}

const char *ELRSCrsfCore::moduleConfigTargetName(uint8_t targetIndex)
{
    switch(targetIndex) {
    case 0:
        return "Telemetry Ratio";
    case 1:
        return "Max Power";
    case 2:
        return "Dynamic Power";
    default:
        return "";
    }
}

unsigned long ELRSCrsfCore::axisSampleIntervalMs(uint16_t packetRateHz)
{
    packetRateHz = elrsPacketRateOrDefault(packetRateHz);
    return (1000UL + packetRateHz - 1) / packetRateHz;
}


#endif
