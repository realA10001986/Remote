/*
 * -------------------------------------------------------------------
 * Remote Control
 * (C) 2024-2026 Thomas Winischhofer (A10001986)
 * https://github.com/realA10001986/Remote
 * https://remote.out-a-ti.me
 *
 * RotEnc Class, RemButton Class: I2C-RotEnc and Button handling
 * 
 * -------------------------------------------------------------------
 * License: Modified MIT NON-AI
 * 
 * Permission is hereby granted, free of charge, to any person 
 * obtaining a copy of this software and associated documentation 
 * files (the "Software"), to deal in the Software without restriction, 
 * including without limitation the rights to use, copy, modify, 
 * merge, publish, distribute, sublicense, and/or sell copies of the 
 * Software, and to permit persons to whom the Software is furnished to 
 * do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be 
 * included in all copies or substantial portions of the Software.
 * 
 * Links inside the Software pointing to the original source must not 
 * be changed or removed.
 *
 * In addition, the following restrictions apply:
 * 
 * 1. The Software and any modifications made to it may not be used 
 * for the purpose of training or improving machine learning algorithms, 
 * including but not limited to artificial intelligence, natural 
 * language processing, or data mining. This condition applies to any 
 * derivatives, modifications, or updates based on the Software code. 
 * Any usage of the Software in an AI-training dataset is considered a 
 * breach of this License.
 *
 * 2. The Software may not be included in any dataset used for 
 * training or improving machine learning algorithms, including but 
 * not limited to artificial intelligence, natural language processing, 
 * or data mining.
 *
 * 3. Any person or organization found to be in violation of these 
 * restrictions will be subject to legal action and may be held liable 
 * for any damages resulting from such use.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * -------------------------------------------------------------------
 */

#include "remote_global.h"

#include <Arduino.h>

#include "input.h"

//#define REMOTE_DBG_ADC

/*
 * REMRotEnc class
 * 
 * Rotary Encoder handling:
 * Supports Adafruit 4991, DuPPA I2CEncoder 2.1, DFRobot Gravity 360,
 * plus ADS 1015 since the handling is identical.
 * 
 * For Volume, the encoders must be configured as follows:
 * - Ada4991: A0 closed (i2c address 0x37)
 * - DFRobot Gravity 360: SW1 off, SW2 on (i2c address 0x55)
 * - DuPPA I2CEncoder 2.1: A0 and A1 closed (i2c address 0x03)
 */

// General

#define HWUPD_DELAY 100     // ms between hw polls for throttle
#define HWUPD_DELAY_VOL 150 // ms between hw polls for volume

// Ada4991

#define SEESAW_STATUS_BASE  0x00
#define SEESAW_ENCODER_BASE 0x11

#define SEESAW_STATUS_HW_ID   0x01
#define SEESAW_STATUS_VERSION 0x02
#define SEESAW_STATUS_SWRST   0x7F

#define SEESAW_ENCODER_STATUS   0x00
#define SEESAW_ENCODER_INTENSET 0x10
#define SEESAW_ENCODER_INTENCLR 0x20
#define SEESAW_ENCODER_POSITION 0x30
#define SEESAW_ENCODER_DELTA    0x40

#define SEESAW_HW_ID_CODE_SAMD09   0x55
#define SEESAW_HW_ID_CODE_TINY806  0x84
#define SEESAW_HW_ID_CODE_TINY807  0x85
#define SEESAW_HW_ID_CODE_TINY816  0x86
#define SEESAW_HW_ID_CODE_TINY817  0x87
#define SEESAW_HW_ID_CODE_TINY1616 0x88
#define SEESAW_HW_ID_CODE_TINY1617 0x89

// Duppa V2.1

#define DUV2_BASE     0x100
#define DUV2_CONF     0x00
#define DUV2_CONF2    0x30
#define DUV2_CVALB4   0x08
#define DUV2_CMAXB4   0x0c
#define DUV2_CMINB4   0x10
#define DUV2_ISTEPB4  0x14
#define DUV2_IDCODE   0x70

