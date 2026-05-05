#include "../../remote_global.h"

#ifdef HAVE_CRSF

#include "elrs_crsf_transport.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace {

constexpr uint8_t CRSF_FRAME_RC_CHANNELS_PACKED = 0x16;
constexpr uint8_t CRSF_SYNC_BYTE = 0xC8;
constexpr uint8_t CRSF_SYNC_BROADCAST = 0x00;
constexpr uint8_t CRSF_ADDR_RADIO_TRANSMITTER = 0xEA;
constexpr uint8_t CRSF_ADDR_CRSF_TRANSMITTER = 0xEE;
constexpr uint8_t CRSF_ADDR_CRSF_RECEIVER = 0xEC;
constexpr unsigned long CRSF_COMM_BURST_WINDOW_MS = 1000;
constexpr uint8_t CRSF_COMM_BURST_THRESHOLD = 3;
constexpr unsigned long CRSF_SERVICE_FRAME_GAP_MS = 100;
constexpr unsigned long CRSF_BOOTSTRAP_SERVICE_REPLY_TIMEOUT_MS = 250;

static bool isLikelySyncByte(uint8_t value)
{
    switch(value) {
    case CRSF_SYNC_BYTE:
    case CRSF_SYNC_BROADCAST:
    case CRSF_ADDR_RADIO_TRANSMITTER:
    case CRSF_ADDR_CRSF_TRANSMITTER:
    case CRSF_ADDR_CRSF_RECEIVER:
        return true;
    default:
        return false;
    }
}

#if defined(REMOTE_DBG) && defined(REMOTE_DBG_CRSF_BUS)
constexpr bool CRSF_BUS_DEBUG_ENABLED = true;
#else
constexpr bool CRSF_BUS_DEBUG_ENABLED = false;
#endif

}

ELRSCrsfTransport::ELRSCrsfTransport()
{
    for(int i = 0; i < 16; i++) {
        _channels[i] = 992;
    }

    memset(_rxFrame, 0, sizeof(_rxFrame));
    memset(_serviceFrame, 0, sizeof(_serviceFrame));
    memset(_lastTxFrame, 0, sizeof(_lastTxFrame));
}

void ELRSCrsfTransport::setSink(ELRSCrsfTransportSink *sink)
{
    _sink = sink;
}

void ELRSCrsfTransport::begin(ELRSCrsfTransportHal &hal, const ELRSCrsfTransportConfig &config, unsigned long now)
{
    begin(hal, config, now, now * 1000UL);
}

void ELRSCrsfTransport::begin(ELRSCrsfTransportHal &hal, const ELRSCrsfTransportConfig &config, unsigned long now, unsigned long nowUs)
{
    _config = config;
    _config.packetRateHz = elrsPacketRateOrDefault(_config.packetRateHz);
    _status.baudRate = _config.baudRate;
    _status.packetRateHz = _config.packetRateHz;
    _status.invertLine = _config.invertLine;
    _status.debugEnabled = _config.debugEnabled;
    _status.rawFrameDebugEnabled = _config.rawFrameDebugEnabled;
    _status.oeActiveLow = _config.oeActiveLow;
    _status.telemetryActive = false;
    _status.replyActive = false;
    _status.synced = false;
    _status.everReplied = false;
    _status.everSynced = false;
    _status.commCode = ELRS_COMM_NONE;
    _status.lastTxAt = 0;
    _status.lastReplyAt = 0;
    _status.lastRxAt = 0;
    _status.lastReplyTimeoutAt = 0;
    _status.lastRawFrame = ELRSCrsfRawFrameInfo();

    _startedAt = now;
    _lastReplyAt = 0;
    _lastTelemetryAt = 0;
    _replyDeadlineAt = 0;
    _nextTxAtUs = nowUs;
    _lastServiceTxAt = 0;
    _serviceReplyHoldoffUntil = 0;
    _echoSuppressUntil = 0;
    _crcBurstAt = 0;
    _frameBurstAt = 0;
    _txIntervalUs = 1000000UL / _config.packetRateHz;
    _txIntervalRemainder = 1000000UL % _config.packetRateHz;
    _txRemainderAccumulator = 0;
    _crcBurstCount = 0;
    _frameBurstCount = 0;
    _haveTelemetry = false;
    _waitingForReply = false;
    _replySeenForTx = false;
    _haveServiceFrame = false;
    _rxFrameLen = 0;
    _serviceFrameLen = 0;
    _lastTxFrameLen = 0;
    memset(_rxFrame, 0, sizeof(_rxFrame));
    memset(_serviceFrame, 0, sizeof(_serviceFrame));
    memset(_lastTxFrame, 0, sizeof(_lastTxFrame));

    hal.stopSerial();
    hal.startSerial(_config.baudRate, _config.invertLine);
    hal.setDriverEnabled(false);
    hal.discardSerialInput();
    resetTxScheduler(nowUs);

    logf(hal, "ELRS/CRSF transport: UART %lu 8N1 invert=%u rate=%uHz oeActiveLow=%u reply=%u telem=%u",
         (unsigned long)_config.baudRate,
         _config.invertLine ? 1U : 0U,
         (unsigned)_config.packetRateHz,
         _config.oeActiveLow ? 1U : 0U,
         (unsigned)_config.replyTimeoutMs,
         (unsigned)_config.telemetryTimeoutMs);
}

