#ifndef _ELRS_CRSF_CORE_H
#define _ELRS_CRSF_CORE_H

#include <stddef.h>
#include <stdint.h>

#include "elrs_crsf_shared.h"
#include "elrs_crsf_transport.h"

enum ELRSCrsfFaultFlags : uint8_t {
    ELRS_FAULT_NONE = 0,
    ELRS_FAULT_ADC_MISSING = (1 << 0),
    ELRS_FAULT_ADC_STALE = (1 << 1),
    ELRS_FAULT_BUTTONPACK_STALE = (1 << 2)
};

struct ELRSCrsfStatus {
    uint32_t baudRate = 400000;
    uint16_t packetRateHz = ELRS_PACKET_RATE_DEFAULT;
    bool telemetryActive = false;
    bool replyActive = false;
    bool synced = false;
    uint8_t activeSpeedSource = 0;
    uint8_t linkQuality = 0;
    uint8_t remoteBatteryPercent = 0;
    float remoteBatteryVoltage = 0.0f;
    uint8_t faultFlags = ELRS_FAULT_NONE;
    uint8_t commCode = ELRS_COMM_NONE;
    bool everReplied = false;
    bool everSynced = false;
    bool invertLine = false;
    bool debugEnabled = false;
    unsigned long lastTxAt = 0;
    unsigned long lastReplyAt = 0;
    unsigned long lastRxAt = 0;
    unsigned long lastReplyTimeoutAt = 0;
    uint8_t lastRawFrameSyncByte = 0;
    uint8_t lastRawFrameType = 0;
    uint8_t lastRawFrameLength = 0;
    bool lastRawFrameCrcValid = false;
    bool fakePowerOn = false;
    bool calibrating = false;
    bool selfTestActive = false;
};

class ELRSCrsfHost : public ELRSCrsfTransportHal {
    public:
        virtual ~ELRSCrsfHost() {}

        virtual bool sampleAxes(int16_t axes[ELRS_GIMBAL_AXIS_COUNT]) = 0;
        virtual bool readFakePowerSwitch() = 0;
        virtual bool readStopSwitch() = 0;
        virtual bool readButtonA() = 0;
        virtual bool readButtonB() = 0;
        virtual bool readCalibrationButton() = 0;
        virtual bool samplePackStates(uint8_t &states) = 0;

        virtual void displayOn() = 0;
        virtual void displaySetText(const char *text) = 0;
        virtual void displaySetSpeed(int speed) = 0;
        virtual void displayShow() = 0;

        virtual void setPowerLed(bool state) = 0;
        virtual bool getPowerLed() const = 0;
        virtual void setLevelMeter(bool state) = 0;
        virtual bool getLevelMeter() const = 0;
        virtual void setStopLed(bool state) = 0;

        virtual void loadCalibration(ELRSAxisCalibrationData *cal, int count) = 0;
        virtual void saveCalibration(const ELRSAxisCalibrationData *cal, int count) = 0;
};

struct ELRSCrsfCoreConfig {
    bool haveButtonPack = false;
    bool usePowerLed = false;
    bool useLevelMeter = false;
    bool powerLedOnFakePower = false;
    bool levelMeterOnFakePower = false;
    uint8_t speedDisplayUnits = ELRS_SPEED_UNITS_DEFAULT;
    uint8_t telemetryRatio = ELRS_TLM_RATIO_DEFAULT;
    uint8_t maxPower = ELRS_MAX_POWER_DEFAULT;
    uint8_t dynamicPower = ELRS_DYNAMIC_POWER_DEFAULT;
    ELRSCrsfTransportConfig transport;
};

class ELRSCrsfCore : private ELRSCrsfTransportSink {
    public:
        enum SpeedSource : uint8_t {
            SPEED_SOURCE_NONE = 0,
            SPEED_SOURCE_GPS,
            SPEED_SOURCE_AIRSPEED
        };

        ELRSCrsfCore();

        bool begin(ELRSCrsfHost &host, const ELRSCrsfCoreConfig &config, unsigned long now);
        bool begin(ELRSCrsfHost &host, const ELRSCrsfCoreConfig &config, unsigned long now, unsigned long nowUs);
        void loop(ELRSCrsfHost &host, unsigned long now, int battWarn);
        void loop(ELRSCrsfHost &host, unsigned long now, unsigned long nowUs, int battWarn);
        void startSelfTest(unsigned long now, unsigned long durationMs = 15000);
        void stopSelfTest();
        bool selfTestActive() const;