enum DUV2_CONF_PARAM {
    DU2_FLOAT_DATA   = 0x01,
    DU2_INT_DATA     = 0x00,
    DU2_WRAP_ENABLE  = 0x02,
    DU2_WRAP_DISABLE = 0x00,
    DU2_DIRE_LEFT    = 0x04,
    DU2_DIRE_RIGHT   = 0x00,
    DU2_IPUP_DISABLE = 0x08,
    DU2_IPUP_ENABLE  = 0x00,
    DU2_RMOD_X2      = 0x10,
    DU2_RMOD_X1      = 0x00,
    DU2_RGB_ENCODER  = 0x20,
    DU2_STD_ENCODER  = 0x00,
    DU2_EEPROM_BANK1 = 0x40,
    DU2_EEPROM_BANK2 = 0x00,
    DU2_RESET        = 0x80
};
enum DUV2_CONF2_PARAM {
    DU2_CLK_STRECH_ENABLE  = 0x01,
    DU2_CLK_STRECH_DISABLE = 0x00,
    DU2_REL_MODE_ENABLE    = 0x02,
    DU2_REL_MODE_DISABLE   = 0x00,
};

// DFRobot Gravity 360

#define DFR_BASE      0x100
#define DFR_PID_MSB   0x00
#define DFR_VID_MSB   0x02
#define DFR_COUNT_MSB 0x08
#define DFR_GAIN_REG  0x0b

#define DFR_PID_U     0x01
#define DFR_PID_L     0xf6

#define DFR_GAIN      51
#define DFR_GAIN_VOL  (1023 / 20)

// ADS 1015
#define ADS_BASE      0x100

#define ADS_CONVERT   0x00
#define ADS_CONFIG    0x01
#define ADS_LTHR      0x02
#define ADS_HTHR      0x03

// ADS measuring range
#define ADS_GAIN_6    0b0000  // -6.144V - 6.144V 
#define ADS_GAIN_4    0b0010  // -4.096V - 4.096V
#define ADS_GAIN_2    0b0100  // -2.048V - 2.048V   <-- our setting for CB <= 1.6
#define ADS_GAIN_1    0b0110  // -1.024V - 1.024V

#define GAIN_OLD_BOARD ADS_GAIN_2
#define GAIN_NEW_BOARD ADS_GAIN_4

#define SCALE_TO_0_U  15
#define SCALE_MULT_U  10000 / (100-SCALE_TO_0_U)

#define SCALE_TO_0_D  33 //20
#define SCALE_MULT_D  10000 / (100-SCALE_TO_0_D)

REMRotEnc::REMRotEnc(int numTypes, const uint8_t *addrArr)
{
    _numTypes = min(6, numTypes);
    _addrArr = addrArr;
}