void ELRSCrsfTransport::setChannels(const uint16_t channels[16])
{
    if(!channels) {
        return;
    }

    memcpy(_channels, channels, sizeof(_channels));
}

void ELRSCrsfTransport::loop(ELRSCrsfTransportHal &hal, unsigned long now)
{
    loop(hal, now, now * 1000UL);
}

void ELRSCrsfTransport::loop(ELRSCrsfTransportHal &hal, unsigned long now, unsigned long nowUs)
{
    pollFrames(hal, now);
    updateState(hal, now);

    if(nowUs >= _nextTxAtUs) {
        if(_haveServiceFrame && (!_lastServiceTxAt || (now - _lastServiceTxAt >= CRSF_SERVICE_FRAME_GAP_MS))) {
            sendFrame(hal, _serviceFrame, _serviceFrameLen, now, nowUs, "ELRS/CRSF CFG", true);
            _serviceReplyHoldoffUntil = now + effectiveReplyTimeoutMs(true);
            _haveServiceFrame = false;
            _serviceFrameLen = 0;
            _lastServiceTxAt = now;
        } else if(!_serviceReplyHoldoffUntil || now >= _serviceReplyHoldoffUntil) {
            sendChannels(hal, now, nowUs);
        } else {
            return;
        }
    }
}

bool ELRSCrsfTransport::queueServiceFrame(const uint8_t *frame, size_t frameLen)
{
    if(!frame || frameLen < 4 || frameLen > sizeof(_serviceFrame)) {
        return false;
    }

    memcpy(_serviceFrame, frame, frameLen);
    _serviceFrameLen = frameLen;
    _haveServiceFrame = true;
    return true;
}

bool ELRSCrsfTransport::hasPendingServiceFrame() const
{
    return _haveServiceFrame;
}

void ELRSCrsfTransport::sendFrame(ELRSCrsfTransportHal &hal, const uint8_t *frame, size_t frameLen, unsigned long now, unsigned long nowUs, const char *prefix, bool resetReplyWindow)
{
    if(!frame || !frameLen) {
        return;
    }

    bool logRawFrame = shouldLogRawFrame(frame, frameLen, resetReplyWindow);

    if(logRawFrame) {
        logf(hal, "ELRS/CRSF transport: OE on @%luus", nowUs);
        logFrame(hal, prefix, frame, frameLen, true);
    }

    hal.setDriverEnabled(true);
    hal.serialWrite(frame, frameLen);
    hal.serialFlush();
    hal.setDriverEnabled(false);

    if(CRSF_BUS_DEBUG_ENABLED && resetReplyWindow) {
        logf(hal, "ELRS/CRSF bus: tx frame len=%u flushDone=%luus",
             (unsigned)frameLen,
             hal.microsNow());
    }

    memcpy(_lastTxFrame, frame, frameLen);
    _lastTxFrameLen = frameLen;
    {
        const unsigned long replyTimeoutMs = effectiveReplyTimeoutMs(resetReplyWindow);
        _echoSuppressUntil = now + ((replyTimeoutMs > 5U) ? replyTimeoutMs : 5U);
    }

    if(logRawFrame) {
        logf(hal, "ELRS/CRSF transport: OE off @%luus", nowUs);
    }

    _status.lastTxAt = now;
    if(resetReplyWindow) {
        size_t drained = drainExactEchoFrame(hal, frame, frameLen);
        const unsigned long replyTimeoutMs = effectiveReplyTimeoutMs(true);
        if(CRSF_BUS_DEBUG_ENABLED) {
            logf(hal, "ELRS/CRSF bus: drained local echo bytes=%u done=%luus",
                 (unsigned)drained,
                 hal.microsNow());
            if(_rxFrameLen > 0) {
                logf(hal, "ELRS/CRSF bus: first non-echo rx byte=0x%02X at %luus",
                     (unsigned)_rxFrame[0],
                     hal.microsNow());
            }
        }
        _waitingForReply = true;
        _replySeenForTx = false;
        _replyDeadlineAt = now + replyTimeoutMs;
    }
    do {
        advanceNextTxDeadline();
    } while(_nextTxAtUs <= nowUs);
}