        bool isCalibrating() const;
        bool fakePowerOn() const;

        uint32_t baudRate() const;
        uint16_t channelAt(uint8_t index) const;
        uint8_t linkQuality() const;
        uint8_t remoteBatteryPercent() const;
        float remoteBatteryVoltage() const;
        uint16_t gpsSpeed10() const;
        uint16_t airspeed10() const;
        bool telemetryActive() const;
        bool replyActive() const;
        bool synced() const;
        SpeedSource activeSpeedSource() const;
        ELRSCrsfStatus getStatus() const;

        static uint8_t crc8D5(const uint8_t *data, size_t len);
        static size_t packRcChannelsFrame(const uint16_t channels[16], uint8_t *frame, size_t frameSize);

    private:
        enum CalStage : uint8_t {
            CAL_IDLE = 0,
            CAL_CENTER,
            CAL_TLOW,
            CAL_THIGH,
            CAL_YLOW,
            CAL_YHIGH,
            CAL_PLOW,
            CAL_PHIGH,
            CAL_RLOW,
            CAL_RHIGH
        };

        bool sampleAxes(ELRSCrsfHost &host, unsigned long now, bool force = false);
        uint8_t samplePackStates(ELRSCrsfHost &host, unsigned long now);
        bool onCrsfFrame(uint8_t syncByte, uint8_t type, const uint8_t *payload, size_t payloadLen, unsigned long now) override;

        void updateCalibrationButton(ELRSCrsfHost &host, unsigned long now, int battWarn);
        void handleCalibrationShort(ELRSCrsfHost &host, unsigned long now, int battWarn);
        void handleCalibrationLong(ELRSCrsfHost &host, unsigned long now, int battWarn);
        const char *getCalibrationPrompt() const;

        void updateChannels(unsigned long now, bool fakePowerOn, bool stopOn, bool buttonAOn, bool buttonBOn, uint8_t packStates);
        uint16_t axisToTicks(uint8_t axis) const;
        uint16_t mapTicks(int16_t raw, int16_t inMin, int16_t inMax, uint16_t outMin, uint16_t outMax) const;

        void applyIdleOutputs(ELRSCrsfHost &host, bool fakePowerOn);
        void updateBatteryWarning(ELRSCrsfHost &host, unsigned long now, int battWarn, bool fakePowerOn);
        void updateInputFaults(ELRSCrsfHost &host, unsigned long now);
        void updateBenchState(ELRSCrsfHost &host, unsigned long now);
        void updateModuleConfig(ELRSCrsfHost &host, unsigned long now);
        void updateDisplay(ELRSCrsfHost &host, unsigned long now, int battWarn);
        bool hasRecentTelemetry(unsigned long now) const;
        bool adcFaultActive(unsigned long now) const;
        bool buttonPackFaultActive(unsigned long now) const;
        uint16_t getDisplaySpeed10(unsigned long now, SpeedSource *source = NULL) const;
        uint16_t getDisplaySpeed10ForUnits(uint16_t speed10) const;
        void resetModuleConfigSession();
        void resetModuleParameters();
        bool buildExtendedFrame(uint8_t type, uint8_t destAddr, uint8_t origAddr, const uint8_t *payload, size_t payloadLen, uint8_t *frame, size_t frameSize) const;
        bool queueModulePing();
        bool queueParameterRead(uint8_t fieldId, uint8_t chunkIndex = 0);
        bool queueParameterWrite(uint8_t fieldId, uint8_t value);
        void startModuleConfigSession(unsigned long now);
        void setModuleConfigBackoff(unsigned long now, unsigned long delayMs);
        void noteModuleConfigResponse();
        void handleDeviceInfo(const uint8_t *payload, size_t payloadLen, unsigned long now);
        void handleParameterSettingsEntry(const uint8_t *payload, size_t payloadLen, unsigned long now);
        void finishParameterChunk(uint8_t fieldId, const uint8_t *data, size_t len, unsigned long now);
        void applyDiscoveredParameter(uint8_t fieldId, const char *name, uint8_t type, const char *options, uint8_t currentValue);
        bool moduleSettingValueForTarget(uint8_t targetIndex, const char *options, uint8_t *value) const;
        void showOverlay(const char *text, unsigned long now, unsigned long durationMs);
        void showCommOverlay(const char *text, unsigned long now, unsigned long durationMs);