bool REMRotEnc::begin(bool forSpeed, bool newBoard)
{
    bool foundSt = false;
    union {
        uint8_t buf[4];
        int32_t ibuf;
    };

    _type = forSpeed ? 0 : 1;
    
    for(int i = 0; i < _numTypes * 2; i += 2) {

        _i2caddr = _addrArr[i];

        Wire.beginTransmission(_i2caddr);
        if(!Wire.endTransmission(true)) {

            switch(_addrArr[i+1]) {
            case REM_RE_TYPE_ADA4991:
                if(read(SEESAW_STATUS_BASE, SEESAW_STATUS_HW_ID, buf, 1) == 1) {
                    switch(buf[0]) {
                    case SEESAW_HW_ID_CODE_SAMD09:
                    case SEESAW_HW_ID_CODE_TINY806:
                    case SEESAW_HW_ID_CODE_TINY807:
                    case SEESAW_HW_ID_CODE_TINY816:
                    case SEESAW_HW_ID_CODE_TINY817:
                    case SEESAW_HW_ID_CODE_TINY1616:
                    case SEESAW_HW_ID_CODE_TINY1617:
                        //_hwtype = buf[0];
                        // Check for board type
                        read(SEESAW_STATUS_BASE, SEESAW_STATUS_VERSION, buf, 4);   
                        if((((uint16_t)buf[0] << 8) | buf[1]) == 4991) {
                            foundSt = true;
                        }
                    }
                }
                break;

            case REM_RE_TYPE_DUPPAV2:
                read(DUV2_BASE, DUV2_IDCODE, buf, 1);
                if(buf[0] == 0x53) {
                    foundSt = true;
                }
                break;

            case REM_RE_TYPE_DFRGR360:
                read(DFR_BASE, DFR_PID_MSB, buf, 2);
                if(((buf[0] & 0x3f) == DFR_PID_U) &&
                    (buf[1]         == DFR_PID_L)) {
                    foundSt = true;
                }
                break;

            case REM_RE_TYPE_CS:
                // TODO
                break;

            case REM_RE_TYPE_ADS1X15:
                foundSt = true;
                break;
            }

            if(foundSt) {
                _st = _addrArr[i+1];
    
                #ifdef REMOTE_DBG
                const char *tpArr[6] = { "ADA4991", "DuPPa V2.1", "DFRobot Gravity 360", "CircuitSetup", "ADS1015", "" };
                Serial.printf("Rotary encoder/ADC: Detected %s\n", tpArr[_st]);
                #endif
    
                break;
            }
        }
    }

    switch(_st) {
      
    case REM_RE_TYPE_ADA4991:
        // Reset
        buf[0] = 0xff;
        write(SEESAW_STATUS_BASE, SEESAW_STATUS_SWRST, &buf[0], 1);
        delay(10);
        // Default values suitable
        _dynZeroPos = true;
        break;

    case REM_RE_TYPE_DUPPAV2:
        // Reset
        buf[0] = DU2_RESET;
        write(DUV2_BASE, DUV2_CONF, &buf[0], 1);
        delay(10);
        // Init
        buf[0] = DU2_INT_DATA     | 
                 DU2_WRAP_DISABLE |
                 DU2_DIRE_RIGHT   |
                 DU2_RMOD_X1      |
                 DU2_STD_ENCODER  |
                 DU2_IPUP_DISABLE;
        buf[1] = DU2_CLK_STRECH_DISABLE |
                 DU2_REL_MODE_DISABLE;
        write(DUV2_BASE, DUV2_CONF, &buf[0], 1);
        write(DUV2_BASE, DUV2_CONF2, &buf[1], 1);
        // Reset counter to 0
        ibuf = 0;
        write(DUV2_BASE, DUV2_CVALB4, &buf[0], 4);
        // Step width 1
        buf[3] = 1;
        write(DUV2_BASE, DUV2_ISTEPB4, &buf[0], 4);
        // Set Min/Max
        buf[0] = 0x80; buf[3] = 0x00; // [1], [2] still zero from above
        write(DUV2_BASE, DUV2_CMINB4, &buf[0], 4);
        buf[0] = 0x7f; buf[1] = buf[2] = buf[3] = 0xff;
        write(DUV2_BASE, DUV2_CMAXB4, &buf[0], 4);
        _dynZeroPos = true;
        break;

    case REM_RE_TYPE_DFRGR360:
        dfrgain = (!_type) ? DFR_GAIN : DFR_GAIN_VOL;
        buf[0] = dfrgain;
        write(DFR_BASE, DFR_GAIN_REG, &buf[0], 1);
        _dynZeroPos = true;
        break;

    case REM_RE_TYPE_CS:
        return false;

    case REM_RE_TYPE_ADS1X15:
        buf[0] = 0b01000000;    // mmm = Mode (Single End, AIN0)
        //         ommmgggc
        buf[0] |= (newBoard ? GAIN_NEW_BOARD : GAIN_OLD_BOARD);
        
        buf[1] = 0b00000011;   // Comparator stuff (disabled)
        //         dddccccc
        write(ADS_BASE, ADS_CONFIG, &buf[0], 2);
        delay(10);
        _dynZeroPos = false;
        scaleThrottlePos = true; // make more tolerant for zero position
        break;
        
    default:
        return false;
    }

    zeroPos();

    return true;
}