size_t ELRSCrsfTransport::drainExactEchoFrame(ELRSCrsfTransportHal &hal, const uint8_t *frame, size_t frameLen)
{
    size_t matched = 0;

    if(!frame || !frameLen) {
        return 0;
    }

    while((matched < frameLen) && hal.serialAvailable()) {
        int ch = hal.serialRead();
        if(ch < 0) {
            break;
        }

        if((uint8_t)ch != frame[matched]) {
            _rxFrame[0] = (uint8_t)ch;
            _rxFrameLen = 1;
            return matched;
        }

        matched++;
    }

    return matched;
}

unsigned long ELRSCrsfTransport::effectiveReplyTimeoutMs(bool resetReplyWindow) const
{
    if(resetReplyWindow && !_status.everReplied && _config.replyTimeoutMs < CRSF_BOOTSTRAP_SERVICE_REPLY_TIMEOUT_MS) {
        return CRSF_BOOTSTRAP_SERVICE_REPLY_TIMEOUT_MS;
    }

    return _config.replyTimeoutMs;
}

bool ELRSCrsfTransport::shouldLogRawFrame(const uint8_t *frame, size_t frameLen, bool resetReplyWindow) const
{
    if(!_status.rawFrameDebugEnabled || !frame || frameLen < 3) {
        return false;
    }

    if(resetReplyWindow) {
        return true;
    }

    return (frame[2] != CRSF_FRAME_RC_CHANNELS_PACKED);
}

const ELRSCrsfTransportConfig &ELRSCrsfTransport::config() const
{
    return _config;
}

const ELRSCrsfTransportStatus &ELRSCrsfTransport::status() const
{
    return _status;
}

void ELRSCrsfTransport::sendChannels(ELRSCrsfTransportHal &hal, unsigned long now, unsigned long nowUs)
{
    uint8_t frame[26];
    size_t frameLen = packRcChannelsFrame(_channels, frame, sizeof(frame));

    if(!frameLen) {
        return;
    }
    sendFrame(hal, frame, frameLen, now, nowUs, "ELRS/CRSF TX", false);
}

