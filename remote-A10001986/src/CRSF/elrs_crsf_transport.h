#ifndef _ELRS_CRSF_TRANSPORT_H
#define _ELRS_CRSF_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

#include "elrs_crsf_shared.h"

enum ELRSCrsfCommCode : uint8_t {
    ELRS_COMM_NONE = 0,
    ELRS_COMM_NRY,
    ELRS_COMM_RLS,
    ELRS_COMM_CRC,
    ELRS_COMM_FRM,
    ELRS_COMM_NSY = ELRS_COMM_NRY,
    ELRS_COMM_LOS = ELRS_COMM_RLS
};

struct ELRSCrsfTransportConfig {
    uint32_t baudRate = 400000;
    bool invertLine = false;
    uint16_t packetRateHz = ELRS_PACKET_RATE_DEFAULT;
    uint16_t frameIntervalMs = 10;
    uint16_t telemetryTimeoutMs = 2000;
    uint16_t replyTimeoutMs = 20;
    bool debugEnabled = false;
    bool rawFrameDebugEnabled = false;
    bool oeActiveLow = true;
};

struct ELRSCrsfRawFrameInfo {
    uint8_t syncByte = 0;
    uint8_t type = 0;
    uint8_t length = 0;
    bool crcValid = false;
};

struct ELRSCrsfTransportStatus {
    uint32_t baudRate = 400000;
    uint16_t packetRateHz = ELRS_PACKET_RATE_DEFAULT;
    bool invertLine = false;
    bool debugEnabled = false;
    bool rawFrameDebugEnabled = false;
    bool oeActiveLow = true;
    bool telemetryActive = false;
    bool replyActive = false;
    bool synced = false;
    bool everReplied = false;
    bool everSynced = false;
    uint8_t commCode = ELRS_COMM_NONE;
    unsigned long lastTxAt = 0;
    unsigned long lastReplyAt = 0;
    unsigned long lastRxAt = 0;
    unsigned long lastReplyTimeoutAt = 0;
    ELRSCrsfRawFrameInfo lastRawFrame;
};

class ELRSCrsfTransportHal {
    public:
        virtual ~ELRSCrsfTransportHal() {}

        virtual void logMessage(const char *message) = 0;
        virtual void startSerial(uint32_t baud, bool invert) = 0;
        virtual void stopSerial() = 0;
        virtual int serialAvailable() = 0;
        virtual int serialRead() = 0;
        virtual size_t serialWrite(const uint8_t *data, size_t len) = 0;
        virtual void serialFlush() = 0;
        virtual void setDriverEnabled(bool enabled) = 0;
        virtual void discardSerialInput() = 0;
        virtual unsigned long microsNow() = 0;
};

class ELRSCrsfTransportSink {
    public:
        virtual ~ELRSCrsfTransportSink() {}

        virtual bool onCrsfFrame(uint8_t syncByte, uint8_t type, const uint8_t *payload, size_t payloadLen, unsigned long now) = 0;
};

class ELRSCrsfTransport {
    public:
        ELRSCrsfTransport();

        void setSink(ELRSCrsfTransportSink *sink);
        void begin(ELRSCrsfTransportHal &hal, const ELRSCrsfTransportConfig &config, unsigned long now, unsigned long nowUs);
        void begin(ELRSCrsfTransportHal &hal, const ELRSCrsfTransportConfig &config, unsigned long now);
        void setChannels(const uint16_t channels[16]);
        void loop(ELRSCrsfTransportHal &hal, unsigned long now, unsigned long nowUs);
        void loop(ELRSCrsfTransportHal &hal, unsigned long now);
        bool queueServiceFrame(const uint8_t *frame, size_t frameLen);
        bool hasPendingServiceFrame() const;

        const ELRSCrsfTransportConfig &config() const;
        const ELRSCrsfTransportStatus &status() const;

        static uint8_t crc8D5(const uint8_t *data, size_t len);
        static size_t packRcChannelsFrame(const uint16_t channels[16], uint8_t *frame, size_t frameSize);

    private:
        void sendFrame(ELRSCrsfTransportHal &hal, const uint8_t *frame, size_t frameLen, unsigned long now, unsigned long nowUs, const char *prefix, bool resetReplyWindow);
        void sendChannels(ELRSCrsfTransportHal &hal, unsigned long now, unsigned long nowUs);
        void pollFrames(ELRSCrsfTransportHal &hal, unsigned long now);
        void resyncRxBuffer(size_t startIndex = 1);
        void noteBadCrc(ELRSCrsfTransportHal &hal, unsigned long now);
        void noteFrameError(ELRSCrsfTransportHal &hal, unsigned long now);
        void setCommCode(uint8_t code);
        void clearCommCode();
        void updateState(ELRSCrsfTransportHal &hal, unsigned long now);
        void resetTxScheduler(unsigned long nowUs);
        void advanceNextTxDeadline();
        size_t drainExactEchoFrame(ELRSCrsfTransportHal &hal, const uint8_t *frame, size_t frameLen);
        unsigned long effectiveReplyTimeoutMs(bool resetReplyWindow) const;
#ifndef REMOTE_CRSF_NO_RAW_DUMPS
        bool shouldLogRawFrame(const uint8_t *frame, size_t frameLen, bool resetReplyWindow) const;
        void log(ELRSCrsfTransportHal &hal, const char *message) const;
        void logf(ELRSCrsfTransportHal &hal, const char *fmt, ...) const;
        void logFrame(ELRSCrsfTransportHal &hal, const char *prefix, const uint8_t *frame, size_t frameSize, bool crcValid) const;
#else
        void log(ELRSCrsfTransportHal &hal, const char *message) const;
        void logf(ELRSCrsfTransportHal &hal, const char *fmt, ...) const;
#endif

        static const char *commCodeName(uint8_t code);

        ELRSCrsfTransportConfig _config;
        ELRSCrsfTransportStatus _status;
        ELRSCrsfTransportSink *_sink = NULL;

        uint16_t _channels[16];
        uint8_t _rxFrame[64];
        uint8_t _serviceFrame[64];
        uint8_t _lastTxFrame[64];
        size_t _rxFrameLen = 0;
        size_t _serviceFrameLen = 0;
        size_t _lastTxFrameLen = 0;
        unsigned long _startedAt = 0;
        unsigned long _lastReplyAt = 0;
        unsigned long _lastTelemetryAt = 0;
        unsigned long _replyDeadlineAt = 0;
        unsigned long _nextTxAtUs = 0;
        unsigned long _lastServiceTxAt = 0;
        unsigned long _serviceReplyHoldoffUntil = 0;
        unsigned long _echoSuppressUntil = 0;
        unsigned long _crcBurstAt = 0;
        unsigned long _frameBurstAt = 0;
        uint32_t _txIntervalUs = 0;
        uint32_t _txIntervalRemainder = 0;
        uint32_t _txRemainderAccumulator = 0;
        uint8_t _crcBurstCount = 0;
        uint8_t _frameBurstCount = 0;
        bool _haveTelemetry = false;
        bool _waitingForReply = false;
        bool _replySeenForTx = false;
        bool _haveServiceFrame = false;
};

#endif