bool REMRotEnc::dynZeroPos()
{
    return _dynZeroPos;
} 

// Init dec into "zero" position
void REMRotEnc::zeroPos(bool calibMode)
{
    if(_dynZeroPos || calibMode) {
        zeroEnc();
        rotEncZeroPos = getEncPos();
    }
    throttlePos = 0;
}

void REMRotEnc::setZeroPos(int32_t num)
{
    rotEncZeroPos = num;
}

bool REMRotEnc::setMaxStepsUp(int32_t num)
{
    int32_t t = (num != 0) ? num : getEncPos() - rotEncZeroPos;
    if(t) {
        throttlePositionsUp = t;
        #ifdef REMOTE_DBG
        Serial.printf("setMaxStepsUp: %d\n", throttlePositionsUp);
        #endif
        return true;
    } else {
        #ifdef REMOTE_DBG
        Serial.printf("setMaxStepsUp: 0 - error\n");
        #endif
        return false;
    }
}

bool REMRotEnc::setMaxStepsDown(int32_t num)
{
    int32_t t = (num != 0) ? num : getEncPos() - rotEncZeroPos;
    if(t) {
        throttlePositionsDown = t;
        #ifdef REMOTE_DBG
        Serial.printf("setMaxStepsDown: %d\n", throttlePositionsDown);
        #endif
        return true;
    } else {
        #ifdef REMOTE_DBG
        Serial.printf("setMaxStepsDown: 0 - error\n");
        #endif
        return false;
    }
}

// Returns -100% - 100%
int32_t REMRotEnc::updateThrottlePos(bool force)
{
    if(force || (millis() - lastUpd > HWUPD_DELAY)) {

        lastUpd = millis();

        int32_t t = getEncPos() - rotEncZeroPos;

        if(t < 0) {
            t *= 100;
            if(throttlePositionsUp < 0) {
                t /= throttlePositionsUp;
            } else {
                t /= (-throttlePositionsDown);
            }
        } else if(t > 0) {
            t *= 100;
            if(throttlePositionsUp > 0) {
                t /= throttlePositionsUp;
            } else {
                t /= (-throttlePositionsDown);
            }
        }

        if(t < -100) t = -100;
        else if(t > 100) t = 100;

        if(scaleThrottlePos) {
            if(t >= -SCALE_TO_0_D && t <= SCALE_TO_0_U) t = 0;
            else if(t < 0) {
                t = (t + SCALE_TO_0_D) * SCALE_MULT_D / 100;
            } else {
                t = (t - SCALE_TO_0_U) * SCALE_MULT_U / 100;
            }

            if(t < -100) t = -100;
            else if(t > 100) t = 100;
        }

        throttlePos = t;
    }
    
    return throttlePos;
}

int32_t REMRotEnc::getMaxStepsUp()
{
    return throttlePositionsUp;
}

int32_t REMRotEnc::getMaxStepsDown()
{
    return throttlePositionsDown;
}

int32_t REMRotEnc::getZeroPos()
{
    return rotEncZeroPos;
}

int REMRotEnc::updateVolume(int curVol, bool force)
{
    if(curVol == 255)
        return curVol;
    
    if(force || (millis() - lastUpd > HWUPD_DELAY_VOL)) {

        lastUpd = millis();
        
        int32_t t = rotEncPos;
        rotEncPos = getEncPos();

        curVol += (rotEncPos - t);

        if(curVol < 0)        curVol = 0;
        else if(curVol > 19)  curVol = 19;
    }
    
    return curVol;
}