void ELRSCrsfTransport::pollFrames(ELRSCrsfTransportHal &hal, unsigned long now)
{
    while(hal.serialAvailable()) {
        int ch = hal.serialRead();
        if(ch < 0) {
            break;
        }

        if(_rxFrameLen == 0) {
            if(!isLikelySyncByte((uint8_t)ch)) {
                continue;
            }
            _rxFrame[_rxFrameLen++] = (uint8_t)ch;
            continue;
        }

        if(_rxFrameLen >= sizeof(_rxFrame)) {
            noteFrameError(hal, now);
            _rxFrameLen = 0;
            _rxFrame[_rxFrameLen++] = (uint8_t)ch;
            continue;
        }

        _rxFrame[_rxFrameLen++] = (uint8_t)ch;

        if(_rxFrameLen == 2) {
            if(_rxFrame[1] < 2 || _rxFrame[1] > 62) {
                noteFrameError(hal, now);
                resyncRxBuffer();
            }
            continue;
        }

        if(_rxFrameLen >= 2) {
            size_t expectLen = _rxFrame[1] + 2;
            if(expectLen > sizeof(_rxFrame)) {
                noteFrameError(hal, now);
                resyncRxBuffer();
                continue;
            }
            if(_rxFrameLen == expectLen) {
                bool crcValid = (crc8D5(_rxFrame + 2, expectLen - 3) == _rxFrame[expectLen - 1]);

                _status.lastRawFrame.syncByte = _rxFrame[0];
                _status.lastRawFrame.type = _rxFrame[2];
                _status.lastRawFrame.length = (uint8_t)expectLen;
                _status.lastRawFrame.crcValid = crcValid;

                if(shouldLogRawFrame(_rxFrame, expectLen, false)) {
                    logFrame(hal, "ELRS/CRSF RX", _rxFrame, expectLen, crcValid);
                }

                if(crcValid) {
                    if(_lastTxFrameLen == expectLen &&
                       _echoSuppressUntil &&
                       now <= _echoSuppressUntil &&
                       !memcmp(_lastTxFrame, _rxFrame, expectLen)) {
                        _rxFrameLen = 0;
                        continue;
                    }

                    bool supportedTelemetry = false;

                    _status.lastReplyAt = now;
                    _status.lastRxAt = now;
                    _lastReplyAt = now;
                    _serviceReplyHoldoffUntil = 0;
                    _status.replyActive = true;
                    _status.synced = true;
                    _status.everReplied = true;
                    _status.everSynced = true;
                    _replySeenForTx = true;
                    clearCommCode();

                    if(_sink) {
                        const uint8_t *payload = _rxFrame + 3;
                        size_t payloadLen = expectLen - 4;
                        supportedTelemetry = _sink->onCrsfFrame(_rxFrame[0], _rxFrame[2], payload, payloadLen, now);
                    }

                    if(supportedTelemetry) {
                        _lastTelemetryAt = now;
                        _haveTelemetry = true;
                        _status.telemetryActive = true;
                    }
                    _rxFrameLen = 0;
                } else {
                    noteBadCrc(hal, now);
                    _rxFrameLen = 0;
                }
            }
        }
    }
}

void ELRSCrsfTransport::resyncRxBuffer(size_t startIndex)
{
    if(startIndex < _rxFrameLen) {
        for(size_t i = startIndex; i < _rxFrameLen; i++) {
            if(isLikelySyncByte(_rxFrame[i])) {
                memmove(_rxFrame, _rxFrame + i, _rxFrameLen - i);
                _rxFrameLen -= i;
                return;
            }
        }
    }

    _rxFrameLen = 0;
}

void ELRSCrsfTransport::noteBadCrc(ELRSCrsfTransportHal &hal, unsigned long now)
{
    if(!_status.everReplied) {
        return;
    }

    if(!_crcBurstAt || (now - _crcBurstAt > CRSF_COMM_BURST_WINDOW_MS)) {
        _crcBurstAt = now;
        _crcBurstCount = 1;
    } else {
        _crcBurstCount++;
    }

    if(_crcBurstCount == CRSF_COMM_BURST_THRESHOLD) {
        setCommCode(ELRS_COMM_CRC);
        log(hal, "ELRS/CRSF transport: repeated bad CRC frames");
    }
}

void ELRSCrsfTransport::noteFrameError(ELRSCrsfTransportHal &hal, unsigned long now)
{
    if(!_status.everReplied) {
        return;
    }

    if(!_frameBurstAt || (now - _frameBurstAt > CRSF_COMM_BURST_WINDOW_MS)) {
        _frameBurstAt = now;
        _frameBurstCount = 1;
    } else {
        _frameBurstCount++;
    }

    if(_frameBurstCount == CRSF_COMM_BURST_THRESHOLD) {
        setCommCode(ELRS_COMM_FRM);
        log(hal, "ELRS/CRSF transport: repeated malformed CRSF frames");
    }
}

void ELRSCrsfTransport::setCommCode(uint8_t code)
{
    if(code == ELRS_COMM_NONE) {
        clearCommCode();
        return;
    }

    _status.commCode = code;
}

void ELRSCrsfTransport::clearCommCode()
{
    _status.commCode = ELRS_COMM_NONE;
    _crcBurstAt = 0;
    _frameBurstAt = 0;
    _crcBurstCount = 0;
    _frameBurstCount = 0;
}