        void log(ELRSCrsfHost &host, const char *message) const;
        void logf(ELRSCrsfHost &host, const char *fmt, ...) const;

        static uint16_t readBE16(const uint8_t *data);
        static size_t copyToken(char *out, size_t outSize, const char *start, size_t len);
        static bool optionEquals(const char *start, size_t len, const char *text);
        static bool parseOptionTerminalMilliwatts(const char *start, size_t len, uint16_t &value);
        static bool readOptionAt(const char *options, uint8_t index, const char **start, size_t *len);
        static const char *speedSourceName(SpeedSource source);
        static const char *commCodeText(uint8_t code);
        static const char *moduleConfigTargetName(uint8_t targetIndex);
        static unsigned long axisSampleIntervalMs(uint16_t packetRateHz);

        enum ModuleConfigState : uint8_t {
            MODULECFG_IDLE = 0,
            MODULECFG_WAIT_START,
            MODULECFG_WAIT_DEVICE_INFO,
            MODULECFG_READ_PARAMETER,
            MODULECFG_WAIT_PARAMETER,
            MODULECFG_APPLY_SETTING,
            MODULECFG_WAIT_WRITE,
            MODULECFG_DONE,
            MODULECFG_BACKOFF
        };

        struct ModuleParameterInfo {
            bool found = false;
            uint8_t fieldId = 0;
            uint8_t currentValue = 0;
            char options[96];
        };

        ELRSCrsfCoreConfig _config;
        ELRSCrsfTransport _transport;

        bool _haveAds = false;
        bool _fakePowerOn = false;
        bool _selfTestActive = false;

        unsigned long _startedAt = 0;
        unsigned long _selfTestUntil = 0;
        unsigned long _lastAxisAttemptAt = 0;
        unsigned long _lastGoodAxesAt = 0;
        unsigned long _lastGoodPackAt = 0;
        unsigned long _lastDisplay = 0;
        unsigned long _lastTelemetry = 0;
        unsigned long _lastLinkStats = 0;
        unsigned long _lastGpsSpeed = 0;
        unsigned long _lastAirspeed = 0;
        unsigned long _batteryBlinkAt = 0;
        unsigned long _batteryBannerAt = 0;
        unsigned long _overlayUntil = 0;
        unsigned long _commOverlayUntil = 0;
        unsigned long _moduleConfigNextAt = 0;
        unsigned long _moduleConfigDeadlineAt = 0;

        uint16_t _channels[16];
        int16_t _rawAxes[ELRS_GIMBAL_AXIS_COUNT];
        ELRSAxisCalibrationData _axisCal[ELRS_GIMBAL_AXIS_COUNT];
        uint8_t _lastPackStates = 0;

        uint8_t _linkQuality = 0;
        uint8_t _remoteBattery = 0;
        float _remoteBatteryVoltage = 0.0f;
        uint8_t _faultFlags = ELRS_FAULT_NONE;
        uint16_t _gpsSpeed10 = 0;
        uint16_t _airspeed10 = 0;
        SpeedSource _activeSpeedSource = SPEED_SOURCE_NONE;
        bool _hasValidPackState = false;
        bool _adcFaultActive = false;
        bool _buttonPackFaultActive = false;
        uint8_t _lastCommCode = ELRS_COMM_NONE;
        bool _lastReplyActive = false;

        ModuleConfigState _moduleConfigState = MODULECFG_IDLE;
        bool _moduleConfigPending = false;
        uint8_t _moduleFieldCount = 0;
        uint8_t _moduleFieldIndex = 0;
        uint8_t _moduleTargetIndex = 0;
        uint8_t _moduleWriteFieldId = 0;
        uint8_t _moduleProbeRetryCount = 0;
        uint8_t _moduleParameterRetryCount = 0;
        bool _moduleChunkActive = false;
        uint8_t _moduleChunkFieldId = 0;
        uint8_t _moduleChunkNextIndex = 0;
        size_t _moduleChunkLen = 0;
        uint8_t _moduleChunkData[192];
        ModuleParameterInfo _moduleTelemetryRatio;
        ModuleParameterInfo _moduleMaxPower;
        ModuleParameterInfo _moduleDynamicPower;

        bool _calibRaw = false;
        bool _calibPressed = false;
        bool _calibLongSent = false;
        unsigned long _calibDebounceAt = 0;
        unsigned long _calibPressedAt = 0;
        CalStage _calStage = CAL_IDLE;

        char _overlayText[4];
        char _commOverlayText[4];
};

#endif