#ifdef REMOTE_DBG
int32_t newRead = 0, oldRead = 0;
#endif

int32_t REMRotEnc::getEncPos()
{
    uint8_t buf[4];

    switch(_st) {
      
    case REM_RE_TYPE_ADA4991:
        read(SEESAW_ENCODER_BASE, SEESAW_ENCODER_POSITION, buf, 4);
        // Ada4991 reports neg numbers when turned cw, so 
        // negate value
        return -(int32_t)(
                    ((uint32_t)buf[0] << 24) | 
                    ((uint32_t)buf[1] << 16) |
                    ((uint32_t)buf[2] <<  8) | 
                     (uint32_t)buf[3]
                );

    case REM_RE_TYPE_DUPPAV2:
        read(DUV2_BASE, DUV2_CVALB4, buf, 4);
        return (int32_t)(
                    ((uint32_t)buf[0] << 24) | 
                    ((uint32_t)buf[1] << 16) |
                    ((uint32_t)buf[2] <<  8) | 
                     (uint32_t)buf[3]
                );

    case REM_RE_TYPE_DFRGR360:
        read(DFR_BASE, DFR_COUNT_MSB, buf, 2);
        return (int32_t)(((buf[0] << 8) | buf[1]) / dfrgain);
        
    case REM_RE_TYPE_CS:
        break;

    case REM_RE_TYPE_ADS1X15:
        read(ADS_BASE, ADS_CONVERT, buf, 2);
        #ifdef REMOTE_DBG_ADC
        newRead = (int32_t)(((int16_t)((buf[0] << 8) | buf[1]))) / 16;
        if(oldRead != newRead) {
            Serial.printf("Read: %d\n", newRead);
            oldRead = newRead;
        }
        #endif
        return (int32_t)(((int16_t)((buf[0] << 8) | buf[1]))) / 16;
    }

    return 0;
}

bool REMRotEnc::zeroEnc()
{
    uint8_t buf[4] = { 0 };
    uint16_t temp;

    // For rotary encoder only: Sets encoder value to "0". 
    // We don't do this for encoders with a 32bit 
    // range, since turning it that far probably 
    // exceeds the encoder's life span anyway.
    
    switch(_st) {
    case REM_RE_TYPE_DFRGR360:   
        temp = 10 * dfrgain;
        buf[0] = temp >> 8;
        buf[1] = temp & 0xff;
        write(DFR_BASE, DFR_COUNT_MSB, &buf[0], 2);
        return true;
    }
    
    return false;
}

int REMRotEnc::read(uint16_t base, uint8_t reg, uint8_t *buf, uint8_t num)
{
    int i2clen;
    
    Wire.beginTransmission(_i2caddr);
    if(base <= 0xff) Wire.write((uint8_t)base);
    Wire.write(reg);
    Wire.endTransmission(true);
    delay(1);
    i2clen = Wire.requestFrom(_i2caddr, (int)num);
    for(int i = 0; i < i2clen; i++) {
        buf[i] = Wire.read();
    }
    return i2clen;
}

void REMRotEnc::write(uint16_t base, uint8_t reg, uint8_t *buf, uint8_t num)
{
    Wire.beginTransmission(_i2caddr);
    if(base <= 0xff) Wire.write((uint8_t)base);
    Wire.write(reg);
    for(int i = 0; i < num; i++) {
        Wire.write(buf[i]);
    }
    Wire.endTransmission();
}

/*
 * RemButton class
 */

RemButton::RemButton()
{
}

/* pin: The pin to be used
 * activeLow: Set to true when the input level is LOW when the button is pressed, Default is true.
 * pullupActive: Activate the internal pullup when available. Default is true.
 */