void ELRSCrsfTransport::updateState(ELRSCrsfTransportHal &hal, unsigned long now)
{
    _status.replyActive = (_status.everReplied && (now - _lastReplyAt < _config.telemetryTimeoutMs));
    _status.telemetryActive = (_haveTelemetry && (now - _lastTelemetryAt < _config.telemetryTimeoutMs));
    _status.synced = _status.replyActive;
    _status.everSynced = _status.everReplied;

    if(_waitingForReply && !_replySeenForTx && _replyDeadlineAt && (now >= _replyDeadlineAt)) {
        _waitingForReply = false;
        _status.lastReplyTimeoutAt = now;
        if(_status.debugEnabled) {
            logf(hal, "ELRS/CRSF transport: reply timeout @%lums after TX @%lums", now, _status.lastTxAt);
        }
    } else if(_waitingForReply && _replySeenForTx) {
        _waitingForReply = false;
    }

    if(!_status.everReplied) {
        if((now - _startedAt >= _config.telemetryTimeoutMs) && (_status.commCode == ELRS_COMM_NONE)) {
            setCommCode(ELRS_COMM_NRY);
            log(hal, "ELRS/CRSF transport: no replies; telemetry may be off, receiver/downlink absent, or module/wiring issue");
        }
        return;
    }

    if(_haveTelemetry && !_status.replyActive && (_status.commCode == ELRS_COMM_NONE)) {
        setCommCode(ELRS_COMM_RLS);
        log(hal, "ELRS/CRSF transport: replies lost; outbound control may still be active");
    }
}

void ELRSCrsfTransport::resetTxScheduler(unsigned long nowUs)
{
    _nextTxAtUs = nowUs;
    _txRemainderAccumulator = 0;
}

void ELRSCrsfTransport::advanceNextTxDeadline()
{
    _nextTxAtUs += _txIntervalUs;
    _txRemainderAccumulator += _txIntervalRemainder;
    if(_txRemainderAccumulator >= _config.packetRateHz) {
        _nextTxAtUs++;
        _txRemainderAccumulator -= _config.packetRateHz;
    }
}

void ELRSCrsfTransport::log(ELRSCrsfTransportHal &hal, const char *message) const
{
    if(message && *message) {
        hal.logMessage(message);
    }
}

void ELRSCrsfTransport::logf(ELRSCrsfTransportHal &hal, const char *fmt, ...) const
{
    char buffer[224];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    hal.logMessage(buffer);
}

void ELRSCrsfTransport::logFrame(ELRSCrsfTransportHal &hal, const char *prefix, const uint8_t *frame, size_t frameSize, bool crcValid) const
{
    char buffer[256];
    size_t used = 0;

    if(!frame || !frameSize) {
        return;
    }

    used += (size_t)snprintf(buffer + used, sizeof(buffer) - used, "%s len=%u crc=%u bytes=",
                             prefix,
                             (unsigned)frameSize,
                             crcValid ? 1U : 0U);
    for(size_t i = 0; i < frameSize && used + 4 < sizeof(buffer); i++) {
        used += (size_t)snprintf(buffer + used, sizeof(buffer) - used, "%02X", frame[i]);
        if(i + 1 < frameSize && used + 2 < sizeof(buffer)) {
            buffer[used++] = ' ';
            buffer[used] = 0;
        }
    }

    hal.logMessage(buffer);
}

uint8_t ELRSCrsfTransport::crc8D5(const uint8_t *data, size_t len)
{
    uint8_t crc = 0;

    while(len--) {
        crc ^= *data++;
        for(uint8_t i = 0; i < 8; i++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0xD5) : (uint8_t)(crc << 1);
        }
    }

    return crc;
}

size_t ELRSCrsfTransport::packRcChannelsFrame(const uint16_t channels[16], uint8_t *frame, size_t frameSize)
{
    uint8_t payload[22];
    uint32_t bitbuf = 0;
    uint8_t bits = 0;
    uint8_t idx = 0;

    if(frameSize < 26) {
        return 0;
    }

    memset(payload, 0, sizeof(payload));

    for(int i = 0; i < 16; i++) {
        bitbuf |= ((uint32_t)(channels[i] & 0x07ff)) << bits;
        bits += 11;
        while(bits >= 8) {
            payload[idx++] = bitbuf & 0xff;
            bitbuf >>= 8;
            bits -= 8;
        }
    }
    if(idx < sizeof(payload)) {
        payload[idx++] = bitbuf & 0xff;
    }

    frame[0] = CRSF_SYNC_BYTE;
    frame[1] = 24;
    frame[2] = CRSF_FRAME_RC_CHANNELS_PACKED;
    memcpy(frame + 3, payload, sizeof(payload));
    frame[25] = crc8D5(frame + 2, 23);

    return 26;
}

const char *ELRSCrsfTransport::commCodeName(uint8_t code)
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

#endif