void RemButton::begin(const int pin, const bool activeLow, const bool pullupActive, const bool pulldownActive)
{
    _pin = pin;

    _buttonPressed = activeLow ? LOW : HIGH;
  
    pinMode(pin, pullupActive ? INPUT_PULLUP : (pulldownActive ? INPUT_PULLDOWN : INPUT));
}


// debounce: Number of millisec that have to pass by before a click is assumed stable
// lPress:   Number of millisec that have to pass by before a long press is detected
void RemButton::setTiming(const int debounceDur, const int lPressDur, const int elPressDur)
{
    _debounceDur = debounceDur;
    _longPressDur = lPressDur;
    _elongPressDur = elPressDur;
}

// Register function for press-down event
void RemButton::attachPressDown(void (*newFunction)())
{
    _pressDownFunc = newFunction;
}

// Register function for short press event
void RemButton::attachPressEnd(void (*newFunction)())
{
    _pressEndFunc = newFunction;
}

// Register function for long press start event
void RemButton::attachLongPressStart(void (*newFunction)())
{
    _longPressStartFunc = newFunction;
}

// Register function for long press stop event
void RemButton::attachLongPressStop(void (*newFunction)())
{
    _longPressStopFunc = newFunction;
}

// Register function for extra long press start event
void RemButton::attachELongPressStart(void (*newFunction)(void))
{
    _elongPressStartFunc = newFunction;
}

// Register function for extra long press stop event
void RemButton::attachELongPressStop(void (*newFunction)(void))
{
    _elongPressStopFunc = newFunction;
}

// Check input of the pin and advance the state machine
void RemButton::scan()
{
    unsigned long now = millis();
    unsigned long waitTime = now - _startTime;
    bool active = (digitalRead(_pin) == _buttonPressed);
    
    switch(_state) {
    case REMBUS_IDLE:
        if(active) {
            transitionTo(REMBUS_PRESSED);
            _startTime = now;
        }
        break;

    case REMBUS_PRESSED:
        if((!active) && (waitTime < _debounceDur)) {  // de-bounce
            transitionTo(_lastState);
        } else if((active) && (waitTime > _longPressDur)) {
            if(_longPressStartFunc) _longPressStartFunc();
            transitionTo(REMBUS_HOLD);
        } else {
            if(!_wasPressed) {
                if(_pressDownFunc) _pressDownFunc();
                _wasPressed = true;
            }
            if(!active) {
                transitionTo(REMBUS_RELEASED);
                _startTime = now;
            }
        }
        break;

    case REMBUS_RELEASED:
        if((active) && (waitTime < _debounceDur)) {  // de-bounce
            transitionTo(_lastState);
        } else if(!active) {
            if(_pressEndFunc) _pressEndFunc();
            reset();
        }
        break;
  
    case REMBUS_HOLD:
        if(!active) {
            transitionTo(REMBUS_HOLDEND);
            _startTime = now;
        } else if(_elongPressDur && (waitTime > _elongPressDur)) {
            if(_elongPressStartFunc) _elongPressStartFunc(); 
            transitionTo(REMBUS_EHOLD);
        }
        break;

    case REMBUS_HOLDEND:
        if((active) && (waitTime < _debounceDur)) { // de-bounce
            transitionTo(_lastState);
        } else if(waitTime >= _debounceDur) {
            if(_longPressStopFunc) _longPressStopFunc();
            reset();
        }
        break;

    case REMBUS_EHOLD:
        if(!active) {
            transitionTo(REMBUS_EHOLDEND);
            _startTime = now;
        }
        break;

    case REMBUS_EHOLDEND:
        if((active) && (waitTime < _debounceDur)) { // de-bounce
            transitionTo(_lastState);
        } else if(waitTime >= _debounceDur) {
            if(_elongPressStopFunc) _elongPressStopFunc();
            reset();
        }
        break;

    default:
        transitionTo(REMBUS_IDLE);
        break;
    }
}

/*
 * Private
 */

void RemButton::reset(void)
{
    _state = REMBUS_IDLE;
    _lastState = REMBUS_IDLE;
    _startTime = 0;
    _wasPressed = false;
}

// Advance to new state
void RemButton::transitionTo(ButState nextState)
{
    _lastState = _state;
    _state = nextState;
}

/*
 * Buttonpack Class
 */
ButtonPack::ButtonPack(int numTypes, const uint8_t *addrArr)
{
    _numTypes = min(4, numTypes);
    _addrArr = addrArr;
}

bool ButtonPack::begin()
{
    bool foundSt = false;

    for(int i = 0; i < _numTypes * 2; i += 2) {

        _i2caddr = _addrArr[i];

        Wire.beginTransmission(_i2caddr);
        if(!Wire.endTransmission(true)) {

            switch(_addrArr[i+1]) {
            case REM_BP_TYPE_PCA8574:
                foundSt = true;
                _pack_size = 8;
                break;

            case REM_BP_TYPE_PCA9554:
                foundSt = true;
                _pack_size = 8;
                break;
            }

            if(foundSt) {
                _st = _addrArr[i+1];
    
                #ifdef REMOTE_DBG
                const char *tpArr[2] = { "PCA8574(A)", "PCA9554(A)" };
                Serial.printf("ButtonPack: Detected %s\n", tpArr[_st]);
                #endif
    
                break;
            }
        }
    }

    switch(_st) {
      
    case REM_BP_TYPE_PCA8574:
        // Reset all pins to INPUT
        port_write(0, 0xff);
        break;

    case REM_BP_TYPE_PCA9554:
        port_write(2, 0x00);
        // Reset all pins to INPUT
        port_write(3, 0xff);
        break;
        
    default:
        return false;
    }

    for(int i = 0; i < _pack_size; i++) {
        _state[i] = _lastState[i] = REMBUS_IDLE;
        _wasPressed[i] = false;
        _startTime[i] = 0;
        _debounceDur[i] = 50;
        _longPressDur[i] = 2000;
    }

    return true;
}

// scanInterval: ms
void ButtonPack::setScanInterval(const unsigned long scanInterval)
{
    _scanInterval = scanInterval;
}

// debounce: Number of millisec that have to pass by before a click is assumed stable
// lPress:   Number of millisec that have to pass by before a long press is detected
void ButtonPack::setTiming(int idx, const int debounceDur, const int lPressDur)
{
    if(idx < 0 || idx >= _pack_size)
        return;
        
    _debounceDur[idx] = debounceDur;
    _longPressDur[idx] = lPressDur;
}

// Register function for press-down event
void ButtonPack::attachPressDown(void (*newFunction)(int))
{
    _pressDownFunc = newFunction;
}

// Register function for short press event
void ButtonPack::attachPressEnd(void (*newFunction)(int))
{
    _pressEndFunc = newFunction;
}

// Register function for long press start event
void ButtonPack::attachLongPressStart(void (*newFunction)(int))
{
    _longPressStartFunc = newFunction;
}

// Register function for long press stop event
void ButtonPack::attachLongPressStop(void (*newFunction)(int))
{
    _longPressStopFunc = newFunction;
}

int ButtonPack::getPackSize()
{
    return _pack_size;
}

// Check input of the pin and advance the state machine
void ButtonPack::scan()
{
    unsigned long now = millis();
    unsigned long waitTime; 
    uint8_t  port;
    bool     active;

    if(millis() - _lastScan < _scanInterval)
        return;

    _lastScan = millis();

    switch(_st) {
    case REM_BP_TYPE_PCA8574:
    case REM_BP_TYPE_PCA9554:
        if(port_read(&port) != 1) {
            return;
        }
        break;
    default:
        return;
    }

    for(int i = 0; i < _pack_size; i++) {

        waitTime = now - _startTime[i];
        active = ((port & (1 << i)) == 0);
    
        switch(_state[i]) {
        case REMBUS_IDLE:
            if(active) {
                transitionTo(i, REMBUS_PRESSED);
                _startTime[i] = now;
            }
            break;
    
        case REMBUS_PRESSED:
            if((!active) && (waitTime < _debounceDur[i])) {
                transitionTo(i, _lastState[i]);
            } else if((active) && (waitTime > _longPressDur[i])) {
                if(_longPressStartFunc) _longPressStartFunc(i);
                transitionTo(i, REMBUS_HOLD);
            } else {
                if(!_wasPressed[i]) {
                    if(_pressDownFunc) _pressDownFunc(i);
                    _wasPressed[i] = true;
                }
                if(!active) {
                    transitionTo(i, REMBUS_RELEASED);
                    _startTime[i] = now;
                }
            }
            break;
    
        case REMBUS_RELEASED:
            if((active) && (waitTime < _debounceDur[i])) {
                transitionTo(i, _lastState[i]);
            } else if(!active) {
                if(_pressEndFunc) _pressEndFunc(i);
                reset(i);
            }
            break;
      
        case REMBUS_HOLD:
            if(!active) {
                transitionTo(i, REMBUS_HOLDEND);
                _startTime[i] = now;
            }
            break;
    
        case REMBUS_HOLDEND:
            if((active) && (waitTime < _debounceDur[i])) {
                transitionTo(i, _lastState[i]);
            } else if(waitTime >= _debounceDur[i]) {
                if(_longPressStopFunc) _longPressStopFunc(i);
                reset(i);
            }
            break;
    
        default:
            transitionTo(i, REMBUS_IDLE);
            break;
        }
    }
}

#ifdef HAVE_CRSF
uint8_t ButtonPack::readStates()
{
    uint8_t port = 0xff;

    switch(_st) {
    case REM_BP_TYPE_PCA8574:
    case REM_BP_TYPE_PCA9554:
        if(port_read(&port) != 1) {
            return 0;
        }
        return (uint8_t)(~port);
    default:
        break;
    }

    return 0;
}

bool ButtonPack::sampleStates(uint8_t &states)
{
    uint8_t port = 0xff;

    switch(_st) {
    case REM_BP_TYPE_PCA8574:
    case REM_BP_TYPE_PCA9554:
        if(port_read(&port) != 1) {
            return false;
        }
        states = (uint8_t)(~port);
        return true;
    default:
        break;
    }

    states = 0;
    return false;
}
#endif

/*
 * Private
 */

void ButtonPack::reset(int i)
{
    _state[i] = REMBUS_IDLE;
    _lastState[i] = REMBUS_IDLE;
    _startTime[i] = 0;
    _wasPressed[i] = false;
}

// Advance to new state
void ButtonPack::transitionTo(int idx, ButState nextState)
{
    _lastState[idx] = _state[idx];
    _state[idx]     = nextState;
}

void ButtonPack::port_write(uint8_t reg, uint8_t val)
{
    switch(_st) {
    case REM_BP_TYPE_PCA8574:
        Wire.beginTransmission(_i2caddr);
        Wire.write(val);
        Wire.endTransmission();
        break;
    case REM_BP_TYPE_PCA9554:
        Wire.beginTransmission(_i2caddr);
        Wire.write(reg);
        Wire.write(val);
        Wire.endTransmission(); 
        break;
    }  
}

int ButtonPack::port_read(uint8_t *buf)
{
    int i2clen = 0;

    switch(_st) {
    case REM_BP_TYPE_PCA8574:
        i2clen = Wire.requestFrom(_i2caddr, (int)1);
        buf[0] = Wire.read();
        break;
    case REM_BP_TYPE_PCA9554:
        Wire.beginTransmission(_i2caddr);
        Wire.write(0);
        Wire.endTransmission();
        delay(1);
        i2clen = Wire.requestFrom(_i2caddr, (int)1);
        buf[0] = Wire.read();
        break;
    }
    return i2clen;
}
